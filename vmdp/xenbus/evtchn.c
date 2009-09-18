/*****************************************************************************
 *
 *   File Name:      evtchn.c
 *
 *   %version:       21 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jun 11 17:13:43 2009 %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2006-2009 Unpublished Work of Novell, Inc. All Rights Reserved.
 *
 * THIS IS AN UNPUBLISHED WORK OF NOVELL, INC.  IT CONTAINS NOVELL'S
 * CONFIDENTIAL, PROPRIETARY, AND TRADE SECRET INFORMATION.  NOVELL
 * RESTRICTS THIS WORK TO NOVELL EMPLOYEES WHO NEED THE WORK TO PERFORM
 * THEIR ASSIGNMENTS AND TO THIRD PARTIES AUTHORIZED BY NOVELL IN WRITING.
 * THIS WORK MAY NOT BE USED, COPIED, DISTRIBUTED, DISCLOSED, ADAPTED,
 * PERFORMED, DISPLAYED, COLLECTED, COMPILED, OR LINKED WITHOUT NOVELL'S
 * PRIOR WRITTEN CONSENT.  USE OR EXPLOITATION OF THIS WORK WITHOUT
 * AUTHORIZATION COULD SUBJECT THE PERPETRATOR TO CRIMINAL AND  CIVIL
 * LIABILITY.
 *****************************************************************************/

#include "xenbus.h"

#define cpu_from_evtchn(port) (0)
#define MAX_EVTCHN 256

static KSPIN_LOCK cmpxchg_lock;
static uint32_t active_evtchns;
static uint32_t masked_evtchns;

/*
 * Interrupt handling:
 * Because only the event_channel has allocate a shared irq
 * for all child devices, child devices cannot set their own
 * ISR. Different from Linux situation, in Windows, we use Dpc
 * instead of irqhandler. Child devices register their customDpc
 * to event channel routines. When event channel routines'
 * ISR runs, it queues the respective device Dpc in its own
 * queue for possible concurrent Dpc handling later.
 */
static struct
{
    PKDPC CustomDpc;
    uint32_t *wants_int_indication;
	int locked;
} evtchns[MAX_EVTCHN] = {0};

static struct event_channel
{
	int port;
	int in_use;
};

static struct
{
	int registered;
	struct event_channel chn[MAX_EVTCHN];
} registered_evtchns;

void mask_evtchn(int port)
{
    shared_info_t *s = shared_info_area;

	masked_evtchns |= (1 << port);
    InterlockedBitTestAndSetCompat(&s->evtchn_mask[0], port);
}

void unmask_evtchn(int port)
{
    ULONG cpu = KeGetCurrentProcessorNumber();
    shared_info_t *s = shared_info_area;
    vcpu_info_t *vcpu_info = &s->vcpu_info[cpu];
	evtchn_unmask_t op;

	DPR_INIT(("unmask_evtchn: mask %x, port %x on cpu %d IN\n",
		s->evtchn_mask[0], port, cpu));
	masked_evtchns &= ~(1 << port);

	/* Seems that even on cpu 0, still need to do the hypercall. */
	op.port = port;
	HYPERVISOR_event_channel_op(EVTCHNOP_unmask, &op);

	DPR_INIT(("unmask_evtchn: HYPERVISOR_event_channel_op mask %x OUT\n",
		s->evtchn_mask[0]));
#if 0
    if (cpu != cpu_from_evtchn(port)) {
        op.port = port;
		DPR_INIT(("unmask_evtchn: EVTCHNOP_unmask %x\n", port));
        HYPERVISOR_event_channel_op(EVTCHNOP_unmask, &op);

		DPR_INIT(("unmask_evtchn: HYPERVISOR_event_channel_op mask %x OUT\n",
			s->evtchn_mask[0]));
        return;
    }

	DPRINT(("unmask_evtchn cpu = %x\n", cpu));
    InterlockedBitTestAndResetCompat(&s->evtchn_mask[0], port);

    if (BitTestCompat(&s->evtchn_pending[0], port) &&
        !InterlockedBitTestAndSetCompat(
          &vcpu_info->evtchn_pending_sel,
          port / (sizeof(xen_ulong_t) * 8))) {
		DPR_INIT(("unmask_evtchn: set and test\n"));
        vcpu_info->evtchn_upcall_pending = 1;
        if (!vcpu_info->evtchn_upcall_mask)
            force_evtchn_callback();
    }
	DPR_INIT(("unmask_evtchn: mask %x OUT\n", s->evtchn_mask[0]));
#endif
}

static int find_evtchn(int port)
{
	int i;

	for (i = 0; i < MAX_EVTCHN; i++) {
		if (registered_evtchns.chn[i].port == port) {
			return i;
		}
	}
	return MAX_EVTCHN;
}

static int find_free_evtchn()
{
	int i;

	if (registered_evtchns.registered < MAX_EVTCHN) {
		i = registered_evtchns.registered;
		registered_evtchns.registered++;
		return i;
	}
	for (i = 0; i < MAX_EVTCHN; i++) {
		if (!registered_evtchns.chn[i].in_use) {
			return i;
		}
	}
	return MAX_EVTCHN;
}

/*
 * Registered Dpc routine will receive 3 additional parameters:
 * The first is the 3rd argument dpccontext passed to register_dpc
 * the second is the 4th argument context the caller registered in
 * the register_dpc. The third is the device extension of xenbus,
 * if the dpc is queued from the interrupt service routine.
 */

NTSTATUS
register_dpc_to_evtchn(ULONG evtchn,
	PKDEFERRED_ROUTINE dpcroutine,
	PVOID dpccontext,
	uint32_t *int_indication)
{
    PKDPC dpc;
    XEN_LOCK_HANDLE lh;
	char *buf;
	int channel;
    
	DPR_INIT(("register_dpc_to_evtchn: evtchn %x\n", evtchn));
    if (evtchn >= MAX_EVTCHN)
        return STATUS_INSUFFICIENT_RESOURCES;

	if (KeGetCurrentIrql() <= DISPATCH_LEVEL) {
		dpc = ExAllocatePoolWithTag(NonPagedPool, sizeof(KDPC),XENBUS_POOL_TAG);
		if (dpc == NULL)
			return STATUS_INSUFFICIENT_RESOURCES;
		KeInitializeDpc(dpc, dpcroutine, dpccontext);
		evtchns[evtchn].CustomDpc = dpc;
	}
	else
		evtchns[evtchn].CustomDpc = NULL;

	XenAcquireSpinLock(&cmpxchg_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_CMP);
	if ((channel = find_free_evtchn()) < MAX_EVTCHN) {
		registered_evtchns.chn[channel].in_use = 1;
		registered_evtchns.chn[channel].port = (int)evtchn;
		DPRINT(("register_dpc ch = %d, reg = %d, evt = %ld\n",
				 channel, registered_evtchns.registered, evtchn));
	}
	else {
		XENBUS_CLEAR_FLAG(xenbus_locks, X_CMP);
		XenReleaseSpinLock(&cmpxchg_lock, lh);
        return STATUS_INSUFFICIENT_RESOURCES;
	}
	XENBUS_CLEAR_FLAG(xenbus_locks, X_CMP);
	XenReleaseSpinLock(&cmpxchg_lock, lh);

    evtchns[evtchn].wants_int_indication = int_indication;
    evtchns[evtchn].locked = 0;
	active_evtchns |= (1 << evtchn);

    unmask_evtchn(evtchn);
	DPR_INIT(("register_dpc_to_evtchn: OUT\n"));
    return STATUS_SUCCESS;
}

VOID
unregister_dpc_from_evtchn(ULONG evtchn)
{
    PKDPC dpc;
    XEN_LOCK_HANDLE lh;
	int channel;

    if (evtchn >= MAX_EVTCHN)
        return;

	DPR_INIT(("unregister_dpc_from_evtchn: mask_evtchn\n"));
    mask_evtchn(evtchn);
    dpc = evtchns[evtchn].CustomDpc;
    evtchns[evtchn].CustomDpc = NULL;

	XenAcquireSpinLock(&cmpxchg_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_CMP);
	DPR_INIT(("unregister_dpc_from_evtchn: find_evtchn\n"));
	if ((channel = find_evtchn((int)evtchn)) < MAX_EVTCHN) {
		registered_evtchns.chn[channel].in_use = 0;
		registered_evtchns.chn[channel].port = -1;
	}
	active_evtchns &= ~(1 << evtchn);
	XENBUS_CLEAR_FLAG(xenbus_locks, X_CMP);
	XenReleaseSpinLock(&cmpxchg_lock, lh);

	if (dpc) {
		DPR_INIT(("unregister_dpc_from_evtchn: KeRemoveQueueDpc %p\n", dpc));
		KeRemoveQueueDpc(dpc);
		ExFreePool(dpc);
	}
}

void notify_remote_via_irq(int irq)
{
    int evtchn = irq;
    notify_remote_via_evtchn(evtchn);
}

void unbind_evtchn_from_irq(unsigned int evtchn)
{
	return;
}

/*
 * We use critical section to do the real ISR thing, so we can
 * ``generate'' our own interrupt
 */
#ifdef DBG
uint32_t cpu_ints = 0;
uint32_t cpu_ints_claimed = 0;

#define INC_CPU_INTS() cpu_ints++;
#define INC_CPU_INTS_CLAIMED() cpu_ints_claimed++;
#else
#define INC_CPU_INTS()
#define INC_CPU_INTS_CLAIMED()
#endif

#ifdef XENBUS_HAS_DEVICE
BOOLEAN EvtchnISR(void *context)
#else
ULONG EvtchnISR(void)
#endif
{
	shared_info_t *s;
	vcpu_info_t *v;
	PKDPC CustomDpc;
	xen_ulong_t l1, l2, l1i, port;
	ULONG ret_val = XEN_INT_NOT_XEN;

	DPRINT(("EvtchnISR: at level %d\n", KeGetCurrentIrql()));

	/* All events are bound to vcpu0, but irq may be redirected. */
	s = shared_info_area;
	v = &s->vcpu_info[0];
	v->evtchn_upcall_pending = 0;
	KeMemoryBarrier();

#ifdef DBG
	if (evt_print)
		DPR_INIT(("EvtchnISR: printing because of evt_print, irql = %d, cpu = %d\n",
				  KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
#endif
	l1 = InterlockedExchangeCompat(&v->evtchn_pending_sel, 0);
	while (l1 != 0) {
		l1i = XbBitScanForwardCompat(&l1);
		l1 &= ~(1 << l1i);
		while ((l2 = s->evtchn_pending[l1i] & ~s->evtchn_mask[l1i])) {
			port = (l1i * sizeof(xen_ulong_t) * 8)
				+ XbBitScanForwardCompat(&l2);
			InterlockedBitTestAndResetCompat(&s->evtchn_pending[0], port);
			if ((CustomDpc = evtchns[port].CustomDpc) != NULL) {
				if (evtchns[port].wants_int_indication) {
					*evtchns[port].wants_int_indication = 1;
					ret_val |= XEN_INT_DISK;
					DPRINT(("EvtchnISR: Should be for disk %x\n", CustomDpc));
					INC_CPU_INTS_CLAIMED();
				}
				else {
					DPRINT(("EvtchnISR: KeInsertQueueDpc %x\n", CustomDpc));
					if (port == xen_store_evtchn) {
						ret_val |= XEN_INT_XS;
						XENBUS_SET_FLAG(rtrace, EVTCHN_F);
					}
					else
						ret_val |= XEN_INT_LAN;
					KeInsertQueueDpc(CustomDpc, NULL, NULL);
					INC_CPU_INTS_CLAIMED();
				}
			}
		}
	}

	XenbusKeepUsSane();
	DPRINT(("EvtchnISR: return %d\n", ret_val));
#ifdef DBG
	evt_print = 0;
#endif

#ifdef XENBUS_HAS_DEVICE
    return (ret_val ? TRUE : FALSE);
#else
	return ret_val;
#endif
}

#ifdef XENBUS_HAS_DEVICE
BOOLEAN XenbusOnInterrupt(
  IN PKINTERRUPT InterruptObject,
  IN PFDO_DEVICE_EXTENSION fdx
  )
{
	DPRINT(("XenbusOnInterrupt.\n"));
    return EvtchnISR(fdx);
}
#endif

void force_evtchn_callback(void)
{
	DPRINT(("force_evtchn_callback\n"));

	DPRINT(("force_evtchn_callback: IN\n"));
#ifdef XENBUS_HAS_DEVICE
    KeSynchronizeExecution(
      DriverInterruptObj,
      EvtchnISR,
      NULL);
#else
	HYPERVISOR_xen_version(0, NULL);
	DPRINT(("force_evtchn_callback: OUT\n"));
	//EvtchnISR(gfdo);
#endif
}

NTSTATUS set_callback_irq(int irq)
{
    struct xen_hvm_param a;

	DPR_INIT(("set_callback_irq: IN %d\n", irq));
    a.domid = DOMID_SELF;
    a.index = HVM_PARAM_CALLBACK_IRQ;
    a.value = irq;
    if (HYPERVISOR_hvm_op(HVMOP_set_param, &a) != 0) {
        DBG_PRINT(("XENBUS: set callback irq fail.\n"));
        return STATUS_UNSUCCESSFUL;
    }

    DPRINT(("XENBUS: callback irq set to %d.\n", irq));
	DPR_INIT(("set_callback_irq: OUT\n"));

    return STATUS_SUCCESS;
}

void evtchn_remove_queue_dpc(void)
{
	uint32_t i;

	for (i = 0; i < MAX_EVTCHN; i++) {
		if (evtchns[i].CustomDpc) {
			if (KeRemoveQueueDpc(evtchns[i].CustomDpc)) {
				DPR_INIT(("evtchn_remove_queue_dpc: removed %d.\n", i));
			}
		}
	}
}

VOID evtchn_init(void)
{
	int i; 

    KeInitializeSpinLock(&cmpxchg_lock);
	registered_evtchns.registered = 0;
	for (i = 0; i < MAX_EVTCHN; i++) {
		if (evtchns[i].CustomDpc) {
			/* Remove any inflight DPCs due to save/restore/migrate. */
			if (KeRemoveQueueDpc(evtchns[i].CustomDpc)) {
				DPR_INIT(("evtchn_init: removed a dpc from %d.\n", i));
			}
			ExFreePool(evtchns[i].CustomDpc);
			evtchns[i].CustomDpc = NULL;
		}
		evtchns[i].locked = 0;
		evtchns[i].wants_int_indication = 0;
		registered_evtchns.chn[i].in_use = 0;
		registered_evtchns.chn[i].port = -1;
	}
	active_evtchns = 0;
	masked_evtchns = 0;
}

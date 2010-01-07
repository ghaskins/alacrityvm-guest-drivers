/*
 * Copyright (C) 2009 Novell.  All Rights Reserved.
 *
 *  pci.c: Routines for managing pci resources.
 *
 * Author:
 *	Gregory Haskins <ghaskins@novell.com>
 *	Peter W. Morreale <pmorreale@novell.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "vbus.h"
#include "precomp.h"
#include "vbus_pci.h"
#include "ioq.h"
#include "shm-signal.h"


struct vbus_pci {
	WDFSPINLOCK		lock;
	WDFINTERRUPT		interrupt;
	WDFDEVICE		bus;
	struct ioq		eventq;
	struct vbus_pci_event	*ring;
	struct vbus_pci_regs	*regs;    /* mmio */
	struct vbus_pci_signals	*signals; /* pio */
	ULONG			signals_length;
	ULONG			regs_length;
	int			enabled;
};

static struct vbus_pci vbus_pci;

struct _signal {
	LIST_ENTRY		list;
	struct shm_signal	signal;
	UINT32			handle;
	LONG			refCount;
	PPDO_DEVICE_DATA	pd;
};



/* 
 * storage for the fastcall.
 */
struct vbus_pci_fastcall_desc 	fastparams;
WDFSPINLOCK			fastlock;




/* Persistent storage for the signal operations. */
static struct shm_signal_ops _signal_ops;
static struct shm_signal_ops eventq_signal_ops;

/* Storage for the eventq notifier */
static struct ioq_notifier eventq_notifier;

/* ISR protos */
EVT_WDF_INTERRUPT_DPC		_signal_dpc;
EVT_WDF_INTERRUPT_ISR		_signal_isr;
EVT_WDF_INTERRUPT_ENABLE	_signal_enable;
EVT_WDF_INTERRUPT_DISABLE	_signal_disable;

EVT_WDF_INTERRUPT_DPC		eventq_intr_dpc;
EVT_WDF_INTERRUPT_ISR		eventq_intr_isr;
EVT_WDF_INTERRUPT_ENABLE	eventq_intr_enable;
EVT_WDF_INTERRUPT_DISABLE	eventq_intr_disable;

/*
 * -------------------
 * common routines
 * -------------------
 */

static __inline UINT64 
__pa(void *ptr)
{
	PHYSICAL_ADDRESS a = MmGetPhysicalAddress(ptr);

	return (UINT64) a.QuadPart;
}

static int
vbus_pci_bridgecall(unsigned long nr, void *data, unsigned long len)
{
	struct vbus_pci_call_desc params;
	int ret;

	params.vector = nr;
	params.len = len;
	params.datap  = __pa(data);

	/* Send over as array of 32bit words */
	len = (sizeof(params) >> 2)  + (sizeof(params) % 4);

	WdfSpinLockAcquire(vbus_pci.lock);
	WRITE_REGISTER_BUFFER_ULONG((PULONG)((PVOID)&vbus_pci.regs->bridgecall),
			((PULONG) ((PVOID) &params)), len);
	ret = READ_REGISTER_ULONG((PULONG) &vbus_pci.regs->bridgecall);
	WdfSpinLockRelease(vbus_pci.lock);

	return ret;
}

static int
vbus_pci_buscall(unsigned long nr, void *data, unsigned long len)
{
	int rc;

	WdfSpinLockAcquire(fastlock);
	fastparams.call.vector = nr;
	fastparams.call.len    = len;
	fastparams.call.datap  = __pa(data);
	WRITE_PORT_ULONG((PULONG) &vbus_pci.signals, 0);
	rc = fastparams.result;
	WdfSpinLockRelease(fastlock);

	return rc;
}


static int 
vbus_pci_setup_fastcall(void)
{
	struct vbus_pci_call_desc	params;
	int				rc;

	params.vector 	= 0;
	params.len	= sizeof(struct vbus_pci_fastcall_desc);
	params.datap 	= __pa(&fastparams);

	rc = vbus_pci_bridgecall(VBUS_PCI_BRIDGE_FASTCALL_ADD, &params,
			sizeof(params));
	return rc;
}


/*
 * -------------------
 * _signal
 * -------------------
 */


static struct _signal *
to_signal(struct shm_signal *signal)
{
       return CONTAINING_RECORD(signal, struct _signal, signal);
}

static void
_signal_init(struct shm_signal *signal,
		struct shm_signal_desc *desc, struct shm_signal_ops *ops)
{
	desc->magic = SHM_SIGNAL_MAGIC;
	desc->ver   = SHM_SIGNAL_VER;

	ShmSignalInit(signal, shm_locality_north, ops, desc);
}

static void _signal_get(struct _signal *s)
{
	InterlockedIncrement(&s->refCount);
}

static void _signal_put(struct _signal *s)
{
	if (!InterlockedDecrement(&s->refCount)) 
		VbusFree(s);
}

static int
_signal_inject(struct shm_signal *signal)
{
	struct _signal *_signal = to_signal(signal);

	WRITE_PORT_ULONG((PULONG)&vbus_pci.signals->shmsignal, _signal->handle);

	return 0;
}

static void
_signal_release(struct shm_signal *signal)
{
	struct _signal *_signal = to_signal(signal);

	_signal_put(_signal);
}

static NTSTATUS
_signal_enable(WDFINTERRUPT in, WDFDEVICE dev)
{
	PINT_DATA	id = IntGetData(in);
	struct _signal	*_signal = (struct _signal *) id->sig;

	ShmSignalEnable(&_signal->signal, 0);
	return STATUS_SUCCESS;
}

static NTSTATUS
_signal_disable(WDFINTERRUPT in, WDFDEVICE dev)
{
	PINT_DATA	id = IntGetData(in);
	struct _signal	*_signal = (struct _signal *) id->sig;

	ShmSignalDisable(&_signal->signal, 0);
	return STATUS_SUCCESS;
}



static VOID
_signal_dpc(WDFINTERRUPT in, WDFOBJECT obj) 
{
	PINT_DATA	id = IntGetData(in);
	struct _signal	*_signal = (struct _signal *) id->sig;

	_ShmSignalWakeup(&_signal->signal);
}


static BOOLEAN 
_signal_isr(WDFINTERRUPT in, ULONG msg_id)
{
	PINT_DATA	id = IntGetData(in);
	struct _signal	*_signal = (struct _signal *) id->sig;

	/* If we are currently disabled, discard this interrupt. */ 
	if (!ShmSignalTestEnabled(&_signal->signal))
		return FALSE;

	/* disable, re-enabled in the DPC... */
	ShmSignalDisable(&_signal->signal, 0);

	WdfInterruptQueueDpcForIsr(in);

	return TRUE;
}


/*
 * -------------------
 * vbus events
 * -------------------
 */

static void
event_devadd(struct vbus_pci_add_event *event)
{
	WDFCHILDLIST	list;
	PDO_ID_DESC	d;
	NTSTATUS	rc;

	vlog("eventq_devadd");
	list = WdfFdoGetDefaultChildList(vbus_pci.bus);

	WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(&d.header, sizeof(d));
	memcpy(d.type, event->type, VBUS_MAX_DEVTYPE_LEN);
	d.id = event->id;

	rc = WdfChildListAddOrUpdateChildDescriptionAsPresent(list, 
			&d.header, NULL);
	if (!NT_SUCCESS(rc)) 
		vlog("event_devadd: AddOrUpdate: %d\n", rc);
}

static void
event_devdrop(struct vbus_pci_handle_event *event)
{
	WDFCHILDLIST		list;
	WDF_CHILD_LIST_ITERATOR	it;
	NTSTATUS		rc;
	WDFDEVICE		dev;
	PPDO_DEVICE_DATA	pd;
	PDO_ID_DESC		d;

	/* 
	 * Search the child list for a child with this handle. 
	 */
	vlog("eventq_devdrop");
	list = WdfFdoGetDefaultChildList(vbus_pci.bus);
	WDF_CHILD_LIST_ITERATOR_INIT(&it, WdfRetrievePresentChildren);

	WdfChildListBeginIteration(list, &it);
	for(;;) {
		rc = WdfChildListRetrieveNextDevice(list, &it, &dev, NULL);
		if (!NT_SUCCESS(rc))
			break;
		pd = PdoGetData(dev);
		if (pd->handle == event->handle) 
			break;
	}
	WdfChildListEndIteration(list, &it);

	if (NT_SUCCESS(rc)) {
		WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(&d.header, 
				sizeof(d));
		d.id = pd->id;
		WdfChildListUpdateChildDescriptionAsMissing(list, &d.header);
	}
}

static void
event_shmsignal(struct vbus_pci_handle_event *event)
{
	struct _signal *s = (struct _signal *)(unsigned long)event->handle;

	vlog("eventq_shmsignal");
	_ShmSignalWakeup(&s->signal);
}

static void
event_shmclose(struct vbus_pci_handle_event *event)
{
	struct _signal *s= (struct _signal *)(unsigned long)event->handle;

	vlog("eventq_shmclose");
	_signal_put(s);
}

/*
 * -------------------
 * eventq routines
 * -------------------
 */



/* Invoked whenever the hypervisor ioq_signal()s our eventq */
static void
eventq_wakeup(struct ioq_notifier *notifier)
{
	struct ioq_iterator iter;
	int ret;

	vlog("eventq_wakeup");

	/* We want to iterate on the head of the in-use index */
	ret = ioq_iter_init(&vbus_pci.eventq, &iter, ioq_idxtype_inuse, 0);
	ASSERT(ret >= 0);

	ret = ioq_iter_seek(&iter, ioq_seek_head, 0, 0);
	ASSERT(ret >= 0);

	/*
	 * The EOM is indicated by finding a packet that is still owned by
	 * the south side.
	 *
	 * FIXME: This in theory could run indefinitely if the host keeps
	 * feeding us events since there is nothing like a NAPI budget.  We
	 * might need to address that
	 */
	while (!iter.desc->sown) {
		struct ioq_ring_desc *desc  = iter.desc;
		struct vbus_pci_event *event;

		event = (struct vbus_pci_event *)(unsigned long)desc->cookie;

		switch (event->eventid) {
		case VBUS_PCI_EVENT_DEVADD:
	vlog("event_wakeup:  devadd");
			event_devadd(&event->data.add);
			break;
		case VBUS_PCI_EVENT_DEVDROP:
	vlog("event_wakeup:  devdrop");
			event_devdrop(&event->data.handle);
			break;
		case VBUS_PCI_EVENT_SHMSIGNAL:
	vlog("event_wakeup:  shmsignal");
			event_shmsignal(&event->data.handle);
			break;
		case VBUS_PCI_EVENT_SHMCLOSE:
	vlog("event_wakeup:  shmclose");
			event_shmclose(&event->data.handle);
			break;
		default:
			break;
		};

		memset(event, 0, sizeof(*event));

		/* Advance the in-use head */
		ret = ioq_iter_pop(&iter, 0);
		ASSERT(ret >= 0);

	}

	/* And let the south side know that we changed the queue */
	ioq_signal(&vbus_pci.eventq, 0);
}



/*
 * -------------------
 */

static int
eventq_signal_inject(struct shm_signal *signal)
{
	/* The eventq uses the special-case handle=0 */
vlog("eventq signal inject!");
	WRITE_REGISTER_ULONG(&vbus_pci.signals->eventq, 0);

	return 0;
}

static void
eventq_signal_release(struct shm_signal *signal)
{
vlog("eventq signal release!");
	if (signal)
		VbusFree(signal);
}


/*
 * -------------------
 */

static void
eventq_ioq_release(struct ioq *ioq)
{
vlog("eventq ioq release!");
	/* released as part of the vbus_pci object */
}

static struct ioq_ops eventq_ioq_ops;

static NTSTATUS 
eventq_init(int qlen)
{
	struct ioq_iterator iter;
	int ret;
	int i;

	vbus_pci.ring = VbusAlloc(sizeof(struct vbus_pci_event) * qlen);
	if (!vbus_pci.ring)
		return STATUS_NO_MEMORY;

	/*
	 * We want to iterate on the "valid" index.  By default the iterator
	 * will not "autoupdate" which means it will not hypercall the host
	 * with our changes.  This is good, because we are really just
	 * initializing stuff here anyway.  Note that you can always manually
	 * signal the host with ioq_signal() if the autoupdate feature is not
	 * used.
	 */
	ioq_iter_init(&vbus_pci.eventq, &iter, ioq_idxtype_valid, 0);

	/*
	 * Seek to the tail of the valid index (which should be our first
	 * item since the queue is brand-new)
	 */
	ioq_iter_seek(&iter, ioq_seek_tail, 0, 0);


	/*
	 * Now populate each descriptor with an empty vbus_event and mark it
	 * valid
	 */
	for (i = 0; i < qlen; i++) {
		struct vbus_pci_event	*event = &vbus_pci.ring[i];
		size_t			len   = sizeof(*event);
		struct ioq_ring_desc	*desc  = iter.desc;

		desc->cookie = (UINT64)(unsigned long)event;
		desc->ptr    = __pa(event);
		desc->len    = len; /* total length  */
		desc->valid  = 1;

		/*
		 * This push operation will simultaneously advance the
		 * valid-tail index and increment our position in the queue
		 * by one.
		 */
		ret = ioq_iter_push(&iter, 0);
		if (ret < 0) 
			return STATUS_NO_MORE_ENTRIES;
	}

	eventq_notifier.signal = eventq_wakeup,
	vbus_pci.eventq.notifier = &eventq_notifier;

	/*
	 * And finally, ensure that we can receive notification
	 */
	ioq_notify_enable(&vbus_pci.eventq, 0);

	return STATUS_SUCCESS;
}

/*
 * -------------------
 */

static int
vbus_pci_open(void)
{
	struct vbus_pci_bridge_negotiate params;
	
	params.magic	= VBUS_PCI_ABI_MAGIC;
	params.version      = VBUS_PCI_HC_VERSION;
	params.capabilities = 0;

	return vbus_pci_bridgecall(VBUS_PCI_BRIDGE_NEGOTIATE,
				  &params, sizeof(params));
}

#define QLEN 1024

static int 
vbus_pci_eventq_register(void)
{
	struct vbus_pci_busreg params;

	params.count = 1;
	params.eventq[0].count = QLEN;
	params.eventq[0].ring  =  __pa(vbus_pci.eventq.head_desc);
	params.eventq[0].data  =  __pa(vbus_pci.ring);

	return vbus_pci_bridgecall(VBUS_PCI_BRIDGE_QREG,
				   &params, sizeof(params));
}

static NTSTATUS
eventq_ioq_init(size_t ringsize, struct ioq *ioq, struct ioq_ops *ops)
{
	struct shm_signal	*signal = NULL;
	struct ioq_ring_head	*head = NULL;
	size_t			len  = IOQ_HEAD_DESC_SIZE(ringsize);

	head = VbusAlloc(len);
	if (!head)
		return STATUS_NO_MEMORY;

	signal = VbusAlloc(sizeof(*signal));
	if (!signal) {
		VbusFree(head);
		return STATUS_NO_MEMORY;
	}

	head->magic     = IOQ_RING_MAGIC;
	head->ver	= IOQ_RING_VER;
	head->count     = ringsize;

	eventq_signal_ops.inject  = eventq_signal_inject,
	eventq_signal_ops.release = eventq_signal_release,
	_signal_init(signal, &head->signal, &eventq_signal_ops);

	ioq_init(ioq, ops, ioq_locality_north, head, signal, ringsize);

	return STATUS_SUCCESS;
}
static NTSTATUS
eventq_intr_enable(WDFINTERRUPT in, WDFDEVICE dev)
{
	vlog("eventq interrupt enable");
	ShmSignalEnable(vbus_pci.eventq.signal, 0);
	return STATUS_SUCCESS;
}

static NTSTATUS
eventq_intr_disable(WDFINTERRUPT in, WDFDEVICE dev)
{
	vlog("eventq interrupt disable");
	ShmSignalDisable(vbus_pci.eventq.signal, 0);
	return STATUS_SUCCESS;
}

static VOID 
eventq_intr_dpc(WDFINTERRUPT in, WDFOBJECT dev)
{
	vlog("eventq dpc");
	_ShmSignalWakeup(vbus_pci.eventq.signal);
	ShmSignalEnable(vbus_pci.eventq.signal, 0);
}

static BOOLEAN 
eventq_intr_isr(WDFINTERRUPT in, ULONG mid)
{
	vlog("*** eventq INTERUPPT ***");

	/* If we are currently disabled, discard this interrupt. */
	if (!ShmSignalTestEnabled(vbus_pci.eventq.signal))
		return FALSE;

	/* disable, re-enabled in the DPC... */
	ShmSignalDisable(vbus_pci.eventq.signal, 0);

	WdfInterruptQueueDpcForIsr(in);

	return TRUE;
}


static NTSTATUS
vbus_pci_parse_resource(WDFCMRESLIST rt, ULONG i)
{
	PCM_PARTIAL_RESOURCE_DESCRIPTOR	d;
	ULONGLONG			len;

	d = WdfCmResourceListGetDescriptor(rt, i);
	if (!d)
		return STATUS_DEVICE_CONFIGURATION_ERROR;

	switch (d->Type) {
		case CmResourceTypePort:
			vbus_pci.signals = ULongToPtr(d->u.Port.Start.LowPart);
			vbus_pci.signals_length = d->u.Port.Length;
			break;

		case CmResourceTypeMemory:
			vbus_pci.regs = MmMapIoSpace(d->u.Memory.Start,
					d->u.Memory.Length,
					MmNonCached);
			vbus_pci.regs_length = d->u.Memory.Length;
			break;

		case CmResourceTypeInterrupt:
			break;

	}

	return STATUS_SUCCESS;
}


static void 
vbus_pci_cleanup(void)
{
	/* Perform all cleanups for a shutdown or restart */

	if (vbus_pci.regs) 
	        MmUnmapIoSpace(vbus_pci.regs, vbus_pci.regs_length);
	VbusFree(vbus_pci.eventq.head_desc);
	VbusFree(vbus_pci.eventq.signal);
	VbusFree(vbus_pci.ring);
	memset(&vbus_pci, '\0', sizeof(vbus_pci));
}

static NTSTATUS
vbus_pci_map_resources(WDFCMRESLIST rt)
{
	NTSTATUS	rc = STATUS_DEVICE_CONFIGURATION_ERROR;
   	ULONG		i;
	ULONG		cnt;

	for (i = 0; i < WdfCmResourceListGetCount(rt); i++) {
		rc = vbus_pci_parse_resource(rt, i);
		if (!NT_SUCCESS(rc))
			break;
	}
	return rc;
}

/*
 * Init the Vbus.
 */
NTSTATUS
VbusPciPrepareHardware(WDFDEVICE dev, WDFCMRESLIST rt)
{
	NTSTATUS 		rc;

	if (vbus_pci.enabled)
		return STATUS_DEVICE_ALREADY_ATTACHED;

	/* Set the bus device */
	vbus_pci.bus = dev;

	/* Create the spin used for bridgecalls */
	rc = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &vbus_pci.lock);
	if (!NT_SUCCESS(rc))
		return rc;

	/* Create the spin used for buscalls */
	rc = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &fastlock);
	if (!NT_SUCCESS(rc))
		return rc;

	/* Get and map our pci hw resources */
	rc = vbus_pci_map_resources(rt);
	if (!NT_SUCCESS(rc))
		goto out_fail;

	/* Negotiate and verify */
	rc = vbus_pci_open();
	if (rc < 0) {
		rc = STATUS_DEVICE_PROTOCOL_ERROR;
		goto out_fail;
	}

	/* Setup a single fastcall region.  Only one for now */
	rc = vbus_pci_setup_fastcall();
	if (rc)  {
		rc = STATUS_DEVICE_CONFIGURATION_ERROR;
		goto out_fail;
	}

	/*
	 * Allocate an IOQ to use for host-2-guest event notification
	 */
	eventq_ioq_ops.release = eventq_ioq_release;
	rc = eventq_ioq_init(QLEN, &vbus_pci.eventq, &eventq_ioq_ops);
	if (!NT_SUCCESS(rc))
		goto out_fail;

	eventq_ioq_ops.release = eventq_ioq_release;
	rc = eventq_init(QLEN);
	if (!NT_SUCCESS(rc))
		goto out_fail;

	/*
	 * Finally register our queue on the host to start receiving events
	 */
	rc = vbus_pci_eventq_register();
	if (rc < 0) {
		rc = STATUS_DEVICE_PROTOCOL_ERROR;
		goto out_fail;
	}

	vbus_pci.enabled = 1;

	return STATUS_SUCCESS;

out_fail:
	vbus_pci_cleanup();
	return rc;
}

/* 
 * Create our interrupt resources 
 */
NTSTATUS 
VbusPciCreateResources(WDFDEVICE dev) 
{
	NTSTATUS                rc;
	WDF_INTERRUPT_CONFIG    ic;

	WDF_INTERRUPT_CONFIG_INIT(&ic, eventq_intr_isr, eventq_intr_dpc);
	ic.EvtInterruptEnable  = eventq_intr_enable;
	ic.EvtInterruptDisable = eventq_intr_disable;
	rc = WdfInterruptCreate(dev, &ic, WDF_NO_OBJECT_ATTRIBUTES, 
			&vbus_pci.interrupt);
	return rc;
}

NTSTATUS
VbusPciReleaseHardware(void)
{
	vbus_pci_cleanup();
	return STATUS_SUCCESS;
}

NTSTATUS 
VbusPciD0Entry(WDFDEVICE dev, WDF_POWER_DEVICE_STATE prev_state)
{
vlog("VbusPciD0Entry: prev_state = %d", prev_state);
	ShmSignalEnable(vbus_pci.eventq.signal, 0);
	return STATUS_SUCCESS;
}

NTSTATUS 
VbusPciD0Exit(WDFDEVICE dev, WDF_POWER_DEVICE_STATE next_state)
{
vlog("VbusPciD0Exit: next_state = %d", next_state);
	ShmSignalDisable(vbus_pci.eventq.signal, 0);
	return STATUS_SUCCESS;
}

int
VbusBridgeCall(unsigned long nr, void *data, unsigned long len)
{
	return (vbus_pci_bridgecall(nr, data, len));
}

int
VbusPciOpen(UINT64 id, UINT64 *bh)
{
	struct vbus_pci_deviceopen 	params;
	int 				rc;

	params.devid   = (UINT32) id;
	params.version = VENET_VERSION;
	params.handle = 0;

	rc = vbus_pci_buscall(VBUS_PCI_HC_DEVOPEN, &params, sizeof(params));
	if (rc < 0)
		return -1;

	*bh = params.handle;

	return 0;
}

void
VbusPciClose(UINT64 bh)
{
	/*
	 * The DEVICECLOSE will implicitly close all of the shm on the
	 * host-side, so there is no need to do an explicit per-shm
	 * hypercall
	 */
	if (bh) {
		vbus_pci_buscall(VBUS_PCI_HC_DEVCLOSE, 
			&bh, sizeof(bh));
	}
}

NTSTATUS
VbusProxyDeviceCall(PDEVICE_OBJECT pdo, UINT32 func, void *data, 
		size_t len, int flags)
{
	PPDO_DEVICE_DATA                pd;
	WDFDEVICE		 	dev;
	struct vbus_pci_devicecall 	params;
	int				rc;

	/* Get the PDO context */
	dev = WdfWdmDeviceGetWdfDeviceHandle(pdo);
	pd = PdoGetData(dev);

	if (!pd->handle)
		return STATUS_INVALID_HANDLE;

	params.devh  = pd->handle;
	params.func  = func;
	params.datap = __pa(data);
	params.len   = len;
	params.flags = flags;

	rc = vbus_pci_buscall(VBUS_PCI_HC_DEVCALL, &params, sizeof(params));
	if (rc < 0) 
		return STATUS_INVALID_DEVICE_REQUEST;

	return STATUS_SUCCESS;
}

NTSTATUS
VbusProxyDeviceShm(PDEVICE_OBJECT pdo, int prio, void *ptr, size_t len,
		    struct shm_signal_desc *sdesc, struct shm_signal **signal,
		    int flags)
{
	PPDO_DEVICE_DATA                pd;
	WDFDEVICE		 	dev;
	struct _signal 			*_signal = NULL;
	struct vbus_pci_deviceshm 	params;
	WDF_INTERRUPT_CONFIG		c;
	WDF_OBJECT_ATTRIBUTES		attr;
	PINT_DATA			id;
	NTSTATUS			status;
	int				rc;

	/* Get the PDO context */
	dev = WdfWdmDeviceGetWdfDeviceHandle(pdo);
	pd = PdoGetData(dev);

	if (!pd->handle)
		return STATUS_INVALID_HANDLE;

	params.devh   = pd->handle;
	params.id     = (UINT32) pd->id;
	params.flags  = flags;
	params.datap  = __pa(ptr);
	params.len    = len;

	if (signal) {
		/*
		 * The signal descriptor must be embedded within the
		 * provided ptr
		 */
		if (!sdesc || (len < sizeof(*sdesc)) 
			|| (UINT64)sdesc < (UINT64)ptr
		    	|| (UINT64)sdesc > ((UINT64)ptr + len - sizeof(*sdesc)))
			return STATUS_INVALID_PARAMETER;

		_signal = VbusAlloc(sizeof(*_signal));
		if (!_signal)
			return STATUS_NO_MEMORY;

		_signal_ops.inject  = _signal_inject,
		_signal_ops.release = _signal_release,
		_signal_init(&_signal->signal, sdesc, &_signal_ops);
		_signal_get(_signal);

		_signal->pd = pd;

		params.signal.offset = (UINT64)(unsigned long)sdesc -
					(UINT64)(unsigned long)ptr;
		params.signal.prio   = prio;
		params.signal.cookie = (UINT64)(unsigned long)_signal;

	} else
		params.signal.offset = -1; /* this is a UINT32, but its ok */

	status = STATUS_INVALID_DEVICE_REQUEST;
	rc = vbus_pci_buscall(VBUS_PCI_HC_DEVSHM, &params, sizeof(params));
	if (rc < 0) 
		goto fail;

	if (signal) {
		/* Create the interrupt object */
		WDF_INTERRUPT_CONFIG_INIT(&c, _signal_isr, _signal_dpc);
		WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, INT_DATA);
		c.EvtInterruptEnable  = _signal_enable;
		c.EvtInterruptDisable = _signal_disable;
		status = WdfInterruptCreate(dev, &c, &attr, &pd->interrupt);
		if (!NT_SUCCESS(status))
			goto fail;
		id = IntGetData(pd->interrupt);
		id->sig = _signal;

		/* Add to the list */
		WdfSpinLockAcquire(vbus_pci.lock);
		InsertTailList(&pd->shms, &_signal->list);
		WdfSpinLockRelease(vbus_pci.lock);
		*signal = &_signal->signal;
	}

	return STATUS_SUCCESS;

fail:
	VbusFree(_signal);
	return status;
}

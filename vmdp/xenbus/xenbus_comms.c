/*****************************************************************************
 *
 *   File Name:      xenbus_comms.c
 *
 *   %version:       20 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jun 11 16:17:23 2009 %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2006-2008 Unpublished Work of Novell, Inc. All Rights Reserved.
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


#if 0
static KSPIN_LOCK xenbus_dpc_lock;
static PIO_WORKITEM xenbus_watch_work_item;
uint32_t xenbus_watch_work_scheduled;

static KEVENT xb_event;

void
XenbusDpcRoutine(
	IN PKDPC Dpc,
	IN PVOID DpcContext,
	IN PVOID RegisteredContext,
	IN PVOID DeviceExtension)
{
	XEN_LOCK_HANDLE lh;

	DPR_WAIT(("XenbusDpcRoutine: cpu %x IN\n", KeGetCurrentProcessorNumber()));
	XenAcquireSpinLock(&xenbus_dpc_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_DPC);
	xb_read_msg();
	XENBUS_CLEAR_FLAG(rtrace, EVTCHN_F);
	DPR_WAIT(("XenbusDpcRoutine: signaling xb_event\n"));
	KeSetEvent(&xb_event, 0, FALSE);
	if (!IsListEmpty(&watch_events)) {
		if (!xenbus_watch_work_scheduled) {
			xenbus_watch_work_scheduled = 1;
			if ((xenbus_watch_work_item =
					IoAllocateWorkItem((PDEVICE_OBJECT)DpcContext)) != NULL) {
				DPR_WAIT(("XenbusDpcRoutine: IoQueueWorkItem\n"));
				IoQueueWorkItem(xenbus_watch_work_item,
					(void (*)(PDEVICE_OBJECT, void *))xenbus_watch_work,
					DelayedWorkQueue, xenbus_watch_work_item);
			}
		}
	}
	XENBUS_CLEAR_FLAG(xenbus_locks, X_DPC);
	XenReleaseSpinLock(&xenbus_dpc_lock, lh);
	DPR_WAIT(("XenbusDpcRoutine: cpu %x OUT\n", KeGetCurrentProcessorNumber()));
}

static void *xenbus_get_output_chunck(XENSTORE_RING_IDX cons,
	XENSTORE_RING_IDX prod,
	char *buf, uint32_t *len)
{
	*len = XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(prod);
	if ((XENSTORE_RING_SIZE - (prod - cons)) < *len)
		*len = XENSTORE_RING_SIZE - (prod - cons);
	return buf + MASK_XENSTORE_IDX(prod);
}

const void *xenbus_get_input_chunk(XENSTORE_RING_IDX cons,
	XENSTORE_RING_IDX prod,
	const char *buf, uint32_t *len)
{
	*len = XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(cons);
	if ((prod - cons) < *len)
		*len = prod - cons;
	return buf + MASK_XENSTORE_IDX(cons);
}

int xenbus_check_indexes(XENSTORE_RING_IDX cons, XENSTORE_RING_IDX prod)
{
	return ((prod - cons) <= XENSTORE_RING_SIZE);
}

int xb_write(const void *buf, unsigned int len)
{
	struct xenstore_domain_interface *intf;
	XENSTORE_RING_IDX cons, prod;
	int rc;
	PUCHAR data = (PUCHAR) buf;
	LARGE_INTEGER timeout;

	intf = xen_store_interface;
	timeout.QuadPart = 0;

	DPR_WAIT(("xb_write: In\n"));
	while (len != 0) {
		PUCHAR dst;
		unsigned int avail;
		NTSTATUS status;

		for (;;) {
			if ((intf->req_prod - intf->req_cons) != XENSTORE_RING_SIZE) {
				DPR_WAIT(("xb_write: break\n"));
				break;
			}

			DPR_WAIT(("xb_write: KeWaitForSingleObject\n"));
			XENBUS_SET_FLAG(xenbus_wait_events, XB_EVENT);
			status = KeWaitForSingleObject(
				&xb_event,
				Executive,
				KernelMode,
				FALSE,
				&timeout);
			if (status != STATUS_SUCCESS) {
				XENBUS_SET_FLAG(rtrace, XB_WRITE_F);
				XenbusDpcRoutine(NULL, gfdo, NULL, NULL);
				XENBUS_CLEAR_FLAG(rtrace, XB_WRITE_F);
			}
			XENBUS_CLEAR_FLAG(xenbus_wait_events, XB_EVENT);
			DPR_WAIT(("xb_write: KeWaitForSingleObject done\n"));

			status = KeWaitForSingleObject(
				&thread_xenbus_kill,
				Executive,
				KernelMode,
				FALSE,
				&timeout);
			if (status == STATUS_SUCCESS || status == STATUS_ALERTED) {
				DPR_WAIT(("xb_write: return -1\n"));
				return -1;
			}

			DPR_WAIT(("xb_write: KeClearEvent\n"));
			KeClearEvent(&xb_event);
		}

		/* Read indexes, then verify. */
		cons = intf->req_cons;
		prod = intf->req_prod;
		KeMemoryBarrier();

		DPR_WAIT(("xb_write: xenbus_check_indexes\n"));
		if (!xenbus_check_indexes(cons, prod)) {
			intf->req_cons = intf->req_prod = 0;
			DPR_WAIT(("XENBUS: xenstore ring overflow! reset.\n"));
			return -EIO;
		}

		DPR_WAIT(("xb_write: xenbus_get_output_chunck\n"));
		dst = xenbus_get_output_chunck(cons, prod, intf->req, &avail);
		if (avail == 0)
			continue;
		if (avail > len)
			avail = len;

		RtlCopyMemory(dst, data, avail);
		data += avail;
		len -= avail;

		KeMemoryBarrier();
		intf->req_prod += avail;

		DPR_WAIT(("xb_write: notify_remote_via_evtchn\n"));
		notify_remote_via_evtchn(xen_store_evtchn);
	}

	DPR_WAIT(("xb_write: Out\n"));
	return 0;
}

/* Set up interrupt handler off store event channel. */
NTSTATUS xb_comms_init(PDEVICE_OBJECT fdo, uint32_t flags)
{
	NTSTATUS status;

	DPR_INIT(("xb_comms_init: %p.\n", fdo));

	if (flags & XENBUS_CRASHDUMP_INIT) {
		return STATUS_SUCCESS;
	}

	status = register_dpc_to_evtchn(xen_store_evtchn,
		XenbusDpcRoutine, fdo, NULL);
	if (!NT_SUCCESS(status)) {
		DBG_PRINT(("XENBUS: request Dpc failed.\n"));
		return status;
	}

	xenbus_watch_work_scheduled = 0;

	if (flags & XENBUS_RESUME_INIT) {
		KeClearEvent(&xb_event);
	}
	else {
		KeInitializeSpinLock(&xenbus_dpc_lock);
		KeInitializeEvent(&xb_event, NotificationEvent, FALSE);
	}

	return STATUS_SUCCESS;
}

VOID xb_comms_cleanup(void)
{
	if (xen_store_evtchn)
		unregister_dpc_from_evtchn(xen_store_evtchn);
#ifdef XENBUS_HAS_DEVICE
	KeSetEvent(&xb_event, 0, FALSE);
#endif
}
#endif

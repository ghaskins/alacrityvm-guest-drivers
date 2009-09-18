/*****************************************************************************
 *
 *   File Name:      xenbus_client.c
 *
 *   %version:       42 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jul 27 14:43:28 2009 %
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

KSPIN_LOCK xenbus_print_lock;
struct xenbus_watch vbd_watch = {0};

static uint32_t xenbus_thread_count = 0;

int
xenbus_grant_ring(domid_t otherend_id, unsigned long ring_mfn)
{
    int err;
    err = gnttab_grant_foreign_access(otherend_id, ring_mfn, 0);
    if (err < 0)
        DBG_PRINT(("XENBUS: granting access to ring page fail.\n"));
    return err;
}

int
xenbus_alloc_evtchn(domid_t otherend_id, int *port)
{
    struct evtchn_alloc_unbound alloc_unbound;
    int err;

    alloc_unbound.dom = DOMID_SELF;
    alloc_unbound.remote_dom = otherend_id;

    err = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound,
                                      &alloc_unbound);

    if (err)
        DBG_PRINT(("XENBUS: allocating event channel fail.\n"));
    else
        *port = alloc_unbound.port;

    return err;
}

int
xenbus_bind_evtchn(domid_t otherend_id, int remote_port, int *port)
{
    struct evtchn_bind_interdomain bind_interdomain;
    int err;

    bind_interdomain.remote_dom = otherend_id;
    bind_interdomain.remote_port = remote_port;

    err = HYPERVISOR_event_channel_op(
      EVTCHNOP_bind_interdomain,
      &bind_interdomain);

    if (err)
        DBG_PRINT(("XENBUS: binding event channel %d from domain %d, fail.\n",
                 remote_port, otherend_id));
    else
        *port = bind_interdomain.local_port;

    return err;
}

int
xenbus_free_evtchn(int port)
{
    struct evtchn_close close;
    int err;

    close.port = port;

    err = HYPERVISOR_event_channel_op(
      EVTCHNOP_close, &close);
    if (err)
        DBG_PRINT(("XENBUS: freeing event channel %d fail.\n", port));

    return err;
}

static BOOLEAN
xenbus_is_valid_pdo(PDEVICE_OBJECT pdo)
{
    PFDO_DEVICE_EXTENSION fdx;
    PPDO_DEVICE_EXTENSION pdx;
    PLIST_ENTRY entry;

	if (gfdo) {
		fdx = (PFDO_DEVICE_EXTENSION) gfdo->DeviceExtension;
		for (entry = fdx->ListOfPDOs.Flink;
				entry != &fdx->ListOfPDOs;
				entry = entry->Flink) {
			pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
			if (pdx->Self == pdo) {
				return TRUE;
			}
		}
	}
	else {
		DBG_PRINT(("xenbus_is_valid_pdo: gfdo is NULL\n"));
	}
	return FALSE;
}

char *
xenbus_get_nodename_from_pdo(PDEVICE_OBJECT pdo)
{
    PPDO_DEVICE_EXTENSION pdx;

	if (xenbus_is_valid_pdo(pdo)) {
		pdx = (PPDO_DEVICE_EXTENSION) pdo->DeviceExtension;

		DPR_INIT(("xenbus_get_nodename_from_pdo %p, %s.\n",pdo, pdx->Nodename));
		return pdx->Nodename;
	}
	return NULL;
}

char *
xenbus_get_otherend_from_pdo(PDEVICE_OBJECT pdo)
{
    PPDO_DEVICE_EXTENSION pdx;

	if (xenbus_is_valid_pdo(pdo)) {
		pdx = (PPDO_DEVICE_EXTENSION) pdo->DeviceExtension;

		DPR_INIT(("xenbus_get_otherend_from_pdo %p, %s.\n",pdo, pdx->Otherend));
		return pdx->Otherend;
	}
	return NULL;
}

char *
xenbus_get_backendid_from_pdo(PDEVICE_OBJECT pdo)
{
    PPDO_DEVICE_EXTENSION pdx;

	if (xenbus_is_valid_pdo(pdo)) {
		pdx = (PPDO_DEVICE_EXTENSION) pdo->DeviceExtension;

		DPR_INIT(("xenbus_get_backendid_from_pdo %p %s\n",pdo, pdx->BackendID));
		return pdx->BackendID;
	}
	return NULL;
}

static PPDO_DEVICE_EXTENSION
xenbus_find_pdx_from_dev(void *dev)
{
    PFDO_DEVICE_EXTENSION fdx;
    PPDO_DEVICE_EXTENSION pdx;
    PLIST_ENTRY entry;

	if (gfdo) {
		fdx = (PFDO_DEVICE_EXTENSION) gfdo->DeviceExtension;
		for (entry = fdx->ListOfPDOs.Flink;
				entry != &fdx->ListOfPDOs;
				entry = entry->Flink) {
			pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
			if (pdx->frontend_dev == dev) {
				return pdx;
			}
		}
	}
	else {
		DBG_PRINT(("xenbus_find_pdx_from_dev: gfdo is NULL\n"));
	}
	return NULL;
}

char *
xenbus_get_nodename_from_dev(void *dev)
{
    PPDO_DEVICE_EXTENSION pdx;

	if ((pdx = xenbus_find_pdx_from_dev(dev)) != NULL) {
		return pdx->Nodename;
	}
    return NULL;;
}

char *
xenbus_get_otherend_from_dev(void *dev)
{
    PPDO_DEVICE_EXTENSION pdx;

	if ((pdx = xenbus_find_pdx_from_dev(dev)) != NULL) {
		return pdx->Otherend;
	}
    return NULL;;
}

char *
xenbus_get_backendid_from_dev(void *dev)
{
    PPDO_DEVICE_EXTENSION pdx;

	if ((pdx = xenbus_find_pdx_from_dev(dev)) != NULL) {
		return pdx->BackendID;
	}
    return NULL;;
}

int
xenbus_switch_state(const char *nodename, enum xenbus_state state)
{
    int current_state;
    int err;

	if (nodename == NULL) {
		if (state == XENBUS_STATE_POWER_OFF) {
			HYPERVISOR_shutdown(SHUTDOWN_poweroff);
		}
		else if (state == XENBUS_STATE_REBOOT) {
			HYPERVISOR_shutdown(SHUTDOWN_reboot);
		}
		return 0;
	}
    err = xenbus_printf(XBT_NIL, nodename, "state", "%d", state);
    if (err) {
        return err;
    }

    return 0;
}

uint32_t
xenbus_get_pv_port_options(xenbus_pv_port_options_t *options)
{
    PFDO_DEVICE_EXTENSION fdx;
	uint32_t devices_to_probe;

	/* Need to use the current value of use_pv_drivers since */
	/* xenbus_determine_pv_driver_usage() may block due to */
	/* the registry not being available now. */

	switch (use_pv_drivers) {
		case (XENBUS_PROBE_PV_DISK | XENBUS_PROBE_PV_NET):
			options->port_offset = XENBUS_PV_ALL_PORTOFFSET; 
			options->value = XENBUS_PV_PORTOFFSET_ALL_VALUE;
			devices_to_probe = XENBUS_PROBE_PV_DISK | XENBUS_PROBE_PV_NET;
			break;
		case XENBUS_PROBE_PV_DISK:
			options->port_offset = XENBUS_PV_SPECIFIC_PORTOFFSET; 
			options->value = XENBUS_PV_PORTOFFSET_DISK_VALUE;
			devices_to_probe = XENBUS_PROBE_PV_DISK;
			break;
		case XENBUS_PROBE_PV_NET:
		case (XENBUS_PROBE_PV_NET | XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK):
			options->port_offset = XENBUS_PV_SPECIFIC_PORTOFFSET; 
			options->value = XENBUS_PV_PORTOFFSET_NET_VALUE;
			devices_to_probe = XENBUS_PROBE_PV_NET;
			break;
		case (XENBUS_PROBE_PV_DISK | XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK):
			options->port_offset = 0;
			options->value = 0;
			devices_to_probe = XENBUS_PROBE_PV_DISK;
			break;
		case (XENBUS_PROBE_PV_DISK |
			XENBUS_PROBE_PV_NET |
			XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK ):
			options->port_offset = XENBUS_PV_SPECIFIC_PORTOFFSET; 
			options->value = XENBUS_PV_PORTOFFSET_NET_VALUE;
			devices_to_probe = XENBUS_PROBE_PV_DISK | XENBUS_PROBE_PV_NET;
			break;
		default:
			if ((use_pv_drivers & (XENBUS_PROBE_PV_DISK | XENBUS_PROBE_PV_NET))
				== (XENBUS_PROBE_PV_DISK | XENBUS_PROBE_PV_NET)) {
			   options->port_offset = XENBUS_PV_ALL_PORTOFFSET; 
			   options->value = XENBUS_PV_PORTOFFSET_ALL_VALUE;
			   devices_to_probe = XENBUS_PROBE_PV_DISK | XENBUS_PROBE_PV_NET;
			}
			else if (use_pv_drivers & XENBUS_PROBE_PV_DISK) {
			   options->port_offset = XENBUS_PV_SPECIFIC_PORTOFFSET; 
			   options->value = XENBUS_PV_PORTOFFSET_DISK_VALUE;
			   devices_to_probe = XENBUS_PROBE_PV_DISK;
			}
			else if (use_pv_drivers & XENBUS_PROBE_PV_NET) {
			   options->port_offset = XENBUS_PV_SPECIFIC_PORTOFFSET; 
			   options->value = XENBUS_PV_PORTOFFSET_NET_VALUE;
			   devices_to_probe = XENBUS_PROBE_PV_NET;
			}
			else {
				options->port_offset = 0;
				options->value = 0;
				devices_to_probe = 0;
			}
			break;
	}

	DPR_INIT(("xenbus_get_pv_port_options: use_pv_drivers %x, probe %x\n",
		use_pv_drivers, devices_to_probe));
	return devices_to_probe;
}
void
xenbus_invalidate_relations(PKDPC dpc, PVOID dcontext, PVOID sa1, PVOID sa2)
{
	DPR_INIT(("xenbus_invalidate_relations.\n"));
	if (dcontext) {
		IoInvalidateDeviceRelations((PDEVICE_OBJECT)dcontext, BusRelations);
	}
}

void
xenbus_external_shutdown(void)
{
	DPR_INIT(("xenbus_external_shutdown - IN.\n"));
	xs_cleanup();
	set_callback_irq(0);
	unregister_xenbus_watch(&vbd_watch);
	DPR_INIT(("xenbus_external_shutdown - OUT.\n"));
}

NTSTATUS
xenbus_register_xenblk(void *controller,
	uint32_t (*ioctl)(void *context, pv_ioctl_t data),
	void ***info)
{
    PFDO_DEVICE_EXTENSION fdx;
	NTSTATUS status;

	DPR_INIT(("xenbus_register_xenblk: %p.\n", gfdo));

	status = STATUS_SUCCESS;
	if (gfdo) {
		fdx = (PFDO_DEVICE_EXTENSION) gfdo->DeviceExtension;
		fdx->disk_controller = controller;
		fdx->disk_ioctl = ioctl;
		*info = fdx->info;
	}
	else {
		DBG_PRINT(("xenbus_register_xenblk: gfdo is NULL\n"));
		status = STATUS_UNSUCCESSFUL;
	}
	DPR_INIT(("xenbus_register_xenblk: OUT\n"));
	return status;
}

NTSTATUS
xenbus_claim_device(void *dev, void *controller,
	XENBUS_DEVICE_TYPE type, XENBUS_DEVICE_SUBTYPE subtype,
	uint32_t (*reserved)(void *context, pv_ioctl_t data),
	uint32_t (*ioctl)(void *context, pv_ioctl_t data))
{
    PFDO_DEVICE_EXTENSION fdx;
    PPDO_DEVICE_EXTENSION pdx;
    PLIST_ENTRY entry;
	NTSTATUS status;
	uint32_t can_claim;

	DPR_INIT(("xenbus_claim_device: %p.\n", gfdo));

	if (!gfdo) {
		DBG_PRINT(("xenbus_claim_device: gfdo is NULL.\n"));
		return STATUS_UNSUCCESSFUL;
	}

	fdx = (PFDO_DEVICE_EXTENSION) gfdo->DeviceExtension;

	if (dev == NULL) {
		DPR_INIT(("xenbus_claim_device: is a claim possible?\n"));
		for (entry = fdx->ListOfPDOs.Flink;
				entry != &fdx->ListOfPDOs;
				entry = entry->Flink) {
			pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
			DPR_INIT(("xenbus_claim_device: pdx %p: %d, %p, %p: %p %p\n",
				pdx, pdx->Type, pdx->frontend_dev, pdx->controller,
				dev, controller));
			if (pdx->Type == type && pdx->subtype == subtype
					&& pdx->frontend_dev == NULL) {
				DPR_INIT(("xenbus_claim_device: it can be claimed.\n"));
				return STATUS_SUCCESS;
			}
		}
		return STATUS_NO_MORE_ENTRIES;
	}

	DPR_INIT(("xenbus_claim_device: start the claim: %p.\n", dev));
	for (entry = fdx->ListOfPDOs.Flink;
		entry != &fdx->ListOfPDOs;
		entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		/* See if it has already been claimed by the dev.*/
		if (pdx->Type == type && pdx->subtype == subtype
				&& pdx->frontend_dev == dev) {
			DPR_INIT(("xenbus_claim_device: %p already claimed.\n", dev));
			return STATUS_RESOURCE_IN_USE;
		}
	}

	/* It was not in the list, see if can be added. */
	for (entry = fdx->ListOfPDOs.Flink;
		entry != &fdx->ListOfPDOs;
		entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (pdx->Type == type && pdx->subtype == subtype
				&& pdx->frontend_dev == NULL) {
			pdx->frontend_dev = dev;
			pdx->controller = controller;
			pdx->ioctl = ioctl;
			DPR_INIT(("xenbus_claim_device: pdx %p claimed it %p.\n",
				pdx, pdx->frontend_dev));
			return STATUS_SUCCESS;
		}
	}

	DPR_INIT(("xenbus_claim_device: returning STATUS_NO_MORE_ENTRIES.\n"));
	return STATUS_NO_MORE_ENTRIES;
}

void
xenbus_release_device(void *dev, void *controller, 
	xenbus_release_device_t release_data)
{
    PFDO_DEVICE_EXTENSION fdx;
    PPDO_DEVICE_EXTENSION pdx;
    PLIST_ENTRY entry, listHead, nextEntry;
	NTSTATUS status;

	DPR_INIT(("xenbus_release_device: %p.\n", gfdo));
	XENBUS_SET_FLAG(rtrace, XENBUS_RELEASE_DEVICE_F);

	if (gfdo) {
		fdx = (PFDO_DEVICE_EXTENSION) gfdo->DeviceExtension;
		DPR_INIT(("  d = %p c = %p t = %d.\n",
			dev, controller, release_data.type));
		for (entry = fdx->ListOfPDOs.Flink;
				entry != &fdx->ListOfPDOs;
				entry = entry->Flink) {
			pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
			DPR_INIT(("xenbus_release_device: pdx %p: %d, %p, %p: %p, %p\n",
				pdx, pdx->Type, pdx->frontend_dev, pdx->controller,
				dev, controller));
			if (pdx->Type == release_data.type
					&& pdx->frontend_dev == dev) {
				DPR_INIT(("  found match, nulling out fronend_dev\n"));
				if (release_data.action == RELEASE_REMOVE
						&& release_data.type == vbd) {
					fdx->NumPDOs--;
					RemoveEntryList(&pdx->Link);
					XenbusDestroyPDO(pdx->Self);
					ExFreePool(pdx->Self);
					DPR_INIT(("  freeing pdx->Self\n"));
				}
				else {
					DPR_INIT(("  pdx %p: nulling %p, %p\n",
						pdx, pdx->frontend_dev, pdx->controller));
					pdx->frontend_dev = NULL;
					pdx->controller = NULL;
				}
				return;
			}
		}
	}
	else {
		DBG_PRINT(("xenbus_release_device: gfdo is NULL\n"));
	}
}

#if 0
PIO_WORKITEM
xenbus_allocate_work_item(void)
{
	PIO_WORKITEM work;
	if (gfdo) {
		work = IoAllocateWorkItem(gfdo);
		DPR_INIT(("work item = %p\n", work));
		return work;
	}
	else {
		DBG_PRINT(("xenbus_allocate_work_item: gfdo is NULL\n"));
	}
	DPR_INIT(("failed to allocate work item\n"));
	return NULL;
}
#endif

ULONG
xenbus_handle_evtchn_callback(void)
{
#ifdef XENBUS_HAS_DEVICE
	return EvtchnISR(NULL);
#else
	return EvtchnISR();
#endif
}

NTSTATUS
xenbus_create_thread(PKSTART_ROUTINE callback, void *context)
{
    HANDLE hthread;
    OBJECT_ATTRIBUTES oa;
    NTSTATUS status;

	DPR_INIT(("xenbus_create_thread IN.\n"));
	InitializeObjectAttributes(
	  &oa, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	DPR_INIT(("xenbus_create_thread PsCreateSystemThread IN.\n"));
	status = PsCreateSystemThread(
	  &hthread,
	  THREAD_ALL_ACCESS,
	  &oa,
	  NULL,
	  NULL,
	  callback,
	  context);
	DPR_INIT(("xenbus_create_thread PsCreateSystemThread OUT.\n"));

	if (NT_SUCCESS(status))
		ZwClose(hthread);
	DPR_INIT(("xenbus_create_thread OUT.\n"));
	xenbus_thread_count++;
	return status;
}

void
xenbus_terminate_thread(void)
{
	xenbus_thread_count--;
	PsTerminateSystemThread(STATUS_SUCCESS);
}

void
xenbus_printk(char *_fmt, ... )
{
	XEN_LOCK_HANDLE lh;
	va_list ap;
	char buf[256];
	char *c;

	va_start(ap, _fmt);
	RtlStringCbVPrintfA(buf, sizeof(buf), _fmt, ap);
	va_end( ap );

	/* Spin locks don't protect against irql > 2.  So if we come in at a */
	/* higl level, just print it and we'll have to maually sort out the */
	/* the possible mixing of multiple output messages. */
	if (KeGetCurrentIrql() > DISPATCH_LEVEL) {
		for (c = buf; *c; c++) {
			WRITE_PORT_UCHAR((PUCHAR)(XENBUS_PRINTK_PORT), *c);
		}
	}
	else {
		XenAcquireSpinLock(&xenbus_print_lock, &lh);
		for (c = buf; *c; c++) {
			WRITE_PORT_UCHAR((PUCHAR)(XENBUS_PRINTK_PORT), *c);
		}
		XenReleaseSpinLock(&xenbus_print_lock, lh);
	}
}

void
xenbus_console_io(char *_fmt, ... )
{
	va_list ap;
	char buf[256];

	va_start(ap, _fmt);
	RtlStringCbVPrintfA(buf, sizeof(buf), _fmt, ap);
	va_end( ap );

	if (!hypercall_page) {
		if (InitializeHypercallPage() != STATUS_SUCCESS) {
			return;
		}
	}
	HYPERVISOR_console_io(0, strlen(buf), buf);
}

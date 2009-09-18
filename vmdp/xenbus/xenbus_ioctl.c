/*****************************************************************************
 *
 *   File Name:      xenbus_ioctl.c
 *
 *   %version:       4 %
 *   %derived_by:    kallan %
 *   %date_modified: Tue Sep 09 16:25:32 2008 %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2008 Unpublished Work of Novell, Inc. All Rights Reserved.
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

#ifdef XENBUS_HAS_IOCTLS

#include "xenbus.h"

VOID
xenbus_cancel_ioctl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFDO_DEVICE_EXTENSION fdx;
	XEN_LOCK_HANDLE lh;
	xenbus_register_shutdown_event_t *ioctl;

	DPR_INIT(("==>xenbus_cancel_ioctl %p\n", Irp));
	fdx = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	IoReleaseCancelSpinLock(Irp->CancelIrql);
	ioctl = Irp->Tail.Overlay.DriverContext[3];
	if (ioctl) {
		XenAcquireSpinLock(&fdx->qlock, &lh);
		RemoveEntryList(&ioctl->list);
		XenReleaseSpinLock(&fdx->qlock, lh);

		Irp->Tail.Overlay.DriverContext[3] = NULL;
		Irp->IoStatus.Status = STATUS_CANCELLED;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
	}
	else {
		DPR_INIT(("xenbus_cancel_ioctl had no context\n", Irp));
	}

	DPR_INIT(("<==xenbus_cancel_ioctl %p\n", Irp));
	return;
}

NTSTATUS
xenbus_ioctl(PFDO_DEVICE_EXTENSION fdx, PIRP Irp)
{
    NTSTATUS status;
    PIO_STACK_LOCATION stack;
	XEN_LOCK_HANDLE lh;

    DPR_INIT(("==> xenbus_ioctl\n"));

	stack = IoGetCurrentIrpStackLocation( Irp );

	switch(stack->Parameters.DeviceIoControl.IoControlCode) {
		case IOCTL_XENBUS_REGISTER_SHUTDOWN_EVENT: {
			xenbus_register_shutdown_event_t *ioctl;

			if (stack->Parameters.DeviceIoControl.InputBufferLength <
				sizeof(xenbus_register_shutdown_event_t)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			ioctl = (xenbus_register_shutdown_event_t *)
				Irp->AssociatedIrp.SystemBuffer;
			ioctl->irp = Irp;
			XenAcquireSpinLock(&fdx->qlock, &lh);
			IoSetCancelRoutine (Irp, xenbus_cancel_ioctl);
			if (Irp->Cancel) {
				if (IoSetCancelRoutine (Irp, NULL) != NULL) {
					/* Since we were able to clear the cancel routine, then */
					/* we can return canceled.  Otherwise the cancel routine */
					/* will take care of it. */
					XenReleaseSpinLock(&fdx->qlock, lh);
					return STATUS_CANCELLED;
				}
			}
			DPR_INIT(("    marking irp pending: shutdown = %x\n",
					  ioctl->shutdown_type));
			IoMarkIrpPending(Irp);
			InsertTailList(&fdx->shutdown_requests, &ioctl->list);
			Irp->Tail.Overlay.DriverContext[3] = ioctl;
			XenReleaseSpinLock(&fdx->qlock, lh);
			status = STATUS_PENDING;;
			break;
		}

		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	} // switch IoControlCode
	DPR_INIT(("<== xenbus_ioctl\n"));
    return status;
}

#endif

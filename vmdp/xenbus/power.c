/*****************************************************************************
 *
 *   File Name:      power.c
 *
 *   %version:       18 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jul 27 14:42:08 2009 %
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

static NTSTATUS FDO_Power(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
static NTSTATUS PDO_Power(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
static IO_WORKITEM_ROUTINE xenbus_shutdown_worker;
static IO_COMPLETION_ROUTINE xenbus_shutdown_completion;

NTSTATUS
XenbusDispatchPower(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PIO_STACK_LOCATION irpStack;
	PCOMMON_DEVICE_EXTENSION pdx;

	DPR_INIT(("xenbu.sys: XenbusDispatchPower\n"));
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	ASSERT(irpStack->MajorFunction == IRP_MJ_POWER);

	pdx = (PCOMMON_DEVICE_EXTENSION) DeviceObject->DeviceExtension;

	if (pdx->pnpstate == Deleted) {
		PoStartNextPowerIrp(Irp);
		Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_NO_SUCH_DEVICE;
	}

	if (pdx->IsFdo)
		return FDO_Power(DeviceObject, Irp);
	else
		return PDO_Power(DeviceObject, Irp);
}

static NTSTATUS
xenbus_shutdown_completion(PDEVICE_OBJECT DeviceObject,	PIRP Irp, void *context)
{
	PFDO_DEVICE_EXTENSION fdx = (PFDO_DEVICE_EXTENSION)context;

	DPR_INIT(("xenbus_shutdown_completeion: irql = %d, irp %p, fdx->irp %p\n",
		KeGetCurrentIrql(),	Irp, fdx->irp));

	IoQueueWorkItem(fdx->item, xenbus_shutdown_worker, DelayedWorkQueue, fdx);

	DPR_INIT(("xenbus_shutdown_completeion: STATUS_MORE_PROCESSING_REQUIRED\n"));
    return STATUS_MORE_PROCESSING_REQUIRED;
}

static void
xenbus_shutdown_worker(PDEVICE_OBJECT DeviceObject, void *context)
{
	PFDO_DEVICE_EXTENSION fdx = (PFDO_DEVICE_EXTENSION)context;
	PPDO_DEVICE_EXTENSION pdx;
	PLIST_ENTRY entry;
	pv_ioctl_t ioctl_data;

	DPR_INIT(("xenbus_shutdown_worker: in: irql = %d\n", KeGetCurrentIrql()));
	DPR_INIT(("xenbus_shutdown_worker: fdx %p, irp %p\n", fdx, fdx->irp));

	for (entry = fdx->ListOfPDOs.Flink;
		entry != &fdx->ListOfPDOs;
		entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (pdx->Type == vnif && pdx->frontend_dev) {
			DPR_INIT(("   xenbus_shutdown_worker: shutting down %s\n",
				pdx->Nodename));
			ioctl_data.cmd = PV_SUSPEND;
			ioctl_data.arg = (uint16_t)SHUTDOWN_poweroff;
			if (pdx->ioctl(pdx->frontend_dev, ioctl_data)) {
				IoCopyCurrentIrpStackLocationToNext(fdx->irp);
				IoSetCompletionRoutine(
					fdx->irp,
					xenbus_shutdown_completion,
					fdx,
					TRUE,
					TRUE,
					TRUE);
				DPR_INIT(("xenbus_shutdown_worker: PoCallDriver\n"));
				PoCallDriver(fdx->LowerDevice, fdx->irp);
				return;
			}
		}
	}

	DPR_INIT(("xenbus_shutdown_worker: IoFreeWorkItem\n"));
	IoFreeWorkItem(fdx->item);

	DPR_INIT(("xenbus_shutdown_worker: PoStartNextPowerIrp\n"));
	PoStartNextPowerIrp(fdx->irp);

	DPR_INIT(("xenbus_shutdown_worker: IoCompleteRequest\n"));
	fdx->irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(fdx->irp, IO_NO_INCREMENT);

#ifdef TARGET_OS_Win2K
	if (fdx->power_state == PowerSystemMaximum) {
		/* Win2k wont power off after receiving a xm shutdown so tell */
		/* Xen to destroy the VM for us. */
		DPR_INIT(("           FDO_Power: HYPERVISOR_shutdown\n"));
		HYPERVISOR_shutdown(SHUTDOWN_poweroff);
	}
#endif

	fdx->power_state = PowerActionShutdownOff;
	DPR_INIT(("xenbus_shutdown_worker out: pdx %p, irp %p\n", fdx, fdx->irp));
}

static NTSTATUS
FDO_Power(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	NTSTATUS status;
	POWER_STATE powerState;
	POWER_STATE_TYPE powerType;
	PFDO_DEVICE_EXTENSION fdx;
	PIO_STACK_LOCATION stack;
	PLIST_ENTRY entry, listHead;
	pv_ioctl_t ioctl_data;
	uint32_t minor_func;
	xenbus_pv_port_options_t options;
	PPDO_DEVICE_EXTENSION pdx;


	DPR_INIT(("xenbu.sys: FDO_Power, gfdo = %p %p, irql %d, cpu %x\n",
		DeviceObject, gfdo, KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	fdx = (PFDO_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
	stack = IoGetCurrentIrpStackLocation(Irp);
	powerType = stack->Parameters.Power.Type;
	powerState = stack->Parameters.Power.State;
	minor_func = stack->MinorFunction;

	DPR_INIT(("           FDO_Power: MinorFunction == %x, t = %x, s = %x\n",
		stack->MinorFunction, powerType, powerState));
	if (fdx->pnpstate == NotStarted) {
		DPR_INIT(("           FDO_Power: notstarted PoStartNextPowerIrp\n"));
		PoStartNextPowerIrp(Irp);
		IoSkipCurrentIrpStackLocation(Irp);
		DPR_INIT(("           FDO_Power: notstarted PoCallDriver\n"));
		status = PoCallDriver(fdx->LowerDevice, Irp);
		DPR_INIT(("           FDO_Power: notstarted OUT\n"));
		return status;
	}

	/* If powering off, shutdown any remaining NICs. */
	if (minor_func == IRP_MN_SET_POWER
			&& powerState.SystemState == PowerActionShutdownOff
			&& powerType == SystemPowerState) {
		unregister_xenbus_watch(&vbd_watch);
		for (entry = fdx->ListOfPDOs.Flink;
			entry != &fdx->ListOfPDOs;
			entry = entry->Flink) {
			pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
			if (pdx->Type == vnif && pdx->frontend_dev) {
				DPR_INIT(("           FDO_Power:shutting down %s: %p, irp %p\n",
					pdx->Nodename, fdx, Irp));
				ioctl_data.cmd = PV_SUSPEND;
				ioctl_data.arg = (uint16_t)SHUTDOWN_poweroff;
				if (pdx->ioctl(pdx->frontend_dev, ioctl_data)) {
					fdx->item = IoAllocateWorkItem(DeviceObject);
					fdx->irp = Irp;
					IoMarkIrpPending(Irp);
					IoQueueWorkItem(
						fdx->item,
						xenbus_shutdown_worker,
						DelayedWorkQueue,
						fdx);
					DPR_INIT(("          FDO_Power: returning PENDING\n"));
					return STATUS_PENDING;
				}
			}
		}
	}
	else if (minor_func == IRP_MN_SET_POWER
			&& powerState.SystemState == PowerSystemHibernate
			&& powerType == SystemPowerState) {
		DPR_INIT(("           FDO_Power: hibernating so unregister\n"));
		unregister_xenbus_watch(&vbd_watch);
	}

	DPR_INIT(("           FDO_Power: PoStartNextPowerIrp, loc %x size %x\n",
			  Irp->CurrentLocation, Irp->StackCount));
	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	DPR_INIT(("           FDO_Power: PoCallDriver\n"));
	status = PoCallDriver(fdx->LowerDevice, Irp);

	if (minor_func == IRP_MN_SET_POWER) {
#ifdef TARGET_OS_Win2K
		if (fdx->power_state == PowerSystemMaximum) {
			/* Win2k wont power off after receiving a xm shutdown so tell */
			/* Xen to destroy the VM for us. */
			DPR_INIT(("           FDO_Power: HYPERVISOR_shutdown\n"));
			HYPERVISOR_shutdown(SHUTDOWN_poweroff);
		}
#endif
		fdx->power_state = powerState.SystemState;
	}

#ifdef DDBG
	DPR_INIT(("           FDO_Power: dumping debug info.\n"));
	xenbus_debug_dump();
	for (entry = fdx->ListOfPDOs.Flink;
		entry != &fdx->ListOfPDOs;
		entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (pdx->Type == vbd && pdx->controller) {
			ioctl_data.cmd = PV_SUSPEND;
			ioctl_data.arg = (uint16_t)SHUTDOWN_DEBUG_DUMP;
			pdx->ioctl(pdx->controller, ioctl_data);
			break;
		}
	}
#endif
	DPR_INIT(("           FDO_Power: OUT %p, state %x %x\n",
			  fdx, fdx->power_state, fdx->sig));
    return status;
}

static NTSTATUS
PDO_Power(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	NTSTATUS status;
	PPDO_DEVICE_EXTENSION pdx;
	PIO_STACK_LOCATION stack;
	POWER_STATE powerState;
	POWER_STATE_TYPE powerType;
	pv_ioctl_t ioctl_data;

	DPR_INIT(("xenbu.sys: PDO_Power, gfdo = %p\n", gfdo));
    pdx = (PPDO_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
	stack = IoGetCurrentIrpStackLocation(Irp);
	powerType = stack->Parameters.Power.Type;
	powerState = stack->Parameters.Power.State;


	DPR_INIT(("    PDO_Power: MinorFunction == %x, t = %x, s = %x, %s\n",
		stack->MinorFunction, powerType, powerState, pdx->Nodename));
	switch (stack->MinorFunction) {
		case IRP_MN_SET_POWER:
			DPR_INIT(("    PDO_Power: IRP_MN_SET_POWER\n"));
			switch (powerType) {
				case DevicePowerState:
					DPR_INIT(("    PDO_Power: DevicePowerState\n"));
					PoSetPowerState (DeviceObject, powerType, powerState);
					pdx->devpower = powerState.DeviceState;
					status = STATUS_SUCCESS;
					break;

				case SystemPowerState:
					DPR_INIT(("    PDO_Power: SystemPowerState\n"));
					pdx->syspower = powerState.SystemState;
					status = STATUS_SUCCESS;

					/* Halt the vnifs first. */
					if (powerState.SystemState == PowerActionShutdownOff
						&& pdx->Type == vnif && pdx->frontend_dev) {
						DPR_INIT(("    PDO_Power: Halting irp %p, %p, %s\n",
							Irp, pdx->frontend_dev, pdx->Nodename));

						/* We'll try once from here.  If still outstanding */
						/* resources, we will try from FDO_Power. */
						ioctl_data.cmd = PV_SUSPEND;
						ioctl_data.arg = (uint16_t)SHUTDOWN_poweroff;
						pdx->ioctl(pdx->frontend_dev, ioctl_data);
					}
					break;

				default:
					DPR_INIT(("    PDO_Power: power type default\n"));
					status = STATUS_NOT_SUPPORTED;
					break;
			}
			break;

		case IRP_MN_QUERY_POWER:
			DPR_INIT(("    PDO_Power: IRP_MN_QUERY_POWER\n"));
			status = STATUS_SUCCESS;
			break;

		case IRP_MN_WAIT_WAKE:
			/* TODO: add support for Wait/Wake */
			DPR_INIT(("    PDO_Power: IRP_MN_WAIT_WAKE\n"));
			status = STATUS_NOT_SUPPORTED;
			break;
		case IRP_MN_POWER_SEQUENCE:
			DPR_INIT(("    PDO_Power: IRP_MN_POWER_SEQUENCE\n"));
			status = STATUS_NOT_SUPPORTED;
			break;
		default:
			DPR_INIT(("    PDO_Power: default\n"));
			status = STATUS_NOT_SUPPORTED;
			break;
	}

	if (status != STATUS_NOT_SUPPORTED) {
		Irp->IoStatus.Status = status;
	}

	DPR_INIT(("    PDO_Power: PoStartNextPowerIrp\n"));
	PoStartNextPowerIrp(Irp);
    status = Irp->IoStatus.Status;
	DPR_INIT(("     PDO_Power: IoCompleteRequest\n"));
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DPR_INIT(("PDO_Power: OUT\n"));
	return status;
}

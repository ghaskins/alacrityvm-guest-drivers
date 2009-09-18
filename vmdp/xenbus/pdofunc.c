/*****************************************************************************
 *
 *   File Name:      pdofunc.c
 *
 *   %version:       12 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jun 18 15:57:02 2009 %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2006-2007 Unpublished Work of Novell, Inc. All Rights Reserved.
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

static NTSTATUS	PDOQueryDeviceCaps(IN PPDO_DEVICE_EXTENSION pdx, IN PIRP Irp);
static NTSTATUS	PDOForwardIrpSynchronous(IN PPDO_DEVICE_EXTENSION pdx,
	IN PIRP Irp);

static NTSTATUS	PDOQueryDeviceId(IN PPDO_DEVICE_EXTENSION pdx, IN PIRP Irp);

static NTSTATUS	PDOQueryDeviceText(IN PPDO_DEVICE_EXTENSION pdx, IN PIRP Irp);

static NTSTATUS	PDOQueryResources(IN PPDO_DEVICE_EXTENSION pdx, IN PIRP Irp);

static NTSTATUS	PDOQueryResourceRequirements(IN PPDO_DEVICE_EXTENSION pdx,
	IN PIRP Irp);

static NTSTATUS	PDOQueryDeviceRelations(IN PPDO_DEVICE_EXTENSION pdx,
	IN PIRP Irp);

static NTSTATUS	PDOQueryBusInformation(IN PPDO_DEVICE_EXTENSION pdx,
	IN PIRP Irp);

static NTSTATUS	PDOQueryInterface(IN PPDO_DEVICE_EXTENSION pdx,
	IN PIRP Irp);

static NTSTATUS	GetDeviceCapabilities(IN PDEVICE_OBJECT DeviceObject,
	IN PDEVICE_CAPABILITIES DeviceCapabilities);

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, PDO_Pnp)
#pragma alloc_text (PAGE, PDOQueryDeviceCaps)
#pragma alloc_text (PAGE, PDOQueryDeviceId)
#pragma alloc_text (PAGE, PDOQueryDeviceText)
#pragma alloc_text (PAGE, PDOQueryResources)
#pragma alloc_text (PAGE, PDOQueryResourceRequirements)
#pragma alloc_text (PAGE, PDOQueryDeviceRelations)
#pragma alloc_text (PAGE, PDOQueryBusInformation)
#pragma alloc_text (PAGE, PDOQueryInterface)
#pragma alloc_text (PAGE, GetDeviceCapabilities)
#endif

#define VENDORNAME L"Xen"
#define VNIFMODEL L"virtual NIC"
#define VBDMODEL L"virtual disk"
#define UNKNOWNMODEL L"unknown device"

static IO_COMPLETION_ROUTINE PDOSignalCompletion;

NTSTATUS PDO_Pnp(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
    NTSTATUS status;
    PPDO_DEVICE_EXTENSION pdx;
    PIO_STACK_LOCATION stack;


    PAGED_CODE();

    pdx = (PPDO_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
    stack = IoGetCurrentIrpStackLocation(Irp);
	DPR_PNP(("PDO_Pnp: DeviceObject %p, pdx %p\n", DeviceObject, pdx));

    switch (stack->MinorFunction) {
		case IRP_MN_START_DEVICE:
		DPR_PNP(("PDO_Pnp: IRP_MN_START_DEVICE\n"));
        pdx->devpower = PowerDeviceD0;
        pdx->pnpstate = Started;
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_STOP_DEVICE:
		DPR_PNP(("PDO_Pnp: IRP_MN_STOP_DEVICE\n"));
        pdx->pnpstate = Stopped;
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_STOP_DEVICE:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_STOP_DEVICE\n"));
        pdx->pnpstate = StopPending;
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_STOP_DEVICE:
		DPR_PNP(("PDO_Pnp: IRP_MN_CANCEL_STOP_DEVICE\n"));
        if (pdx->pnpstate == StopPending)
            pdx->pnpstate = Started;
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_REMOVE_DEVICE:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_REMOVE_DEVICE\n"));
        if (pdx->InterfaceRefCount != 0) {
            status = STATUS_UNSUCCESSFUL;
            break;
        }
        pdx->pnpstate = RemovePending;
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_REMOVE_DEVICE:
		DPR_PNP(("PDO_Pnp: IRP_MN_CANCEL_REMOVE_DEVICE\n"));
        if (pdx->pnpstate == RemovePending)
            pdx->pnpstate = Started;
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_SURPRISE_REMOVAL:
		DPR_PNP(("PDO_Pnp: IRP_MN_SURPRISE_REMOVAL\n"));
        pdx->pnpstate = SurpriseRemovePending;
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_REMOVE_DEVICE:
		DPR_PNP(("PDO_Pnp: IRP_MN_REMOVE_DEVICE\n"));
        /* TODO: review this section of code */
        if (pdx->ReportedMissing) {
            PFDO_DEVICE_EXTENSION fdx;

            pdx->pnpstate = Deleted;
            if (pdx->ParentFdo) {
                fdx = pdx->ParentFdo->DeviceExtension;
                ExAcquireFastMutex(&fdx->Mutex);
                RemoveEntryList(&pdx->Link);
                fdx->NumPDOs--;
                ExReleaseFastMutex(&fdx->Mutex);
            }
            status = XenbusDestroyPDO(DeviceObject);
            break;
        }
        if (pdx->Present) {
            pdx->pnpstate = NotStarted;
        }
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_CAPABILITIES:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_CAPABILITIES\n"));
        status = PDOQueryDeviceCaps(pdx, Irp);
        break;

    case IRP_MN_QUERY_ID:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_ID\n"));
        status = PDOQueryDeviceId(pdx, Irp);
        break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_DEVICE_RELATIONS\n"));
        status = PDOQueryDeviceRelations(pdx, Irp);
        break;

    case IRP_MN_QUERY_DEVICE_TEXT:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_DEVICE_TEXT\n"));
        status = PDOQueryDeviceText(pdx, Irp);
        break;

    case IRP_MN_QUERY_RESOURCES:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_RESOURCES\n"));
        status = PDOQueryResources(pdx, Irp);
        break;

    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_RESOURCE_REQUIREMENTS\n"));
        status = PDOQueryResourceRequirements(pdx, Irp);
        break;

    case IRP_MN_QUERY_BUS_INFORMATION:
		DPR_PNP(("PDO_Pnp: IRP_MN_QUERY_BUS_INFORMATION\n"));
        status = PDOQueryBusInformation(pdx, Irp);
        break;

    case IRP_MN_DEVICE_USAGE_NOTIFICATION:
		DPR_PNP(("PDO_Pnp: IRP_MN_DEVICE_USAGE_NOTIFICATION\n"));
        /*
         * TODO: We are here failing this Irp. For future VBD support,
         * it is possible that Windows may put a page file on a VBD device,
         * So we must properly handle this Irp in the future.
         */
		switch(stack->Parameters.UsageNotification.Type) {

			case DeviceUsageTypePaging: {

				BOOLEAN setPagable;

				if((stack->Parameters.UsageNotification.InPath) &&
				   (pdx->pnpstate != Started)) {

					//
					// Device isn't started.  Don't allow adding a
					// paging file, but allow a removal of one.
					//

					status = STATUS_DEVICE_NOT_READY;
					break;
				}

				/*
				 *  Ensure that this user thread is not suspended while we
				 *  are holding the PathCountEvent.
				 */
				KeEnterCriticalRegion();

				status = KeWaitForSingleObject(&pdx->PathCountEvent,
											   Executive, KernelMode,
											   FALSE, NULL);
				ASSERT(NT_SUCCESS(status));
				status = STATUS_SUCCESS;

				//
				// If the volume is removable we should try to lock it in
				// place or unlock it once per paging path count
				//

				if (pdx->IsFdo){
					DBG_PRINT(("PDO_Pnp: DeviceUsageTypePaging pdx->IsFdo lock.\n"));
						/*
					status = ClasspEjectionControl(
									DeviceObject,
									Irp,
									InternalMediaLock,
									(BOOLEAN)stack->Parameters.UsageNotification.InPath);
					*/
				}

				/*
				if (!NT_SUCCESS(status)){
					KeSetEvent(&commonExtension->PathCountEvent, IO_NO_INCREMENT, FALSE);
					KeLeaveCriticalRegion();
					break;
				}
				*/

				//
				// if removing last paging device, need to set DO_POWER_PAGABLE
				// bit here, and possible re-set it below on failure.
				//

				setPagable = FALSE;

				if (!stack->Parameters.UsageNotification.InPath &&
					pdx->PagingPathCount == 1) {

					//
					// removing last paging file
					// must have DO_POWER_PAGABLE bits set, but only
					// if noone set the DO_POWER_INRUSH bit
					//


					if ((DeviceObject->Flags & DO_POWER_INRUSH) != 0) {
						DPR_PNP(("PDO_pnp (%p,%p): Last "
									"paging file removed, but "
									"DO_POWER_INRUSH was set, so NOT "
									"setting DO_POWER_PAGABLE\n",
									DeviceObject, Irp));
					} else {
						DPR_PNP(("PDO_pnp (%p,%p): Last "
									"paging file removed, "
									"setting DO_POWER_PAGABLE\n",
									DeviceObject, Irp));
						DeviceObject->Flags |= DO_POWER_PAGABLE;
						setPagable = TRUE;
					}

				}

				//
				// forward the irp before finishing handling the
				// special cases
				//

				status = PDOForwardIrpSynchronous(pdx, Irp);

				//
				// now deal with the failure and success cases.
				// note that we are not allowed to fail the irp
				// once it is sent to the lower drivers.
				//

				if (NT_SUCCESS(status)) {

					IoAdjustPagingPathCount(
						&pdx->PagingPathCount,
						stack->Parameters.UsageNotification.InPath);

					if (stack->Parameters.UsageNotification.InPath) {
						if (pdx->PagingPathCount == 1) {
							DPR_PNP(("PDO_pnp (%p,%p): "
										"Clearing PAGABLE bit\n",
										DeviceObject, Irp));
							DeviceObject->Flags &= ~DO_POWER_PAGABLE;
						}
					}

				} else {

					//
					// cleanup the changes done above
					//

					if (setPagable == TRUE) {
						DPR_PNP(("PDO_pnp (%p,%p): Unsetting "
									"PAGABLE bit due to irp failure\n",
									DeviceObject, Irp));
						DeviceObject->Flags &= ~DO_POWER_PAGABLE;
						setPagable = FALSE;
						UNREFERENCED_PARAMETER(setPagable); // disables prefast warning; defensive coding...
					}

					//
					// relock or unlock the media if needed.
					//

					if (pdx->IsFdo) {
						DBG_PRINT(("PDO_Pnp: DeviceUsageTypePaging pdx->IsFdo unlock.\n"));

							/*
						ClasspEjectionControl(
								DeviceObject,
								Irp,
								InternalMediaLock,
								(BOOLEAN)!stack->Parameters.UsageNotification.InPath);
						*/
					}
				}

				//
				// set the event so the next one can occur.
				//

				KeSetEvent(&pdx->PathCountEvent,
						   IO_NO_INCREMENT, FALSE);
				KeLeaveCriticalRegion();
				break;
			}

			case DeviceUsageTypeHibernation: {

				IoAdjustPagingPathCount(
					&pdx->HibernationPathCount,
					stack->Parameters.UsageNotification.InPath
					);
				status = PDOForwardIrpSynchronous(pdx, Irp);
				if (!NT_SUCCESS(status)) {
					IoAdjustPagingPathCount(
						&pdx->HibernationPathCount,
						!stack->Parameters.UsageNotification.InPath
						);
				}

				break;
			}

			case DeviceUsageTypeDumpFile: {
				IoAdjustPagingPathCount(
					&pdx->DumpPathCount,
					stack->Parameters.UsageNotification.InPath
					);
				status = PDOForwardIrpSynchronous(pdx, Irp);
				if (!NT_SUCCESS(status)) {
					IoAdjustPagingPathCount(
						&pdx->DumpPathCount,
						!stack->Parameters.UsageNotification.InPath
						);
				}

				break;
			}

			default: {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
		}
        break;

    case IRP_MN_EJECT:
		DPR_PNP(("PDO_Pnp: IRP_MN_EJECT\n"));
        /*
         * We don't handle this Irp yet, so we leave IoStatus.Status untouched.
         */
        status = Irp->IoStatus.Status;
        break;

    default:
		DPR_PNP(("PDO_Pnp: default %x\n", stack->MinorFunction));
        status = Irp->IoStatus.Status;
        break;
    }


    Irp->IoStatus.Status = status;
	DPR_PNP(("PDO_Pnp: calling IoCompleteRequest\n"));
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
	DPR_PNP(("PDO_Pnp: fininshed IoCompleteRequest\n"));
    return status;
}

static NTSTATUS
PDOSignalCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT Event
    )
{
    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

static NTSTATUS
PDOSendIrpSynchronous(
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PIRP Irp
    )
{
    KEVENT event;
    NTSTATUS status;

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ASSERT(TargetDeviceObject != NULL);
    ASSERT(Irp != NULL);
    ASSERT(Irp->StackCount >= TargetDeviceObject->StackSize);

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);
    IoSetCompletionRoutine(Irp, PDOSignalCompletion, &event,
                           TRUE, TRUE, TRUE);

    status = IoCallDriver(TargetDeviceObject, Irp);

    if (status == STATUS_PENDING) {
            KeWaitForSingleObject(&event,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  NULL);
        status = Irp->IoStatus.Status;
    }

    return status;
}

static NTSTATUS
PDOForwardIrpSynchronous(
    IN PPDO_DEVICE_EXTENSION pdx,
    IN PIRP Irp
    )
{
    PFDO_DEVICE_EXTENSION fdx;

	DPR_INIT(("PDOForwardIrpSynchronous: ParentFdo = %p.\n", pdx->ParentFdo));
    fdx = (PFDO_DEVICE_EXTENSION) pdx->ParentFdo->DeviceExtension;

    IoCopyCurrentIrpStackLocationToNext(Irp);
    return PDOSendIrpSynchronous(fdx->LowerDevice, Irp);
} // end PDOForwardIrpSynchronous()

static NTSTATUS
PDOQueryDeviceCaps(
  IN PPDO_DEVICE_EXTENSION pdx,
  IN PIRP Irp
  )
{
    PIO_STACK_LOCATION stack;
    PDEVICE_CAPABILITIES devcap;
    DEVICE_CAPABILITIES parentcap;
    NTSTATUS status;

    PAGED_CODE();

    /*
     * XXX: I don't fully understand Windows power management mechanism yet.
     * This chunck of code is mainly directly copied from DDK samples. As we
     * really don't care more on power state of virtual devices, this may
     * simplified when it's get clear what kind of power management we need.
     */
    stack = IoGetCurrentIrpStackLocation(Irp);

    devcap = stack->Parameters.DeviceCapabilities.Capabilities;

    if (devcap->Version != 1 ||
        devcap->Size < sizeof(DEVICE_CAPABILITIES))
        return STATUS_UNSUCCESSFUL;

    status = GetDeviceCapabilities(
      PDX_TO_FDX(pdx)->LowerDevice, &parentcap);

    if (!NT_SUCCESS(status)) {
        DBG_PRINT(("QueryDeviceCaps fail.\n"));
        return status;
    }

    RtlCopyMemory(
      devcap->DeviceState,
      parentcap.DeviceState,
      (PowerSystemShutdown+1) * sizeof(DEVICE_POWER_STATE));

	DPR_INIT(("PDOQueryDeviceCaps default DeviceState 1 %x, 2 %x, 3 %x, h %x\n",
		devcap->DeviceState[PowerSystemSleeping1],
		devcap->DeviceState[PowerSystemSleeping2],
		devcap->DeviceState[PowerSystemSleeping3],
		devcap->DeviceState[PowerSystemHibernate]));
	DPR_INIT(("PDOQueryDeviceCaps default WakeFromD0 %x, d1 %x d2 %x d3 %x.\n",
		devcap->WakeFromD0,
		devcap->WakeFromD1,
		devcap->WakeFromD2,
		devcap->WakeFromD3));
	DPR_INIT(("PDOQueryDeviceCaps default DeviceWake %x, SystemWake %x.\n",
		devcap->DeviceWake, devcap->SystemWake));
	DPR_INIT(("PDOQueryDeviceCaps default DeviceD1 %x, D2 %x.\n",
		devcap->DeviceD1,
		devcap->DeviceD2));

    devcap->DeviceState[PowerSystemWorking] = PowerDeviceD0;

    if (devcap->DeviceState[PowerSystemSleeping1] != PowerDeviceD0)
        devcap->DeviceState[PowerSystemSleeping1] = PowerDeviceD1;

    if (devcap->DeviceState[PowerSystemSleeping2] != PowerDeviceD0)
        devcap->DeviceState[PowerSystemSleeping2] = PowerDeviceD3;

    if (devcap->DeviceState[PowerSystemSleeping3] != PowerDeviceD0)
        devcap->DeviceState[PowerSystemSleeping3] = PowerDeviceD3;

    devcap->DeviceWake = PowerDeviceD1;

    devcap->DeviceD1 = TRUE;
    devcap->DeviceD2 = FALSE;

    //
    // Specifies whether the device can respond to an external wake
    // signal while in the D0, D1, D2, and D3 state.
    // Set these bits explicitly.
    //

    devcap->WakeFromD0 = FALSE;
    devcap->WakeFromD1 = TRUE; //Yes we can
    devcap->WakeFromD2 = FALSE;
    devcap->WakeFromD3 = FALSE;

    devcap->D1Latency = 0;
    devcap->D2Latency = 0;
    devcap->D3Latency = 0;

    devcap->LockSupported = FALSE;
    devcap->EjectSupported = FALSE;
    devcap->HardwareDisabled = FALSE;
    devcap->Removable = FALSE;
    devcap->SurpriseRemovalOK = TRUE;
    devcap->UniqueID = TRUE;
    devcap->SilentInstall=FALSE;

    //devcap->Address = 0xFFFFFFFF;
    //devcap->UINumber = none;

    return STATUS_SUCCESS;
}

static NTSTATUS
PDOQueryDeviceId(
  IN PPDO_DEVICE_EXTENSION pdx,
  IN PIRP Irp
  )
{
    PIO_STACK_LOCATION stack;
    PWCHAR buffer;
    ULONG length;
    NTSTATUS status;
    ANSI_STRING astr;
    UNICODE_STRING ustr;

    PAGED_CODE();

    stack = IoGetCurrentIrpStackLocation(Irp);

    switch(stack->Parameters.QueryId.IdType) {
    case BusQueryDeviceID:
        DPR_PNP(("BusQueryDeviceID.\n"));
        length = pdx->HardwareIDs.Length + 2 * sizeof(WCHAR);

        buffer = ExAllocatePoolWithTag(NonPagedPool, length, XENBUS_POOL_TAG);

        if (!buffer) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        RtlZeroMemory(buffer, length);
        RtlStringCchCopyW(buffer, pdx->HardwareIDs.Length, pdx->HardwareIDs.Buffer);

        Irp->IoStatus.Information = (ULONG_PTR) buffer;
        status = STATUS_SUCCESS;
        break;

    case BusQueryInstanceID:
        DPR_PNP(("BusQueryInstacneID.\n"));
        RtlInitAnsiString(&astr, pdx->Nodename);
        RtlAnsiStringToUnicodeString(&ustr, &astr, TRUE);
        length = strlen(pdx->Nodename) + 1;
        buffer = ExAllocatePoolWithTag(NonPagedPool, length*sizeof(WCHAR),
			XENBUS_POOL_TAG);
        if (!buffer) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }
        RtlStringCchCopyW(buffer, length, ustr.Buffer);
        RtlFreeUnicodeString(&ustr);

        Irp->IoStatus.Information = (ULONG_PTR) buffer;
        status = STATUS_SUCCESS;
        break;

    case BusQueryHardwareIDs:
        DPR_PNP(("BusQueryHarwareIDs.\n"));
        length = pdx->HardwareIDs.Length + 2 * sizeof(WCHAR);

        buffer = ExAllocatePoolWithTag(NonPagedPool, length, XENBUS_POOL_TAG);

        if (!buffer) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        RtlZeroMemory(buffer, length);
        RtlStringCchCopyW(buffer, pdx->HardwareIDs.Length, pdx->HardwareIDs.Buffer);

        Irp->IoStatus.Information = (ULONG_PTR) buffer;
        status = STATUS_SUCCESS;
        break;

    default:
        status = Irp->IoStatus.Status;
    }

    return status;
}

static NTSTATUS
PDOQueryDeviceText(
  IN PPDO_DEVICE_EXTENSION pdx,
  IN PIRP Irp
  )
{
    PWCHAR buffer, model;
    USHORT length;
    PIO_STACK_LOCATION stack;
    NTSTATUS status;

    PAGED_CODE();

    stack = IoGetCurrentIrpStackLocation(Irp);

    switch (stack->Parameters.QueryDeviceText.DeviceTextType) {
    case DeviceTextDescription:
        switch (stack->Parameters.QueryDeviceText.LocaleId) {
        default:
        case 0x00000409:  /* English */
            switch (pdx->Type) {
            case vnif:
                model = VNIFMODEL;
                break;
            case vbd:
                model = VBDMODEL;
                break;
            case unknown:
            default:
                model = UNKNOWNMODEL;
                break;
            }

            length = (wcslen(VENDORNAME) + 2 + wcslen(model) + 1)
                * sizeof(WCHAR);
            buffer = ExAllocatePoolWithTag
                (NonPagedPool,
                 length,
                 XENBUS_POOL_TAG);
            if (buffer == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            RtlStringCchPrintfW(buffer, length, L"%ws %ws", VENDORNAME,
                                model);

            Irp->IoStatus.Information = (ULONG_PTR) buffer;
            status = STATUS_SUCCESS;
            break;
        }
        break;
    default:
        status = Irp->IoStatus.Status;
        break;
    }

    return status;
}


/* we need no resource for virtual devices */
static NTSTATUS
PDOQueryResources(
  IN PPDO_DEVICE_EXTENSION pdx,
  IN PIRP Irp
  )
{
    PAGED_CODE();

    return Irp->IoStatus.Status;
}

static NTSTATUS
PDOQueryResourceRequirements(
  IN PPDO_DEVICE_EXTENSION pdx,
  IN PIRP Irp
  )
{
    PAGED_CODE();

    return Irp->IoStatus.Status;
}

static NTSTATUS
PDOQueryDeviceRelations(
  IN PPDO_DEVICE_EXTENSION pdx,
  IN PIRP Irp
  )
{
    PIO_STACK_LOCATION stack;
    PDEVICE_RELATIONS deviceRelations;
    NTSTATUS status;

    PAGED_CODE ();

    stack = IoGetCurrentIrpStackLocation (Irp);

    switch (stack->Parameters.QueryDeviceRelations.Type) {
    case TargetDeviceRelation:
        deviceRelations = (PDEVICE_RELATIONS)
            ExAllocatePoolWithTag (NonPagedPool,
                                   sizeof(DEVICE_RELATIONS),
                                   XENBUS_POOL_TAG);
        if (!deviceRelations) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        //
        // There is only one PDO pointer in the structure
        // for this relation type. The PnP Manager removes
        // the reference to the PDO when the driver or application
        // un-registers for notification on the device.
        //

        deviceRelations->Count = 1;
        deviceRelations->Objects[0] = pdx->Self;
        ObReferenceObject(pdx->Self);

        status = STATUS_SUCCESS;
        Irp->IoStatus.Information = (ULONG_PTR) deviceRelations;
        break;

    case BusRelations: // Not handled by PDO
    case EjectionRelations: // optional for PDO
    case RemovalRelations: // // optional for PDO
    default:
        status = Irp->IoStatus.Status;
    }

    return status;
}

static NTSTATUS
PDOQueryBusInformation(
  IN PPDO_DEVICE_EXTENSION pdx,
  IN PIRP Irp
  )
{

    PPNP_BUS_INFORMATION busInfo;

    PAGED_CODE ();

    busInfo = ExAllocatePoolWithTag (
      NonPagedPool,
      sizeof(PNP_BUS_INFORMATION),
      XENBUS_POOL_TAG);

    if (busInfo == NULL) {
      return STATUS_INSUFFICIENT_RESOURCES;
    }

    busInfo->BusTypeGuid = GUID_DEVCLASS_XENBUS;
    busInfo->LegacyBusType = PNPBus;
    busInfo->BusNumber = 0;

    Irp->IoStatus.Information = (ULONG_PTR)busInfo;

    return STATUS_SUCCESS;
}

static NTSTATUS
PDOQueryInterface(
  IN PPDO_DEVICE_EXTENSION pdx,
  IN PIRP Irp
  )
{
    PAGED_CODE();

    return Irp->IoStatus.Status;
}

static NTSTATUS
GetDeviceCapabilities(
  IN PDEVICE_OBJECT DeviceObject,
  IN PDEVICE_CAPABILITIES DeviceCapabilities
  )
{
    IO_STATUS_BLOCK     ioStatus;
    KEVENT              pnpEvent;
    NTSTATUS            status;
    PDEVICE_OBJECT      targetObject;
    PIO_STACK_LOCATION  irpStack;
    PIRP                pnpIrp;

    PAGED_CODE();

    //
    // Initialize the capabilities that we will send down
    //
    RtlZeroMemory(DeviceCapabilities, sizeof(DEVICE_CAPABILITIES));
    DeviceCapabilities->Size = sizeof(DEVICE_CAPABILITIES);
    DeviceCapabilities->Version = 1;
    DeviceCapabilities->Address = -1;
    DeviceCapabilities->UINumber = -1;

    //
    // Initialize the event
    //
    KeInitializeEvent(&pnpEvent, NotificationEvent, FALSE);

    targetObject = IoGetAttachedDeviceReference(DeviceObject);

    //
    // Build an Irp
    //
    pnpIrp = IoBuildSynchronousFsdRequest(
        IRP_MJ_PNP,
        targetObject,
        NULL,
        0,
        NULL,
        &pnpEvent,
        &ioStatus
        );
    if (pnpIrp == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetDeviceCapabilitiesExit;

    }

    //
    // Pnp Irps all begin life as STATUS_NOT_SUPPORTED;
    //
    pnpIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    //
    // Get the top of stack
    //
    irpStack = IoGetNextIrpStackLocation( pnpIrp );

    //
    // Set the top of stack
    //
    RtlZeroMemory( irpStack, sizeof(IO_STACK_LOCATION ) );
    irpStack->MajorFunction = IRP_MJ_PNP;
    irpStack->MinorFunction = IRP_MN_QUERY_CAPABILITIES;
    irpStack->Parameters.DeviceCapabilities.Capabilities = DeviceCapabilities;

    //
    // Call the driver
    //
    status = IoCallDriver( targetObject, pnpIrp );
    if (status == STATUS_PENDING) {

        //
        // Block until the irp comes back.
        // Important thing to note here is when you allocate
        // the memory for an event in the stack you must do a
        // KernelMode wait instead of UserMode to prevent
        // the stack from getting paged out.
        //

        KeWaitForSingleObject(
            &pnpEvent,
            Executive,
            KernelMode,
            FALSE,
            NULL
            );
        status = ioStatus.Status;

    }

GetDeviceCapabilitiesExit:
    //
    // Done with reference
    //
    ObDereferenceObject( targetObject );

    //
    // Done
    //
    return status;

}


/*****************************************************************************
 *
 *   File Name:      pnp.c
 *
 *   %version:       19 %
 *   %derived_by:    kallan %
 *   %date_modified: Fri Jul 10 13:17:52 2009 %
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

#ifdef XENBUS_HAS_DEVICE
PKINTERRUPT DriverInterruptObj;
#endif

static IO_COMPLETION_ROUTINE XenbusIoCompletion;

static NTSTATUS SendIrpSynchronous(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp);

static NTSTATUS FDOStartDevice(
	IN PDEVICE_OBJECT fdo,
	IN PCM_PARTIAL_RESOURCE_LIST raw,
	IN PCM_PARTIAL_RESOURCE_LIST translated);

static VOID FDOStopDevice(IN PDEVICE_OBJECT fdo); 
static VOID FDORemoveDevice(IN PDEVICE_OBJECT fdo);

NTSTATUS FDO_Pnp(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
    NTSTATUS status;
    ULONG length, prevcount, numNew, i;
    PFDO_DEVICE_EXTENSION fdx;
    PPDO_DEVICE_EXTENSION pdx;
    PIO_STACK_LOCATION stack;
    PCM_PARTIAL_RESOURCE_LIST raw, translated;
    PLIST_ENTRY entry, listHead, nextEntry;
    PDEVICE_RELATIONS relations, oldRelations;

    //PAGED_CODE();

    fdx = (PFDO_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
    stack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IRP_MJ_PNP == stack->MajorFunction);

    switch (stack->MinorFunction) {
		case IRP_MN_START_DEVICE:
			DPR_PNP(("FDO_Pnp: IRP_MN_START_DEVICE.\n"));
			status = SendIrpSynchronous(fdx->LowerDevice, Irp);

			if  (NT_SUCCESS(status)) {
				raw = &stack->Parameters.StartDevice
					.AllocatedResources->List[0].PartialResourceList;
				translated = &stack->Parameters.StartDevice
					.AllocatedResourcesTranslated->List[0].PartialResourceList;
				FDOStartDevice(DeviceObject, raw, translated);
			}

			Irp->IoStatus.Status = status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);

			return status;

		case IRP_MN_QUERY_STOP_DEVICE:
			DPR_PNP(("FDO_Pnp: IRP_MN_QUERY_STOP_DEVICE.\n"));
			fdx->pnpstate = StopPending;
			Irp->IoStatus.Status = STATUS_SUCCESS;
			break;

		case IRP_MN_CANCEL_STOP_DEVICE:
			DPR_PNP(("FDO_Pnp: IRP_MN_CANCEL_STOP_DEVICE.\n"));
			if (fdx->pnpstate == StopPending)
				fdx->pnpstate = Started;
			Irp->IoStatus.Status = STATUS_SUCCESS;
			break;

		case IRP_MN_STOP_DEVICE:
			DPR_INIT(("FDO_Pnp: IRP_MN_STOP_DEVICE.\n"));
			/* TODO: Irps and resources */

			FDOStopDevice(DeviceObject);

			fdx->pnpstate = Stopped;
			Irp->IoStatus.Status = STATUS_SUCCESS;
			break;

		case IRP_MN_QUERY_REMOVE_DEVICE:
			DPR_INIT(("FDO_Pnp: IRP_MN_QUERY_REMOVE_DEVICE.\n"));
			Irp->IoStatus.Status = status = STATUS_UNSUCCESSFUL;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return status;

		case IRP_MN_CANCEL_REMOVE_DEVICE:
			DPR_INIT(("FDO_Pnp: IRP_MN_CANCEL_REMOVE_DEVICE.\n"));
			if (fdx->pnpstate == RemovePending)
				fdx->pnpstate = Started;
			Irp->IoStatus.Status = STATUS_SUCCESS;
			break;

		case IRP_MN_SURPRISE_REMOVAL:
			DPR_INIT(("FDO_Pnp: IRP_MN_SURPRISE_REMOVAL.\n"));
			fdx->pnpstate = SurpriseRemovePending;
			FDORemoveDevice(DeviceObject);

			ExAcquireFastMutex(&fdx->Mutex);

			/* Test the alloc of gfdx was successful.  If not do the best */
			/* we can with fdx. */
			if (gfdx)
				listHead = &gfdx->ListOfPDOs;
			else
				listHead = &fdx->ListOfPDOs;

			for (entry=listHead->Flink, nextEntry = entry->Flink;
				 entry!= listHead;
				 entry = nextEntry, nextEntry = entry->Flink) {
				PPDO_DEVICE_EXTENSION pdx =
					CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
				if (pdx->Type != vbd) {
					RemoveEntryList(&pdx->Link);
					InitializeListHead(&pdx->Link);
					pdx->ParentFdo = NULL;
					pdx->ReportedMissing = TRUE;
					if (gfdx) 
						gfdx->NumPDOs--;
					else
						fdx->NumPDOs--;
				}
			}

			ExReleaseFastMutex(&fdx->Mutex);

			Irp->IoStatus.Status = STATUS_SUCCESS;
			break;

		case IRP_MN_REMOVE_DEVICE:
			DPR_INIT(("FDO_Pnp: IRP_MN_REMOVE_DEVICE.\n"));
			if (fdx->pnpstate != SurpriseRemovePending) {
				FDOStopDevice(DeviceObject);
				FDORemoveDevice(DeviceObject);
			}

			fdx->pnpstate = Deleted;

			/* TODO: some kind of wait for IO */

			ExAcquireFastMutex(&fdx->Mutex);

			if (gfdx)
				listHead = &gfdx->ListOfPDOs;
			else
				listHead = &fdx->ListOfPDOs;

			DPR_INIT(("FDO_Pnp: IRP_MN_REMOVE_DEVICE start for loop.\n"));
			for (entry=listHead->Flink, nextEntry = entry->Flink;
				 entry!= listHead;
				 entry = nextEntry, nextEntry = entry->Flink) {
				PPDO_DEVICE_EXTENSION pdx =
					CONTAINING_RECORD (entry, PDO_DEVICE_EXTENSION, Link);

				if (pdx->Type != vbd) {
					DPR_INIT(("FDO_Pnp: IRP_MN_REMOVE_DEVICE remove entry list.\n"));
					RemoveEntryList (&pdx->Link);
					if(pdx->pnpstate == SurpriseRemovePending)
					{
						DPR_INIT((" IRP_MN_REMOVE_DEVICE susprise remove %s.\n",
							  pdx->Nodename));
						InitializeListHead (&pdx->Link);
						pdx->ParentFdo  = NULL;
						pdx->ReportedMissing = TRUE;
						continue;
					}
					fdx->NumPDOs--;
					DPR_INIT((" IRP_MN_REMOVE_DEVICE destroy %s.\n",
						  pdx->Nodename));
					XenbusDestroyPDO(pdx->Self);
				}
			}

			ExReleaseFastMutex(&fdx->Mutex);

			Irp->IoStatus.Status = STATUS_SUCCESS;
			IoSkipCurrentIrpStackLocation(Irp);
			status = IoCallDriver(fdx->LowerDevice, Irp);

			DPR_INIT(("FDO_Pnp: irql %d, gfdo %p, dev %p.\n",
				KeGetCurrentIrql(), gfdo, DeviceObject));

			/* Seems we crash if we try to print from here down. */
			IoDetachDevice(fdx->LowerDevice);

			/* The DeviceObject, aka gfdo, should be able to be set to NULL */
			/* eventhough there is an interaction between xnebus and xenblk.*/
			xs_cleanup();
			gfdo = NULL;
			IoDeleteDevice(DeviceObject);

			return status;

		case IRP_MN_QUERY_DEVICE_RELATIONS:
			if (stack->Parameters.QueryDeviceRelations.Type != BusRelations)
				break;

			DPR_INIT(("FDO_Pnp: Query bus relation.\n"));

			ExAcquireFastMutex(&fdx->Mutex);

			/*
			 * upper drivers may already presented a relation,
			 * we should keep the existing ones and add ours.
			 */
			oldRelations = (PDEVICE_RELATIONS) Irp->IoStatus.Information;
			if (oldRelations) {
				prevcount = oldRelations->Count;
				if (!fdx->NumPDOs) {
					ExReleaseFastMutex(&fdx->Mutex);
					break;
				}
			}
			else {
				prevcount = 0;
			}
			DPR_INIT(("FDO_Pnp: relation, prevcount %x.\n", prevcount));

			numNew = 0;
			for (entry = fdx->ListOfPDOs.Flink;
				 entry != &fdx->ListOfPDOs;
				 entry = entry->Flink) {
				pdx = CONTAINING_RECORD (entry, PDO_DEVICE_EXTENSION, Link);
				if (pdx->Present && pdx->origin == created)
					numNew++;
				DPR_INIT(("FDO_Pnp: new relation %s.\n", pdx->Nodename));
			}

			length = sizeof(DEVICE_RELATIONS) +
				((numNew + prevcount) * sizeof(PDEVICE_OBJECT)) - 1;

			relations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag(
			  NonPagedPool, length, XENBUS_POOL_TAG);

			if (relations == NULL) {
				ExReleaseFastMutex(&fdx->Mutex);
				DBG_PRINT(("FDO_Pnp: BusRelation fail due to not no memory.\n"));
				Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				return status;
			}

			if (prevcount) {
				RtlCopyMemory(relations->Objects, oldRelations->Objects,
							  prevcount * sizeof(PDEVICE_OBJECT));
			}

			relations->Count = prevcount + numNew;

			for (entry = fdx->ListOfPDOs.Flink;
				 entry != &fdx->ListOfPDOs;
				 entry = entry->Flink) {
				pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
				if (pdx->Present && pdx->origin == created) {
					relations->Objects[prevcount] = pdx->Self;
					DPR_INIT(("FDO_Pnp: new relation ObRef %s, self %p.\n",
						pdx->Nodename, pdx->Self));
					ObReferenceObject(pdx->Self);
					prevcount++;
					DPR_INIT(("FDO_Pnp: adding relation %s.\n", pdx->Nodename));
				}
			}

			if (oldRelations) {
				ExFreePool(oldRelations);
			}

			Irp->IoStatus.Information = (ULONG_PTR) relations;

			ExReleaseFastMutex(&fdx->Mutex);

			DPR_INIT(("FDO_Pnp: presenting to pnp manager new relations: %d.\n",
				relations->Count));

			Irp->IoStatus.Status = STATUS_SUCCESS;
			break;

		default:
			DPR_PNP(("FDO_Pnp: default irp %x.\n", stack->MinorFunction));
			break;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    status = IoCallDriver(fdx->LowerDevice, Irp);

    return status;
}

static NTSTATUS SendIrpSynchronous(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
    NTSTATUS status;
    KEVENT event;

    //PAGED_CODE();

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(
      Irp,
      XenbusIoCompletion,
      &event,
      TRUE,
      TRUE,
      TRUE);
    status = IoCallDriver(DeviceObject, Irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(
          &event,
          Executive,
          KernelMode,
          FALSE,
          NULL);
        status = Irp->IoStatus.Status;
    }

    return status;
}

static NTSTATUS XenbusIoCompletion(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp,
  IN PVOID Context
  )
{
    if (Irp->PendingReturned == TRUE) {
        KeSetEvent((PKEVENT) Context, IO_NO_INCREMENT, FALSE);
    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

#ifdef XENBUS_HAS_DEVICE
static NTSTATUS FDOSetResources(
  IN PFDO_DEVICE_EXTENSION fdx,
  IN PCM_PARTIAL_RESOURCE_LIST raw,
  IN PCM_PARTIAL_RESOURCE_LIST translated
  )
{
	PHYSICAL_ADDRESS portBase;
	PHYSICAL_ADDRESS memBase;
	xenbus_pv_port_options_t options;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;
	KIRQL irql;
	KINTERRUPT_MODE mode;
	KAFFINITY affinity;
	ULONG nres, i;
	ULONG vector;
	ULONG deviceIRQ;
	NTSTATUS status;
	uint32_t mmiolen;
	BOOLEAN irqshared;

	resource = translated->PartialDescriptors;
	nres = translated->Count;

	for (i = 0; i < nres; i++, resource++) {
		switch (resource->Type) {
			case CmResourceTypePort:
				portBase = resource->u.Port.Start;
				fdx->NumPorts = resource->u.Port.Length;
				fdx->MappedPort =
					(resource->Flags & CM_RESOURCE_PORT_IO) == 0;
				break;

			case CmResourceTypeInterrupt:
				irql = (KIRQL) resource->u.Interrupt.Level;
				vector = resource->u.Interrupt.Vector;
				affinity = resource->u.Interrupt.Affinity;
				mode = (resource->Flags == CM_RESOURCE_INTERRUPT_LATCHED)?
					Latched: LevelSensitive;
				irqshared =
					resource->ShareDisposition == CmResourceShareShared;
				break;

			case CmResourceTypeMemory:
				memBase = resource->u.Memory.Start;
				mmiolen = resource->u.Memory.Length;
				break;

			case CmResourceTypeDma:
				break;
		}
	}

	// retrieve the IRQ of the evtchn-pci device 
	resource = raw->PartialDescriptors;
	nres = raw->Count;
	for (i=0; i<nres; i++, resource++) {
		switch (resource->Type) {
			case CmResourceTypeInterrupt:
				deviceIRQ = resource->u.Interrupt.Vector;
				fdx->device_irq = deviceIRQ;
				break;
		}
	}

	// I/O initialization 

	if (fdx->MappedPort) {
		fdx->PortBase = (PUCHAR) MmMapIoSpace(
			portBase,
			fdx->NumPorts,
			MmNonCached);
		if (!fdx->PortBase)
			return STATUS_NO_MEMORY;
	}
	else {
		fdx->PortBase = (PUCHAR) portBase.QuadPart;
	}

	if (memBase.QuadPart) {
		fdx->mem = MmMapIoSpace(memBase, mmiolen, MmNonCached);
		if (!fdx->mem)
			return STATUS_NO_MEMORY;
	}

	/* interrupt initialization */
	DBG_PRINT(("XenBus resources: vector %d, irql %d, dev %d.\n",
		vector, irql, deviceIRQ));
	status = IoConnectInterrupt(
		&DriverInterruptObj,
		(PKSERVICE_ROUTINE) XenbusOnInterrupt,
		(PVOID) fdx,
		NULL,
		vector,
		irql,
		irql,
		LevelSensitive,
		irqshared,
		affinity,
		FALSE);

	if (!NT_SUCCESS(status)) {
		DBG_PRINT(("XENBUS: IoConnectInterrupt fail.\n"));
		return status;
	}

	status = xenbus_xen_shared_init(memBase.QuadPart, mmiolen,
		deviceIRQ, OP_MODE_NORMAL);
	if (!NT_SUCCESS(status)) {
		if (fdx->PortBase && fdx->MappedPort)
			MmUnmapIoSpace(fdx->PortBase, fdx->NumPorts);
		fdx->PortBase = NULL;
		return status;
	}

	if (xenbus_get_pv_port_options(&options)) {
		DPR_INIT(("XenBus resources: write port %x, offset %x.\n",
			(PULONG)fdx->PortBase, options.port_offset));
		WRITE_PORT_ULONG((PULONG)(fdx->PortBase + options.port_offset),
			options.value);
	}

	return STATUS_SUCCESS;
}
#endif

static NTSTATUS FDOStartDevice(
  IN PDEVICE_OBJECT fdo,
  IN PCM_PARTIAL_RESOURCE_LIST raw,
  IN PCM_PARTIAL_RESOURCE_LIST translated
  )
{
    NTSTATUS status;
    PFDO_DEVICE_EXTENSION fdx;

    DPR_INIT(("XENBUS: FDOStartDevice IN, fdo = %p, gfdo = %p.\n", fdo ,gfdo));
    fdx =(PFDO_DEVICE_EXTENSION) fdo->DeviceExtension;

    status = IoSetDeviceInterfaceState(&fdx->ifname, TRUE);
    if (!NT_SUCCESS(status)) {
        DBG_PRINT(("xenbusdrv.sys: IosetDeviceInterfaceState failed: 0x%x\n",
			status));
    }

	status = FDOSetResources(fdx, raw, translated);

	fdx->power_state = PowerSystemWorking;
    fdx->pnpstate = Started;


	/* If we've setup shared_info_area, then we need to reinitialize. */
	if (shared_info_area) {
		/* If we've done IoDeleteDevice, node will be null so reinitialize. */
		if (vbd_watch.node == NULL)
			xs_finish_init(fdo, OP_MODE_NORMAL);

		DPR_INIT(("FDOStartDevice: start xenbus_probe_init, NumPDOs = %d\n",
			fdx->NumPDOs));
		status = xenbus_probe_init(fdo, OP_MODE_NORMAL);

		/* The system will cause a RELATIONS query, so we don't need to. */
	}

    DPR_INIT(("XENBUS: start FDO device successfully: %x.\n", status));

    return status;
}

static VOID FDOStopDevice(IN PDEVICE_OBJECT fdo)
{
    PFDO_DEVICE_EXTENSION fdx;

    //PAGED_CODE();

    DPR_INIT(("XENBUS: entring FDOStopDevice.\n"));

#if 0
#ifdef XENBUS_HAS_DEVICE
    fdx = (PFDO_DEVICE_EXTENSION) fdo->DeviceExtension;

    IoDisconnectInterrupt(DriverInterruptObj);

    if (fdx->PortBase && fdx->MappedPort)
        MmUnmapIoSpace(fdx->PortBase, fdx->NumPorts);
    fdx->PortBase = NULL;

    /*
    if (fdx->MemBase)
        MmUnmapIoSpace(fdx->MemBase, fdx->MemLength);
    fdx->MemBase = NULL;
    */

    DPR_INIT(("XENBUS: beginning to cleanup xs, gnttab, xeninfo.\n"));
    xs_cleanup();
    gnttab_suspend();
    xen_info_cleanup();
    FreeHypercallPage();
#endif
#endif

    DPR_INIT(("XENBUS: FDO stop device successfully.\n"));
}


static VOID FDORemoveDevice(IN PDEVICE_OBJECT fdo)
{
    PFDO_DEVICE_EXTENSION fdx;

    //PAGED_CODE();

    DPR_INIT(("XENBUS: entering FDORemoveDevice.\n"));

    fdx = (PFDO_DEVICE_EXTENSION) fdo->DeviceExtension;

    if (fdx->ifname.Buffer != NULL) {
        IoSetDeviceInterfaceState(&fdx->ifname, FALSE);

        ExFreePool(fdx->ifname.Buffer);
        RtlZeroMemory(&fdx->ifname, sizeof(UNICODE_STRING));
    }

	gfdx = (PFDO_DEVICE_EXTENSION) ExAllocatePoolWithTag(
	  NonPagedPool, sizeof(FDO_DEVICE_EXTENSION), XENBUS_POOL_TAG);
	if (gfdx) {
		InitializeListHead(&gfdx->ListOfPDOs);
		xenbus_copy_fdx(gfdx, fdx);
	}

    DPR_INIT(("XENBUS: leaving FDORemoveDevice.\n"));
}

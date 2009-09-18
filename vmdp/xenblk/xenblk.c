/*****************************************************************************
 *
 *   File Name:      xenblk.c
 *   Created by:     KRA
 *   Date created:   12-07-06
 *
 *   %version:       57 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 15:16:41 2009 %
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

#include "xenblk.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#endif // ALLOC_PRAGMA

#ifdef DBG
uint32_t srbs_seen;
uint32_t srbs_returned;
uint32_t io_srbs_seen;
uint32_t io_srbs_returned;
uint32_t sio_srbs_seen;
uint32_t sio_srbs_returned;
#endif
//
// Miniport entry point decls.
//

static NTSTATUS XenBlkFindAdapter(
	IN PVOID dev_ext,
	IN PVOID HwContext,
	IN PVOID BusInformation,
	IN PCSTR ArgumentString,
	IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
	OUT PBOOLEAN Again);

static uint32_t is_xenbus_running(void);

static BOOLEAN XenBlkInitialize(XENBLK_DEVICE_EXTENSION *dev_ext);

static BOOLEAN XenBlkStartIo(XENBLK_DEVICE_EXTENSION *dev_ext,
	PSCSI_REQUEST_BLOCK Srb);

static NTSTATUS XenBlkStartReadWrite(XENBLK_DEVICE_EXTENSION *dev_ext,
	SCSI_REQUEST_BLOCK *Srb);

static KDEFERRED_ROUTINE XenBlkRestartDpc;

#ifndef XENBLK_STORPORT
static KDEFERRED_ROUTINE XenBlkStartReadWriteDpc;
#endif

static BOOLEAN XenBlkBuildIo(XENBLK_DEVICE_EXTENSION *dev_ext,
	PSCSI_REQUEST_BLOCK Srb);

static BOOLEAN XenBlkResetBus(XENBLK_DEVICE_EXTENSION *dev_ext, ULONG PathId);

static BOOLEAN XenBlkInterrupt(IN PVOID dev_ext);

static SCSI_ADAPTER_CONTROL_STATUS XenBlkAdapterControl (
	IN PVOID dev_ext,
	IN SCSI_ADAPTER_CONTROL_TYPE ControlType,
	IN PVOID Parameters);

static void XenBlkRestartAdapter(IN PDEVICE_OBJECT DeviceObject,
	XENBLK_DEVICE_EXTENSION *dev_ext);

static void XenBlkResume(XENBLK_DEVICE_EXTENSION *dev_ext,
	uint32_t suspend_canceled);
static uint32_t XenBlkSuspend(XENBLK_DEVICE_EXTENSION *dev_ext,uint32_t reason);

static uint32_t
XenBlkIoctl(XENBLK_DEVICE_EXTENSION *dev_ext, pv_ioctl_t data);

#ifdef DBG
uint32_t should_print_int = 0;
#endif


/*++

Routine Description:

    This routine initializes the XenBlk Storage class driver.

Arguments:

    DriverObject - Pointer to driver object created by system.
    RegistryPath - Pointer to the name of the services node for this driver.

Return Value:

    The function value is the final status from the initialization operation.

--*/

NTSTATUS
DriverEntry(IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath)
{

	HW_INITIALIZATION_DATA hwInitializationData;
	NTSTATUS status;
	uint32_t i;
	uint8_t vendorId[4]   = {'5', '8', '5', '3'};
	uint8_t deviceId1[4]  = {'0', '0', '0', '1'};
	KIRQL irql;

	irql = KeGetCurrentIrql();
	if (irql == PASSIVE_LEVEL) {
		if (is_xenbus_running() == 0) {
			/* Can not call into xenbus if we are not to use pv drivers. */
			DPR_EARLY(("xenblk DriverEntry: PV drivers not to be used.\n"));
			return STATUS_UNSUCCESSFUL;
		}
	}

	/* Don't printf before we know if we should be running. */
	DBG_PRINT(("Xenblk: Version %s.\n", VER_FILEVERSION_STR));
	DPR_INIT(("XenBlk: DriverEntry %x, irql = %d, HIGH_LEVEL = %d\n",
		DriverObject, irql, HIGH_LEVEL));
	if (irql >= CRASHDUMP_LEVEL) {
		DBG_PRINT(("XenBlk DriverEntry for crashdump: Begin.\n"));
	}

	for (i = 0; i < sizeof(HW_INITIALIZATION_DATA); i++) {
       ((PCHAR)&hwInitializationData)[i] = 0;
	}

	hwInitializationData.HwInitializationDataSize =
		sizeof(HW_INITIALIZATION_DATA);

	//
	// Set entry points into the miniport.
	//
	hwInitializationData.HwInitialize = XenBlkInitialize;
	hwInitializationData.HwFindAdapter = XenBlkFindAdapter;
	hwInitializationData.HwResetBus = XenBlkResetBus;
	hwInitializationData.HwAdapterControl = XenBlkAdapterControl;

	//
	// Sizes of the structures that port needs to allocate.
	//
	hwInitializationData.DeviceExtensionSize = sizeof(XENBLK_DEVICE_EXTENSION);
	hwInitializationData.SrbExtensionSize = sizeof(xenblk_srb_extension);
	hwInitializationData.SpecificLuExtensionSize = 0;

	DPR_INIT(("  setting acces ranges to 2\n"));
	hwInitializationData.NeedPhysicalAddresses = TRUE;
	hwInitializationData.TaggedQueuing = TRUE;
	hwInitializationData.AutoRequestSense = TRUE;
	hwInitializationData.MultipleRequestPerLu = TRUE;
	hwInitializationData.HwInterrupt = XenBlkInterrupt;

#ifdef XENBUS_HAS_DEVICE
	hwInitializationData.NumberOfAccessRanges = 0;
	hwInitializationData.AdapterInterfaceType = Internal;
#else
	hwInitializationData.NumberOfAccessRanges = 2;
	//
	// Set PCIBus, vendor/device id.
	//
	hwInitializationData.AdapterInterfaceType = PCIBus;
	DPR_INIT(("  set vendor stuff\n"));
	hwInitializationData.VendorIdLength = 4;
	hwInitializationData.VendorId = vendorId;
	hwInitializationData.DeviceIdLength = 4;
	hwInitializationData.DeviceId = deviceId1;
#endif


#ifndef XENBLK_STORPORT
	DPR_INIT(("  calling ScsiPortInitialize\n"));
	hwInitializationData.HwStartIo = XenBlkBuildIo;
	hwInitializationData.MapBuffers = TRUE;
	status = ScsiPortInitialize(DriverObject,
		RegistryPath,
		&hwInitializationData,
		NULL);
	DPR_INIT(("  DriverEntry - out ScsiPortInitialize status = %x\n", status));
#else
	DPR_INIT(("  lets try storport\n"));
	hwInitializationData.HwStartIo = XenBlkStartIo;
	hwInitializationData.HwBuildIo = XenBlkBuildIo;

	//
	// For StorPort MapBuffers is set to STOR_MAP_NON_READ_WRITE_BUFFERS so
	// that virtual addresses are only generated for non read/write requests.  
	//
	hwInitializationData.MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS;

	//
	// Call StorPort for each supported adapter.
	//
	DPR_INIT(("  calling StorPoprtInitialize\n"));
	status = StorPortInitialize(DriverObject,
		RegistryPath,
		&hwInitializationData,
		NULL);
	DPR_INIT(("  DriverEntry - out StorPortInitialize status = %x\n", status));
#endif
	if (irql >= CRASHDUMP_LEVEL) {
		DBG_PRINT(("XenBlk DriverEntry crashdump: returning with status %x.\n",
			status));
		DBG_PRINT(("XenBlk: *** Crashdump should now begin ***\n"));
	}
   
	return status;
}

static uint32_t
is_xenbus_running(void)
{
	UNICODE_STRING xenbus_str;
	PFILE_OBJECT file_obj;
	PDEVICE_OBJECT device_obj;
	NTSTATUS status;

	RtlInitUnicodeString(&xenbus_str, XENBUS_DEVICE_NAME_WSTR);
	status = IoGetDeviceObjectPointer(&xenbus_str,
		STANDARD_RIGHTS_ALL,
		&file_obj,
		&device_obj);

	if (NT_SUCCESS(status)) {
		ObDereferenceObject(file_obj);
		return 1;
	}
	return 0;

#if 0
	IO_STATUS_BLOCK  ioStatus;
	OBJECT_ATTRIBUTES objectAttributes;
	HANDLE handle;

	RtlInitUnicodeString (&xenbus_str, XENBUS_DEVICE_NAME_STRING);
	InitializeObjectAttributes(&objectAttributes,
		&xenbus_str,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		(HANDLE)NULL,
		(PSECURITY_DESCRIPTOR)NULL);

	status = ZwOpenFile(&handle,
		STANDARD_RIGHTS_ALL,
		&objectAttributes,
		&ioStatus,
		0,
		FILE_NON_DIRECTORY_FILE );

	if (NT_SUCCESS( status )) {
		ZwClose(handle);
		return 1;
	}
	return 0;
#endif
}

static NTSTATUS
XenBlkInitDevExt(
	XENBLK_DEVICE_EXTENSION *dev_ext,
	PPORT_CONFIGURATION_INFORMATION config_info,
	KIRQL irql)
{
	NTSTATUS status = 0;
    PACCESS_RANGE accessRange = &((*(config_info->AccessRanges))[0]);

#ifndef XENBLK_STORPORT
	KeInitializeDpc(&dev_ext->rwdpc, XenBlkStartReadWriteDpc, dev_ext);
	KeInitializeSpinLock(&dev_ext->dev_lock);
#endif
	KeInitializeDpc(&dev_ext->restart_dpc, XenBlkRestartDpc, dev_ext);

	DPR_INIT(("  BusInterruptLevel = %d, BusInterruptVector = %d.\n",
		config_info->BusInterruptLevel,
		config_info->BusInterruptVector));
	dev_ext->vector = config_info->BusInterruptVector;
	dev_ext->irql = (KIRQL)config_info->BusInterruptLevel;

	XENBLK_ZERO_VALUE(dev_ext->alloc_cnt_i);
	XENBLK_ZERO_VALUE(dev_ext->alloc_cnt_s);
	XENBLK_ZERO_VALUE(dev_ext->alloc_cnt_v);
	dev_ext->DevState = STOPPED;

	if (irql <= DISPATCH_LEVEL) {
		if (irql == PASSIVE_LEVEL)
			dev_ext->op_mode = OP_MODE_NORMAL;
		else
			dev_ext->op_mode = OP_MODE_HIBERNATE;
	}
	else {
		dev_ext->op_mode = OP_MODE_CRASHDUMP;
	}

	XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, 0xffffffff);
	XENBLK_CLEAR_FLAG(dev_ext->cpu_locks, 0xffffffff);

#ifdef XENBUS_HAS_DEVICE
    dev_ext->port = 0;
	dev_ext->mmio = 0;
	dev_ext->mmio_len = 0;
	dev_ext->mem = NULL;
#else
	DPR_INIT(("  Map the port io space\n"));
	DPR_INIT(("\tport start = %x %x, len = %x, rim %x\n",
		(uint32_t)(accessRange->RangeStart.QuadPart >> 32),
		(uint32_t)accessRange->RangeStart.QuadPart,
		accessRange->RangeLength,
		accessRange->RangeInMemory));

    dev_ext->port = xenblk_get_device_base(dev_ext,
		PCIBus,
		config_info->SystemIoBusNumber,
		accessRange->RangeStart,
		accessRange->RangeLength,
		(BOOLEAN)!accessRange->RangeInMemory);

	accessRange++;
	dev_ext->mmio = (uint64_t)accessRange->RangeStart.QuadPart;
	dev_ext->mmio_len = accessRange->RangeLength;

	DPR_INIT(("\tmmio start = %x %x, len = %x, rim = %x\n",
		(uint32_t)(dev_ext->mmio >> 32),
		(uint32_t)dev_ext->mmio,
		dev_ext->mmio_len,
		accessRange->RangeInMemory));

	dev_ext->mem = xenblk_get_device_base(dev_ext,
		PCIBus,
		config_info->SystemIoBusNumber,
		accessRange->RangeStart,
		accessRange->RangeLength,
		(BOOLEAN)!accessRange->RangeInMemory);

	if (!dev_ext->mem) {
		DBG_PRINT(("\tFindAdapter xenblk_get_device_base failed\n"));
		return SP_RETURN_ERROR;
	}
#endif
	return STATUS_SUCCESS;
}

static NTSTATUS
XenBlkFindAdapter(
	IN PVOID dev_extt,
	IN PVOID HwContext,
	IN PVOID BusInformation,
	IN PCSTR ArgumentString,
	IN OUT PPORT_CONFIGURATION_INFORMATION config_info,
	OUT PBOOLEAN Again)
{
	XENBLK_DEVICE_EXTENSION *dev_ext = (XENBLK_DEVICE_EXTENSION *)dev_extt;
	NTSTATUS status = 0;
    PACCESS_RANGE accessRange = &((*(config_info->AccessRanges))[0]);
	KIRQL irql;
	uint32_t flags = 0;

	irql = KeGetCurrentIrql();
	if (irql >= CRASHDUMP_LEVEL) {
		DBG_PRINT(("XenBlk XenBlkFindAdapter for crashdump: Begin.\n"));
	}
	DPR_INIT(("XenBlk: XenBlkFindAdapter - IN %s, irql = %d, dev = %p\n",
			 ArgumentString, irql, dev_ext));
	DPR_INIT(("  MTS = %ld, NPB = %ld, BA = %d, CD = %d\n",
		config_info->MaximumTransferLength, config_info->NumberOfPhysicalBreaks,
		config_info->BufferAccessScsiPortControlled, config_info->CachesData));

	config_info->MaximumTransferLength   =
		XENBLK_MAX_SGL_ELEMENTS * PAGE_SIZE;
	config_info->NumberOfPhysicalBreaks  = XENBLK_MAX_SGL_ELEMENTS;

	config_info->MaximumNumberOfTargets  = XENBLK_MAXIMUM_TARGETS;  
	config_info->MaximumNumberOfLogicalUnits = 1;  

	config_info->NumberOfBuses           = 1;
	config_info->InitiatorBusId[0]       = 7;
	//config_info->CachesData              = TRUE;
	config_info->Master                  = TRUE;
	config_info->Dma32BitAddresses       = TRUE;
	config_info->NeedPhysicalAddresses   = TRUE;
	config_info->TaggedQueuing           = TRUE;
	config_info->AlignmentMask           = 0x3;
	config_info->BufferAccessScsiPortControlled = TRUE;
	config_info->CachesData = TRUE;
	config_info->ScatterGather           = TRUE;
	//config_info->MapBuffers              = STOR_MAP_NON_READ_WRITE_BUFFERS;
	//config_info->AdapterInterfaceType = PCIBus;
	/*
	config_info->BusInterruptLevel = 9;
	config_info->BusInterruptVector = 9;// 58; //0x3a;
	config_info->InterruptMode = Latched; //LevelSensitive;
	*/
#ifdef XENBLK_STORPORT
	config_info->SynchronizationModel    = StorSynchronizeFullDuplex;
#endif

	if (dev_ext->DevState == REMOVED) {
		DPR_INIT(("XenBlkFindAdapter returning FOUND: in REMOVED state.\n"));
		return SP_RETURN_FOUND;
	}

#ifdef DBG
	srbs_seen = 0;
	srbs_returned = 0;
	io_srbs_seen = 0;
	io_srbs_returned = 0;
	sio_srbs_seen = 0;
	sio_srbs_returned = 0;
#endif
	XenBlkInitDevExt(dev_ext, config_info, irql);

	DPR_INIT(("  Level %x, vector %x, mode %x\n",config_info->BusInterruptLevel,
		config_info->BusInterruptVector, config_info->InterruptMode));
	DPR_INIT(("  XenBlkFindAdapter - out: d = %x, c = %x, mtl = %x, npb = %x\n",
		dev_ext, config_info, config_info->MaximumTransferLength,
		config_info->NumberOfPhysicalBreaks));
	if (irql >= CRASHDUMP_LEVEL) {
		DBG_PRINT(("XenBlk XenBlkFindAdapter for crashdump: Returning.\n"));
	}

	return SP_RETURN_FOUND;
}

static NTSTATUS
XenBlkClaim(XENBLK_DEVICE_EXTENSION *dev_ext)
{
	struct blkfront_info *info;
	NTSTATUS status;
	int i;

	/* The info array of pointers comes form xenbus and all pointers */
	/* will be null to start with but will be filled out already */
	/* when hibernating. */
	for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
		if (dev_ext->info[i]) {
			/* info already exists, no need to try to claim it again. */
			continue;
		}

		/* Check if we would succeed in claiming the device. */
		status = xenbus_claim_device(NULL, dev_ext, vbd, disk,
			XenBlkIoctl, XenBlkIoctl);
		if (status == STATUS_NO_MORE_ENTRIES) {
			/* There are no more devices to claim so return success. */
			status = STATUS_SUCCESS;
			break;
		}
		if (status != STATUS_SUCCESS) {
			break;
		}
		info = ExAllocatePoolWithTag(NonPagedPool, 
			sizeof(struct blkfront_info), XENBLK_TAG_GENERAL);
		if (info == NULL) {
			DBG_PRINT(("  failed to alloc info.\n"));
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		XENBLK_INC(dev_ext->alloc_cnt_i);
		memset(info, 0, sizeof(struct blkfront_info));
		info->xbdev = dev_ext;

		/* Use info as the identifier to xenbus. */
		status = xenbus_claim_device(info, dev_ext, vbd, disk,
			XenBlkIoctl, XenBlkIoctl);
		if (status == STATUS_UNSUCCESSFUL || status == STATUS_NO_MORE_ENTRIES) {
			DBG_PRINT(("  failed to claim device: %x.\n", status));
			ExFreePool(info);
			XENBLK_DEC(dev_ext->alloc_cnt_i);
			break;
		}

		/* Now do the Xen initialization. */
		status = blkfront_probe(info);
		if (status != STATUS_SUCCESS) {
			DBG_PRINT(("  blkfront_probe failed: %x\n", status));
			/* We cannot release the device because the next time through */
			/* the loop we would just try to claim it again. */
			ExFreePool(info);
			XENBLK_DEC(dev_ext->alloc_cnt_i);
			continue;
		}
		dev_ext->info[i] = info;
	}
	return status;
}

static BOOLEAN
XenBlkInit(XENBLK_DEVICE_EXTENSION *dev_ext)
{
	xenbus_pv_port_options_t options;
	xenbus_release_device_t release_data;
	NTSTATUS status;
	uint32_t devices_to_probe;
	uint32_t i;

	DPR_INIT(("XenBlk: XenBlkInit - IN dev %p, sizeof(info) %d, irql = %d\n",
		dev_ext, sizeof(struct blkfront_info), KeGetCurrentIrql(),
		KeGetCurrentProcessorNumber()));
	
	if (dev_ext->DevState == WORKING) {
		DPR_INIT(("  XenBlkInit already initialized %p\n", dev_ext));
		return TRUE;
	}

	if (xenbus_xen_shared_init(dev_ext->mmio, dev_ext->mmio_len,
			dev_ext->vector, dev_ext->op_mode)
		!= STATUS_SUCCESS) {
		DBG_PRINT(("XenBlkInit: failed to initialize shared info.\n"));
		dev_ext->mem = NULL;
		dev_ext->info = (struct blkfront_info **)&dev_ext->mem;
		dev_ext->DevState = WORKING;
		dev_ext->op_mode = OP_MODE_SHUTTING_DOWN;
		return TRUE;
	}

	if (xenbus_register_xenblk(dev_ext, XenBlkIoctl, &dev_ext->info)
		!= STATUS_SUCCESS) {
		DBG_PRINT(("XenBlkInit: xenbus_register_xenblk failed\n"));
		dev_ext->DevState = WORKING;
		return TRUE;
	}

#ifndef XENBUS_HAS_DEVICE
	if ((devices_to_probe = xenbus_get_pv_port_options(&options))) {
		DPR_INIT(("  Write the port io space %x, offset %x, value %x\n",
			dev_ext->port, options.port_offset, options.value));
		if (options.value) {
			xenblk_write_port_ulong(dev_ext,
				(PULONG)(((PCHAR)dev_ext->port) + options.port_offset),
				options.value);
			DPR_INIT(("  Done Write to the port io space\n"));
		}
	}
	else {
		DPR_INIT(("  No PV drivers are to be used\n"));
		dev_ext->DevState = WORKING;
		return TRUE;
	}

	if (!(devices_to_probe & XENBUS_PROBE_PV_DISK)) {
		/* We are not to control any disks but we sitll need to handle */
		/* interrupts for other device types. */
		DPR_INIT(("Xenblk driver is not to be used\n"));
		dev_ext->DevState = WORKING;
		return TRUE;
	}
#endif

	status = XenBlkClaim(dev_ext);

	xenblk_resume(dev_ext);
	dev_ext->DevState = WORKING;
	DPR_INIT(("XenBlkInit - OUT status %x, irql %d, cpu %x\n", status,
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	return TRUE;
}

static void
XenBlkInitHiberCrash(XENBLK_DEVICE_EXTENSION *dev_ext)
{
	uint32_t i, j;
	KIRQL irql;

	DPR_INIT(("XenBlk XenBlkInitHiberCrash: IN.\n"));
	irql = KeGetCurrentIrql();
	if (irql >= CRASHDUMP_LEVEL) {
		DBG_PRINT(("XenBlk XenBlkInitHiberCrash: Begin.\n"));
	}
	for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {

		/* The info array of pointers comes form xenbus and all pointers */
		/* will be null to start with but will be filled out already */
		/* when hibernating. */
		if (dev_ext->info[i] != NULL) {
			DPR_INIT(("\thibernate or crash dump\n"));
			xenblk_print_save_req(&dev_ext->info[i]->xbdev->req);
			mask_evtchn(dev_ext->info[i]->evtchn);

			/* Clear out any grants that may still be around. */
			DPR_INIT(("\tdoing shadow completion\n"));
			for (j = 0; j < BLK_RING_SIZE; j++) {
				dev_ext->info[i]->shadow[j].req.nr_segments = 0;
			}

			/* In hibernate mode we get a new dev_ext, but we are using */
			/* the original info.  Replace the original dev_ext in info */
			/* with the one used to hibernate. */
			DPR_INIT(("\tdev_ext dev_ext->info[i]->xbdev: %p, %p, op %x, xop %x\n",
				dev_ext, dev_ext->info[i]->xbdev,
				dev_ext->op_mode, dev_ext->info[i]->xbdev->op_mode));
			if (dev_ext->info[i]->xbdev) {
				dev_ext->info[i]->xbdev->DevState = REMOVED;
				dev_ext->info[i]->xbdev->op_mode = dev_ext->op_mode;
			}
			dev_ext->info[i]->xbdev = dev_ext;

			/* Since we are hibernating and didn't go through probe, */
			/* we need to regrant the shared ring. */
			DPR_INIT(("\tgrant the ring\n"));
			xenbus_grant_ring(dev_ext->info[i]->otherend_id,
				virt_to_mfn(dev_ext->info[i]->ring.sring));
		}
	}
	xenblk_resume(dev_ext);
	dev_ext->DevState = WORKING;
	if (irql >= CRASHDUMP_LEVEL) {
		DBG_PRINT(("XenBlk XenBlkInitHiberCrash: Returning.\n"));
	}
	DPR_INIT(("XenBlk XenBlkInitHiberCrash: OUT.\n"));
}

static BOOLEAN
XenBlkInitialize(XENBLK_DEVICE_EXTENSION *dev_ext)
{
	uint32_t i;
	KIRQL irql;

	irql = KeGetCurrentIrql();
	if (irql >= CRASHDUMP_LEVEL) {
		DBG_PRINT(("XenBlk XenBlkInitialize for crashdump: Begin.\n"));
	}
	XENBLK_SET_FLAG(dev_ext->xenblk_locks, (BLK_IZE_L | BLK_INT_L));
	XENBLK_SET_FLAG(dev_ext->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
	DPR_INIT(("XenBlk: XenBlkInitialize - IN irql = %d, dev = %p, op_mode %x\n",
		irql, dev_ext, dev_ext->op_mode));

	if (dev_ext->DevState == REMOVED) {
		for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
			if (dev_ext->info[i]) {
				unmask_evtchn(dev_ext->info[i]->evtchn);
			}
		}
		dev_ext->DevState = WORKING;
		DPR_INIT(("XenBlkInitialize returning TRUE: in REMOVED state.\n"));
		XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_IZE_L | BLK_INT_L));
		XENBLK_CLEAR_FLAG(dev_ext->cpu_locks,
			(1 << KeGetCurrentProcessorNumber()));
		return TRUE;
	}

	dev_ext->DevState = INITIALIZING;

	if (dev_ext->op_mode == OP_MODE_NORMAL) {
		
		/* Scsi passive initialization will start from XenBlkAdapterControl. */
        StorPortEnablePassiveInitialization(dev_ext, XenBlkInit);
		DPR_INIT(("XenBlk: XenBlkInitialize, we'll do init from control\n"));
	}
	else {
		if (xenbus_xen_shared_init(dev_ext->mmio, dev_ext->mmio_len,
				dev_ext->vector, dev_ext->op_mode)
			!= STATUS_SUCCESS) {
			DBG_PRINT(("XenBlkInitialize: failed to initialize shared info\n"));
			dev_ext->mem = NULL;
			dev_ext->info = (struct blkfront_info **)&dev_ext->mem;
			dev_ext->DevState = WORKING;
			dev_ext->op_mode = OP_MODE_SHUTTING_DOWN;
			return TRUE;
		}

		if (xenbus_register_xenblk(dev_ext, XenBlkIoctl, &dev_ext->info)
			!= STATUS_SUCCESS) {
			DBG_PRINT(("XenBlkInitialize: xenbus_register_xenblk failed\n"));
			dev_ext->DevState = WORKING;
			return TRUE;
		}
		if (dev_ext->op_mode == OP_MODE_HIBERNATE) {
#ifdef XENBLK_STORPORT
#ifdef XENBUS_HAS_DEVICE
			XenBlkInitHiberCrash(dev_ext);
#else
			XenBlkClaim(dev_ext);
			dev_ext->DevState = WORKING;
#endif
#else
			XenBlkInitHiberCrash(dev_ext);
#endif
		}
		else {
			XenBlkInitHiberCrash(dev_ext);
		}
	}

	XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_IZE_L | BLK_INT_L));
	XENBLK_CLEAR_FLAG(dev_ext->cpu_locks,
		(1 << KeGetCurrentProcessorNumber()));
	DPR_INIT(("  XenBlkInitialize - OUT\n"));
	if (irql >= CRASHDUMP_LEVEL) {
		DBG_PRINT(("XenBlk XenBlkInitialize for crashdump: Returning.\n"));
	}
	return TRUE;
}

static BOOLEAN
XenBlkStartIo(XENBLK_DEVICE_EXTENSION *dev_ext, PSCSI_REQUEST_BLOCK Srb)
{
	struct blkfront_info *info;
    XENBLK_LOCK_HANDLE lh;
	KIRQL oldirql;
	uint32_t i;

	/* StorPort already has the lock.  Just need it for scsiport. */
	XENBLK_INC(sio_srbs_seen);
#ifdef DBG
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INIT(("XenBlkStartIo i %d c %d l %x: should dev %p srb %p - IN\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
			dev_ext->xenblk_locks, dev_ext, Srb));
	}
#endif
	scsiport_acquire_spinlock(&dev_ext->dev_lock, &lh);
	XENBLK_SET_FLAG(dev_ext->xenblk_locks, (BLK_STI_L | BLK_SIO_L));
#ifdef DBG
	if (dev_ext->info[0] && dev_ext != dev_ext->info[0]->xbdev) {
		DPR_INIT(("XenBlkStartIo: dev_ext != dev_ext->info[i]->xbdev: %p, %p\n",
			dev_ext, dev_ext->info[0]->xbdev));
		DPR_INIT(("   irql = %d, tid = %d, func = %x\n",
			KeGetCurrentIrql(), Srb->TargetId, Srb->Cdb[0]));
	}
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INIT(("   XenBlkStartIo should irql = %d, s = %p, f = %x, c = %x\n",
			KeGetCurrentIrql(), Srb, Srb->Cdb[0],
			KeGetCurrentProcessorNumber()));
	}
#endif
	DPRINT(("    XenBlkStartIo dev %x - IN irql = %d, tid = %d, func = %x\n",
		dev_ext, KeGetCurrentIrql(), Srb->TargetId, Srb->Cdb[0]));

	info = dev_ext->info[Srb->TargetId];
	if (info == NULL || info->connected != BLKIF_STATE_CONNECTED) {
		DPR_INIT(("XenBlkStartIo irql %d, dev %p, tid = %d, fnc = %x\n",
			KeGetCurrentIrql(), dev_ext, Srb->TargetId, Srb->Cdb[0]));

		if (info == NULL && dev_ext->DevState == WORKING) {
			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			DPR_INIT(("\tReturning SRB_STATUS_NO_DEVICE\n"));
		}
		else {
			Srb->SrbStatus = SRB_STATUS_BUSY;
			DPR_INIT(("\tBlk not ready yet, returning SRB_STATUS_BUSY\n"));
		}

		XENBLK_INC(srbs_returned);
		XENBLK_INC(sio_srbs_returned);
        xenblk_request_complete(RequestComplete, dev_ext, Srb);
		xenblk_next_request(NextRequest, dev_ext);
		if (Srb->SrbStatus == SRB_STATUS_BUSY) {
			xenblk_pause(dev_ext, 1);
		}
		XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_STI_L | BLK_SIO_L));
		scsiport_release_spinlock(&dev_ext->dev_lock, lh);
#ifdef DBG
		if (should_print_int < SHOULD_PRINT_INT) {
			xenbus_debug_dump();
			XenBlkDebugDump(dev_ext);
		}
#endif
        return TRUE;
	}
	if (dev_ext->DevState != WORKING && dev_ext->DevState != STOPPED) {
		DPR_INIT(("XenBlkStartIo dev %p, i = %d, t = %d, f = %x cb = %x\n",
			dev_ext, KeGetCurrentIrql(),
			Srb->TargetId, Srb->Function, Srb->Cdb[0]));
		Srb->SrbStatus = SRB_STATUS_BUSY;
		XENBLK_INC(srbs_returned);
		XENBLK_INC(sio_srbs_returned);
		xenblk_request_complete(RequestComplete, dev_ext, Srb);
		xenblk_next_request(NextRequest, dev_ext);
		XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_STI_L | BLK_SIO_L));
		scsiport_release_spinlock(&dev_ext->dev_lock, lh);
		return TRUE;
	}

	if (Srb->Lun > 0) {
		DPR_INIT(("  TargetId = %d, Lun = %d, func = %d, sf = %x.\n",
			Srb->TargetId, Srb->Length, Srb->Function, Srb->Cdb[0]));
		Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
		XENBLK_INC(srbs_returned);
		XENBLK_INC(sio_srbs_returned);
        xenblk_request_complete(RequestComplete, dev_ext, Srb);
		xenblk_next_request(NextRequest, dev_ext);
		XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_STI_L | BLK_SIO_L));
		scsiport_release_spinlock(&dev_ext->dev_lock, lh);
        return TRUE;
	}
	XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_STI_L | BLK_SIO_L));
	scsiport_release_spinlock(&dev_ext->dev_lock, lh);

#ifdef XENBUS_HAS_DEVICE
	//KeAcquireSpinLock(&info->lock, &oldirql);
#endif
    switch (Srb->Function) {
        case SRB_FUNCTION_EXECUTE_SCSI: {
            switch (Srb->Cdb[0]) {
                case SCSIOP_MEDIUM_REMOVAL:
					DPR_INIT(("%x: SCSIOP_MEDIUM_REMOVAL\n", Srb->TargetId));
                    //
                    // BsaPowerManagement 6.4.6.13
                    //
                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;
                case SCSIOP_REQUEST_SENSE:
					DPR_INIT(("%x: SCSIOP_REQUEST_SENSE\n", Srb->TargetId));
                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    break;

                case SCSIOP_MODE_SENSE:
					DPR_INIT(("%x: SCSIOP_MODE_SENSE\n", Srb->TargetId));
                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    break;

				case SCSIOP_READ_CAPACITY: {
					uint32_t last_sector;

					DPR_INIT(("%x: SCSIOP_READ_CAPACITY\n", Srb->TargetId));
					REVERSE_BYTES(
						&((PREAD_CAPACITY_DATA)Srb->DataBuffer)->BytesPerBlock,
						&info->sector_size);

					if (info->sectors > 0xffffffff) {
						DPR_INIT(("%x: Disk > 2TB: %x%08x, returning 0xffffffff.\n", Srb->TargetId,
							(uint32_t)(info->sectors >> 32),
							(uint32_t)info->sectors));
						last_sector = 0xffffffff;
					}
					else {
						last_sector = (uint32_t)(info->sectors - 1);
					}
					REVERSE_BYTES(
						&((PREAD_CAPACITY_DATA)
							Srb->DataBuffer)->LogicalBlockAddress,
						&last_sector);

					DPR_INIT(("%x: sectrs %x%08x, sectr-sz %u, lst sectr %x\n", Srb->TargetId,
						(uint32_t)(info->sectors >> 32),
						(uint32_t)info->sectors,
						info->sector_size,
						last_sector));

                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;
				}

				case SCSIOP_READ_CAPACITY16: {
					uint64_t last_sector;

					DPR_INIT(("%x: SCSIOP_READ_CAPACITY16\n", Srb->TargetId));
					REVERSE_BYTES(
						&((PREAD_CAPACITY_DATA)Srb->DataBuffer)->BytesPerBlock,
						&info->sector_size);

					last_sector = info->sectors - 1;
					REVERSE_BYTES_QUAD(
						&((PREAD_CAPACITY_DATA_EX)
							Srb->DataBuffer)->LogicalBlockAddress,
						&last_sector);

					DPR_INIT(("%x: \tsctr %x%08x, sctrsz 0x%x, lsctr 0x%x%08x\n",
						Srb->TargetId,
						(uint32_t)(info->sectors >> 32),
						(uint32_t)info->sectors,
						info->sector_size,
						(uint32_t)(last_sector >> 32),
						(uint32_t)last_sector));

                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;
				}

				case SCSIOP_READ:
                case SCSIOP_WRITE:
				case SCSIOP_READ16:
                case SCSIOP_WRITE16: {
					NTSTATUS status;

					DPR_TRC(("%x: SCSIOP_WRITE SCSIOP_READ %x, dev=%x,srb=%x\n",
						Srb->TargetId, Srb->Cdb[0], dev_ext, Srb));
#ifdef DBG
					if (dev_ext->DevState != WORKING) {
						DPR_INIT(("%x: %p stopped:irql = %d, tid = %d, f=%x\n",
							Srb->TargetId,
							dev_ext, KeGetCurrentIrql(), Srb->TargetId,
							Srb->Cdb[0]));
					}
					if (should_print_int < SHOULD_PRINT_INT) {
						DPR_INIT(("XenBlkStartIo id %d i %d c %d: dev %p srb %p do\n",
							Srb->TargetId,
							KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
							dev_ext, Srb));
					}
#endif

					status = do_blkif_request(info, Srb);
					if (status  == STATUS_SUCCESS) {
						xenblk_next_request(NextRequest, dev_ext);
#ifdef DBG
						if (should_print_int < SHOULD_PRINT_INT) {
							DPR_INIT(("%x: XBStrtIo: do success OUT cpu=%x.\n",
								Srb->TargetId,
								KeGetCurrentProcessorNumber()));
								DPR_INIT(("IO: srbs_seen = %x, srbs_returned = %x\n",
										   srbs_seen, srbs_returned));
								DPR_INIT(("IO: sio_srbs_seen = %x, sio_srbs_returned = %x\n",
									sio_srbs_seen, sio_srbs_returned));
								DPR_INIT(("IO: io_srbs_seen = %x, io_srbs_returned = %x\n",
									io_srbs_seen, io_srbs_returned));
						}
#endif
						DPR_TRC(("%x: SCSIOP_WRITE SCSIOP_READ returning %x\n",
							Srb->TargetId));
						DPR_TRC(("    dev = %x, srb = %x\n",
							Srb->Cdb[0], dev_ext, Srb));
						XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks,
							(BLK_STI_L | BLK_SIO_L));
#ifdef XENBUS_HAS_DEVICE
						//KeReleaseSpinLock(&info->lock, oldirql);
#endif
						return TRUE;
					}
					else {
						Srb->SrbStatus = SRB_STATUS_BUSY;
						DBG_PRINT(("%x:  SRB_STATUS_BUSY\n", Srb->TargetId));
					}
					DPR_TRC(("    SCSIOP_WRITE SCSIOP_READ out\n"));
					break;
				}

                case SCSIOP_INQUIRY: {
					PINQUIRYDATA inquiryData;
					uint8_t *rbuf;

					DPR_INIT(("%x: SCSIOP_INQUIRY l = %x, isz = %x, srb = %x\n",
						Srb->TargetId,
						Srb->DataTransferLength, sizeof(INQUIRYDATA), Srb));
					DPR_INIT(("    0 %x, 1 %x, 2 %x, 3 %x, 4 %x\n",
						Srb->TargetId,
						Srb->Cdb[0],Srb->Cdb[1],Srb->Cdb[2],
						Srb->Cdb[3],Srb->Cdb[4]));

					Srb->SrbStatus = SRB_STATUS_SUCCESS;
					if (Srb->Cdb[1] == 0) {
						inquiryData = Srb->DataBuffer;
						memset(inquiryData, 0, Srb->DataTransferLength);

						inquiryData->DeviceType = DIRECT_ACCESS_DEVICE;
						inquiryData->DeviceTypeQualifier = DEVICE_CONNECTED;
						inquiryData->RemovableMedia = 0;
						//inquiryData->CommandQueue = 1; // tagged queueing 
						//inquiryData->Versions = 2;
						//inquiryData->ResponseDataFormat = 2;
						//inquiryData->Wide32Bit = 1;


						for (i = 0; i < 8; i++) {
							inquiryData->VendorId[i] = ' ';
						}

						inquiryData->VendorId[0] = 'N';
						inquiryData->VendorId[1] = 'o';
						inquiryData->VendorId[2] = 'v';
						inquiryData->VendorId[3] = 'e';
						inquiryData->VendorId[4] = 'l';
						inquiryData->VendorId[5] = 'l';

						for (i = 0; i < 16; i++) {
							inquiryData->ProductId[i] = ' ';
						}

						inquiryData->ProductId[0] = 'X';
						inquiryData->ProductId[1] = 'e';
						inquiryData->ProductId[2] = 'n';
						inquiryData->ProductId[3] = ' ';
						inquiryData->ProductId[4] = 'B';
						inquiryData->ProductId[5] = 'l';
						inquiryData->ProductId[6] = 'o';
						inquiryData->ProductId[7] = 'c';
						inquiryData->ProductId[8] = 'k';

						inquiryData->ProductRevisionLevel[0] = '0';
						inquiryData->ProductRevisionLevel[1] = '.';
						inquiryData->ProductRevisionLevel[2] = '0';
						inquiryData->ProductRevisionLevel[3] = '1';
					}
					else if (Srb->Cdb[1] & 1) {
						/* The EVPD bit is set.  Check which page to return. */
						switch (Srb->Cdb[2]) {
							case VPD_SUPPORTED_PAGES: {
								PVPD_SUPPORTED_PAGES_PAGE rbuf;

								rbuf = (PVPD_SUPPORTED_PAGES_PAGE )
									Srb->DataBuffer;

								DPR_INIT(("%x: SCSIOP_INQUIRY page 0.\n", Srb->TargetId));
								rbuf->DeviceType = DIRECT_ACCESS_DEVICE;
								rbuf->DeviceTypeQualifier = DEVICE_CONNECTED;
								rbuf->PageCode = 0;
								/* rbuf->Reserved; */
								rbuf->PageLength = 3;
								rbuf->SupportedPageList[0] = 0;
								rbuf->SupportedPageList[1] = 0x80;
								rbuf->SupportedPageList[2] = 0x83;
								break;
							}
							case VPD_DEVICE_IDENTIFIERS: {
								PVPD_IDENTIFICATION_PAGE rbuf;

								rbuf = (PVPD_IDENTIFICATION_PAGE)
									Srb->DataBuffer;

								DPR_INIT(("%x: SCSIOP_INQUIRY page 83 %s, %d.\n", Srb->TargetId,
										  XENBLK_DESIGNATOR_STR,
										  strlen(XENBLK_DESIGNATOR_STR)));

								rbuf->DeviceType = DIRECT_ACCESS_DEVICE;
								rbuf->DeviceTypeQualifier = DEVICE_CONNECTED;
								rbuf->PageCode = 0x83;
								/* rbuf->Reserved; */
								rbuf->PageLength =
									(uint8_t)strlen(XENBLK_DESIGNATOR_STR);
								rbuf->Descriptors[0] = VpdCodeSetAscii;
								rbuf->Descriptors[1] =
									VpdIdentifierTypeVendorSpecific;
								/* rbuf->Descriptors[2] = reserved; */
								rbuf->Descriptors[3] =
									(uint8_t)strlen(XENBLK_DESIGNATOR_STR);

								memcpy(&rbuf->Descriptors[4],
									XENBLK_DESIGNATOR_STR,
									strlen(XENBLK_DESIGNATOR_STR));
								break;
							}
							case VPD_SERIAL_NUMBER: {
								PVPD_SERIAL_NUMBER_PAGE rbuf;

								rbuf = (PVPD_SERIAL_NUMBER_PAGE)
									Srb->DataBuffer;

								DPR_INIT(("%x: SCSIOP_INQUIRY page 80.\n", Srb->TargetId));
								rbuf->DeviceType = DIRECT_ACCESS_DEVICE;
								rbuf->DeviceTypeQualifier = DEVICE_CONNECTED;
								rbuf->PageCode = 0x80;
								/* rbuf->Reserved; */
								rbuf->PageLength = 8;
								memset(rbuf->SerialNumber, 0x20, 8);
								break;
							}
							default:
								Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
								break;
						}
					}
					else
						Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

                    break;
				}

                case SCSIOP_TEST_UNIT_READY:
					DPR_INIT(("%x: SCSIOP_TEST_UNIT_READY\n", Srb->TargetId));
                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;

                case SCSIOP_VERIFY:
                case SCSIOP_VERIFY16:
					DPR_INIT(("%x: SCSIOP_VERIFY %x\n", Srb->TargetId, Srb->Cdb[0]));
                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;
                    
                case SCSIOP_SYNCHRONIZE_CACHE:
					DPR_INIT(("%x: SCSIOP_SYNCHRONIZE_CACHE\n", Srb->TargetId));
                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;
                    
                case SCSIOP_START_STOP_UNIT:
					DPR_INIT(("%x: SCSIOP_START_STOP_UNIT\n", Srb->TargetId));
                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;
                    
                case SCSIOP_RESERVE_UNIT:
					DPR_INIT(("%x: SCSIOP_RESERVE_UNIT\n", Srb->TargetId));
                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;

                case SCSIOP_RELEASE_UNIT:
					DPR_INIT(("%x: SCSIOP_RELEASE_UNIT\n", Srb->TargetId));
                    Srb->SrbStatus = SRB_STATUS_SUCCESS;
                    break;

                case SCSIOP_REPORT_LUNS:
					DPR_INIT(("%x: SCSIOP_REPORT_LUNSS\n", Srb->TargetId));
                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    break;

                default:
					DPR_INIT(("%x: default %x\n", Srb->TargetId,Srb->Cdb[0]));
                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    break;
            }
            break;

        }
        case SRB_FUNCTION_SHUTDOWN:
			if (KeGetCurrentIrql() >= CRASHDUMP_LEVEL) {
				DBG_PRINT(("XenBlk: *** Crashdump is now complete ***\n"));
				DBG_PRINT(("  XenBlkStartIo crashdump: Shutting down.\n"));
			}
			DPR_INIT(("%x: SRB_FUNCTION_SHUTDOWN %d: m = %x, st = %x\n",
				Srb->TargetId,
				KeGetCurrentIrql(), dev_ext->op_mode, dev_ext->DevState));
			for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
				if (dev_ext->info[i]) {
					blkif_quiesce(dev_ext->info[i]);
				}
			}
			if (dev_ext->op_mode == OP_MODE_HIBERNATE) {
				XenBlkFreeAllResources(dev_ext, RELEASE_ONLY);
				xenbus_switch_state(NULL, XENBUS_STATE_POWER_OFF);
			}
			if (dev_ext->op_mode == OP_MODE_NORMAL)
				dev_ext->op_mode = OP_MODE_SHUTTING_DOWN;
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			if (KeGetCurrentIrql() >= CRASHDUMP_LEVEL) {
				DBG_PRINT(("  XenBlkStartIo crashdump: Shutdown returning.\n"));
			}
            break;
            
        case SRB_FUNCTION_FLUSH:
			DPRINT(("%x: SRB_FUNCTION_FLUSH\n", Srb->TargetId));
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
            break;

        case SRB_FUNCTION_IO_CONTROL:
			DPR_INIT(("%x: SRB_FUNCTION_IO_CONTROL\n", Srb->TargetId));
			Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
            break;

        case SRB_FUNCTION_RESET_LOGICAL_UNIT:
        case SRB_FUNCTION_RESET_DEVICE:
			DPR_INIT(("%x: SRB_FUNCTION_RESET_DEVICE\n", Srb->TargetId));
			if (dev_ext->op_mode == OP_MODE_SHUTTING_DOWN)
				dev_ext->op_mode = OP_MODE_NORMAL;
			Srb->SrbStatus = SRB_STATUS_SUCCESS;

			xenbus_debug_dump();
			XenBlkDebugDump(dev_ext);

			xenblk_acquire_spinlock(dev_ext, &dev_ext->dev_lock,
				InterruptLock, NULL, &lh);
			DPR_INIT(("XenBlkStartIo calling XenBlkInterrupt\n"));

			/* Don't set BLK_INT_L because XenBlkInterrupt will. */
			XenBlkInterrupt(dev_ext);

			DPR_INIT(("XenBlkStartIo back form calling XenBlkInterrupt\n"));
			xenblk_release_spinlock(dev_ext, &dev_ext->dev_lock, lh);

            break;

		case SRB_FUNCTION_PNP: {
			SCSI_PNP_REQUEST_BLOCK *pnp = (SCSI_PNP_REQUEST_BLOCK *)Srb;
			DPR_INIT(("%x:%x: SRB_FUNCTION_PNP, action %x, sub %x, path %x\n",
					  Srb->TargetId, pnp->Lun,
					  pnp->PnPAction, pnp->PnPSubFunction, pnp->PathId));
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			break;
		}

		case SRB_FUNCTION_WMI: {
			SCSI_WMI_REQUEST_BLOCK *wmi = (SCSI_WMI_REQUEST_BLOCK *)Srb;
			DPR_INIT(("%x: SRB_FUNCTION_WMI, flag %x, sub %x, lun %x\n",
					  Srb->TargetId,
					  wmi->WMIFlags, wmi->WMISubFunction, wmi->Lun));
			Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;
		}
                                                
        default:
			DPR_INIT(("%x: SRB_ default %x\n", Srb->TargetId, Srb->Function));
            Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
            break;
    }

#ifdef DBG
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INIT(("XenBlkStartIo i %d c %d: dev %p srb %p - xbk_req_complete\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(), dev_ext, Srb));
	}
#endif
	XENBLK_INC(srbs_returned);
	XENBLK_INC(sio_srbs_returned);
	xenblk_request_complete(RequestComplete, dev_ext, Srb);

#ifdef DBG
	if (srbs_seen != srbs_returned) {
		DBG_PRINT(("C: srbs_seen = %x, srbs_returned = %x, diff %d.\n",
				   srbs_seen, srbs_returned, srbs_seen - srbs_returned));
	}
	if (sio_srbs_seen != sio_srbs_returned) {
		DBG_PRINT(("C: sio_srbs_seen = %x, sio_srbs_returned = %x, diff %d.\n",
			sio_srbs_seen, sio_srbs_returned, sio_srbs_seen-sio_srbs_returned));
	}
	if (io_srbs_seen != io_srbs_returned) {
		DBG_PRINT(("C: io_srbs_seen = %x, io_srbs_returned = %x, diff %d.\n",
			io_srbs_seen, io_srbs_returned, io_srbs_seen - io_srbs_returned));
	}

	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INIT(("XenBlkStartIo i %d c %d: dev %p srb %p - xbk_next_request\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(), dev_ext, Srb));
	}
#endif
	xenblk_next_request(NextRequest, dev_ext);

	DPR_IO(("  XenBlkStartIo srb %x, status = %x - Out\n", Srb,Srb->SrbStatus));
	XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_STI_L | BLK_SIO_L));
#ifdef DBG
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INIT(("XenBlkStartIo i %d c %d: dev %p srb %p - OUT XBDD\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(), dev_ext, Srb));
		//XenBlkDebugDump(dev_ext);
	}
#endif
#ifdef XENBUS_HAS_DEVICE
	//KeReleaseSpinLock(&info->lock, oldirql);
#endif
	return TRUE;
}

#ifdef DBG
static void
XenBlkVerifySGL(xenblk_srb_extension *srb_ext, ULONG tlen)
{
	uint32_t i;
	uint32_t len;

	if (srb_ext->sgl->NumberOfElements > XENBLK_MAX_SGL_ELEMENTS)
		DBG_PRINT(("XenBlkVerifySGL va %p: too many sgl emements %x.\n",
		   srb_ext->va, srb_ext->sgl->NumberOfElements));

	len = 0;
	for (i = 0; i < srb_ext->sgl->NumberOfElements; i++) {
		if ((((uint32_t)srb_ext->sgl->List[i].PhysicalAddress.QuadPart)
				& (PAGE_SIZE - 1)
			&& ((uint32_t)srb_ext->sgl->List[i].PhysicalAddress.QuadPart)
				& 0x1ff)) {
			DBG_PRINT(("XenBlkVerifySGL va %p:SGL element %x not aligned;%x.\n",
				srb_ext->va, i,
				((uint32_t)srb_ext->sgl->List[i].PhysicalAddress.QuadPart)));
		}
		if (srb_ext->sgl->List[i].Length % 512) {
			DBG_PRINT(("XenBlkVerifySGL va %p: SGL element %x has lenght %x.\n",
				srb_ext->va, i,	srb_ext->sgl->List[i].Length));
		}
		if (srb_ext->sgl->List[i].Length == 0) {
			DBG_PRINT(("XenBlkVerifySGL: SGL element %x has lenght %x.\n",
				i,	srb_ext->sgl->List[i].Length));
		}
		len += srb_ext->sgl->List[i].Length;
	}
	if (len != tlen) {
		DBG_PRINT(("XenBlkVerifySGL sgl len %x != DataTransferlen %x.\n",
			len, tlen));
	}

	len = 0;
	for (i = 0; i < srb_ext->sys_sgl->NumberOfElements; i++) {
		len += srb_ext->sys_sgl->List[i].Length;
	}
	if (len != tlen) {
		DBG_PRINT(("XenBlkVerifySGL sys_sgl len %x != DataTransferlen %x.\n",
			len, tlen));
	}
}
#endif

#ifdef XENBLK_DBG_MAP_SGL_ONLY
static void
xenblk_map_system_sgl_only(SCSI_REQUEST_BLOCK *srb,
	MEMORY_CACHING_TYPE cache_type)
{
	xenblk_srb_extension *srb_ext;
	uint32_t i;

#ifdef DBG
	KIRQL irql;

	irql = KeGetCurrentIrql();
	if (irql > DISPATCH_LEVEL && irql < HIGH_LEVEL) {
		DPR_INIT(("*** xenblk_map_system_sgl_only at irql %d *** \n", irql));
	}
#endif
	srb_ext = (xenblk_srb_extension *)srb->SrbExtension;
	ASSERT(srb_ext->sys_sgl->NumberOfElements <= XENBLK_MAX_SGL_ELEMENTS);
	for (i = 0; i < srb_ext->sys_sgl->NumberOfElements; i++) {
		srb_ext->sa[i] = MmMapIoSpace(
			srb_ext->sys_sgl->List[i].PhysicalAddress,
			srb_ext->sys_sgl->List[i].Length,
			cache_type);
		if (srb_ext->sa[i] == NULL) {
			DBG_PRINT(("xenblk_map_system_sgl_only: MmMapIoSpace failed.\n"));
		}
		DPR_MM(("\tMmMapIoSpace addr = %p, paddr = %lx, len = %x\n",
			srb_ext->sa[i],
			(uint32_t)srb_ext->sys_sgl->List[0].PhysicalAddress.QuadPart,
			srb_ext->sys_sgl->List[0].Length));
	}
}
#endif

static NTSTATUS
XenBlkStartReadWrite(XENBLK_DEVICE_EXTENSION *dev_ext,
	SCSI_REQUEST_BLOCK *Srb)
{
	xenblk_srb_extension *srb_ext;
	NTSTATUS status = STATUS_SUCCESS;
	ULONG len;

	DPR_TRC((" XenBlkStartReadWrite dev %x-IN srb = %p, ext = %p, irql = %d\n",
		dev_ext, Srb, Srb->SrbExtension, KeGetCurrentIrql()));
#ifdef DBG
	if (dev_ext->DevState != WORKING) {
		DPR_INIT(("XenBlkStartReadWrite dev %p is stopped: irql = %d\n",
			dev_ext, KeGetCurrentIrql()));
	}
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INIT(("\tXenBlkStartReadWrite i %d c %d: dev %p  srb %p - IN\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(), dev_ext, Srb));
	}
#endif
	srb_ext = (xenblk_srb_extension *)Srb->SrbExtension;
	srb_ext->sys_sgl = xenblk_build_sgl(dev_ext, Srb);
	if (srb_ext->sys_sgl) {
		/* If not on a good xen boundry, double buffer. */
		if ((((uint32_t)srb_ext->sys_sgl->List[0].PhysicalAddress.QuadPart)
					& 0x1ff)) {	//|| Srb->Cdb[0] == SCSIOP_READ) {
			DPR_INIT(("%x  Need to alloc: srb %p, op %x, addr %x, irql = %d\n",
				Srb->TargetId,
				Srb, Srb->Cdb[0],
				((uint32_t)srb_ext->sys_sgl->List[0].PhysicalAddress.QuadPart),
				KeGetCurrentIrql()));

			if ((srb_ext->va = ExAllocatePoolWithTag(NonPagedPool, 
					((Srb->DataTransferLength >> PAGE_SHIFT)
						+ PAGE_ROUND_UP) << PAGE_SHIFT,
					XENBLK_TAG_GENERAL))) {
				XENBLK_INC(dev_ext->alloc_cnt_v);
				srb_ext->pa.QuadPart = __pa(srb_ext->va);
				DPR_MM((", paddr = %lx, npaddr = %lx\n",
				   (uint32_t)srb_ext->sys_sgl->List[0].PhysicalAddress.QuadPart,
				   (uint32_t)srb_ext->pa.QuadPart));

				xenblk_map_system_sgl(Srb, MmCached);
				XenBlkVerifySGL(srb_ext, Srb->DataTransferLength);

				if (Srb->Cdb[0] == SCSIOP_WRITE
					|| Srb->Cdb[0] == SCSIOP_WRITE16){
					DPR_MM(("  Doing a write, do memcpy.\n"));
					xenblk_cp_from_sa(srb_ext->sa, srb_ext->sys_sgl,
						srb_ext->va);
					xenblk_unmap_system_address(srb_ext->sa, srb_ext->sys_sgl);
				}
#ifdef XENBLK_REQUEST_VERIFIER
				else {
					memset(srb_ext->va + Srb->DataTransferLength,
						0xab, PAGE_SIZE);
				}
#endif
			}
			else {
				DBG_PRINT(("XenBlkStartReadWrite: Failed to alloc memory.\n"));
				status = STATUS_NO_MEMORY;
			}
			DPR_MM(("XenBlkStartReadWrite: Srb %p, ext %p, va %p, sa %p\n",
					Srb, srb_ext, srb_ext->va, srb_ext->sa));
		}
		else {
			srb_ext->va = NULL;
			srb_ext->sgl = srb_ext->sys_sgl;
#ifdef XENBLK_DBG_MAP_SGL_ONLY
			if (Srb->Cdb[0] == SCSIOP_READ || Srb->Cdb[0] == SCSIOP_READ16)
				xenblk_map_system_sgl_only(Srb, MmCached);
#endif
			XenBlkVerifySGL(srb_ext, Srb->DataTransferLength);
		}
	}
	else {
		status = STATUS_UNSUCCESSFUL;
	}
	DPR_TRC((" XenBlkStartReadWrite dev %x-IN srb = %p, ext = %p, irql = %d\n",
		dev_ext, Srb, Srb->SrbExtension, KeGetCurrentIrql()));
	return status;
}

#ifndef XENBLK_STORPORT
static void
XenBlkStartReadWriteDpc(PKDPC dpc, XENBLK_DEVICE_EXTENSION *dev_ext,
	SCSI_REQUEST_BLOCK *Srb, PVOID sa2)
{
	XenBlkStartReadWrite(dev_ext, Srb);
	XenBlkStartIo(dev_ext, Srb);
}
#endif

static BOOLEAN
XenBlkBuildIo(XENBLK_DEVICE_EXTENSION *dev_ext, PSCSI_REQUEST_BLOCK Srb)
{
#ifndef XENBLK_STORPORT
	KIRQL irql;
#endif

	DPR_TRC(("XenBlk: XenBlkBuildIo %p-IN irql %d\n", Srb, KeGetCurrentIrql()));

	XENBLK_INC(srbs_seen);
#ifdef DBG
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INIT(("\tXenBlkBuildIo i %d c %d l %x: dev %p srb %p - IN\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
			dev_ext->xenblk_locks, dev_ext, Srb));
		should_print_int++;
	}
#endif
    if ((Srb->Function == SRB_FUNCTION_EXECUTE_SCSI) &&
		((Srb->Cdb[0] == SCSIOP_READ)
			|| (Srb->Cdb[0] == SCSIOP_WRITE)
			|| (Srb->Cdb[0] == SCSIOP_READ16)
			|| (Srb->Cdb[0] == SCSIOP_WRITE16))) {
		DPRINT(("  It's a read or write: srb = %p, data = %p, l = %lx\n",
			Srb, Srb->DataBuffer, Srb->DataTransferLength));

#ifdef DBG
		if (dev_ext->DevState != WORKING) {
			DPR_INIT(("XenBlkBuildIodev %p is stopped: irql = %d\n",
				dev_ext, KeGetCurrentIrql()));
		}
#endif

#ifndef XENBLK_STORPORT
		/* Scsiport StartIo comes in at greater than DISPATCH_LEVEL. */
		irql = KeGetCurrentIrql();
		if (irql >= CRASHDUMP_LEVEL) {
			XenBlkStartReadWriteDpc(NULL, dev_ext, Srb, NULL);
		}
		else if (irql > DISPATCH_LEVEL) {
			if (KeInsertQueueDpc(&dev_ext->rwdpc, Srb, NULL) == FALSE) {
				DBG_PRINT(("XenBlkBuildIo  SRB_STATUS_BUSY irql = %d\n", irql));
				Srb->SrbStatus = SRB_STATUS_BUSY;
				ScsiPortNotification(RequestComplete, dev_ext, Srb);
			}
		}
		else {
			XenBlkStartReadWriteDpc(NULL, dev_ext, Srb, NULL);
		}
#else
		if (XenBlkStartReadWrite(dev_ext, Srb) != STATUS_SUCCESS) {
			Srb->SrbStatus = SRB_STATUS_BUSY;
			XENBLK_INC(srbs_returned);
			xenblk_request_complete(RequestComplete, dev_ext, Srb);
			xenblk_next_request(NextRequest, dev_ext);
			return FALSE;
		}
#endif

    }
#ifndef XENBLK_STORPORT
	else {
		/* This not a read/write, so for scsiport call StartIo. */
		DPRINT(("  Call into StartIo\n"));
		if (!XenBlkStartIo(dev_ext, Srb))
			return FALSE;
	}
#endif
	DPR_TRC(("XenBlk: XenBlkBuildIo %p - Out\n", Srb));
	return TRUE;
}

static BOOLEAN
XenBlkResetBus(
	XENBLK_DEVICE_EXTENSION *dev_ext,
	IN ULONG PathId)
{
	XENBLK_SET_FLAG(dev_ext->xenblk_locks, (BLK_RBUS_L | BLK_SIO_L));
    xenblk_complete_all_requests(dev_ext,
		(UCHAR)PathId, 0xFF, 0xFF, SRB_STATUS_BUS_RESET);
	xenblk_next_request(NextRequest, dev_ext);
	DPR_INIT(("XenBlk: XenBlkResetBus - Out\n"));
	XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_RBUS_L | BLK_SIO_L));
	return TRUE;
}

static BOOLEAN
XenBlkInterrupt(XENBLK_DEVICE_EXTENSION *dev_ext)
{
	struct blkfront_info *info;
	ULONG whose_int;
	int i;
	BOOLEAN did_work = FALSE;

#ifdef DBGG
	if (dev_ext->xenblk_locks & BLK_INT_L) {
		DPR_INIT(("XBKInt already set for dev:irql %x cpu %x xlocks %x on %x\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
			dev_ext->xenblk_locks, dev_ext->cpu_locks));
	}
	if (dev_ext->DevState != WORKING) {
		DPR_INIT(("XBKInt dev state %x: irql %x, cpu %x\n",
			dev_ext->DevState, KeGetCurrentIrql(),
			KeGetCurrentProcessorNumber()));
	}
	if (KeGetCurrentProcessorNumber() > 4)
		DPR_INIT(("XBKInt KeGetCurrentProcessorNumber %x > 4\n",
			KeGetCurrentProcessorNumber()));
#endif
	XENBLK_SET_FLAG(dev_ext->xenblk_locks, (BLK_ISR_L | BLK_INT_L));
	XENBLK_SET_FLAG(dev_ext->cpu_locks, (1 << KeGetCurrentProcessorNumber()));

	DPR_TRC(("XenBlk: XBKInt - IN\n"));

	//if (dev_ext->DevState & (REMOVED | PENDINGREMOVE | RESTARTING)) {
	if (dev_ext->DevState & (INITIALIZING | PENDINGREMOVE | RESTARTING)) {
		DPR_INTERRUPT(("XBKInt %p - OUT DevState == %x\n",
			dev_ext, dev_ext->DevState));
		XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_ISR_L | BLK_INT_L));
		XENBLK_CLEAR_FLAG(dev_ext->cpu_locks,
			(1 << KeGetCurrentProcessorNumber()));
#ifdef DBGG
		if (dev_ext->xenblk_locks & BLK_INT_L) {
			DPR_INIT(("XBKInt 1: still set: irql %x cpu %x xlocks %x on %x\n",
				KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
					  dev_ext->xenblk_locks, dev_ext->cpu_locks));
		}
#endif
		DPR_TRC(("XBKInt - OUT\n"));
		return TRUE;
	}

	whose_int = xenbus_handle_evtchn_callback();
	if (whose_int) {
		did_work = TRUE;
	}

#ifdef DBGG
	if (dev_ext->info[0] && dev_ext != dev_ext->info[0]->xbdev) {
		DPR_INTERRUPT(("XBKInt dev_ext != xbdev: %p, %p, %d\n",
			dev_ext, dev_ext->info[0]->xbdev, KeGetCurrentIrql()));
		XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_ISR_L | BLK_INT_L));
		XENBLK_CLEAR_FLAG(dev_ext->cpu_locks,
			(1 << KeGetCurrentProcessorNumber()));
		if (dev_ext->xenblk_locks & BLK_INT_L) {
			DPR_INIT(("XBKInt 2: still set: irql %x cpu %x xlocks %x on %x\n",
				KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
				dev_ext->xenblk_locks, dev_ext->cpu_locks));
		}
		DPR_TRC(("XBKInt - OUT\n"));
		return TRUE;
	}
	if (dev_ext->DevState != WORKING) {
		DPR_INTERRUPT(("XBKInt: %p state %x, whose_int = %x %d, cpu= %x\n",
			dev_ext, dev_ext->DevState, whose_int, should_print_int,
			KeGetCurrentProcessorNumber()));
	}
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INTERRUPT(("XBKInt: %p st %x, whose = %x %d, cpu= %x\n",
			dev_ext, dev_ext->DevState, whose_int, should_print_int,
			KeGetCurrentProcessorNumber()));
		should_print_int++;
	}

	if (whose_int && !(whose_int & XEN_INT_DISK)) {
		if (should_print_int < SHOULD_PRINT_INT) {
			DPR_INTERRUPT(("XBKInt: cpu = %x not disks.\n",
				KeGetCurrentProcessorNumber()));
			should_print_int++;
		}
		if (dev_ext->op_mode == OP_MODE_NORMAL) {
			for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
				info = dev_ext->info[i];
				if (info) {
					if (RING_HAS_UNCONSUMED_RESPONSES(&info->ring)) {
						DBG_PRINT(("XBKInt: cpu = %x work to do on disk %x, but not disk's int %x. returning\n",
							KeGetCurrentProcessorNumber(), i, whose_int));
					}
				}
			}
			XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_ISR_L | BLK_INT_L));
			XENBLK_CLEAR_FLAG(dev_ext->cpu_locks,
				(1 << KeGetCurrentProcessorNumber()));
			DPR_TRC(("XBKInt - OUT\n"));
			return TRUE;
		}
	}
	else if (!whose_int) {
		if (dev_ext->op_mode == OP_MODE_NORMAL) {
			for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
				info = dev_ext->info[i];
				if (info) {
					if (RING_HAS_UNCONSUMED_RESPONSES(&info->ring)) {
						DBG_PRINT(("XBKInt: cpu = %x work to do on disk %x, but not xen's int %x. returning\n",
							KeGetCurrentProcessorNumber(), i, whose_int));
					}
				}
			}
			XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_ISR_L | BLK_INT_L));
			XENBLK_CLEAR_FLAG(dev_ext->cpu_locks,
				(1 << KeGetCurrentProcessorNumber()));
			DPR_TRC(("XBKInt - OUT\n"));
			return TRUE;
		}
	}
#endif

	/* In hibernation mode, we have masked the event channel and our */
	/* interrupt routine is polled by the system.  So we still need to */
	/* check if the disk has work to do eventhough we don't have an */
	/* indication. */
	for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
		info = dev_ext->info[i];
		if (info) {
#ifdef DBGG
			if (info->xenblk_locks & BLK_INT_L) {
				DPR_INIT(("XBKInt already set for disk %x: irql %x, cpu %x, xlocks %x on %x\n",
					i, KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
					info->xenblk_locks,	info->cpu_locks));
			}
#endif
			XENBLK_SET_FLAG(info->xenblk_locks, (BLK_ISR_L | BLK_INT_L));
			XENBLK_SET_FLAG(info->cpu_locks,
				(1 << KeGetCurrentProcessorNumber()));

			info->has_interrupt = 0;

			/*
			 * We wont mask event channels since we cannot take an interrupt
			 * until we finish with this one. Also if we call
			 * ScsiPortNotification(CallEnableInterrupts, dev_ext,
			 * (void (*)(void *))blkif_complete_int); is called, there is a 
			 * chance that we will take another interrupt before the 
			 * matching call with CallDisableInterrupts.  This would cause
			 * ScsiPort to hang.
			 */

			if (RING_HAS_UNCONSUMED_RESPONSES(&info->ring)) {
#ifdef DBGG
				if (!(whose_int & XEN_INT_DISK)) {
					if (dev_ext->op_mode == OP_MODE_NORMAL) {
					DBG_PRINT(("XBKInt:cpu = %x work to do, not disk int %x.\n",
						KeGetCurrentProcessorNumber(), whose_int));
					}
				}
				if (should_print_int < SHOULD_PRINT_INT) {
					DPR_INTERRUPT(("XBKInt: blkif_complete_int cpu = %x - IN\n",
						KeGetCurrentProcessorNumber()));
					should_print_int++;
				}
#endif
				if (blkif_complete_int(info)) {
					did_work = TRUE;
				}
#ifdef DBGG
				if (should_print_int < SHOULD_PRINT_INT) {
					DPR_INTERRUPT(("XBKInt: blkif_complete_int cpu = %x - O\n",
						KeGetCurrentProcessorNumber()));
					should_print_int++;
				}
#endif
			}
			XENBLK_CLEAR_FLAG(info->xenblk_locks, (BLK_ISR_L | BLK_INT_L));
			XENBLK_CLEAR_FLAG(info->cpu_locks,
				(1 << KeGetCurrentProcessorNumber()));
		}
	}
#ifdef DBGG
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INTERRUPT(("XBKInt: did_work %x, cpu = %x - OUT\n",
			did_work, KeGetCurrentProcessorNumber()));
		should_print_int++;
	}
	if (should_print_int < SHOULD_PRINT_INT) {
		xenbus_debug_dump();
		XenBlkDebugDump(dev_ext);
	}
#endif
	XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_ISR_L | BLK_INT_L));
	XENBLK_CLEAR_FLAG(dev_ext->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
#ifdef DBG
	if (dev_ext->xenblk_locks & BLK_INT_L) {
		DPR_INIT(("XBKInt 3: still set: irql %x cpu %x xlocks %x on %x\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
			dev_ext->xenblk_locks, dev_ext->cpu_locks));
	}
#endif
	DPR_TRC(("XBKInt - OUT\n"));
	return did_work;
}

static void
XenBlkRestartAdapter(
	IN PDEVICE_OBJECT DeviceObject, // place holder if scheduled as a work item
	XENBLK_DEVICE_EXTENSION *dev_ext)
{
#ifdef DBG
	should_print_int = 0;
#endif
	DPR_INIT(("Rs: srbs_seen = %x, srbs_returned = %x\n",
			   srbs_seen, srbs_returned));
	DPR_INIT(("Rs: sio_srbs_seen = %x, sio_srbs_returned = %x\n",
		sio_srbs_seen, sio_srbs_returned));
	DPR_INIT(("Rs: io_srbs_seen = %x, io_srbs_returned = %x\n",
		io_srbs_seen, io_srbs_returned));

#ifdef DBG
	if (srbs_seen != srbs_returned) {
		DBG_PRINT(("R: srbs_seen = %x, srbs_returned = %x, diff %d.\n",
				   srbs_seen, srbs_returned, srbs_seen - srbs_returned));
	}
	if (sio_srbs_seen != sio_srbs_returned) {
		DBG_PRINT(("R: sio_srbs_seen = %x, sio_srbs_returned = %x, diff %d.\n",
			sio_srbs_seen, sio_srbs_returned, sio_srbs_seen-sio_srbs_returned));
	}
	if (io_srbs_seen != io_srbs_returned) {
		DBG_PRINT(("R: io_srbs_seen = %x, io_srbs_returned = %x, diff %d.\n",
			io_srbs_seen, io_srbs_returned, io_srbs_seen - io_srbs_returned));
	}
#endif

	DPR_INIT(("XenBlkRestartAdapter IN: dev = %p, irql = %d, cpu %x, op = %x\n",
		dev_ext, KeGetCurrentIrql(),
		KeGetCurrentProcessorNumber(), dev_ext->op_mode));

	XenBlkFreeAllResources(dev_ext, RELEASE_ONLY);

	//dev_ext->DevState = RESTARTED;
	XenBlkInit(dev_ext);

#ifdef DBG
	if (srbs_seen != srbs_returned) {
		DBG_PRINT(("Re: srbs_seen = %x, srbs_returned = %x, diff %d.\n",
				   srbs_seen, srbs_returned, srbs_seen - srbs_returned));
	}
	if (sio_srbs_seen != sio_srbs_returned) {
		DBG_PRINT(("Re: sio_srbs_seen = %x, sio_srbs_returned = %x, diff %d.\n",
			sio_srbs_seen, sio_srbs_returned, sio_srbs_seen-sio_srbs_returned));
	}
	if (io_srbs_seen != io_srbs_returned) {
		DBG_PRINT(("Re: io_srbs_seen = %x, io_srbs_returned = %x, diff %d.\n",
			io_srbs_seen, io_srbs_returned, io_srbs_seen - io_srbs_returned));
	}
#endif

	DPR_INIT(("XenBlkRestartAdapter OUT: dev = %p, irql = %d, cput %x\n",
		dev_ext, KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
}

static void
XenBlkRestartDpc(PKDPC dpc, XENBLK_DEVICE_EXTENSION *dev_ext,
	void *s1, void *s2)
{
    XENBLK_LOCK_HANDLE lh;

	DPR_INIT(("XenBlk: XenBlkRestartDpc - IN irql = %d, cpu %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	xenblk_acquire_spinlock(dev_ext, &dev_ext->dev_lock, StartIoLock,
		NULL, &lh);
	XENBLK_SET_FLAG(dev_ext->xenblk_locks, (BLK_RDPC_L | BLK_SIO_L));

	XenBlkRestartAdapter(NULL, dev_ext);
	XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_RDPC_L | BLK_SIO_L));
	xenblk_release_spinlock(dev_ext, &dev_ext->dev_lock, lh);

	DPR_INIT(("XenBlk: XenBlkRestartDpc - OUT irql = %d, cpu %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
}

static SCSI_ADAPTER_CONTROL_STATUS
XenBlkAdapterControl (
	IN XENBLK_DEVICE_EXTENSION *dev_ext,
	IN SCSI_ADAPTER_CONTROL_TYPE ControlType,
	IN PVOID Parameters)
{
    XENBLK_LOCK_HANDLE lh;
	int i;
	int j;
	KIRQL irql;

	irql = KeGetCurrentIrql();
	DPR_INIT(("XenBlk: ACtrl-IN ct = %x, dev = %p, irql = %d, cpu %d, op %x, st %x\n",
		ControlType, dev_ext, irql, KeGetCurrentProcessorNumber(),
		dev_ext->op_mode, dev_ext->DevState));

	DPR_INIT(("AC: srbs_seen = %x, srbs_returned = %x\n",
			   srbs_seen, srbs_returned));
	DPR_INIT(("AC: sio_srbs_seen = %x, sio_srbs_returned = %x\n",
		sio_srbs_seen, sio_srbs_returned));
	DPR_INIT(("AC: io_srbs_seen = %x, io_srbs_returned = %x\n",
		io_srbs_seen, io_srbs_returned));

#ifdef DBG
	if (srbs_seen != srbs_returned) {
		DBG_PRINT(("AC: srbs_seen = %x, srbs_returned = %x, diff %d.\n",
				   srbs_seen, srbs_returned, srbs_seen - srbs_returned));
	}
	if (sio_srbs_seen != sio_srbs_returned) {
		DBG_PRINT(("AC: sio_srbs_seen = %x, sio_srbs_returned = %x, diff %d.\n",
			sio_srbs_seen, sio_srbs_returned, sio_srbs_seen - sio_srbs_returned));
	}
	if (io_srbs_seen != io_srbs_returned) {
		DBG_PRINT(("AC: io_srbs_seen = %x, io_srbs_returned = %x, diff %d.\n",
			io_srbs_seen, io_srbs_returned, io_srbs_seen - io_srbs_returned));
	}
#endif

    switch (ControlType) {
        case ScsiQuerySupportedControlTypes: {
            PSCSI_SUPPORTED_CONTROL_TYPE_LIST supportedList = Parameters;

            //
            // Indicate support for this type + Stop and Restart.
            // 
            supportedList->SupportedTypeList[ScsiStopAdapter] = TRUE;
            supportedList->SupportedTypeList[ScsiRestartAdapter] = TRUE;
            supportedList->SupportedTypeList[ScsiQuerySupportedControlTypes] =
				TRUE;

			xenblk_acquire_spinlock(dev_ext, &dev_ext->dev_lock, StartIoLock,
				NULL, &lh);
			XENBLK_SET_FLAG(dev_ext->xenblk_locks, (BLK_ACTR_L | BLK_SIO_L));

			if (dev_ext->DevState == REMOVING) {
				/* If we are not at passive level, we are not powering off. */
				if (irql == PASSIVE_LEVEL
					&& dev_ext->op_mode == OP_MODE_HIBERNATE) {
					DPR_INIT(("Passive hibernate: xenbus_external_cleanup.\n"));
					//xenbus_external_cleanup();
				}
				for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
					if (dev_ext->info[i]) {
						blkif_quiesce(dev_ext->info[i]);
					}
				}
				/* Needed for 2008 R2.  It unloads the drivers. */
				DPR_INIT(("AC: blkif_disconnect_backend.\n"));
				blkif_disconnect_backend(dev_ext);

				dev_ext->DevState = REMOVED;
				XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks,
					(BLK_ACTR_L | BLK_SIO_L));
				xenblk_release_spinlock(dev_ext, &dev_ext->dev_lock, lh);

				if (dev_ext->op_mode == OP_MODE_SHUTTING_DOWN) {
					xenbus_external_shutdown();
				}
				break;
			}

			if (dev_ext->DevState == REMOVED) {
				dev_ext->DevState = RESTARTING;
				XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks,
					(BLK_ACTR_L | BLK_SIO_L));
				xenblk_release_spinlock(dev_ext, &dev_ext->dev_lock, lh);
				break;
			}

			if (dev_ext->DevState == RESTARTING) {
				XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks,
					(BLK_ACTR_L | BLK_SIO_L));
				xenblk_release_spinlock(dev_ext, &dev_ext->dev_lock, lh);

				/* If OP_MODE_NORMAL, we didn't power off. */
				if (dev_ext->op_mode == OP_MODE_NORMAL) {
					DPR_INIT(("  ScsiQuery RESTARTING - just unmask.\n"));
					for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
						if (dev_ext->info[i]) {
							unmask_evtchn(dev_ext->info[i]->evtchn);
						}
					}
					dev_ext->DevState = WORKING;
				}
				break;
			}

			if (dev_ext->DevState == INITIALIZING) {
				XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks,
					(BLK_ACTR_L | BLK_SIO_L));
				xenblk_release_spinlock(dev_ext, &dev_ext->dev_lock, lh);
				XenBlkInit(dev_ext);
				break;
			}
			XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_ACTR_L | BLK_SIO_L));
			xenblk_release_spinlock(dev_ext, &dev_ext->dev_lock, lh);
            
            break;
        }

		case ScsiStopAdapter:
			XENBLK_SET_FLAG(dev_ext->xenblk_locks, (BLK_ACTR_L | BLK_INT_L));
			XENBLK_SET_FLAG(dev_ext->cpu_locks,
				(1 << KeGetCurrentProcessorNumber()));
			dev_ext->DevState = REMOVING;
			for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
				if (dev_ext->info[i]) {
					mask_evtchn(dev_ext->info[i]->evtchn);
					/* Can't quiesce at this time because the irql */
					/* may be too high. */
					//blkif_quiesce(dev_ext->info[i]);
				}
			}
			XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_ACTR_L | BLK_INT_L));
			XENBLK_CLEAR_FLAG(dev_ext->cpu_locks,
				(1 << KeGetCurrentProcessorNumber()));
			break;

		case ScsiRestartAdapter:
			if (dev_ext->op_mode == OP_MODE_NORMAL
				&& dev_ext->DevState == WORKING) {
				/* We didn't power down, so just unmask the evtchn. */
				DPR_INIT(("  ScsiRestartAdapter - just unmask.\n"));
				for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
					if (dev_ext->info[i]) {
						unmask_evtchn(dev_ext->info[i]->evtchn);
					}
				}
			}
			else {
				dev_ext->op_mode = OP_MODE_NORMAL;
				dev_ext->DevState = STOPPED;

				for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
					if (dev_ext->info[i]) {
						dev_ext->info[i]->xbdev = dev_ext;
						dev_ext->info[i]->connected = BLKIF_STATE_DISCONNECTED;
					}
				}

				if (irql <= DISPATCH_LEVEL) {
					XenBlkRestartDpc(NULL, dev_ext, NULL, NULL);
				}
				else {
					KeInsertQueueDpc(&dev_ext->restart_dpc, NULL,NULL);
				}
			}
			break;
	}
	DPR_INIT(("  XenBlkAdapterControl -  irql %d, cpu %d locks %x OUT:\n",
	   irql, KeGetCurrentProcessorNumber(), dev_ext->xenblk_locks));

#ifdef DBG
	if (srbs_seen != srbs_returned) {
		DBG_PRINT(("ACE: srbs_seen = %x, srbs_returned = %x, diff %d.\n",
				   srbs_seen, srbs_returned, srbs_seen - srbs_returned));
	}
	if (sio_srbs_seen != sio_srbs_returned) {
		DBG_PRINT(("ACE: sio_srbs_seen = %x, sio_srbs_returned = %x, diff %d.\n",
			sio_srbs_seen, sio_srbs_returned, sio_srbs_seen - sio_srbs_returned));
	}
	if (io_srbs_seen != io_srbs_returned) {
		DBG_PRINT(("ACE: io_srbs_seen = %x, io_srbs_returned = %x, diff %d.\n",
			io_srbs_seen, io_srbs_returned, io_srbs_seen - io_srbs_returned));
	}
#endif
    return ScsiAdapterControlSuccess;
}

void
XenBlkFreeResource(struct blkfront_info *info, uint32_t info_idx,
	XENBUS_RELEASE_ACTION action)
{
	xenbus_release_device_t release_data;
	uint32_t i;

	release_data.action = action;
	release_data.type = vbd;
	if (info) {
		/* We don't need to unregister watches here.  If we get here due */
		/* to a shutdown/hibernate/crashdump, the watch has already been */
		/* unregistered in disconnect_backend.  It we get here from a */
		/* resume ,we didn't need to unregister the watches. */
		info->xbdev->info[info_idx] = NULL;
		xenblk_unmap_system_addresses(info);
		xenbus_release_device(info, info->xbdev, release_data);
		if (info->ring.sring) {
			DPR_INIT(("XenBlkFreeResource: free sring: %p **.\n",
				info->ring.sring));
			ExFreePool(info->ring.sring);
			XENBLK_DEC(info->xbdev->alloc_cnt_s);
		}
		if (action == RELEASE_REMOVE) {
			xenblk_notification(BusChangeDetected, info->xbdev, 0);
		}
		DPR_INIT(("XenBlkFreeResource: resume free info: %p **.\n", info));
		XENBLK_DEC(info->xbdev->alloc_cnt_i);
		ExFreePool(info);
	}
}

void
XenBlkFreeAllResources(XENBLK_DEVICE_EXTENSION *dev_ext,
	XENBUS_RELEASE_ACTION action)
{
	uint32_t i;

	for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
		XenBlkFreeResource(dev_ext->info[i], i, action);
	}
}

static void
XenBlkResume(XENBLK_DEVICE_EXTENSION *dev_ext, uint32_t suspend_canceled)
{
	xenbus_release_device_t release_data;
	struct blkfront_info *info;
	uint32_t i;

	DPR_INIT(("XenBlk: XenBlkResume - IN irql = %d\n", KeGetCurrentIrql()));
	if (suspend_canceled) {
		/* We were only suspneded long enough to do a checkpoint. Just */
		/* mark the state as working and continue as if nothing happened. */
		dev_ext->DevState = WORKING;
	}
	else {
		//xenbus_external_cleanup();
		dev_ext->DevState = RESTARTING;
		//XenBlkFreeAllResources(dev_ext, RELEASE_ONLY);
		XenBlkRestartAdapter(NULL, dev_ext);
	}

	/* Now that we are back up, we can release the spin lock. */
	XENBLK_CLEAR_FLAG(dev_ext->xenblk_locks, (BLK_RSU_L | BLK_SIO_L));
	xenblk_release_spinlock(dev_ext, &dev_ext->dev_lock, dev_ext->lh);
#ifdef DBG
		should_print_int = 0;
#endif
}

static uint32_t
XenBlkSuspend(XENBLK_DEVICE_EXTENSION *dev_ext, uint32_t reason)
{
	uint32_t i;

	if (reason == SHUTDOWN_suspend) {
		xenblk_acquire_spinlock(dev_ext, &dev_ext->dev_lock, StartIoLock,
			NULL, &dev_ext->lh);
		XENBLK_SET_FLAG(dev_ext->xenblk_locks, (BLK_RSU_L | BLK_SIO_L));

		/* Keep requests from being put on the ring. */
		dev_ext->DevState = PENDINGREMOVE;

		for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
			if (dev_ext->info[i]) {
				/* Wait until all outstanding requests have finished. */
				DPR_INIT(("XenBlkSuspend: blkif_quiesce\n"));
				blkif_quiesce(dev_ext->info[i]);
			}
		}

		/* Let XenBlkResume release the spin lock. If the guest is going */
		/* away, then it doesn't matter.  If the guest will be resumed, then */
		/* we don't want any I/O started until we have finished the resume. */
	}
	else if (reason == SHUTDOWN_DEBUG_DUMP) {
		XenBlkDebugDump(dev_ext);
	}
	return 0;
}

static uint32_t
XenBlkIoctl(XENBLK_DEVICE_EXTENSION *dev_ext, pv_ioctl_t data)
{
	uint32_t cc = 0;

	switch (data.cmd) {
		case PV_SUSPEND:
			cc = XenBlkSuspend(dev_ext, data.arg);
			break;
		case PV_RESUME:
			XenBlkResume(dev_ext, data.arg);
			break;
		case PV_ATTACH:
			DPR_INIT(("XenBlkIoctl called to do an attach.\n"));
			if (XenBlkClaim(dev_ext) == STATUS_SUCCESS) {
				DPR_INIT(("XenBlkIoctl calling StorPortNotification.\n"));
				xenblk_notification(BusChangeDetected, dev_ext, 0);
			}
			break;
		case PV_DETACH:
			break;
		default:
			break;
	}
	return cc;
}

void
XenBlkDebugDump(XENBLK_DEVICE_EXTENSION *dev_ext)
{
	uint32_t i;

	for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
		if (dev_ext->info[i]) {
			DBG_PRINT(("*** XenBlk state dump for disk %d:\n", i));
			DBG_PRINT(("\tstate %x, connected %x, irql %d, cpu %x\n",
				dev_ext->DevState, dev_ext->info[i]->connected,
				KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
			DBG_PRINT(("\tsring: req_prod %x, rsp_prod %x, req_event %x, rsp_event %x\n",
				dev_ext->info[i]->ring.sring->req_prod,
				dev_ext->info[i]->ring.sring->rsp_prod,
				dev_ext->info[i]->ring.sring->req_event,
				dev_ext->info[i]->ring.sring->rsp_event));
			DBG_PRINT(("\tring: req_prod_pvt %x, rsp_cons %x\n",
				dev_ext->info[i]->ring.req_prod_pvt,
				dev_ext->info[i]->ring.rsp_cons));
#ifdef DBG
			DBG_PRINT(("\tlocks held: %x\n", dev_ext->info[i]->xenblk_locks));
#endif
		}
		else
			break;
	}
}

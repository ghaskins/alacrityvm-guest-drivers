/*****************************************************************************
 *
 *   File Name:      xenbus.c
 *
 *   %version:       28 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jun 17 11:25:01 2009 %
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

DRIVER_INITIALIZE DriverEntry;

DRIVER_ADD_DEVICE XenbusAddDevice;

__drv_dispatchType(IRP_MJ_PNP) 
DRIVER_DISPATCH XenbusDispatchPnp;

__drv_dispatchType(IRP_MJ_POWER) 
DRIVER_DISPATCH XenbusDispatchPower;

__drv_dispatchType(IRP_MJ_CREATE) 
DRIVER_DISPATCH XenbusDispatchCreate;

__drv_dispatchType(IRP_MJ_CLOSE) 
DRIVER_DISPATCH XenbusDispatchClose;

__drv_dispatchType(IRP_MJ_SYSTEM_CONTROL) 
DRIVER_DISPATCH XenbusDispatchSystemControl;

__drv_dispatchType(IRP_MJ_SYSTEM_CONTROL) 
DRIVER_DISPATCH XenbusDispatchDeviceControl;

static VOID	XenbusUnload(IN PDRIVER_OBJECT DriverObject);

static NTSTATUS XenbusDispatchCreateClose(PDEVICE_OBJECT DeviceObject,PIRP Irp);

PUCHAR hypercall_page = NULL;
PDEVICE_OBJECT gfdo = NULL;
PFDO_DEVICE_EXTENSION gfdx = NULL;
void *ginfo[XENBLK_MAXIMUM_TARGETS] = {0};
uint32_t use_pv_drivers = 0;

static uint32_t xenbus_determine_pv_driver_usage(void);

static void xenbus_finish_fdx_init(PDEVICE_OBJECT fdo,
	PFDO_DEVICE_EXTENSION fdx,
	PDEVICE_OBJECT pdo);

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, XenbusUnload)
#pragma alloc_text (PAGE, XenbusAddDevice)
#pragma alloc_text (PAGE, XenbusUnload)
#pragma alloc_text (PAGE, XenbusDispatchPnp)
#pragma alloc_text (PAGE, XenbusDispatchCreateClose)
#pragma alloc_text (PAGE, XenbusDispatchDeviceControl)
#endif

NTSTATUS DriverEntry (
  IN PDRIVER_OBJECT DriverObject,
  IN PUNICODE_STRING RegistryPath
  )
{
	KeInitializeSpinLock(&xenbus_print_lock);

	DBG_PRINT(("Xenbus: Version %s.\n", VER_FILEVERSION_STR));

    DriverObject->DriverUnload = NULL;
    DriverObject->DriverExtension->AddDevice = XenbusAddDevice;

    DriverObject->MajorFunction[IRP_MJ_PNP] = XenbusDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = XenbusDispatchPower;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = XenbusDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = XenbusDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] =
		XenbusDispatchSystemControl;
#ifdef XENBUS_HAS_IOCTLS
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
		XenbusDispatchDeviceControl;
#endif

	if (xenbus_determine_pv_driver_usage() == 0)
		return STATUS_UNSUCCESSFUL;

	if (InitializeHypercallPage() != STATUS_SUCCESS)
		return STATUS_UNSUCCESSFUL;

	return STATUS_SUCCESS;
}


static VOID XenbusUnload (
  IN PDRIVER_OBJECT DriverObject
  )
{
    DPR_INIT(("XenbusUnload\n"));
    PAGED_CODE();
}

static NTSTATUS XenbusAddDevice (
  IN PDRIVER_OBJECT DriverObject,
  IN PDEVICE_OBJECT pdo
  )
{
    NTSTATUS status;
    PDEVICE_OBJECT fdo;
    PFDO_DEVICE_EXTENSION fdx;
    UNICODE_STRING xenbus_dev_name;
	uint32_t shutdown;
	uint32_t notify;

    PAGED_CODE();

    DPR_INIT(("xenbusdrv.sys: Add Device\n"));

	RtlInitUnicodeString(&xenbus_dev_name, XENBUS_DEVICE_NAME_WSTR);

	status = IoCreateDevice(
		DriverObject,
		sizeof(FDO_DEVICE_EXTENSION),
		&xenbus_dev_name,
		FILE_DEVICE_BUS_EXTENDER,     /* bus driver */
		FILE_DEVICE_SECURE_OPEN,
		FALSE,                        /* exclusive */
		&fdo);
	if (!NT_SUCCESS(status)) {
		DPR_INIT(("\tIoCreateDevice returned 0x%x\n", status));
		return status;
	}

	fdx = (PFDO_DEVICE_EXTENSION) fdo->DeviceExtension;
	RtlZeroMemory (fdx, sizeof (FDO_DEVICE_EXTENSION));

    DPR_INIT(("xenbusdrv.sys: Add Device dobj = %p, pdo = %p, fdo = %p, fdx = %p, obj = %p\n",
			  DriverObject, pdo, fdo, fdx, fdo->DriverObject));

	status = IoRegisterDeviceInterface(
		pdo,
		(LPGUID) &GUID_DEVINTERFACE_XENBUS,
		NULL,
		&fdx->ifname);
	if (!NT_SUCCESS(status)) {
		DBG_PRINT(("xenbusdrv.sys: IoRegisterDeviceInterface failed (%x)",
			status));
		IoDeleteDevice(fdo);
		return status;
	}

	fdx->LowerDevice = IoAttachDeviceToDeviceStack(fdo, pdo);
	if (fdx->LowerDevice == NULL) {
		IoDeleteDevice(fdo);
		return STATUS_NO_SUCH_DEVICE;
	}

	xenbus_finish_fdx_init(fdo, fdx, pdo);

	/* Setup to do xm shutdown or reboot via the registry. */
	shutdown = 0;
	notify = XENBUS_NO_SHUTDOWN_NOTIFICATION;
	xenbus_shutdown_setup(&shutdown, &notify);

	gfdo = fdo;
	if (gfdx) {
		xenbus_copy_fdx(fdx, gfdx);
		ExFreePool(gfdx);
		gfdx = NULL;
	}

	return STATUS_SUCCESS;
}

static NTSTATUS XenbusDispatchPnp(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
    PCOMMON_DEVICE_EXTENSION fdx;

    PAGED_CODE();

    DPR_PNP(("XenbusDispatchPnp\n"));
    fdx = (PCOMMON_DEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if (fdx->pnpstate == Deleted) {
        Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NO_SUCH_DEVICE;
    }

    if (fdx->IsFdo)
        return FDO_Pnp(DeviceObject, Irp);
    else
        return PDO_Pnp(DeviceObject, Irp);
}

static NTSTATUS XenbusDispatchCreateClose(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
    NTSTATUS status;
    PIO_STACK_LOCATION stack;
    PFDO_DEVICE_EXTENSION fdx;

    PAGED_CODE();

    fdx = (PFDO_DEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if (!fdx->IsFdo) {
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto END;
    }

    if (fdx->pnpstate == Deleted) {
        status = STATUS_NO_SUCH_DEVICE;
        goto END;
    }
    else {
        stack = IoGetCurrentIrpStackLocation(Irp);

        switch (stack->MajorFunction) {
        case IRP_MJ_CREATE:
        case IRP_MJ_CLOSE:
            status = STATUS_SUCCESS;
            break;

        default:
            break;
        }
    }

    Irp->IoStatus.Information = 0;

 END:
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

static NTSTATUS XenbusDispatchCreate(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
	return XenbusDispatchCreateClose(DeviceObject, Irp);
}

static NTSTATUS XenbusDispatchClose(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
	return XenbusDispatchCreateClose(DeviceObject, Irp);
}

static NTSTATUS
XenbusDispatchSystemControl(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
	NTSTATUS status;
    PFDO_DEVICE_EXTENSION fdx;

    DPR_INIT(("XenbusDispatchSystemControl\n"));
    fdx = (PFDO_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
    if (!fdx->IsFdo) {
        //
        // The PDO, just complete the request with the current status
        //
        status = Irp->IoStatus.Status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(fdx->LowerDevice, Irp);
}

#ifdef XENBUS_HAS_IOCTLS
static NTSTATUS XenbusDispatchDeviceControl(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )
{
    NTSTATUS status;
    PIO_STACK_LOCATION stack;
    PFDO_DEVICE_EXTENSION fdx;
    xenbus_register_shutdown_event_t *ioctl;

    PAGED_CODE();

    DPR_INIT(("XenbusDispatchDeviceControl\n"));
    fdx = (PFDO_DEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if (fdx->IsFdo) {
		if (fdx->pnpstate != Deleted) {
			status = xenbus_ioctl(fdx, Irp);
		}
		else {
			status = STATUS_NO_SUCH_DEVICE;
		}
	}
	else {
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

	if (status != STATUS_PENDING) {
		DPR_INIT(("   completing irp %x\n", status));
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );
	}

	DPR_INIT(("<== XenbusDispatchDeviceControl %x\n", status));
	return status;
}
#endif

static uint32_t
xenbus_determine_pv_driver_usage(void)
{
    RTL_QUERY_REGISTRY_TABLE paramTable[2] = {0};
    WCHAR wbuffer[SYSTEM_START_OPTIONS_LEN] = {0};
    UNICODE_STRING str;
	NTSTATUS status;

	/* Read the registry to see if we are to actually us the PV drivers. */
    DPR_EARLY(("xenbus_determine_pv_driver_usage\n"));
    str.Length = 0;
    str.MaximumLength = sizeof(wbuffer);
    str.Buffer = wbuffer;

    paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[0].Name = SYSTEM_START_OPTIONS_WSTR;
    paramTable[0].EntryContext = &str;
    paramTable[0].DefaultType = REG_SZ;
    paramTable[0].DefaultData = L"";
    paramTable[0].DefaultLength = 0;

	use_pv_drivers = 0;
	DPRINT(("RtlQueryRegistryValues. "));
    if ((status = RtlQueryRegistryValues(
			RTL_REGISTRY_CONTROL | RTL_REGISTRY_OPTIONAL,
			NULL,
			&paramTable[0],
			NULL,
			NULL)) == STATUS_SUCCESS) {
		DPR_EARLY(("SystemStartOptions = %ws.\n", wbuffer));
		if (wcsstr(wbuffer, SAFE_BOOT_WSTR)) {
			DPR_EARLY(("In safe mode, don't load xenbus.\n"));
			return use_pv_drivers;
		}
	}
	else {
		DBG_PRINT(("Xenbus: Failed to read registry startup values: 0x%x.\n",
			status));
	}

	/* We are not in safe mode, check the registry for use_pv_driers. */
	DPRINT(("Xenbus: Setting up to read from services.\n"));
	paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
	paramTable[0].Name = USE_PV_DRIVERS_WSTR;
	paramTable[0].EntryContext = &use_pv_drivers;
	paramTable[0].DefaultType = REG_DWORD;
	paramTable[0].DefaultData = &use_pv_drivers;
	paramTable[0].DefaultLength = sizeof(uint32_t);
	if ((status = RtlQueryRegistryValues(
			RTL_REGISTRY_SERVICES | RTL_REGISTRY_OPTIONAL,
			XENBUS_DEVICE_KEY_WSTR,
			&paramTable[0],
			NULL,
			NULL)) == STATUS_SUCCESS) {
		DPR_EARLY(("Xenbus: registry parameter use_pv_drivers = %d.\n",
			use_pv_drivers));
	}
	else {
		DBG_PRINT(("Xenbus: Failed to read registry services parameter 0x%x.\n",
			status));
	}

	return use_pv_drivers;
}

void
xenbus_copy_fdx(PFDO_DEVICE_EXTENSION dfdx, PFDO_DEVICE_EXTENSION sfdx)
{
	DPR_INIT(("xenbus_copy_fdx: IN.\n"));
	while (!IsListEmpty(&sfdx->ListOfPDOs)) {
		InsertTailList(&dfdx->ListOfPDOs, RemoveHeadList(&sfdx->ListOfPDOs));
	}
	DPR_INIT(("xenbus_copy_fdx: done with lists.\n"));
	dfdx->NumPDOs = sfdx->NumPDOs;
	dfdx->mmio = sfdx->mmio;
	dfdx->mem = sfdx->mem;
	dfdx->mmiolen = sfdx->mmiolen;
	dfdx->device_irq = sfdx->device_irq;
	dfdx->gnttab_list = sfdx->gnttab_list;
	dfdx->gnttab_free_head = sfdx->gnttab_free_head;
	dfdx->gnttab_free_count = sfdx->gnttab_free_count;
	dfdx->info = sfdx->info;
#ifdef XENBUS_HAS_DEVICE
	dfdx->PortBase = sfdx->PortBase;
	dfdx->NumPorts = sfdx->NumPorts;
	dfdx->MappedPort = sfdx->MappedPort;
#endif
	DPR_INIT(("xenbus_copy_fdx: OUT.\n"));
}

static void xenbus_finish_fdx_init(PDEVICE_OBJECT fdo,
	PFDO_DEVICE_EXTENSION fdx,
	PDEVICE_OBJECT pdo)
{
	fdx->Pdo = pdo;
	fdx->Self = fdo;
	fdx->IsFdo = TRUE;
	fdx->sig = 0xaabbccdd;

	//KeInitializeDpc(&fdx->dpc, xenbus_invalidate_relations, pdo);
	IoInitializeRemoveLock(&fdx->RemoveLock, 0, 0, 0);
	fdx->pnpstate = NotStarted;
	fdx->devpower = PowerDeviceD0;
	fdx->syspower = PowerSystemWorking;

	ExInitializeFastMutex(&fdx->Mutex);
	KeInitializeSpinLock(&fdx->qlock);
	InitializeListHead(&fdx->ListOfPDOs);
	InitializeListHead(&fdx->shutdown_requests);
	fdx->gnttab_list = g_gnttab_list;
	fdx->gnttab_free_count = &g_gnttab_free_count;
	fdx->gnttab_free_head = &g_gnttab_free_head;
	fdx->info = ginfo;
	fdx->disk_controller = NULL;
	fdx->disk_ioctl = NULL;

	//IoInitializeDpcRequest(fdo, XenbusDpcForIsr);

	fdo->Flags |=  DO_POWER_PAGABLE;

	fdo->Flags &= ~DO_DEVICE_INITIALIZING;
}

NTSTATUS xenbus_open_device_key(HANDLE *registryKey)
{
	OBJECT_ATTRIBUTES objectAttributes;
	UNICODE_STRING keyName;
	UNICODE_STRING valueName;

	RtlInitUnicodeString(&keyName, XENBUS_FULL_DEVICE_KEY_WSTR);   

	InitializeObjectAttributes(&objectAttributes,
		&keyName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	return ZwOpenKey(registryKey, KEY_ALL_ACCESS, &objectAttributes);
}

void xenbus_shutdown_setup(uint32_t *shutdown, uint32_t *notify)
{
	HANDLE registryKey;
	UNICODE_STRING keyName;
	UNICODE_STRING valueName;
	NTSTATUS status;

    DPR_INIT(("xenbus_shutdown_setup- IN\n"));

	if (!shutdown && !notify) {
		return;
	}

	status = xenbus_open_device_key(&registryKey);

	if (NT_SUCCESS(status)) {
		if (shutdown) {
			RtlInitUnicodeString(&valueName, XENBUS_SHUTDOWN_WSTR);

			status = ZwSetValueKey(registryKey,
				&valueName,
				0,
				REG_DWORD,
				shutdown,
				sizeof(uint32_t));
			if (!NT_SUCCESS(status)) {
				/* If we failed to write the string, no need to load. */
				/* Others will not see the value. */
				DBG_PRINT(("xenbus: failed to set shutdown value.\n"));
			}
		}

		if (notify) {
			RtlInitUnicodeString(&valueName, XENBUS_SHUTDOWN_NOTIFICATION_WSTR);

			status = ZwSetValueKey(registryKey,
				&valueName,
				0,
				REG_DWORD,
				notify,
				sizeof(uint32_t));
			if (!NT_SUCCESS(status)) {
				/* If we failed to write the string, no need to load. */
				/* Others will not see the value. */
				DBG_PRINT(("xenbus: failed to set shutdown_notification.\n"));
			}
		}

		ZwClose(registryKey);
	}
	else
		DBG_PRINT(("xenbus: failed to open xenbus key.\n"));
    DPR_INIT(("xenbus_shutdown_setup- OUT\n"));
}

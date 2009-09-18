/*****************************************************************************
 *
 *   File Name:      xenbus_porbe.c
 *
 *   %version:       27 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jun 18 16:29:40 2009 %
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

static NTSTATUS xenbus_probe_node(
  PDEVICE_OBJECT fdo,
  const char *type,
  const char *nodename
  );

static NTSTATUS xenbus_probe_device(
  PDEVICE_OBJECT fdo,
  const char *type,
  const char *name
  );

static NTSTATUS xenbus_probe_type(
  PDEVICE_OBJECT fdo,
  const char *type
  );

static char *xenbus_nodename_to_hwid(const char *nodename);
static NTSTATUS xenbus_probe_bus(PDEVICE_OBJECT fdo);

#define STRLEN_MAX  512

NTSTATUS xenbus_probe_init(PDEVICE_OBJECT fdo, uint32_t reason)
{
    PFDO_DEVICE_EXTENSION fdx;
	uint32_t num_pdos;
    NTSTATUS status;

	if (reason == OP_MODE_CRASHDUMP) {
		return STATUS_SUCCESS;
	}

    fdx = (PFDO_DEVICE_EXTENSION) fdo->DeviceExtension;
	num_pdos = fdx->NumPDOs;
    status = xenbus_probe_bus(fdo);
	DPR_INIT(("xenbus_probe_init: irql %d cpu %x n_pdos %d NPDOs %d\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
		num_pdos, fdx->NumPDOs));
	return status;
}

/* the following functions is called when the bus first enumerates */
static NTSTATUS xenbus_probe_node(
  PDEVICE_OBJECT fdo,
  const char *type,
  const char *nodename
  )
{
    int err;
    size_t stringlen, res;
    char *tmpstring, *ptr;
    NTSTATUS status = STATUS_SUCCESS;

    DPR_INIT(("XENBUS: xenbus_probe_node().\n"));
    /* tmpstring is a multi sz */
    RtlStringCbLengthA(nodename, STRLEN_MAX, &stringlen);
    RtlStringCbLengthA(type, STRLEN_MAX, &res);
    stringlen += res + 3;

    tmpstring = ExAllocatePoolWithTag(
      NonPagedPool, stringlen, XENBUS_POOL_TAG);

    if (tmpstring == NULL) {
        DBG_PRINT(("XENBUS: fail to allocate memory when probe device.\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlStringCbCopyExA(
      tmpstring,
      stringlen,
      nodename,
      &ptr,
      NULL,
      STRSAFE_IGNORE_NULLS);

    RtlStringCbCopyExA(
      ptr + 1,
      res + 2,
      type,
      NULL,
      NULL,
      STRSAFE_FILL_BEHIND_NULL);

    status = XenbusInitializePDO(fdo, type, tmpstring);

	DPR_INIT(("XENBUS: xenbus_probe_node returning %x\n", status));
    return status;
}

static NTSTATUS xenbus_probe_device(
  PDEVICE_OBJECT fdo,
  const char *type,
  const char *name
  )
{
    char *nodename;
    NTSTATUS status;
    size_t i, j;

    DPR_INIT(("XENBUS: xenbus_probe_device().\n"));
    i = strlen(type);
    j = strlen(name);
    nodename = kasprintf(i + j + 2 + 6, "%s/%s/%s", "device", type, name);

    if (nodename == NULL) {
		DPR_INIT(("XENBUS: xenbus_probe_device failed kasprintf.\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
	}

    status = xenbus_probe_node(fdo, type, nodename);

    ExFreePool(nodename);

    return status;
}

static NTSTATUS xenbus_probe_type(
  PDEVICE_OBJECT fdo,
  const char *type
  )
{
    NTSTATUS status;
    char **dir;
    unsigned int i, dir_n = 0;

    DPR_INIT(("XENBUS: xenbus_probe_type().\n"));
    dir = xenbus_directory(XBT_NIL, "device", type, &dir_n);
    if (IS_ERR((PVOID)dir)) {
        DBG_PRINT(("XENBUS: xs get directory device/%s/ fail.\n", type));
        return STATUS_UNSUCCESSFUL;
    }

    for (i=0; i<dir_n; i++) {
        status = xenbus_probe_device(fdo, type, dir[i]);
        if (!NT_SUCCESS(status))
            return status;
    }

    ExFreePool(dir);

    return STATUS_SUCCESS;
}

/* called at device start */
static NTSTATUS xenbus_probe_bus(PDEVICE_OBJECT fdo)
{
    NTSTATUS status;
    char **dir;
    unsigned int i, dir_n;

    DPR_INIT(("XENBUS: xenbus_probe_bus().\n"));
    dir = xenbus_directory(XBT_NIL, "device", "", &dir_n);
    DPR_INIT(("XENBUS: device/* check finished.\n"));
    if (IS_ERR(dir)) {
        DBG_PRINT(("XENBUS: xs get directory device/ fail.\n"));
        return STATUS_UNSUCCESSFUL;
    }

    for (i=0; i<dir_n; i++) {
		if (strcmp(dir[i], "vif") == 0 &&
				!(use_pv_drivers & XENBUS_PROBE_PV_NET))
			continue;
		else if (strcmp(dir[i], "vbd") == 0 &&
				!(use_pv_drivers & XENBUS_PROBE_PV_DISK))
			continue;
        DPR_INIT(("XENBUS: beginning to probe type %s.\n", dir[i]));
        status = xenbus_probe_type(fdo, dir[i]);
        if (!NT_SUCCESS(status))
            return status;
    }

    ExFreePool(dir);

	DPR_INIT(("XENBUS: xenbus_probe_bus returning SUCCESS.\n"));
    return STATUS_SUCCESS;
}

static char *xenbus_nodename_to_hwid(const char *nodename)
{
    char *tmp, *ptr;
    size_t len, typelen;
    char *hardwareid;

    /* device/type/id -> to type/id */
    tmp = strchr(nodename, '/');
    if (tmp == NULL)
        return NULL;

    tmp += 1;
    len = strlen(tmp);

    ptr = strchr(tmp, '/');
    if (ptr == NULL)
        return NULL;

    typelen = ptr - tmp;
    ptr += 1;

    /* <type>/<id> --> XEN\TYPE_<type>, typelen+10 */
    typelen += 10;
    hardwareid = ExAllocatePoolWithTag(
      NonPagedPool,
      typelen,
      XENBUS_POOL_TAG);

    RtlStringCbCopyA(hardwareid, typelen, "XEN\\TYPE_");
    RtlStringCbCatA(hardwareid, typelen, tmp);
    DPR_INIT(("XENBUS: generate hwID, %s, %d.\n", hardwareid, len));

    return hardwareid;
}

PPDO_DEVICE_EXTENSION
xenbus_find_pdx_from_nodename(PFDO_DEVICE_EXTENSION fdx, char *nodename)
{
    PPDO_DEVICE_EXTENSION pdx;
    PLIST_ENTRY entry;

	for (entry = fdx->ListOfPDOs.Flink;
			entry != &fdx->ListOfPDOs;
			entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (strcmp(pdx->Nodename, nodename) == 0)
			return pdx;
	}
	return NULL;
}

static int
xenbus_check_disk_override(char *otherend)
{
    char *res;
	char tmp_char;

	if ((use_pv_drivers & XENBUS_PROBE_PV_DISK)
		&& (use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_DISK_MASK)) {
		res = xenbus_read(XBT_NIL, otherend, "type", NULL);
		if (res) {
			DPR_INIT(("xenbus_check_disk_override: type = %s\n", res));

			if ((use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_FILE_DISK)) {
				if (strcmp(res, "file") == 0) {
					use_pv_drivers &= ~XENBUS_PROBE_PV_DISK;
					DPR_INIT(("\tNo file: use_pv_drivers = %x\n", use_pv_drivers));
					xenbus_free_string(res);
					return 1;
				}
			}

			if ((use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_ALL_PHY_DISK)) {
				if (strcmp(res, "phy") == 0) {
					use_pv_drivers &= ~XENBUS_PROBE_PV_DISK;
					DPR_INIT(("\tNo phy: use_pv_drivers = %x\n", use_pv_drivers));
					xenbus_free_string(res);
					return 1;
				}
			}

			xenbus_free_string(res);
			res = xenbus_read(XBT_NIL, otherend, "params", NULL);
			if (res) {
				DPR_INIT(("xenbus_check_disk_override: params = %s\n", res));
				if (strlen(res) > 17) {
					if ((use_pv_drivers
						& XENBUS_PROBE_PV_OVERRIDE_ALL_BY_DISK)) {
						tmp_char = res[13];
						res[13] = '\0';
						if (strcmp(res, "/dev/disk/by-") == 0) {
							use_pv_drivers &= ~XENBUS_PROBE_PV_DISK;
							DPR_INIT(("\tNo by all: use_pv_drivers = %x\n",
								use_pv_drivers));
							xenbus_free_string(res);
							return 1;
						}
						res[13] = tmp_char;
					}

					if ((use_pv_drivers
						& XENBUS_PROBE_PV_OVERRIDE_BY_ID_DISK)) {
						tmp_char = res[15];
						res[15] = '\0';
						if (strcmp(res, "/dev/disk/by-id") == 0) {
							use_pv_drivers &= ~XENBUS_PROBE_PV_DISK;
							DPR_INIT(("\tNo by-id: use_pv_drivers = %x\n",
								use_pv_drivers));
							xenbus_free_string(res);
							return 1;
						}
						res[15] = tmp_char;
					}

					if ((use_pv_drivers
						& XENBUS_PROBE_PV_OVERRIDE_BY_NAME_DISK)) {
						tmp_char = res[17];
						res[17] = '\0';
						if (strcmp(res, "/dev/disk/by-name") == 0) {
							use_pv_drivers &= ~XENBUS_PROBE_PV_DISK;
							DPR_INIT(("\tNo by-name: use_pv_drivers = %x\n",
								use_pv_drivers));
							xenbus_free_string(res);
							return 1;
						}
						res[17] = tmp_char;
					}

					if ((use_pv_drivers
						& XENBUS_PROBE_PV_OVERRIDE_BY_PATH_DISK)) {
						tmp_char = res[17];
						res[17] = '\0';
						if (strcmp(res, "/dev/disk/by-path") == 0) {
							use_pv_drivers &= ~XENBUS_PROBE_PV_DISK;
							DPR_INIT(("\tNo by-path: use_pv_drivers = %x\n",
								use_pv_drivers));
							xenbus_free_string(res);
							return 1;
						}
						res[17] = tmp_char;
					}

					if ((use_pv_drivers
						& XENBUS_PROBE_PV_OVERRIDE_BY_UUID_DISK)) {
						tmp_char = res[17];
						res[17] = '\0';
						if (strcmp(res, "/dev/disk/by-uuid") == 0) {
							use_pv_drivers &= ~XENBUS_PROBE_PV_DISK;
							DPR_INIT(("\tNo by-uuid: use_pv_drivers = %x\n",
								use_pv_drivers));
							xenbus_free_string(res);
							return 1;
						}
						res[17] = tmp_char;
					}
				}
				xenbus_free_string(res);
			}
		}
	}
	return 0;
}

static XENBUS_DEVICE_ORIGIN
xenbus_determine_creation_type(PFDO_DEVICE_EXTENSION fdx,
	XENBUS_DEVICE_TYPE xtype,
	XENBUS_DEVICE_SUBTYPE subtype)
{
    PPDO_DEVICE_EXTENSION pdx;
    PLIST_ENTRY entry;

	if (xtype == vnif)
		return created;
	if (xtype != vbd)
		return unknown;
#ifdef XENBUS_HAS_DEVICE
	for (entry = fdx->ListOfPDOs.Flink;
			entry != &fdx->ListOfPDOs;
			entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (pdx->Type == vbd
				&& pdx->subtype == disk
				&& pdx->origin == created) {
			/* We only want one created, the rest need to be allocated. */
			return alloced;
		}
	}
	return created;
#else
	/* Always allocate when xenblk has the device. */
	return alloced;
#endif
}

NTSTATUS
XenbusInitializePDO(PDEVICE_OBJECT fdo, const char *type, char *multiID)
{
    PFDO_DEVICE_EXTENSION fdx;
    PDEVICE_OBJECT pdo;
    PPDO_DEVICE_EXTENSION pdx;
    NTSTATUS status;
    ANSI_STRING astr;
    char *res;
    char *otherend;
    ULONG len;
    XENBUS_DEVICE_TYPE xtype;
    XENBUS_DEVICE_SUBTYPE subtype;
    XENBUS_DEVICE_ORIGIN origin;
	DEVICE_TYPE dev_type;

    DPR_INIT(("XenbusInitializePDO: new PDO, id: %s, type = %s.\n",
		multiID, type));

    fdx = (PFDO_DEVICE_EXTENSION) fdo->DeviceExtension;

    if (strcmp(type, "vif") == 0) {
        xtype = vnif;
		subtype = none;
		dev_type = FILE_DEVICE_NETWORK;
	}
    else if (strcmp(type, "vbd") == 0) {
		res = xenbus_read(XBT_NIL, multiID, "device-type", NULL);
		if (res) {
			DPR_INIT(("XenbusInitializePDO: xenbus_read device-type %s\n",res));
			if (strcmp(res, "disk") != 0) {
				/* We only control disks. */
				xenbus_free_string(res);
				return STATUS_SUCCESS;
			}
			xenbus_free_string(res);
			xtype = vbd;
			subtype = disk;
			dev_type = FILE_DEVICE_DISK;
		}
		else {
			DPR_INIT(("XenbusInitializePDO: xenbus_read device-type failed\n"));
			return STATUS_SUCCESS;
		}
	}
    else
		return STATUS_SUCCESS; /* we don't wan to create an unknown device. */

	DPR_INIT(("XenbusInitializePDO: xenbus_read backend\n"));
	otherend = xenbus_read(XBT_NIL, multiID, "backend", NULL);
	if (otherend == NULL) {
		DBG_PRINT(("XenbusInitializePDO: error-failed to read backend.\n"));
		return STATUS_SUCCESS;
	}

	if (xtype == vbd && subtype == disk) {
		if (use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK) {
			res = xenbus_read(XBT_NIL, otherend, "dev", NULL);
			if (res) {
				DPR_INIT(("XenbusInitializePDO: dev = %s\n", res));
				if (res[0] == 'h') {
					DPR_INIT(("\tDon't control IOEMU diskx\n"));
					xenbus_free_string(res);
					xenbus_free_string(otherend);
					return STATUS_SUCCESS;
				}
				xenbus_free_string(res);
			}
		}
		else {
			if (xenbus_check_disk_override(otherend)) {
				DPR_INIT(("XenbusInitializePDO Don't control %s\n", otherend));
				xenbus_free_string(otherend);
				return STATUS_SUCCESS;
			}
		}
	}

	/* If we have already created the pdo, don't do it again. */
	if ((pdx = xenbus_find_pdx_from_nodename(fdx, multiID)) == NULL) {
		origin = xenbus_determine_creation_type(fdx, xtype, subtype);
		if (origin == alloced) {
			DPR_INIT(("XenbusInitializePDO: allocating PDO, id: %s.\n",
				multiID));
			if (pdo = ExAllocatePoolWithTag(NonPagedPool,
					sizeof(DEVICE_OBJECT) + sizeof(PDO_DEVICE_EXTENSION),
					XENBUS_POOL_TAG)) {
				pdo->DeviceExtension = pdo + 1;
				status = STATUS_SUCCESS;
			}
			else
				status = STATUS_NO_MEMORY;
		}
		else if (origin == created) {
			DPR_INIT(("XenbusInitializePDO: creating PDO, id: %s.\n",
				multiID));
			status = IoCreateDeviceSecure(
				fdo->DriverObject,
				sizeof(PDO_DEVICE_EXTENSION),
				NULL,
				dev_type,
				FILE_AUTOGENERATED_DEVICE_NAME | FILE_DEVICE_SECURE_OPEN,
				FALSE,
				&SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
				(LPCGUID)&GUID_SD_XENBUS_PDO,
				&pdo);
		}
		else {
			xenbus_free_string(otherend);
			return STATUS_SUCCESS;
		}

		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("XENBUS: create pdo device fail for %s.\n", multiID));
			xenbus_free_string(otherend);
			return status;
		}
		pdx = (PPDO_DEVICE_EXTENSION) pdo->DeviceExtension;
		DPR_INIT(("XenbusInitializePDO: pdo = %p, pdx = %p, obj = %p\n",
				  pdo, pdx, pdo->DriverObject));
		pdx->Nodename = multiID;

		DPR_INIT(("XenbusInitializePDO: xenbus_nodename_to_hwid\n"));
		res = xenbus_nodename_to_hwid(multiID);
		if (res == NULL) {
			DPR_INIT(("XenbusInitializePDO: failed xenbus_nodename_to_hwid\n"));
			xenbus_free_string(otherend);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		DPR_INIT(("XenbusInitializePDO: RtlInitAnsiString\n"));
		RtlInitAnsiString(&astr, res);
		DPR_INIT(("XenbusInitializePDO: RRtlAnsiStringToUnicodeStrin\n"));
		status = RtlAnsiStringToUnicodeString(&pdx->HardwareIDs, &astr, TRUE);
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("XENBUS: not enough memory for hardware ID.\n"));
			if (origin == alloced)
				ExFreePool(pdo);
			else
				IoDeleteDevice(pdo);
			ExFreePool(res);
			xenbus_free_string(otherend);
			return status;
		}
		ExFreePool(res);

		ExAcquireFastMutex(&fdx->Mutex);
		InsertTailList(&fdx->ListOfPDOs, &pdx->Link);
		fdx->NumPDOs++;
		ExReleaseFastMutex(&fdx->Mutex);

		pdx->IsFdo = FALSE;
		pdx->Self = pdo;
		pdx->ParentFdo = fdo;

		pdx->Type = xtype;
		pdx->subtype = subtype;
		pdx->origin = origin;
		pdx->Present = TRUE;
		pdx->ReportedMissing = FALSE;

		pdx->pnpstate = NotStarted;
		pdx->devpower = PowerDeviceD3;
		pdx->syspower = PowerSystemWorking;

		pdo->Flags |= DO_POWER_PAGABLE;
		pdx->InterfaceRefCount = 0;
		pdx->PagingPathCount = 0;
		pdx->DumpPathCount = 0;
		pdx->HibernationPathCount = 0;
		pdx->frontend_dev = NULL;
		pdx->controller = NULL;
		DPR_INIT(("XenbusInitializePDO: KeInitializeEvent\n"));
		KeInitializeEvent(&pdx->PathCountEvent, SynchronizationEvent, TRUE);

		pdo->Flags &= ~DO_DEVICE_INITIALIZING;

	}

	/* The backend-id always needs to be updated incase a hibernate happened. */
	DPR_INIT(("XenbusInitializePDO: xenbus_read backend-id\n"));
	res = xenbus_read(XBT_NIL, multiID, "backend-id", &len);
	if (res)
		pdx->BackendID = res;
	else
		pdx->BackendID = NULL;

	pdx->Otherend = otherend;

	DPR_INIT(("XenbusInitializePDO: out\n"));
    return STATUS_SUCCESS;
}

NTSTATUS
XenbusDestroyPDO(PDEVICE_OBJECT pdo)
{
    PPDO_DEVICE_EXTENSION pdx;

    pdx = (PPDO_DEVICE_EXTENSION) pdo->DeviceExtension;

	if (pdx->HardwareIDs.Buffer) {
		RtlFreeUnicodeString(&pdx->HardwareIDs);
		pdx->HardwareIDs.Buffer = NULL;
	}

	if (pdx->BackendID)
		ExFreePool(pdx->BackendID);

	if (pdx->Otherend)
		ExFreePool(pdx->Otherend);

	if (pdx->Nodename) {
		DPR_INIT(("XenbusDestroyPDO: %s\n", pdx->Nodename));
		ExFreePool(pdx->Nodename);
	}

	if (pdx->origin == created) {
		DPR_INIT(("XenbusDestroyPDO: IoDeleteDevice(pdo)\n"));
		IoDeleteDevice(pdo);
	}
    return STATUS_SUCCESS;
}

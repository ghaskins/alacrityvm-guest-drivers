/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  pdo.c:  Routines for managing and creating PDOs.
 * 
 *
 * Author:
 *     Peter W. Morreale <pmorreale@novell.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "vbus.h"
#include "precomp.h"


NTSTATUS
VbusIdDup( WDFCHILDLIST DeviceList,
	PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER sid,
	PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER did)
{
	PPDO_ID_DESC 	src;
	PPDO_ID_DESC 	dst;
	NTSTATUS 	rc;

	src = CONTAINING_RECORD(sid, PDO_ID_DESC, header); 
	dst = CONTAINING_RECORD(did, PDO_ID_DESC, header);

	dst->id = src->id;
	memcpy(dst->type, src->type, VBUS_MAX_DEVTYPE_LEN);

	return STATUS_SUCCESS;
}

BOOLEAN
VbusIdCmp(WDFCHILDLIST DeviceList,
	PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER l,
	PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER r)
{
	PPDO_ID_DESC 	lhs;
	PPDO_ID_DESC 	rhs;
	BOOLEAN		rc;

	lhs = CONTAINING_RECORD(l, PDO_ID_DESC, header);
	rhs = CONTAINING_RECORD(r, PDO_ID_DESC, header);

	rc = FALSE;
	if (lhs->id == rhs->id && 
		!memcmp(lhs->type, rhs->type, VBUS_MAX_DEVTYPE_LEN))
		rc = TRUE;

	return rc;
}

NTSTATUS
VbusCreatePdo(WDFDEVICE dev, PWDFDEVICE_INIT init, PPDO_ID_DESC d)
{
	NTSTATUS			rc;
	PPDO_DEVICE_DATA		pd;
	WDFDEVICE			hChild;
	WDF_QUERY_INTERFACE_CONFIG	conf;
	WDF_OBJECT_ATTRIBUTES		attr;
	WDF_DEVICE_PNP_CAPABILITIES	pnpCaps;
	WDF_DEVICE_POWER_CAPABILITIES	powCaps;
	VBUS_IF  			vif;
	ANSI_STRING			atype;
	DECLARE_CONST_UNICODE_STRING(deviceLocation, L"VBUS\\");
	DECLARE_UNICODE_STRING_SIZE(type, VBUS_MAX_DEVTYPE_LEN*2);
	DECLARE_UNICODE_STRING_SIZE(hwid, VBUS_MAX_DEVTYPE_LEN*2);

	/*
	 * Set DeviceType
	 */
	WdfDeviceInitSetDeviceType(init, FILE_DEVICE_BUS_EXTENDER);

	RtlInitAnsiString(&atype, d->type);
	RtlAnsiStringToUnicodeString(&type, &atype, FALSE);
	RtlUnicodeStringCat(&hwid, &deviceLocation);
	RtlUnicodeStringCat(&hwid, &type);

	/*
	 * Provide DeviceID, HardwareIDs and InstanceId
	 */
	rc = WdfPdoInitAssignDeviceID(init, &hwid);
	if (!NT_SUCCESS(rc)) 
		return rc;

	rc = WdfPdoInitAddHardwareID(init, &hwid);
	if (!NT_SUCCESS(rc)) 
		return rc;

	rc = WdfPdoInitAddCompatibleID(init, &hwid);
	if (!NT_SUCCESS(rc)) 
		return rc;

	RtlUnicodeStringPrintf(&type, L"%d", d->id);
	rc = WdfPdoInitAssignInstanceID(init, &type);
	if (!NT_SUCCESS(rc)) 
		return rc;
	/* 
	 * Description of the device 
	 * XXX only valid for virtual ethernet 
	 */
	RtlUnicodeStringPrintf(&type, L"Vbus virtual ethernet id = %d", d->id);

	rc = WdfPdoInitAddDeviceText(init, &type, &deviceLocation, 0x409 );
	if (!NT_SUCCESS(rc)) {
		return rc;
	}
	WdfPdoInitSetDefaultLocale(init, 0x409);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, PDO_DEVICE_DATA);
	rc = WdfDeviceCreate(&init, &attr, &hChild);
	if (!NT_SUCCESS(rc)) 
		return rc;

	pd = PdoGetData(hChild);
	pd->id = d->id;
	memcpy(pd->type, d->type, VBUS_MAX_DEVTYPE_LEN);

	/* PnP capabilities */
	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
	pnpCaps.Removable		= WdfTrue;
	pnpCaps.EjectSupported		= WdfTrue;
	pnpCaps.SurpriseRemovalOK 	= WdfTrue;
	pnpCaps.Address  		= (ULONG) d->id;
	pnpCaps.UINumber 		= (ULONG) d->id;
	WdfDeviceSetPnpCapabilities(hChild, &pnpCaps);

	/* Power capabilities */
	WDF_DEVICE_POWER_CAPABILITIES_INIT(&powCaps);
	powCaps.DeviceD1 = WdfTrue;
	powCaps.WakeFromD1 = WdfTrue;
	powCaps.DeviceWake = PowerDeviceD1;
	powCaps.DeviceState[PowerSystemWorking]   = PowerDeviceD1;
	powCaps.DeviceState[PowerSystemSleeping1] = PowerDeviceD1;
	powCaps.DeviceState[PowerSystemSleeping2] = PowerDeviceD2;
	powCaps.DeviceState[PowerSystemSleeping3] = PowerDeviceD2;
	powCaps.DeviceState[PowerSystemHibernate] = PowerDeviceD3;
	powCaps.DeviceState[PowerSystemShutdown]  = PowerDeviceD3;
	WdfDeviceSetPowerCapabilities(hChild, &powCaps);

	/*
	 * Create a custom interface so that other drivers can
	 * query (IRP_MN_QUERY_INTERFACE) and use our callbacks directly.
	 */
	RtlZeroMemory(&vif, sizeof(vif));
	vif.inf.Size 	= VBUS_IF_SIZE;
	vif.inf.Version = VBUS_IF_VERSION;
	vif.inf.Context = (PVOID) hChild;

	/*
	 * Set the child device(s) interfaces.  Children (eg: venet) 
	 * will query and use these if's for reading and writing data.
	 * 
	 * Let framework handle reference counts too.
	 */
	vif.inf.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
	vif.inf.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;
	vif.open	= VbusInterfaceOpen;
	vif.close	= VbusInterfaceClose;
	vif.read	= VbusInterfaceRead;
	vif.write	= VbusInterfaceWrite;
	vif.querymac	= VbusInterfaceQueryMac;
	WDF_QUERY_INTERFACE_CONFIG_INIT(&conf, (PINTERFACE) &vif, 
			&VBUS_INTERFACE_GUID, NULL);

	/* Add the interface to the child. */
	rc = WdfDeviceAddQueryInterface(hChild, &conf);
vlog("create pdo return %d", rc);

	return rc;
}


/* 
 * Called by framework when we enumerate a vbus device.
 */
NTSTATUS
VbusDevicePdo(WDFCHILDLIST list, 
		PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER id, 
		PWDFDEVICE_INIT init)
{
	PPDO_ID_DESC d;

	d = CONTAINING_RECORD(id, PDO_ID_DESC, header);
	return VbusCreatePdo(WdfChildListGetDevice(list), init, d);
}

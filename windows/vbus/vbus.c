/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  vbus.c:  Main driver source file.  
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

static NTSTATUS
VbusPrepareHardware(WDFDEVICE dev, WDFCMRESLIST r, WDFCMRESLIST rt)
{
	vlog("VbusPrepareHardware");
	return VbusPciPrepareHardware(dev, rt);
}

static NTSTATUS
VbusReleaseHardware(WDFDEVICE dev,WDFCMRESLIST rlist)
{
	vlog("VbusReleaseHardware");
	return VbusPciReleaseHardware();
}

static NTSTATUS
VbusAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
	WDF_CHILD_LIST_CONFIG	c;
	WDF_OBJECT_ATTRIBUTES	attr;
	NTSTATUS		rc;
	WDFDEVICE		dev;
	WDF_IO_QUEUE_CONFIG	qc;
	PNP_BUS_INFORMATION	info;
	WDF_PNPPOWER_EVENT_CALLBACKS pnp;
	WDFQUEUE		queue;
		
	vlog("VbusAdd() start");
	/* Non-buffered IO */
	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);

	WDF_CHILD_LIST_CONFIG_INIT(&c, sizeof(PDO_ID_DESC), VbusDevicePdo);
	c.EvtChildListIdentificationDescriptionDuplicate = VbusIdDup;
	c.EvtChildListIdentificationDescriptionCompare = VbusIdCmp;
	WdfFdoInitSetDefaultChildListConfig(DeviceInit, &c, 
			WDF_NO_OBJECT_ATTRIBUTES);

	/* Setup and tear down of the 'hardware' state. */
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
	pnp.EvtDevicePrepareHardware = VbusPrepareHardware;
	pnp.EvtDeviceReleaseHardware = VbusReleaseHardware;
	pnp.EvtDeviceD0Entry= VbusPciD0Entry;
	pnp.EvtDeviceD0Exit = VbusPciD0Exit;

	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

	/* Create the device */
	WDF_OBJECT_ATTRIBUTES_INIT(&attr);
	rc = WdfDeviceCreate(&DeviceInit, &attr, &dev);
	if (!NT_SUCCESS(rc)) 
		return rc;

	/* Now create various resources */
	rc = VbusPciCreateResources(dev);
	if (!NT_SUCCESS(rc))
		return rc;

	rc = WdfDeviceCreateDeviceInterface(dev, &GUID_VBUS_BUS, NULL);  
	if (!NT_SUCCESS(rc)) 
		return rc;

	info.BusTypeGuid = VBUS_BUS_GUID;
	info.LegacyBusType = PNPBus;
	info.BusNumber = 0;  
	WdfDeviceSetBusInformationForChildren(dev, &info);

	vlog("VbusAdd() end rc = %d", rc);

	return rc;
}

static VOID
VbusUnload(WDFDRIVER d)
{
	vlog("VbusUnload()\n\n");
}

PVOID 
VbusAlloc(LONG	len)
{
	PVOID	p;

	p = ExAllocatePoolWithTag(NonPagedPool, len, VTAG);
	if (p) 
		memset(p, '\0', len);
	return p;
}

VOID
VbusFree(PVOID p) 
{
	if (p) 
		ExFreePoolWithTag(p, VTAG);
}

NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	WDF_DRIVER_CONFIG   	c;
	NTSTATUS		rc;
	WDFDRIVER 		driver;
	WDF_OBJECT_ATTRIBUTES	attr;

	vlog("DriverEntry start");

	WDF_OBJECT_ATTRIBUTES_INIT(&attr);
	WDF_DRIVER_CONFIG_INIT(&c, VbusAdd);
	c.EvtDriverUnload = VbusUnload;
	rc = WdfDriverCreate(DriverObject, RegistryPath, &attr, &c, &driver);

	vlog("DriverEntry end, rc = %d", rc);
	return rc;
}

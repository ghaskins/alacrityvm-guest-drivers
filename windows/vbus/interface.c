/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  interface.c:  Bus interface.
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

/*
 * VbusInterfaceOpen - Child device interface for open
 */
int
VbusInterfaceOpen(PDEVICE_OBJECT pdo, int version, UINT64 *bh, IoqCB func)
{
	PPDO_DEVICE_DATA                pd;
	WDFDEVICE		 	dev;
	struct vbus_pci_deviceopen 	params;
	int				rc = 0;

	/* Get the PDO context */
	dev = WdfWdmDeviceGetWdfDeviceHandle(pdo);
	pd = PdoGetData(dev);

	/* Open device */
	params.devid   = (UINT32) pd->id;
	params.version = version;
	params.handle = 0;
	rc = VbusBusCall(VBUS_PCI_HC_DEVOPEN, &params, sizeof(params));
	if (rc < 0)
		return -1;

	pd->bh = params.handle;

	/* Create event IOQ */


	vlog("**** interface open called rc = %d****", rc);
	return rc;
}

/*
 * VbusInterfaceClose - Child device interface for close
 */
void
VbusInterfaceClose(UINT64 bh)
{
	/*
	 * The DEVICECLOSE will implicitly close all of the shm on the
	 * host-side, so there is no need to do an explicit per-shm
	 * hypercall
	 */
	if (bh) {
		VbusBusCall(VBUS_PCI_HC_DEVCLOSE, 
			&bh, sizeof(bh), 0);
	}
}

/*
 * VbusInterfaceCall - Child device interface for buscall
 */
int
VbusInterfaceCall(UINT64 bh, int type, void *data, int len, int flags)
{
	vlog("**** interface call called ****");
	return VbusBusCall(type, data, len);
}

/*
 * VbusInterfaceCreate -  Create an IOQ.
 */
void *
VbusInterfaceCreate(UINT64 bh, int qlen, void *data, int type, IoqCB func)
{
	PIOH		ioh;
	UNREFERENCED_PARAMETER(type);

	ioh = VbusAlloc(sizeof(IOH));
	if (ioh) {
		ioh->type = type;
		ioh->notify_func = func;
		ioh->context = data;
	}
	vlog("**** interface attach called ****");
	return ioh;
}

/*
 * VbusInterfaceDestroy - destroy an IOQ
 */
void
VbusInterfaceDestroy(void *handle)
{
	if (handle) {
		VbusFree(handle);
	}

	vlog("**** interface detach called ****");
	return ;
}

/*
 * VbusInterfaceIoqctl -  IOQ cntl ops.
 */
int
VbusInterfaceIoqctl(void *handle, int op)
{
	vlog("**** interface ioqctl called ****");
	return 0;
}

/*
 * VbusInterfaceSeek -  IOQ  seek
 */
int
VbusInterfaceSeek(void *handle, int offset)
{
	vlog("**** interface seek called ****");
	return 0;
}

/*
 * VbusInterfaceSend - Child device interface for reading a packet.
 */
int
VbusInterfaceSend(void *handle, void *data, int len)
{
	UNREFERENCED_PARAMETER(handle);
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(len);
	return 0;
}

/*
 * VbusInterfaceRecv - Child device interface for reading a packet.
 */
int
VbusInterfaceRecv(void *handle)
{
	UNREFERENCED_PARAMETER(handle);
	return 0;
}


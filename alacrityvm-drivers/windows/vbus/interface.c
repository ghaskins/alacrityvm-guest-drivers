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
 * vbus_pdo_open - Child device interface for open
 */
int
VbusInterfaceQueryMac(PUCHAR buffer)
{

	/* XXX for now, create one */
	buffer[0] = 0x01;
	buffer[1] = 0xFF;
	buffer[2] = 0xEF;
	buffer[3] = 0x23;
	buffer[4] = 0x11;
	buffer[5] = 0x01;

	return 0;
}
/*
 * VbusInterfaceOpen - Child device interface for open
 */
int
VbusInterfaceOpen(PDEVICE_OBJECT pdo, UINT64 *bh)
{
	PPDO_DEVICE_DATA                pd;
	WDFDEVICE		 	dev;
	int				rc;

	/* Get the PDO context */
	dev = WdfWdmDeviceGetWdfDeviceHandle(pdo);
	pd = PdoGetData(dev);
	rc = VbusPciOpen(pd->id, bh);

	vlog("**** interface open called rc = %d****", rc);
	return rc;
}

/*
 * VbusInterfaceClose - Child device interface for close
 */
void
VbusInterfaceClose(UINT64 bh)
{
	vlog("**** interface close called ****");
	VbusPciClose(bh);
}

/*
 * VbusInterfaceAttach - attach a tx/rx ioq
 */
void *
VbusInterfaceAttach(UINT64 bh, void *data, int type, Notifier func)
{
	PIOH		ioh;
	UNREFERENCED_PARAMETER(type);

	ioh = VbusAlloc(sizeof(IOH));
	if (ioh) {
		ioh->bh = bh;
		ioh->type = type;
		ioh->notify_func = func;
		ioh->context = data;
	}
	vlog("**** interface attach called ****");
	return ioh;
}

/*
 * VbusInterfaceDetach - detach a tx/rx ioq
 */
void
VbusInterfaceDetach(void *handle)
{
	if (handle) {
		VbusFree(handle);
	}

	vlog("**** interface detach called ****");
	return ;
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


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
NTSTATUS
VbusInterfaceQueryMac(PUCHAR buffer)
{

	/* XXX for now, create one */
	buffer[0] = 0x01;
	buffer[1] = 0xFF;
	buffer[2] = 0xEF;
	buffer[3] = 0x23;
	buffer[4] = 0x11;
	buffer[5] = 0x01;

	return STATUS_SUCCESS;
}
/*
 * vbus_pdo_open - Child device interface for open
 */
NTSTATUS
VbusInterfaceOpen(void)
{
	vlog("**** Hello World, this is vbus ***");
	return STATUS_SUCCESS;
}

/*
 * vbus_pdo_close - Child device interface for close
 */
NTSTATUS
VbusInterfaceClose(void)
{
	return STATUS_SUCCESS;
}

/*
 * vbus_pdo_read - Child device interface for reading a packet.
 */
NTSTATUS
VbusInterfaceRead(void)
{
	return STATUS_SUCCESS;
}

/*
 * vbus_pdo_write - Child device interface for reading a packet.
 */
NTSTATUS
VbusInterfaceWrite(void)
{
	return STATUS_SUCCESS;
}


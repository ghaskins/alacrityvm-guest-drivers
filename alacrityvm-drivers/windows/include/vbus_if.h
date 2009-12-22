/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  vbus_if.h:  Header for the Vbus interface.
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


/* Define an interface guid */
DEFINE_GUID(VBUS_INTERFACE_GUID,
    0xe0b27631, 0x5434, 0x1111, 0xb8, 0x90, 0x0, 0x11, 0x4f, 0xad, 0x51, 0x71);

/* Only interfaces are protected against multiple inclusion... */
#ifndef _VBUS_IF_H_
#define _VBUS_IF_H_

typedef NTSTATUS (*vbus_if_querymac)(PVOID buffer);
typedef NTSTATUS (*vbus_if_read)(VOID);
typedef NTSTATUS (*vbus_if_write)(VOID);
typedef NTSTATUS (*vbus_if_open)(VOID);
typedef NTSTATUS (*vbus_if_close)(VOID);

#define VBUS_IF_VERSION		1
#define VBUS_IF_SIZE		(sizeof(VBUS_IF))

/*
 * Vbus Child device interface.  
 */
typedef struct _VBUS_IF {
    INTERFACE		inf;
    vbus_if_open	open;
    vbus_if_close	close;
    vbus_if_read	read;
    vbus_if_write	write;
    vbus_if_querymac	querymac;
} VBUS_IF, *VBUS_PIF;


#endif /* _VBUS_IF_H_ */

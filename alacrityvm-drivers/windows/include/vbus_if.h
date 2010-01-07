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

#define VBUS_IF_VERSION		1
#define VBUS_IF_SIZE		(sizeof(VBUS_IF))

typedef void (*Notifier)(void *data);

#define VBUS_ATTACH_SEND		0x02
#define VBUS_ATTACH_RECV		0x04

/*
 * Vbus Child device interface.  
 */
typedef struct _VBUS_IF {
    INTERFACE		inf;
    int			(*open)(PDEVICE_OBJECT pdo, UINT64 *bh);
    void		(*close)(UINT64 bh);
    void		*(*attach)(UINT64 bh, int type, Notifier func);
    void		(*detach)(void *handle);
    int			(*inject)(void *handle);
    int			(*send)(void *handle, void *data, int len);
    int			(*recv)(void *handle);
    int			(*querymac)(void *buffer);
} VBUS_IF, *VBUS_PIF;


#endif /* _VBUS_IF_H_ */


/*
 * Copyright 2009 Novell.  All Rights Reserved.
 *
 * PCI to Virtual-Bus Bridge
 *
 * Author:
 *      Gregory Haskins <ghaskins@novell.com>
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

#ifndef _LINUX_VBUS_PCI_H
#define _LINUX_VBUS_PCI_H


#define VBUS_PCI_ABI_MAGIC 0xbf53eef5
#define VBUS_PCI_ABI_VERSION 2
#define VBUS_PCI_HC_VERSION 1

enum {
	VBUS_PCI_BRIDGE_NEGOTIATE,
	VBUS_PCI_BRIDGE_QREG,
	VBUS_PCI_BRIDGE_SLOWCALL,
	VBUS_PCI_BRIDGE_FASTCALL_ADD,
	VBUS_PCI_BRIDGE_FASTCALL_DROP,

	VBUS_PCI_BRIDGE_MAX, /* must be last */
};

enum {
	VBUS_PCI_HC_DEVOPEN,
	VBUS_PCI_HC_DEVCLOSE,
	VBUS_PCI_HC_DEVCALL,
	VBUS_PCI_HC_DEVSHM,

	VBUS_PCI_HC_MAX,      /* must be last */
};

struct vbus_pci_bridge_negotiate {
	UINT32 magic;
	UINT32 version;
	UINT64 capabilities;
};

struct vbus_pci_deviceopen {
	UINT32 devid;
	UINT32 version; /* device ABI version */
	UINT64 handle; /* return value for devh */
};

struct vbus_pci_devicecall {
	UINT64 devh;   /* device-handle (returned from DEVICEOPEN */
	UINT32 func;
	UINT32 len;
	UINT32 flags;
	UINT64 datap;
};

struct vbus_pci_deviceshm {
	UINT64 devh;   /* device-handle (returned from DEVICEOPEN */
	UINT32 id;
	UINT32 len;
	UINT32 flags;
	struct {
		UINT32 offset;
		UINT32 prio;
		UINT64 cookie; /* token to pass back when signaling client */
	} signal;
	UINT64 datap;
};

struct vbus_pci_call_desc {
	UINT32 vector;
	UINT32 len;
	UINT64 datap;
};

struct vbus_pci_fastcall_desc {
	struct vbus_pci_call_desc call;
	UINT32                     result;
};

struct vbus_pci_regs {
	struct vbus_pci_call_desc bridgecall;
	UINT8                      pad[48];
};

struct vbus_pci_signals {
	UINT32 eventq;
	UINT32 fastcall;
	UINT32 shmsignal;
	UINT8  pad[20];
};

struct vbus_pci_eventqreg {
	UINT32 count;
	UINT64 ring;
	UINT64 data;
};

struct vbus_pci_busreg {
	UINT32 count;  /* supporting multiple queues allows for prio, etc */
	struct vbus_pci_eventqreg eventq[1];
};

enum vbus_pci_eventid {
	VBUS_PCI_EVENT_DEVADD,
	VBUS_PCI_EVENT_DEVDROP,
	VBUS_PCI_EVENT_SHMSIGNAL,
	VBUS_PCI_EVENT_SHMCLOSE,
};

#define VBUS_MAX_DEVTYPE_LEN 128

struct vbus_pci_add_event {
	UINT64 id;
	char  type[VBUS_MAX_DEVTYPE_LEN];
};

struct vbus_pci_handle_event {
	UINT64 handle;
};

struct vbus_pci_event {
	UINT32 eventid;
	union {
		struct vbus_pci_add_event    add;
		struct vbus_pci_handle_event handle;
	} data;
};


#define VBUS_PIO 	01
#define VBUS_MMIO	02

void vbus_pci_set_reg(int, PVOID *addr, ULONG len);

#endif /* _LINUX_VBUS_PCI_H */

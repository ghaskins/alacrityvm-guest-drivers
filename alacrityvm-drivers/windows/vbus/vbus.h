/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 * Header file and definitions for the vbus driver.
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

#ifndef _VBUS_H_
#define _VBUS_H_

#include <ntddk.h>
#include <wdf.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <ntintsafe.h>
#include <initguid.h>

#include "vbus_if.h"
#include "vbus_pci.h"
#include "ioq.h"


/*
 * Context for a PDO.  Currently a placeholder
 */
typedef struct _PDO_DEVICE_DATA
{
	UINT64 		id;
	char		type[VBUS_MAX_DEVTYPE_LEN];

} PDO_DEVICE_DATA, *PPDO_DEVICE_DATA;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PDO_DEVICE_DATA, PdoGetData)

/*
 * PDO Id struct 
 */
typedef struct _PDO_ID_DESC
{
	WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER header; 
	UINT64		id;
	char		type[VBUS_MAX_DEVTYPE_LEN];
} PDO_ID_DESC, *PPDO_ID_DESC;

/*
 * For logging to COM1.
 */
#define VBUS_DEBUG	1

#ifdef VBUS_DEBUG

void vlog(const char *fmt, ...);
#define VBUS_DBG_PORT	 ((PUCHAR) 0x3F8)
#else
#define vlog(a, ...)  /**/
#endif

/*
 * Protos....
 */

DRIVER_INITIALIZE 					DriverEntry;
EVT_WDF_OBJECT_CONTEXT_CLEANUP				VbusCleanup;
EVT_WDF_OBJECT_CONTEXT_CLEANUP				VbusDeviceCleanup;
EVT_WDF_DRIVER_DEVICE_ADD 				VbusAdd;
EVT_WDF_CHILD_LIST_CREATE_DEVICE 			VbusDevicePdo;
EVT_WDF_CHILD_LIST_IDENTIFICATION_DESCRIPTION_COMPARE 	VbusIdCmp;
EVT_WDF_CHILD_LIST_IDENTIFICATION_DESCRIPTION_CLEANUP 	VbusIdRelease;
EVT_WDF_CHILD_LIST_IDENTIFICATION_DESCRIPTION_DUPLICATE VbusIdDup;
EVT_WDF_DEVICE_RELEASE_HARDWARE 			VbusReleaseHardware;
EVT_WDF_DEVICE_PREPARE_HARDWARE 			VbusPrepareHardware;
EVT_WDF_DEVICE_D0_ENTRY 				VbusPciD0Entry;
EVT_WDF_DEVICE_D0_EXIT 					VbusPciD0Exit;
EVT_WDF_INTERRUPT_ISR 					VbusIntrIsr;
EVT_WDF_INTERRUPT_DPC 					VbusIntrDPC;
EVT_WDF_INTERRUPT_ENABLE 				VbusIntrEnable;
EVT_WDF_INTERRUPT_DISABLE 				VbusIntrDisable;


extern NTSTATUS VbusPciPrepareHardware(WDFDEVICE dev, WDFCMRESLIST rt);
extern NTSTATUS VbusPciReleaseHardware(void);
extern NTSTATUS VbusPciCreateResources(WDFDEVICE dev);

extern NTSTATUS VbusInterfaceQueryMac(PUCHAR buffer);
extern NTSTATUS VbusInterfaceOpen(VOID);
extern NTSTATUS VbusInterfaceClose(VOID);
extern NTSTATUS VbusInterfaceRead(VOID);
extern NTSTATUS VbusInterfaceWrite(VOID);

/* Do not add anything below this line */
#endif

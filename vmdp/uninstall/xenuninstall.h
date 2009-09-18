/*****************************************************************************
 *
 *   File Name:      xenunist.h
 *   Created by:     WTT
 *   Date created:   01/26/07
 *
 *   %version:       12 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 10:01:02 2009 %
 *
 *****************************************************************************/
/*****************************************************************************
 *                                                                           *
 * Copyright (C) Unpublished Work of Novell, Inc. All Rights Reserved.       *
 *                                                                           *
 * THIS WORK IS AN UNPUBLISHED WORK AND CONTAINS CONFIDENTIAL, PROPRIETARY   *
 * AND TRADE SECRET INFORMATION OF NOVELL, INC. ACCESS TO THIS WORK IS       *
 * RESTRICTED TO (I) NOVELL, INC. EMPLOYEES WHO HAVE A NEED TO KNOW HOW TO   *
 * PERFORM TASKS WITHIN THE SCOPE OF THEIR ASSIGNMENTS AND (II) ENTITIES     *
 * OTHER THAN NOVELL, INC. WHO HAVE ENTERED INTO APPROPRIATE LICENSE         *
 * AGREEMENTS. NO PART OF THIS WORK MAY BE USED, PRACTICED, PERFORMED,       *
 * COPIED, DISTRIBUTED, REVISED, MODIFIED, TRANSLATED, ABRIDGED, CONDENSED,  *
 * EXPANDED, COLLECTED, COMPILED, LINKED, RECAST, TRANSFORMED OR ADAPTED     *
 * WITHOUT THE PRIOR WRITTEN CONSENT OF NOVELL, INC. ANY USE OR EXPLOITATION *
 * OF THIS WORK WITHOUT AUTHORIZATION COULD SUBJECT THE PERPETRATOR TO       *
 * CRIMINAL AND CIVIL LIABILITY.                                             *
 *                                                                           *
 *****************************************************************************/
/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    xenuninstall.h

Abstract:

    Header files and resource IDs used by the xenuninstall.h pulled from the TOASTVA sample.

--*/
#ifndef _XENUNINSTALL_H_
#define _XENUNINSTALL_H_

#include <windows.h>
#include <windowsx.h>
#include <string.h>
#include <shellapi.h>
#include <prsht.h>
#include <windef.h>
#include <regstr.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <newdev.h>
#include <strsafe.h>
#include <process.h>
#include <Difxapi.h>
#include <shlobj.h>
#include <win_commonkey.h>
#include "xencleanup.h"


#define SHOW_PROGRESS_F			0x1
#define AUTO_REBOOT_F			0x2

//
// String constants
//

//#define DEVICE_INF_NAME     L"xenbus\\xenbus.inf"
//#define BLOCK_DEVICE_INF_NAME		L"xenblk.inf"
//#define NET_DEVICE_INF_NAME		L"xennet.inf"
//#define UNINSTALL_NAME		L"uninstall.exe /u"
#define UNINSTALL_U			" /u"
#define UNINSTALL_U_SWITCH	"/u"
#define UNINSTALL_N			" /n"
#define UNINSTALL_N_SWITCH	"/n"
#define UNINSTALL_HELP		"/?"
#define AUTO_REBOOT_SWITCH	"/auto_reboot"
#define REBOOT_TIMEOUT		0

#define UNINSTALL_PHASE_0	0
#define UNINSTALL_PHASE_1	1
#define UNINSTALL_PHASE_2	2



#define MBOX_TITLE				3001
#define MBOX_UNINSTALL_TEXT		3002
#define MBOX_NOUNINSTALL_TEXT	3003
#define MBOX_FILE_CLEANUP_TEXT	3004
#define MBOX_UNINSTALL_HELP		3005
#define MBOX_NON_MATCHING_OS	3006
#define MBOX_UNSUPPORTED_OS		3007

#define PB_CLASS_NAME "MainPBClass"
#define PB_WIN_WIDTH 275
#define PB_WIN_HEIGHT 100
#define PB_INTERVALS 5

//
// global variables
//
extern HINSTANCE g_hInstance;

LRESULT
MainWndProc(HWND h, UINT i, WPARAM w, LPARAM l);

//
// utility routines
//
BOOL
IsDeviceInstallInProgress(VOID);

VOID
MarkDevicesAsNeedReinstall(
    __in HDEVINFO DeviceInfoSet
    );

DWORD
GetDeviceConfigFlags(
    __in HDEVINFO         DeviceInfoSet,
    __in PSP_DEVINFO_DATA DeviceInfoData
    );

VOID
SetDeviceConfigFlags(
    __in HDEVINFO         DeviceInfoSet,
    __in PSP_DEVINFO_DATA DeviceInfoData,
    __in DWORD            ConfigFlags
    );

HDEVINFO
GetNonPresentDevices(
    __in  LPCWSTR Enumerator OPTIONAL,
    __in  LPCWSTR HardwareID
    );

#endif

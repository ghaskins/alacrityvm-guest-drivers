/*****************************************************************************
 *
 *   File Name:      xensetup.h
 *   Created by:     WTT
 *   Date created:   01/26/07
 *
 *   %version:       12 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 10:00:52 2009 %
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

    toastva.h

Abstract:

    Header files and resource IDs used by the TOASTVA sample.

--*/
#ifndef _XENSETUP_H_
#define _XENSETUP_H_

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
#include <winpv_defs.h>
#include "xencleanup.h"
//#include "resource.h"

#define SHOW_PROGRESS_F					0x01
#define AUTO_REBOOT_F					0x02
#define NO_REBOOT_F						0x04
#define EULA_ACCEPTED_F					0x08
#define FORCE_INSTALL_F					0x10
//
// String constants
//
#define AUTO_REBOOT "/auto_reboot"
#define EULA_ACCEPTED "/eula_accepted"
#define NO_REBOOT "/no_reboot"
#define FORCE_INSTALL "/force"
#define SETUP_HELP "/?"

#define PB_CLASS_NAME "MainPBClass"
#define PB_WIN_WIDTH 275
#define PB_WIN_HEIGHT 100
#define PB_INTERVALS 5

#define REBOOT_TIMEOUT	0

#define MAX_LINE_LEN 256
#define MAX_DATE_LEN 12
#define ACTUAL_DATE_LEN 10
#define MAX_VER_LEN 20
#define BASE_YEAR 1980
#define YEAR_SHIFT 9
#define MONTH_SHIFT 5

//#define HW_ID_TO_UPDATE					L"root\\xenbus"
//#define DEVICE_INF_NAME					L"xenbus.inf"
//#define DEVICE_INF_INSTALL_SECTION_NAME	L"Manufacturer"

//#define BLOCK_DEVICE_INF_NAME			L"xenblk.inf"
//#define BLOCK_HW_ID_TO_UPDATE			L"PCI\\VEN_5853&DEV_0001"
//#define NET_DEVICE_INF_NAME				L"xennet.inf"
//#define NET_HW_ID_TO_UPDATE				L"XEN\\TYPE_vif"

#define	SETUP_RUNNIN_VN "setup_running"
#define VMDP_INSTALLED_VN "Xenpvd Installed"


#define MBOX_TITLE						2001
#define MBOX_UPDATE_TEXT				2002
#define MBOX_FILE_CLEANUP_TEXT			2003
#define MBOX_XENBUS_FAILURE_TEXT		2004
#define MBOX_DOING_UNINSTALL_TEXT		2005
#define MBOX_SETUP_HELP					2006
#define MBOX_NON_MATCHING_OS			2007
#define MBOX_UNSUPPORTED_OS				2008
#define MBOX_SETUP_RUNNING				2009

#define CP_XENBUS_F						0x1
#define CP_XENBLK_F						0x2
#define CP_XENNET_F						0x4
#define CP_XENSVC_F						0x8
#define CP_ALL_F		(CP_XENBUS_F | CP_XENBLK_F | CP_XENNET_F | CP_XENSVC_F)
//
// global variables
//
extern HINSTANCE g_hInstance;

typedef struct pv_update_s
{
	DWORD cp_flags;
	BYTE xenbus;
	BYTE xenblk;
	BYTE xennet;
	BYTE xensvc;
} pv_update_t;

//
// xensetup
//
LRESULT
MainWndProc(HWND h, UINT i, WPARAM w, LPARAM l);

DWORD
CopyXenFiles(__in LPCWSTR MediaRootDirectory, DWORD DirPathLength,
	DWORD cp_flags);

void
cleanup_setup(void);

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

VOID
DoValueAddWizard(
    __in LPCWSTR MediaRootDirectory
    );

//
// xenupdate
//

DWORD
get_guid_drv_key(TCHAR *guid, TCHAR *matching_id, HKEY *hkey_drv);

DWORD
install_driver(DWORD stup_flags);

DWORD
update_xen(LPCWSTR MediaRootDirectory, DWORD DirPathLength,
	TCHAR *src_path, size_t src_path_len, DWORD setup_flags,
	HWND hwnd_parent, HWND hwnd_pbar, BOOL *reboot);


#endif

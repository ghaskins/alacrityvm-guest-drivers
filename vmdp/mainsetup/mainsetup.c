/*****************************************************************************
 *
 *   File Name:      mainsetup.c
 *   Created by:     kra
 *   Date created:   03/20/09
 *
 *   %version:       6 %
 *   %derived_by:    kallan %
 *   %date_modified: Fri Jun 05 17:18:12 2009 %
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
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h> //for sprintf for debugging
#include "win_commonkey.h"
#pragma hdrstop

//
// Constants
//
#define WINDOWS_64_BIT	0x80000000
#define WINDOWS_2000	0x1
#define WINDOWS_XP_32	0x2
#define WINDOWS_XP_64	(WINDOWS_XP_32 | WINDOWS_64_BIT)
#define WINDOWS_2003_32	0x4
#define WINDOWS_2003_64	(WINDOWS_2003_32 | WINDOWS_64_BIT)
#define WINDOWS_2008_32 0x8
#define WINDOWS_2008_64	(WINDOWS_2008_32 | WINDOWS_64_BIT)

//
// Globals
//

//
// Function prototypes
//
DWORD running_os(void);

/******************************************************************************
	WinMain
******************************************************************************/
INT
WINAPI
WinMain(
    __in HINSTANCE hInstance,
    __in_opt HINSTANCE hPrevInstance,
    __in_opt LPSTR lpCmdLine,
    __in int nShowCmd
    )
{
	LPWSTR *arg_list;
	LPWSTR cmd_line;
	INT num_args;
	WCHAR exe_dir[MAX_PATH];
	LPWSTR file_name_part;
	DWORD len;
	size_t arg_start;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nShowCmd);

	cmd_line = GetCommandLine();
	arg_start = wcscspn(cmd_line, TEXT(" \t\n"));

	arg_list = CommandLineToArgvW(cmd_line, &num_args);

	if (!arg_list || (num_args == 0)) {
		return 0;
	}

	len = GetFullPathName(arg_list[0], MAX_PATH, exe_dir, &file_name_part);
	if (len >= MAX_PATH || len == 0) {
		return 0;
	}

	// Strip the filename off the path.
	*file_name_part = L'\0';

	GlobalFree(arg_list);

	switch (running_os()) {
		case WINDOWS_2000:
			PathAppend(exe_dir, TEXT("win2000\\x86"));
			break;
		case WINDOWS_XP_32:
			PathAppend(exe_dir, TEXT("winxp\\x86"));
			break;
		case WINDOWS_XP_64:
			/* XP 64 uses the 2003 64 files. */
			PathAppend(exe_dir, TEXT("win2003\\x64"));
			break;
		case WINDOWS_2003_32:
			PathAppend(exe_dir, TEXT("win2003\\x86"));
			break;
		case WINDOWS_2003_64:
			PathAppend(exe_dir, TEXT("win2003\\x64"));
			break;
		case WINDOWS_2008_32:
			PathAppend(exe_dir, TEXT("win2008\\x86"));
			break;
		case WINDOWS_2008_64:
			PathAppend(exe_dir, TEXT("win2008\\x64"));
			break;
		default:
			return 0;
	}
	SetCurrentDirectory(exe_dir);
	return (INT)_wspawnl(_P_WAIT, TEXT("setup.exe"), TEXT("setup.exe"),
		&cmd_line[arg_start], NULL);
}

/* Windows 2000 can't tell if we're running on 32 or 64 bit processors. */
/* So use the registry to find out. */
static DWORD
get_reg_proc_id_val(void)
{
	HKEY hkey;
	TCHAR val[MAX_PATH];
	DWORD len;
	int ccode;
	
	if (ERROR_SUCCESS == RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_PROC_ID),
			0,
			KEY_ALL_ACCESS,
			&hkey)) {
		len = sizeof(val);
		if(ERROR_SUCCESS == RegQueryValueEx(hkey,
				TEXT(RKEY_PROC_ID_NAME),
				NULL,
				NULL,
				(LPBYTE)val,
				&len)) {
			if (_wcsnicmp(val, TEXT("x86"), lstrlen(TEXT("x86"))) == 0)
				ccode = 32;
			else
				ccode = 64;
			
		}
		RegCloseKey(hkey);
	}
	return ccode;
}

DWORD
running_os(void)
{
	OSVERSIONINFOEX osvi;
	DWORD id;
	DWORD os = 0;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((OSVERSIONINFO *)&osvi);

	/* Windows 2000 can't tell if we're running on 32 or 64 bit processors. */
	id = get_reg_proc_id_val();
	if (osvi.dwMinorVersion == 0 && osvi.dwMajorVersion == 5) {
		os = WINDOWS_2000;
	}
	else if (osvi.dwMinorVersion == 1 && osvi.dwMajorVersion == 5) {
		if (id == 32)
			os = WINDOWS_XP_32;
		else
			os = WINDOWS_XP_64;
	}
	else if (osvi.dwMinorVersion == 2 && osvi.dwMajorVersion == 5) {
		if (id == 32){
			os = WINDOWS_2003_32;
		}
		else
			os = WINDOWS_2003_64;
	}
	else if (osvi.dwMajorVersion == 6) {
		if (id == 32)
			os = WINDOWS_2008_32;
		else
			os = WINDOWS_2008_64;
	}
	return os;
}

/*****************************************************************************
 *
 *   File Name:      pvctrl.c
 *   Created by:     KRA
 *   Date created:   07/22/08
 *
 *   %version:       2 %
 *   %derived_by:    kallan %
 *   %date_modified: Tue Aug 19 13:06:47 2008 %
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
#include <stdio.h>
#include <string.h>
#include <shellapi.h>
#include <winpv_defs.h>
#include <win_commonkey.h>

#define BLKCTRL_DISK						L"disk"
#define BLKCTRL_NO_DISK						L"no_disk"
#define BLKCTRL_LAN							L"lan"
#define BLKCTRL_NO_LAN						L"no_lan"
#define BLKCTRL_OVERRIDE_OPTION				L"-o"
#define BLKCTRL_OVERRIDE_FILE_DISK			L"file"
#define BLKCTRL_OVERRIDE_ALL_PHY_DISK		L"phy"
#define BLKCTRL_OVERRIDE_ALL_BY_DISK		L"by_disk"
#define BLKCTRL_OVERRIDE_BY_ID_DISK			L"by_id"
#define BLKCTRL_OVERRIDE_BY_NAME_DISK		L"by_name"
#define BLKCTRL_OVERRIDE_BY_PATH_DISK		L"by_path"
#define BLKCTRL_OVERRIDE_BY_UUID_DISK		L"by_uudi"
#define BLKCTRL_OVERRIDE_IOEMU_DISK			L"ioemu"

#define BLKCTRL_OVERRIDE_OPTION_VALUE		0x80000000
#define BLKCTRL_DISK_NO_VALUE				0x40000000
#define BLKCTRL_LAN_NO_VALUE				0x20000000
#define BLKCTRL_MASK						(BLKCTRL_DISK_NO_VALUE | \
											BLKCTRL_LAN_NO_VALUE | \
											BLKCTRL_OVERRIDE_OPTION_VALUE)

#define ERROR_GETTING_COMMAND_LINE			0x80000001
#define ERROR_INCOMPATIBLE_OPTION			0x80000002
#define ERROR_DISK_NO_DISK					0x80000003
#define ERROR_LAN_NO_LAN					0x80000004
#define ERROR_OPTION_NO_DISK				0x80000005
#define ERROR_MISSING_OPTION				0x80000006
#define ERROR_IOEMU_AND_OPTION				0x80000007

void usage(LPWSTR cmd)
{
	if (cmd) {
		printf("\nInvalid argument: %ws.\n", cmd);
	}
	printf("\nUsage: pvctrl [%ws | %ws] [%ws | %ws] [%ws <disk override options>]\n",
		BLKCTRL_DISK,BLKCTRL_NO_DISK, BLKCTRL_LAN,
		BLKCTRL_NO_LAN, BLKCTRL_OVERRIDE_OPTION);

	printf("\n<disk override options> - multiple options may be combined\n");
	printf("execpt with %ws and include:\n",
		BLKCTRL_OVERRIDE_IOEMU_DISK);
	printf("\t%ws - do not control any file back disks.\n",
		BLKCTRL_OVERRIDE_FILE_DISK);
	printf("\t%ws - do not control any physically back disks.\n",
		BLKCTRL_OVERRIDE_ALL_PHY_DISK);
	printf("\t%ws - do not control any disks referenced with /dev/by_*.\n",
		BLKCTRL_OVERRIDE_ALL_BY_DISK);
	printf("\t%ws - do not control disks referenced /dev/by_id.\n",
		BLKCTRL_OVERRIDE_BY_ID_DISK);
	printf("\t%ws - do not control disks referenced /dev/by_name.\n",
		BLKCTRL_OVERRIDE_BY_NAME_DISK);
	printf("\t%ws - do not control disks referenced /dev/by_path.\n",
		BLKCTRL_OVERRIDE_BY_PATH_DISK);
	printf("\t%ws - do not control disks referenced /dev/by_uuid.\n",
		BLKCTRL_OVERRIDE_BY_UUID_DISK);
	printf("\t%ws - do not control the boot disk\n",
		BLKCTRL_OVERRIDE_IOEMU_DISK);
	printf("\t\tor those referenced by hd<x> in the VM config file.\n");
	printf("\t\tioemu is not compatible with other <disk override options>.\n");

	printf("\nIf using any of the <disk override options>, disk and -o must be specified.\n");

	printf("\nThe absence of %ws implies %ws.\n",
		BLKCTRL_DISK, BLKCTRL_NO_DISK);
	printf("The absence of %ws implies %ws.\n",
		BLKCTRL_LAN, BLKCTRL_NO_LAN);
}

void print_error(LONG error)
{
	switch (error) {
		case ERROR_GETTING_COMMAND_LINE:
			wprintf(L"pvctrl: Failed to get command line.\n");
			break;   
		case ERROR_INCOMPATIBLE_OPTION:
			printf("\npvctrl: no_disk and -o are incompatible.\n");
			break;   
		case ERROR_DISK_NO_DISK:
			printf("\npvctrl: disk and no_disk are incompatible.\n");
			break;   
		case ERROR_OPTION_NO_DISK:
			printf("\npvctrl: override optoins present, but disk not specified.\n");
			break;   
		case ERROR_LAN_NO_LAN:
			printf("\npvctrl: lan and no_lan are incompatible.\n");
			break;   
		case ERROR_FILE_NOT_FOUND:
			printf("\npvctrl: Command failed.  Check that VMDP is installed.\n");
			break;   
		case ERROR_ACCESS_DENIED:
			printf("\npvctrl: User not privileged to perform command.\n");
			break;   
		case ERROR_MISSING_OPTION:
			printf("\npvctrl: overrides specified with out -o.\n");
			break;   
		case ERROR_IOEMU_AND_OPTION:
			printf("\npvctrl: ioemu and other disk override options are incompatible.\n");
			break;   
	}
}

int get_command_value(LPWSTR cmd, unsigned long *val)
{
	do {
		if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_OPTION) == 0) {
			*val = BLKCTRL_OVERRIDE_OPTION_VALUE;
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_DISK) == 0) {
			*val = XENBUS_PROBE_PV_DISK;					
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_NO_DISK) == 0) {
			*val = BLKCTRL_DISK_NO_VALUE;					
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_LAN) == 0) {
			*val = XENBUS_PROBE_PV_NET;					
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_NO_LAN) == 0) {
			*val = BLKCTRL_LAN_NO_VALUE;					
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_NO_LAN) == 0) {
			*val = 0;					
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_FILE_DISK) == 0) {
			*val = XENBUS_PROBE_PV_OVERRIDE_FILE_DISK;					
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_ALL_PHY_DISK) == 0) {
			*val = XENBUS_PROBE_PV_OVERRIDE_ALL_PHY_DISK;   
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_ALL_BY_DISK) == 0) {
			*val = XENBUS_PROBE_PV_OVERRIDE_ALL_BY_DISK;
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_BY_ID_DISK) == 0) {
			*val = XENBUS_PROBE_PV_OVERRIDE_BY_ID_DISK;
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_BY_NAME_DISK) == 0) {
			*val = XENBUS_PROBE_PV_OVERRIDE_BY_NAME_DISK;
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_BY_PATH_DISK) == 0) {
			*val = XENBUS_PROBE_PV_OVERRIDE_BY_PATH_DISK;
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_BY_UUID_DISK) == 0) {
			*val = XENBUS_PROBE_PV_OVERRIDE_BY_UUID_DISK;
			break;
		}
		else if (_wcsicmp(cmd, BLKCTRL_OVERRIDE_IOEMU_DISK) == 0) {
			*val = XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK;
			break;
		}
		else {
			return 1;
		}
	} while (FALSE);
	return 0;
}

LONG
set_use_pv_drivers(DWORD use_pv_drivers)
{
	LONG ccode;
	HKEY hKey;

	ccode = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		TEXT(RKEY_SYS_PVDRIVERS),
		0,
		KEY_ALL_ACCESS,
		&hKey);
	if (ERROR_SUCCESS == ccode) {
		ccode = RegSetValueEx(
			hKey,
			TEXT(RKEY_SYS_PVDRIVERS_VN),
			0,
			REG_DWORD,
			(PBYTE)&use_pv_drivers,
			sizeof(use_pv_drivers));
		RegCloseKey(hKey);
	}
	return ccode;
}

INT __cdecl
wmain()
{
	LPWSTR *szArglist;
	DWORD cmd_val;
	DWORD use_pv_drivers;
	LONG ccode;
	int nArgs;
	int i;

	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if( NULL == szArglist ) {
		print_error(ERROR_GETTING_COMMAND_LINE);
		return 0;
	}
	if (nArgs < 2) {
		usage(NULL);
		LocalFree(szArglist);
		return 0;
	}

	use_pv_drivers = 0;
	for (i = 1; i < nArgs; i++) {
		if (get_command_value(szArglist[i], &cmd_val) == 0) {
			use_pv_drivers |= cmd_val;
		}
		else {
			usage(szArglist[i]);
			LocalFree(szArglist);
			return 0;
		}
	}
	LocalFree(szArglist);

	if ((use_pv_drivers & BLKCTRL_OVERRIDE_OPTION_VALUE) && nArgs < 3) {
		usage(NULL);
		return 0;
	}

	if (!(use_pv_drivers & BLKCTRL_OVERRIDE_OPTION_VALUE) &&
		(use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_DISK_MASK)) {
		print_error(ERROR_MISSING_OPTION);
		usage(NULL);
		return 0;
	}

	if ((use_pv_drivers & BLKCTRL_OVERRIDE_OPTION_VALUE) &&
		(use_pv_drivers & BLKCTRL_DISK_NO_VALUE)) {
		print_error(ERROR_INCOMPATIBLE_OPTION);
		usage(NULL);
		return 0;
	}

	if ((use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK) &&
		!(use_pv_drivers & XENBUS_PROBE_PV_DISK)) {
		print_error(ERROR_OPTION_NO_DISK);
		usage(NULL);
		return 0;
	}

	if ((use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_DISK_MASK) &&
		(use_pv_drivers & BLKCTRL_DISK_NO_VALUE)) {
		print_error(ERROR_INCOMPATIBLE_OPTION);
		usage(NULL);
		return 0;
	}

	if ((use_pv_drivers & XENBUS_PROBE_PV_DISK) &&
		(use_pv_drivers & BLKCTRL_DISK_NO_VALUE)) {
		print_error(ERROR_DISK_NO_DISK);
		usage(NULL);
		return 0;
	}

	if ((use_pv_drivers & XENBUS_PROBE_PV_NET) &&
		(use_pv_drivers & BLKCTRL_LAN_NO_VALUE)) {
		print_error(ERROR_LAN_NO_LAN);
		usage(NULL);
		return 0;
	}

	if ((use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_DISK_MASK)
		&& !(use_pv_drivers & XENBUS_PROBE_PV_DISK)) {
		print_error(ERROR_OPTION_NO_DISK);
		usage(NULL);
		return 0;
	}

	if ((use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK) &&
		(use_pv_drivers & XENBUS_PROBE_PV_OVERRIDE_DISK_MASK)) {
		print_error(ERROR_MISSING_OPTION);
		usage(NULL);
		return 0;
	}

	ccode = set_use_pv_drivers(use_pv_drivers & ~BLKCTRL_MASK);
	if (ccode != ERROR_SUCCESS) {
		print_error(ccode);
		return 0;
	}
	return 1;
}

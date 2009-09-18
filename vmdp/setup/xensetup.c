/*****************************************************************************
 *
 *   File Name:      xensetup.c
 *   Created by:     WTT
 *   Date created:   01/26/07
 *
 *   %version:       34 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 09:48:08 2009 %
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

Abstract:

    The XENSETUP application illustrates installation techniques that can be
    used to seamlessly integrate PnP device installation with installation of
    value-added software, regardless of whether the software installation
    preceeded the hardware installation, or vice versa.

--*/

#include "xensetup.h"
#include <shlwapi.h>

#pragma hdrstop

//
// Constants
//

#if defined(_IA64_)
    #define XENSETUP_PLATFORM_SUBDIRECTORY L"ia64"
#elif defined(_X86_)
    #define XENSETUP_PLATFORM_SUBDIRECTORY L"i386"
#elif defined(_AMD64_)
    #define XENSETUP_PLATFORM_SUBDIRECTORY L"amd64"
#else
#error Unsupported platform
#endif

#define XENSETUP_PLATFORM_SUBDIRECTORY_SIZE (sizeof(XENSETUP_PLATFORM_SUBDIRECTORY) / sizeof(WCHAR))

//
// Globals
//
HINSTANCE g_hInstance;

//
// Function prototypes
//
static DWORD WINAPI	InstallXen(__in LPCWSTR MediaRootDirectory,
	DWORD DirPathLength, DWORD setup_flags);
static DWORD WINAPI	InstallXenBus(__in LPCWSTR MediaRootDirectory);
static DWORD DeleteXenFiles(void);
static BOOL DeleteInstallKey(); 
static BOOL checkSetupDuringUninstall(void);
static BOOL createXenPVDKey(DWORD xenpvdInstalled);
static BOOL usePvDrivers(DWORD useDrivers);
static int process_arglist(int argc, LPWSTR *argv, DWORD *setup_flags);
static void	update_xennet_inf_reg(void);
static DWORD is_setup_running(void);
static DWORD is_vmdp_installed(void);


/******************************************************************************
	WinMain
******************************************************************************/
LRESULT MainWndProc(HWND h, UINT i, WPARAM w, LPARAM l)
{
	return DefWindowProc(h, i, w, l);
}

INT
WINAPI
WinMain(
    __in HINSTANCE hInstance,
    __in_opt HINSTANCE hPrevInstance,
    __in_opt LPSTR lpCmdLine,
    __in int nShowCmd
    )
{
	LPWSTR *ArgList;
	INT NumArgs;
	WCHAR MediaRootDirectory[MAX_PATH];
	LPWSTR FileNamePart;
	DWORD DirPathLength;
	DWORD setup_flags;
	DWORD cc;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nShowCmd);

	g_hInstance = hInstance;

	if (is_setup_running()) {
		CreateMessageBox(MBOX_TITLE, MBOX_SETUP_RUNNING);
		return 0;
	}

	if ((cc = xen_matches_os()) != MATCH_OS_SUCCESS) {
		if (cc == MATCH_OS_MISMATCH)
			CreateMessageBox(MBOX_TITLE, MBOX_NON_MATCHING_OS);
		else if (cc == MATCH_OS_UNSUPPORTED)
			CreateMessageBox(MBOX_TITLE, MBOX_UNSUPPORTED_OS);
		cleanup_setup();
		return 0;
	}

	//
	// Windows 2000 doesn't suppress auto-run applications when media (e.g.,
	// a CD) is inserted while a "Found New Hardware" popup is onscreen.  This
	// means that, by default, inserting a CD in order to supply PnP with the
	// necessary INF and driver files will result in the autorun app launching,
	// and obscuring the wizard, causing user confusion, etc.
	//
	// To avoid this, we retrieve an entrypoint to a Windows 2000 (and later)
	// Configuration Manager (CM) API that allows us to detect when a device
	// installation is in-progress, and suppress our own application from
	// starting.
	//
	if (IsDeviceInstallInProgress()) {
		//
		// We don't want to startup right now.  Don't worry--the value-added
		// software part of the device installation will be invoked (if
		// necessary) by our device co-installer during finish-install
		// processing.
		//
		cleanup_setup();
		return 0;
	}

	// Can't run setup if uninstall is not done and there has been a reboot.
	if (checkSetupDuringUninstall()) {
		CreateMessageBox(MBOX_TITLE, MBOX_DOING_UNINSTALL_TEXT);
		cleanup_setup();
		return 0;
	}

	//
	// Retrieve the full directory path from which our setup program was
	// invoked.
	//
	ArgList = CommandLineToArgvW(GetCommandLine(), &NumArgs);

	if (ArgList && (NumArgs >= 1)) {
		DirPathLength = GetFullPathName(ArgList[0],
			MAX_PATH,
			MediaRootDirectory,
			&FileNamePart);

		if (DirPathLength >= MAX_PATH) {
			//
			// The directory is too large for our buffer.  Set our directory
			// path length to zero so we'll simply bail out in this rare case.
			//
			DirPathLength = 0;
		}

		if (DirPathLength) {
			//
			// Strip the filename off the path.
			//
			*FileNamePart = L'\0';
			DirPathLength = (DWORD)(FileNamePart - MediaRootDirectory);
		}

	}
	else {
		//
		// For some reason, we couldn't get the command line arguments that
		// were used when invoking our setup app.  Assume current directory
		// instead.
		//
		DirPathLength = GetCurrentDirectory(MAX_PATH, MediaRootDirectory);
		if (DirPathLength >= MAX_PATH) {
			//
			// The current directory is too large for our buffer.  Set our
			// directory path length to zero so we'll simply bail out in this
			// rare case.
			//
			DirPathLength = 0;
		}
		if (DirPathLength) {
			//
			// Ensure that path ends in a path separator character.
			//
			if ((MediaRootDirectory[DirPathLength-1] != L'\\') &&
				(MediaRootDirectory[DirPathLength-1] != L'/'))
			{
				MediaRootDirectory[DirPathLength++] = L'\\';

				if (DirPathLength < MAX_PATH) {
					MediaRootDirectory[DirPathLength] = L'\0';
				}
				else {
					//
					// Not enough room in buffer to add path separator char
					//
					DirPathLength = 0;
				}
			}
		}
	}

	setup_flags = 0;
	if(NumArgs >= 2) {
		if (!process_arglist(NumArgs, ArgList, &setup_flags)) {
			CreateMessageBox(MBOX_TITLE, MBOX_SETUP_HELP);
			GlobalFree(ArgList);
			cleanup_setup();
			return 0;
		}
	}

	if (ArgList) {
		GlobalFree(ArgList);
	}

	if (!DirPathLength) {
		//
		// Couldn't figure out what the root directory of our installation
		// media was.  Bail out.
		//
		cleanup_setup();
		return 0;
	}

	//
	// If we're being invoked from a platform-specific subdirectory (i.e.,
	// \i386 or \ia64), then strip off that subdirectory to get the true media
	// root path.
	//
	if (DirPathLength > XENSETUP_PLATFORM_SUBDIRECTORY_SIZE) {
		//
		// We know that the last character in our MediaRootDirectory string is
		// a path separator character.  Check to see if the preceding
		// characters match our platform-specific subdirectory.
		//
		if (!_wcsnicmp(
			&(MediaRootDirectory[
				DirPathLength - XENSETUP_PLATFORM_SUBDIRECTORY_SIZE]),
			XENSETUP_PLATFORM_SUBDIRECTORY,
			XENSETUP_PLATFORM_SUBDIRECTORY_SIZE - 1)) {
			//
			// Platform-specific part matches, just make sure preceding char
			// is a path separator char.
			//
			if ((MediaRootDirectory[DirPathLength -
					XENSETUP_PLATFORM_SUBDIRECTORY_SIZE - 1] == L'\\')
				|| (MediaRootDirectory[DirPathLength -
					XENSETUP_PLATFORM_SUBDIRECTORY_SIZE - 1] == L'/')) {
				MediaRootDirectory[DirPathLength
					- XENSETUP_PLATFORM_SUBDIRECTORY_SIZE] = L'\0';
			}
		}
	}

	OutputDebugString(TEXT("**** Calling InstallXen ****\n"));

	InstallXen(MediaRootDirectory, DirPathLength, setup_flags);

	cleanup_setup();
	return 0;
}

/******************************************************************************
	CopyXenFiles
******************************************************************************/
DWORD
CopyXenFiles(__in LPCWSTR MediaRootDirectory, DWORD DirPathLength,
	DWORD cp_flags)
{
	DWORD Err = NO_ERROR;
	LANGID langID;
	size_t src_len = 0;
	size_t src_alt_len = 0;
	size_t dest_len = 0;
	size_t bus_len = 0;
	WCHAR langIDString[128];
	TCHAR src[MAX_PATH];
	TCHAR dest[MAX_PATH];

	OutputDebugString(TEXT("**** ENTER CopyXenFiles ****\n"));

	if (FAILED(StringCchCopy(src, MAX_PATH, MediaRootDirectory))) {
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	StringCchLength( src, MAX_PATH, &src_len);

	/* Get the complete path to Program Files */
	if (SUCCEEDED(SHGetFolderPath(
		NULL, 
		CSIDL_PROGRAM_FILES, 
		NULL, 
		SHGFP_TYPE_CURRENT, 
		dest))) {
		OutputDebugString(TEXT("**** Get Folder Path was Successful ****\n"));
	}
	else {
		OutputDebugString(TEXT("**** FAILED to get Path ****\n"));
		return (1);
	}

	/* Create Directory Program Files\Novell */
	PathAppend(dest, TEXT("Novell"));
	if (!CreateDirectory(dest, NULL)) { 
		Err = GetLastError();
		if (Err != ERROR_ALREADY_EXISTS) {
			OutputDebugString(TEXT("**** Failed to create Novell dir ****.\n"));
			return(Err);
		}
	}
	SetFileAttributes(dest, FILE_ATTRIBUTE_NORMAL);

	/* Create Directory Program Files\Novell\XenDRV */
	PathAppend(dest, TEXT("XenDrv"));
	StringCchLength( dest, MAX_PATH, &dest_len);
	if (!CreateDirectory(dest, NULL)) {
		Err = GetLastError();
		if (Err != ERROR_ALREADY_EXISTS) {
			OutputDebugString(TEXT("**** Failed to create XenDrv dir ****.\n"));
			return(Err);
		}
	}
	SetFileAttributes(dest, FILE_ATTRIBUTE_NORMAL);

	/* Create Directory Program Files\Novell\XenDrv\xenbus */
	PathAppend(dest, TEXT("xenbus"));
	if (!CreateDirectory(dest, NULL)) {
		Err = GetLastError();
		if (Err != ERROR_ALREADY_EXISTS) {
			OutputDebugString(TEXT("**** Failed to create xenbus dir ****.\n"));
			return(Err);
		}
	}
	SetFileAttributes(dest, FILE_ATTRIBUTE_NORMAL);
	dest[dest_len] = '\0';

	/* Copy files to Program Files\Novell\XenDrv */
	OutputDebugString(TEXT("**** Copy Files to the XenDrv directory. ****\n"));
	cp_file(src, dest, src_len, dest_len, TEXT("setup.exe"));
	cp_file(src, dest, src_len, dest_len, TEXT("uninstall.exe"));
	cp_file(src, dest, src_len, dest_len, TEXT("pvctrl.exe"));

	if (cp_flags & CP_XENSVC_F) {
		/* It can sometimes take xensvc a while to not be in use before */
		/* trying to copy a new one on update, so wait a little if needed. */
		Err = 0;
		do {
			if (cp_file(src, dest, src_len, dest_len, TEXT("xensvc.exe")) != 0)
				break;
			if (GetLastError() != ERROR_SHARING_VIOLATION)
				break;
			Sleep(1000);
			Err++;
		} while (Err < 60);
	}

	if (cp_flags & CP_XENBLK_F) {
		cp_file(src, dest, src_len, dest_len, TEXT("xenblk.sys"));
		cp_file(src, dest, src_len, dest_len, TEXT("xenblk.cat"));
		cp_file(src, dest, src_len, dest_len, TEXT("xenblk.inf"));
	}

	if (cp_flags & CP_XENNET_F) {
		cp_file(src, dest, src_len, dest_len, TEXT("xennet.sys"));
		cp_file(src, dest, src_len, dest_len, TEXT("xennet.cat"));
		cp_file(src, dest, src_len, dest_len, TEXT("xennet.inf"));
	}

	if (cp_flags & CP_XENBUS_F) {
		PathAppend(dest, TEXT("xenbus"));
		StringCchLength( dest, MAX_PATH, &bus_len);
		cp_file(src, dest, src_len, bus_len, TEXT("xenbus.sys"));
		cp_file(src, dest, src_len, bus_len, TEXT("xenbus.cat"));
		cp_file(src, dest, src_len, bus_len, TEXT("xenbus.inf"));
		dest[dest_len] = '\0';
	}

	PathAppend(src, TEXT("..\\.."));
	StringCchLength(src, MAX_PATH, &src_len);

	PathAppend(src, TEXT("dpinst"));
	cp_file(src, dest, src_len, dest_len, TEXT("dpinst.xml"));

#ifdef ARCH_x86
	PathAppend(src, TEXT("dpinst\\x86"));
	StringCchLength(src, MAX_PATH, &src_alt_len);
#else
	PathAppend(src, TEXT("dpinst\\x64"));
	StringCchLength(src, MAX_PATH, &src_alt_len);
#endif
	cp_file(src, dest, src_alt_len, dest_len, TEXT("dpinst.exe"));
	cp_file(src, dest, src_alt_len, dest_len, TEXT("difxapi.dll"));
	src[src_len] = '\0';

	/* Copy files to Program Files\Novell\XenDrv\xenbus */
	src[src_len] = '\0';
	PathAppend(dest, TEXT("xenbus"));

#ifdef ARCH_x86
	PathAppend(src, TEXT("dpinst\\x86"));
	StringCchLength(src, MAX_PATH, &src_alt_len);
#else
	PathAppend(src, TEXT("dpinst\\x64"));
	StringCchLength(src, MAX_PATH, &src_alt_len);
#endif
	//???????????cp_file(src, dest, src_alt_len, bus_len, TEXT("dpinst.xml"));
	cp_file(src, dest, src_alt_len, bus_len, TEXT("dpinst.exe"));
	cp_file(src, dest, src_alt_len, bus_len, TEXT("difxapi.dll"));

	/* Move back to Program Files\Novell\XenDrv */
	src[src_len] = '\0';
	dest[dest_len] = '\0';

	/* Copy the eula from the source media based on Language ID */
	OutputDebugString(TEXT("* CopyXenFiles - Calling GetUserDefaultLangID*\n"));
	PathAppend(src, TEXT("eula"));
	langID = GetUserDefaultLangID();
	Err = _ultow_s(langID, langIDString, 128,16);
	if (Err == NO_ERROR) {
		PathAppend(src, langIDString);
	}
	else {
		OutputDebugString(TEXT("**Failed to convert LangID: default to 409\n"));
		PathAppend(src, TEXT("409"));
	}

	OutputDebugString(TEXT("**** CopyXenFiles - Copy eula ****\n"));
	PathAppend(src, TEXT("vmdpeula.txt"));
	PathAppend(dest, TEXT("vmdpeula.txt"));
	Err = CopyFile(src, dest, FALSE);
	if (Err == 0) { /* Default to English */
		OutputDebugString(TEXT("**** Copy eula failed try 409 ****\n"));
		src[src_len] = '\0';
		PathAppend(src, TEXT("eula\\409\\vmdpeula.txt"));
		CopyFile(src, dest, FALSE);
	}
	SetFileAttributes(dest, FILE_ATTRIBUTE_NORMAL);
	src[src_len] = '\0';
	dest[dest_len] = '\0';

	return(NO_ERROR);
}

/******************************************************************************
 checkSetupDuringUninstall
 We have a condition where one could select to uninstall then run setup
 prior to the reboot.  This check for this condition and return 
 whether we are in that state or not.

 Uninstall marks files to be remoevd on reboot.  So the reboot must happen
 before we try to install again.
******************************************************************************/
static BOOL
checkSetupDuringUninstall(void)
{
	HKEY hkey;
	TCHAR val[MAX_PATH];
	DWORD len;
	BOOL ccode = FALSE;
	
	OutputDebugString(TEXT("**** ENTER checkSetupDuringUninstall ****\n"));

	if (ERROR_SUCCESS == RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_S_RUNONCE),
			0,
			KEY_ALL_ACCESS,
			&hkey)) {
		len = sizeof(val);
		if(ERROR_SUCCESS == RegQueryValueEx(hkey,
				TEXT(RKEY_S_RUNONCE_VN),
				NULL,
				NULL,
				(LPBYTE)val,
				&len)) {
			ccode = TRUE;	 // Value was set for uninstall to run when reboot.
		}
		RegCloseKey(hkey);
	}
	OutputDebugString(TEXT("**** checkSetupDuringUninstall ****\n"));
	return(ccode);
}

/******************************************************************************
	InstallXen
******************************************************************************/

static DWORD
WINAPI
InstallXen(
    __in LPCWSTR MediaRootDirectory, DWORD DirPathLength, DWORD setup_flags)
{
	DWORD Err = NO_ERROR;
	DWORD RegDataType;
	DWORD numOfChars = 1024;
	size_t sourcePathSize = 0;
	size_t pf_path_len = 0;
	HKEY hkey_drv;
	TCHAR destInfPath[MAX_PATH];
	TCHAR sourcePath[MAX_PATH];
	wchar_t debuggerString[128];
	HWND hwnd_parent = NULL;
	HWND hwnd_pbar = NULL;
	BOOL RebootRequired = FALSE;
	BOOL rebooting;

	OutputDebugString(TEXT("**** ENTER InstallXen ****\n"));

	if (SUCCEEDED(SHGetFolderPath(
			NULL, 
            CSIDL_PROGRAM_FILES, 
            NULL, 
            SHGFP_TYPE_CURRENT, 
            sourcePath))) {
		StringCchLength(sourcePath, MAX_PATH, &pf_path_len);
		PathAppend(sourcePath, TEXT("Novell\\XenDrv"));
		StringCchLength(sourcePath, MAX_PATH, &sourcePathSize);
	}
	else {
		OutputDebugString(TEXT("**** FAILED to get Path ****\n"));
		return (1);
	}

	if (create_progress_bar(&hwnd_parent, &hwnd_pbar, PB_INTERVALS)) {
		setup_flags |= SHOW_PROGRESS_F;
		advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);
	}

	/* Check if we are doing a first time install or an update/upgrade. */
	if (is_vmdp_installed() == 0) {
		CopyXenFiles(MediaRootDirectory, DirPathLength, CP_ALL_F);

		/*********************************************************************
			If we can't open this key, this is the first time, so the key 
			doesn't exist yet.  Or the condition of someone selecting not
			to reboot at the first phase of the uninstall and then re-ran 
			the setup.
		**********************************************************************/
		
		OutputDebugString(TEXT("**** Calling InstallXenBus ****\n"));
		Err = InstallXenBus(MediaRootDirectory);
		if ( Err == NO_ERROR) {
			advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);
			createXenPVDKey(1);

			/******************************************************************
			The install is done in two steps to work around a difference  
			in the way DPINST behaves on Windows 2000 compared to other 
			versions of Windows 
			******************************************************************/

			/* Install xenbus */
			OutputDebugString(TEXT("*** Set Current Directory - xenbus **\n"));
			PathAppend(sourcePath, TEXT("xenbus"));
			SetCurrentDirectory(sourcePath);
			sourcePath[sourcePathSize] = '\0';

			OutputDebugString(TEXT("**** Spawning dpinst for xenbus ****\n"));

#ifndef TARGET_OS_Win2K
			/* Setup xenbus to load and be active so that xenblk will */
			/* come up enough for xennet to install before the reboot. */
			/* Hovever, this causes problems with Win 2000. */
			usePvDrivers(XENBUS_PROBE_PV_DISK_NET_NO_IOEMU);
#endif

			/* Install XENBUS.SYS.  We always supress the eula for xenbus. */
			Err = install_driver(EULA_ACCEPTED_F);
			if (Err == (DWORD) -1 || (Err & 0x80000000)) {
				/* Failed to install xenbus */
				swprintf_s(	&debuggerString[0], 
					sizeof(debuggerString), 
					L"**** spawn dpinst FAILED for xenbus ****  Err = %x \n", 
					Err);
					OutputDebugString(debuggerString);

				/* Cleanup Files */
				usePvDrivers(0);
				remove_xen_reg_entries(TEXT(ENUM_SYSTEM),
					TEXT(XENBUS_MATCHING_ID));
				sourcePath[pf_path_len] = '\0';
				SetCurrentDirectory(sourcePath);
				del_xen_files(setup_flags);
				DeleteInstallKey();
				destroy_progress_bar(hwnd_parent, hwnd_pbar);
				CreateMessageBox(MBOX_TITLE, MBOX_XENBUS_FAILURE_TEXT);
				return(1);
			}
			advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);

			swprintf_s(&debuggerString[0], 
				sizeof(debuggerString),
				L"**** spawn dpinst return value = %x \n", 
				Err);
			OutputDebugString(debuggerString);

			/* Install xenblk and xennet */
			OutputDebugString(TEXT("*** Set Current Directory - XenDrv ***\n"));
			SetCurrentDirectory(sourcePath);

			/* Now that xenbus is up and running, reset use_pv_drivers to */
			/* use both disk  and lan.  xenbus wont re-look at this valuse. */
			/* This is another 2000 issue with reboots. */
			usePvDrivers(XENBUS_PROBE_PV_DISK | XENBUS_PROBE_PV_NET);

			/* Install the xensvc.  It will be automatically .*/
			/* started on reboot. Do it here because on 2000, */
			/* the reboot for xenblk and xennet will keep it */
			/* from being installed. */
			OutputDebugString(TEXT("**** Spawn xensvc from "));
			OutputDebugString(sourcePath);
			OutputDebugString(TEXT("  ****\n"));
			_spawnl(_P_WAIT, "xensvc", "xensvc", "install", NULL);
			advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);

			/* Update the registry to reflect the xennet date and */
			/* version being installed now.  If the install fails */
			/* or aborts, that's ok, the next time setup is run, */
			/* things will get fixed back up. */
			if (get_guid_drv_key(TEXT(XENNET_GUID),
					TEXT(XENNET_MATCHING_ID),
					&hkey_drv)) {
				RegCloseKey(hkey_drv);
				update_xennet_inf_reg();
			}

			/* If dpinst asks for a reboot, we will never come back so */
			/* do the cleanup here even though that leave a big hole. */
			cleanup_setup();

			OutputDebugString(TEXT("** Spawning dpinst xenblk and xennet *\n"));
			Err = install_driver(setup_flags);
			advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);
			swprintf_s(&debuggerString[0], sizeof(debuggerString),
				L"**** spawn dpinst return value = %x \n", Err);
			OutputDebugString(debuggerString);

			if (Err != (DWORD) -1 && !(Err & 0x80000000)) {
				OutputDebugString(TEXT("* Spawn OK for xenblk, xennet\n"));
				RebootRequired = TRUE;

				// if reboot required or a driver package copied to store
				// but not installed. i.e xennet. 
				/*
				if (Err & 0x40000000 || Err & 0x100 ) {
					OutputDebugString(TEXT("**** Reboot Required ***\n"));
					RebootRequired = TRUE;
				}
				*/

				/* xennet.sys didn't get copied, do that now. */
				restore_sys32_files(XENNET_F);
			}
			else {
				OutputDebugString(TEXT("* User canceled xenblk, xennet\n"));
				OutputDebugString(TEXT("* Uninstall XenBus ****\n"));

				/* Uninstall xenblk or xennet if needed. */
				_spawnl(_P_WAIT, "dpinst", "dpinst", "/q","/U",
					XENNET_INF, NULL);
				_spawnl(_P_WAIT, "dpinst", "dpinst", "/q","/U",
					XENBLK_INF, NULL);

				/* Uninstall xenbus */
				PathAppend(sourcePath, TEXT("xenbus"));
				SetCurrentDirectory(sourcePath);
				_spawnl(_P_WAIT, "dpinst", "dpinst", "/q","/U",
					XENBUS_INF, NULL);
				remove_xen_reg_entries(TEXT(ENUM_SYSTEM),
					TEXT(XENBUS_MATCHING_ID));

				/* Cleanup Files */
				sourcePath[pf_path_len] = '\0';
				SetCurrentDirectory(sourcePath);
				del_xen_files(setup_flags);
				DeleteInstallKey();
				usePvDrivers(0);
			}
		}
	}
	else { /* We are doing an upgrade. */
		update_xen(MediaRootDirectory, DirPathLength,
			sourcePath, sourcePathSize, setup_flags,
			hwnd_parent, hwnd_pbar, &RebootRequired);
	}
	destroy_progress_bar(hwnd_parent, hwnd_pbar);

	cleanup_setup();
	if (RebootRequired) {
		OutputDebugString(TEXT("**** Calling SetupPromptReboot ****\n"));

		if (!(setup_flags & NO_REBOOT_F)) {
			rebooting = FALSE;
			if (setup_flags & AUTO_REBOOT_F) {
				rebooting = xen_shutdown_system(TEXT("xen setup"),
					REBOOT_TIMEOUT, TRUE);
			}
			if (!rebooting) {
				if (SetupPromptReboot(NULL, NULL, FALSE) == -1)	{
					OutputDebugString(TEXT("** SetupPromptReboot FAILED **\n"));
					Err = GetLastError();
				}
			}
		}
	}

	OutputDebugString(TEXT("**** EXIT InstallXen ****\n"));

	return Err;
}

/******************************************************************************
	InstallXenBus
******************************************************************************/
static DWORD
WINAPI
InstallXenBus(__in LPCWSTR MediaRootDirectory)
{
	HINF	hInf;
	SP_DEVINFO_DATA DeviceInfoData;
	GUID ClassGUID;
	TCHAR sourcePath[MAX_PATH];
	TCHAR ClassName[MAX_CLASS_NAME_LEN];
	TCHAR hwIdList[LINE_LEN+4];
	TCHAR InstallSection[256];
	TCHAR *pInstallSection=InstallSection;
	LPCTSTR hwid = NULL;
	HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
	DWORD Err = NO_ERROR;

	OutputDebugString(TEXT("**** InstallXenBus ****\n"));
	hwid = TEXT("root\\xenbus");

	OutputDebugString(TEXT("**** STRING COPIES ****\n"));
	if (FAILED(StringCchCopy(InstallSection, 256, TEXT("XenBus_Inst")))) {
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	if (SUCCEEDED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			sourcePath))) {
		PathAppend(sourcePath, TEXT("Novell\\XenDrv\\xenbus\\xenbus.inf"));
	}
	else {
		OutputDebugString(TEXT("**** FAILED to get Path ****\n"));
		return (1);
	}

	/* List of hardware ID's must be double zero-terminated */
	ZeroMemory(hwIdList,sizeof(hwIdList));
	if (FAILED(StringCchCopy(hwIdList,LINE_LEN,hwid))) {
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	/* Use the INF File to extract the Class GUID. */
	OutputDebugString(TEXT("**** Calling SetupDiGetINFClass ****\n"));
	if (!SetupDiGetINFClass(sourcePath, &ClassGUID, ClassName,
			sizeof(ClassName)/sizeof(ClassName[0]),0)) {
		goto final;
	}

	/* Create the container for the to-be-created Device Information Element. */
	OutputDebugString(TEXT("**** Calling SetupDiCreateDeviceInfoList ****\n"));
	DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID,NULL);
	if (DeviceInfoSet == INVALID_HANDLE_VALUE) {
		goto final;
	}

	//
	// Now create the element.
	// Use the Class GUID and Name from the INF file.
	//

	OutputDebugString(TEXT("**** Calling SetupDiCreateDeviceInfo ****\n"));
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	if (!SetupDiCreateDeviceInfo(DeviceInfoSet,
			ClassName,
			&ClassGUID,
			NULL,
			NULL,
			DICD_GENERATE_ID,
			&DeviceInfoData)) {
		goto final;
	}

	/* Add the HardwareID to the Device's HardwareID property. */
	OutputDebugString(TEXT("** Calling SetupDiSetDeviceRegistryProperty **\n"));
	if (!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
			&DeviceInfoData,
			SPDRP_HARDWAREID,
			(LPBYTE)hwIdList,
			(lstrlen(hwIdList)+1+1)*sizeof(TCHAR)))	{
		goto final;
	}

	// Transform the registry element into an actual devnode
	// in the PnP HW tree.

	OutputDebugString(TEXT("* SetupDiCallClassInstaller DIF_REGISTERDEVICE\n"));

	if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,
			DeviceInfoSet,
			&DeviceInfoData)) {
		OutputDebugString(TEXT("**** SetupDiCallClassInstaller FAILED ****\n"));
		Err = GetLastError();
		goto final;
	}

final:

	OutputDebugString(TEXT("**** FINAL ****\n"));
	if (DeviceInfoSet != INVALID_HANDLE_VALUE) {

		OutputDebugString(TEXT("* Calling SetupDiDestroyDeviceInfoList *\n"));
		SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	}

	return Err;
}


/******************************************************************************
	createXenPVDKey
	
	creates key and sets value for xenpvdinstalled
	returns:  TRUE - Key created, FALSE - key wasn't created
******************************************************************************/
static BOOL
createXenPVDKey(DWORD xenpvdInstalled)
{
	HKEY hKey;
	BOOL ccode = FALSE;
	
	OutputDebugString(TEXT("**** ENTER CreateXenPVDKey ****\n"));

	if (ERROR_SUCCESS == RegCreateKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_S_NOVELL_XENPVD),
			0,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,
			NULL,
			&hKey,
			NULL)) {
		OutputDebugString(TEXT("**** Set Value xenpvd Installed ****\n"));
		RegSetValueEx(
			hKey,
			TEXT(VMDP_INSTALLED_VN),
			0,
			REG_DWORD,
			(PBYTE)&xenpvdInstalled,
			sizeof(xenpvdInstalled));

		RegCloseKey(hKey);
		ccode = TRUE;
	}
	OutputDebugString(TEXT("**** EXIT createXenPVDKey ****\n"));
	return(ccode);
}

/******************************************************************************
	usePvDrivers
	
	creates driver key and sets value for use_pv_drivers
	returns:  TRUE - Key created, FALSE - key wasn't created
******************************************************************************/
static BOOL
usePvDrivers(DWORD useDrivers)
{
	HKEY hKey;
	BOOL ccode = FALSE;

	OutputDebugString(TEXT("**** ENTER usePvDrivers ****\n"));

	if (ERROR_SUCCESS == RegCreateKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_SYS_PVDRIVERS),
			0,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,
			NULL,
			&hKey,
			NULL)) {
		OutputDebugString(TEXT("**** Set Value use_pv_drivers ****\n"));
		RegSetValueEx(
			hKey,
			TEXT(RKEY_SYS_PVDRIVERS_VN),
			0,
			REG_DWORD,
			(PBYTE)&useDrivers,
			sizeof(useDrivers));

		RegCloseKey(hKey);
		ccode = TRUE;
	}
	OutputDebugString(TEXT("**** EXIT usePvDrivers ****\n"));
	return(ccode);
}



/******************************************************************************
	DeleteInstallkey()
******************************************************************************/
static BOOL
DeleteInstallKey()
{
	HKEY hKey;
	BOOL ccode = FALSE;
	
	OutputDebugString(TEXT("**** ENTER DeleteInstallKey ****\n"));
	if (ERROR_SUCCESS == RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_S_NOVELL),
			0,
			KEY_ALL_ACCESS,
			&hKey)) {
		RegDeleteKey(hKey, TEXT(RKEY_INSTALL_FLAG));
		RegCloseKey(hKey);
		ccode = TRUE;
	}
	
	OutputDebugString(TEXT("**** EXIT DeleteInstallKey ****\n"));
	return(ccode);
}

static int
process_arglist(int argc, LPWSTR *argv, DWORD *setup_flags)
{
	int i;
	int cc;

	*setup_flags = 0;
	cc = 1;
	for (i = 1; i < argc; i++) {
		if (_wcsnicmp(argv[i], TEXT(AUTO_REBOOT),
				lstrlen(TEXT(AUTO_REBOOT)))	== 0) {
			if (*setup_flags & NO_REBOOT_F) {
				*setup_flags = 0;
				cc = 0;
				break;
			}
			*setup_flags |= AUTO_REBOOT_F;
		}
		else if (_wcsnicmp(argv[i], TEXT(NO_REBOOT),
				lstrlen(TEXT(NO_REBOOT))) == 0) {
			if (*setup_flags & AUTO_REBOOT_F) {
				*setup_flags = 0;
				cc = 0;
				break;
			}
			*setup_flags |= NO_REBOOT_F;
		}
		else if (_wcsnicmp(argv[i], TEXT(EULA_ACCEPTED),
				lstrlen(TEXT(EULA_ACCEPTED))) == 0) {
			*setup_flags |= EULA_ACCEPTED_F;
		}
		else if (_wcsnicmp(argv[i], TEXT(FORCE_INSTALL),
				lstrlen(TEXT(FORCE_INSTALL))) == 0) {
			*setup_flags |= FORCE_INSTALL_F;
		}
		else if (_wcsnicmp(argv[i], TEXT(SETUP_HELP),
				lstrlen(TEXT(SETUP_HELP))) == 0) {
			*setup_flags = 0;
			cc = 0;
			break;
		}
		else {
			*setup_flags = 0;
			cc = 0;
			break;
		}
	}
	return cc;
}

static void
update_xennet_inf_reg(void)
{
	HKEY hkey_class;
	HKEY hkey_guid;
	HKEY hkey_drv;
	FILE *fp;
	TCHAR tver[MAX_VER_LEN];
	TCHAR tdate[MAX_DATE_LEN];
	TCHAR key_name[MAX_DRV_LEN];   
	TCHAR reg_matching_id[MAX_MATCHING_ID_LEN];   
	char line[MAX_LINE_LEN];
	char ver[MAX_VER_LEN];
	char date[MAX_DATE_LEN];
	char *buf;
	FILETIME ft;
	size_t s, e;
	DWORD i;
	DWORD len;
	int day, month, year;
	WORD wdate;

	if ((fp = fopen(XENNET_INF, "r" )) == NULL)
		return;

	while(fgets(line, MAX_LINE_LEN - 1, fp) != NULL) {
		if (_strnicmp(line, "DriverVer", strlen("DriverVer")) == 0) {
			break;
		}
	}
	fclose(fp);

	if (_strnicmp(line, "DriverVer", strlen("DriverVer")) != 0)
		return;

	/* scan 1 past the = */
	s = strcspn(line, "=") + 1;

	/* eat whith space */
	s += strspn(&line[s], " \t");

	/* scan to end of the date */
	e = strcspn(&line[s], " ,\t\n");
	if (e > ACTUAL_DATE_LEN) {
		return;
	}
	StringCchCopyNA(date, sizeof(date), &line[s], e);
	date[e] = '\0';

	/* scan to the beginnign of the version */
	s += e + 1;
	s += strspn(&line[s], ", \t\n");

	/* scan to the end of the version */
	e = strcspn(&line[s], " \t\n;");
	if (e > sizeof(ver)) {
		return;
	}
	StringCchCopyNA(ver, sizeof(ver), &line[s], e);
	ver[e] = '\0';

	/* replace the / with - in the date and get numeric value */
	/* of the date components */
	s = strcspn(date, "/");
	date[s] = '\0';
	sscanf(date, "%d", &month);
	date[s] = '-';
	s++;
	e = s + strcspn(&date[s], "/");
	date[e] = '\0';
	sscanf(&date[s], "%d", &day);
	date[e] = '-';
	e++;
	sscanf(&date[e], "%d", &year);

	wdate = ((year - BASE_YEAR) << YEAR_SHIFT)
		| (month << MONTH_SHIFT)
		| day;
	DosDateTimeToFileTime(wdate, 0, &ft);

	memset(tdate, 0, sizeof(tdate));
	memset(tver, 0, sizeof(tver));
	buf = (char *)tdate;
	for (s = 0, e = 0; s < strlen(date); s++, e += 2) {
		buf[e] = date[s];
	}
	buf = (char *)tver;
	for (s = 0, e = 0; s < strlen(ver); s++, e += 2) {
		buf[e] = ver[s];
	}

	if (RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_CLASS),
			0,
			KEY_ALL_ACCESS,
			&hkey_class) != ERROR_SUCCESS ) {
		return;
	}

	/* Open the requested GUID key. */
	if (RegOpenKeyEx(hkey_class,
			TEXT(XENNET_GUID),
			0,
			KEY_ALL_ACCESS,
			&hkey_guid)	== ERROR_SUCCESS ) {

		/* Enumerate all the keys in the GUID. */
		len = MAX_DRV_LEN;
		i = 0;
		while (RegEnumKeyEx(hkey_guid, i, key_name, &len, 
				NULL, NULL, NULL, NULL)	== ERROR_SUCCESS) {
			/* Open the enumerate key. */
			if (RegOpenKeyEx(hkey_guid,
					key_name,
					0,
					KEY_ALL_ACCESS,
					&hkey_drv) == ERROR_SUCCESS ) {
				len = sizeof(reg_matching_id);
				if (RegQueryValueEx(hkey_drv,
						TEXT("MatchingDeviceId"),
						NULL,
						NULL,
						(LPBYTE)reg_matching_id,
						&len) == ERROR_SUCCESS) {
					if (lstrcmpi(reg_matching_id, TEXT(XENNET_MATCHING_ID))
							== 0) {
						RegSetValueEx(
							hkey_drv,
							TEXT("DriverDate"),
							0,
							REG_SZ,
							(BYTE *)tdate,
							lstrlen(tdate) * sizeof(TCHAR));
						RegSetValueEx(
							hkey_drv,
							TEXT("DriverVersion"),
							0,
							REG_SZ,
							(BYTE *)tver,
							lstrlen(tver) * sizeof(TCHAR));
						RegSetValueEx(
							hkey_drv,
							TEXT("DriverDateData"),
							0,
							REG_BINARY,
							(PBYTE)&ft,
							sizeof(ft));
					}
				}
				RegCloseKey(hkey_drv);
			}
			len = MAX_DRV_LEN;
			i++;
		}
		RegCloseKey(hkey_guid);
	}
	RegCloseKey(hkey_class);
}

static DWORD
is_setup_running(void)
{
	HKEY hkey;
	DWORD val;
	DWORD len;
	DWORD cc;

	if (ERROR_SUCCESS != RegCreateKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_S_NOVELL_XENPVD),
			0,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,
			NULL,
			&hkey,
			NULL)) {
		return 0;
	}
	len = sizeof(val);
	if(ERROR_SUCCESS == RegQueryValueEx(hkey,
			TEXT(SETUP_RUNNIN_VN),
			NULL,
			NULL,
			(LPBYTE)&val,
			&len)) {
		cc = 1;
	}
	else {
		val = 1;
		RegSetValueEx(
			hkey,
			TEXT(SETUP_RUNNIN_VN),
			0,
			REG_DWORD,
			(PBYTE)&val,
			sizeof(val));
		cc = 0;
	}
	RegCloseKey(hkey);
	return cc;
}

static DWORD
is_vmdp_installed(void)
{
	HKEY hkey;
	DWORD val;
	DWORD len;
	DWORD cc = 0;

	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			TEXT(RKEY_S_NOVELL_XENPVD),
			0,
			KEY_ALL_ACCESS,
			&hkey)) {
		len = sizeof(val);
		if(ERROR_SUCCESS == RegQueryValueEx(hkey,
				TEXT(VMDP_INSTALLED_VN),
				NULL,
				NULL,
				(LPBYTE)&val,
				&len)) {
			cc = 1;
		}
		RegCloseKey(hkey);
	}
	return cc;
}

void
cleanup_setup(void)
{
	HKEY hkey;

	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			TEXT(RKEY_S_NOVELL_XENPVD),
			0,
			KEY_ALL_ACCESS,
			&hkey)) {
		RegDeleteValue(hkey, TEXT(SETUP_RUNNIN_VN));
		RegCloseKey(hkey);
	}
}

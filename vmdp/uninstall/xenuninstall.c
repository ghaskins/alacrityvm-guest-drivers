/*****************************************************************************
 *
 *   File Name:      xenuninstall.c
 *   Created by:     WTT
 *   Date created:   01/26/07
 *
 *   %version:       21 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 09:49:40 2009 %
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

	Uninstall utillity for removing the XenBus, XenBlk, and XenNet driver
	from a windows system.

--*/

#include "xenuninstall.h"
#include <shlwapi.h>
#pragma hdrstop

/******************************************************************************
	Constants
******************************************************************************/

#if defined(_IA64_)
    #define XEN_UNINSTALL_PLATFORM_SUBDIRECTORY L"ia64"
#elif defined(_X86_)
    #define XEN_UNINSTALL_PLATFORM_SUBDIRECTORY L"i386"
#elif defined(_AMD64_)
    #define XEN_UNINSTALL_PLATFORM_SUBDIRECTORY L"amd64"
#else
#error Unsupported platform
#endif

#define XEN_UNINSTALL_PLATFORM_SUBDIRECTORY_SIZE (sizeof(XEN_UNINSTALL_PLATFORM_SUBDIRECTORY) / sizeof(WCHAR))

/******************************************************************************
	Globals
******************************************************************************/

/******************************************************************************
	Function prototypes
******************************************************************************/

static void CleanUpRegKeys(void);
static void CleanUpRegKeyForService(HKEY *hkey);
static void CleanUpDriverKey(HKEY *driverControlKey);
static void DisableXenDrivers(void);
static void RemoveXenService(WCHAR *path);
static void CleanUpInstallFlag(void);
static BOOL ScanForHardwareChanges(void);
static BOOL TestInstallFlag(void);
static DWORD WINAPI	UnInstallXen(DWORD);
static DWORD WINAPI	UnInstallXenDrivers(void);
static int process_arglist(int argc, LPWSTR *argv,
	DWORD *uninstall_flags, DWORD *phase);

HINSTANCE g_hInstance;

LRESULT MainWndProc(HWND h, UINT i, WPARAM w, LPARAM l)
{
	return DefWindowProc(h, i, w, l);
}

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
	LPWSTR *ArgList;
	INT NumArgs;
	WCHAR MediaRootDirectory[MAX_PATH];
	LPWSTR FileNamePart;
	DWORD DirPathLength;
	DWORD retVal = ERROR_SUCCESS;
	DWORD uninstallPhase;
	DWORD uninstall_flags;

	LPCTSTR runOnceString;
	WCHAR FullPathAndName[MAX_PATH];
	WCHAR srcPath[MAX_PATH];

	size_t srcPathSize = 0;
	size_t programPathSize = 0;

	HKEY hKey;
	DWORD xenpvdUnInstalled;
	DWORD ok_to_remove = 0;
	HWND hwnd_parent = NULL;
	HWND hwnd_pbar = NULL;
	BOOL rebooting;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nShowCmd);

	g_hInstance = hInstance;

	if ((retVal = xen_matches_os()) != MATCH_OS_SUCCESS) {
		if (retVal == MATCH_OS_MISMATCH)
			CreateMessageBox(MBOX_TITLE, MBOX_NON_MATCHING_OS);
		else if (retVal == MATCH_OS_UNSUPPORTED)
			CreateMessageBox(MBOX_TITLE, MBOX_UNSUPPORTED_OS);
		return 0;
	}

	//
	// Retrieve the full directory path from which our setup program was
	// invoked.
	//
	ArgList = CommandLineToArgvW(GetCommandLine(), &NumArgs);
	if(ArgList && (NumArgs >= 1)) {
		DirPathLength = GetFullPathName(ArgList[0],
			MAX_PATH,
			MediaRootDirectory,
			&FileNamePart);
		if(DirPathLength >= MAX_PATH) {
			//
			// The directory is too large for our buffer.  Set our directory
			// path length to zero so we'll simply bail out in this rare case.
			//
			DirPathLength = 0;
		}
		if(DirPathLength) {
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
        if(DirPathLength >= MAX_PATH) {
			//
			// The current directory is too large for our buffer.  Set our
			// directory path length to zero so we'll simply bail out in this
			// rare case.
			//
			DirPathLength = 0;
		}
		if(DirPathLength) {
			//
			// Ensure that path ends in a path separator character.
			//
			if((MediaRootDirectory[DirPathLength-1] != L'\\') &&
					(MediaRootDirectory[DirPathLength-1] != L'/')) {
                MediaRootDirectory[DirPathLength++] = L'\\';
                if(DirPathLength < MAX_PATH) {
                    MediaRootDirectory[DirPathLength] = L'\0';
                } else {
					//
					// Not enough room in buffer to add path separator char
					//
					DirPathLength = 0;
				}
			}
		}
	}

	uninstall_flags = 0;
	uninstallPhase = 0;
	if(NumArgs >= 2)
	{
		OutputDebugString(TEXT("**** Process ArgList ****\n"));
		if (!process_arglist(NumArgs, ArgList,
				&uninstall_flags, &uninstallPhase)) {
			CreateMessageBox(MBOX_TITLE, MBOX_UNINSTALL_HELP);
			GlobalFree(ArgList);
			return 0;
		}

		if (uninstallPhase == UNINSTALL_PHASE_2) {
			GlobalFree(ArgList);
			return 0;
		}
	}

	if(ArgList) {
		GlobalFree(ArgList);
	}

	if(!DirPathLength) {
		//
		// Couldn't figure out what the root directory of our installation
		// media was.  Bail out.
		//
		return 0;
	}

	//
	// If we're being invoked from a platform-specific subdirectory (i.e.,
	// \i386 or \ia64), then strip off that subdirectory to get the true media
	// root path.
	//
	if(DirPathLength > XEN_UNINSTALL_PLATFORM_SUBDIRECTORY_SIZE) {
		//
		// We know that the last character in our MediaRootDirectory string is
		// a path separator character.  Check to see if the preceding
		// characters match our platform-specific subdirectory.
		//
		if(!_wcsnicmp(
			&(MediaRootDirectory[
				DirPathLength - XEN_UNINSTALL_PLATFORM_SUBDIRECTORY_SIZE]),
			XEN_UNINSTALL_PLATFORM_SUBDIRECTORY,
			XEN_UNINSTALL_PLATFORM_SUBDIRECTORY_SIZE - 1)) {
			//
			// Platform-specific part matches, just make sure preceding char
			// is a path separator char.
			//
			if((MediaRootDirectory[DirPathLength
					- XEN_UNINSTALL_PLATFORM_SUBDIRECTORY_SIZE - 1] == L'\\')
				|| (MediaRootDirectory[DirPathLength
					- XEN_UNINSTALL_PLATFORM_SUBDIRECTORY_SIZE - 1] == L'/')) {
				MediaRootDirectory[DirPathLength
					- XEN_UNINSTALL_PLATFORM_SUBDIRECTORY_SIZE] = L'\0';
			}
		}
	}

	if(SUCCEEDED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			srcPath))) {
		StringCchLength(srcPath, MAX_PATH, &programPathSize);
		PathAppend(srcPath, TEXT("Novell\\XenDrv"));
	}
	else {
		OutputDebugString(TEXT("**** WinMain - FAILED to get Path ****\n"));
		return (1);
	}

	if (uninstallPhase == UNINSTALL_PHASE_0) {
		if (!TestInstallFlag())	{
			CreateMessageBox(MBOX_TITLE, MBOX_NOUNINSTALL_TEXT);
			return(0);
		}

		if (_wcsnicmp(srcPath, MediaRootDirectory, programPathSize) == 0) {
			/* Since uninstall was launched from the "Program Files" dir */
			/* we can go ahead and uninstall everyting now.  This will */
			/* cause the LAN to stop working but since we are on the local */
			/* drive, we should be ok.  A reboot will be required. */
			ok_to_remove = 1;
		}
		else {
			if (!CreateOkCancelMessageBox(MBOX_TITLE, MBOX_UNINSTALL_TEXT))
				return(0);
		}

		OutputDebugString(TEXT("**** Calling DisableXenDrivers ****\n"));
		if (create_progress_bar(&hwnd_parent, &hwnd_pbar, PB_INTERVALS)) {
			uninstall_flags |= SHOW_PROGRESS_F;
			advance_progress_bar(hwnd_parent, hwnd_pbar, uninstall_flags);
		}
		DisableXenDrivers();

		advance_progress_bar(hwnd_parent, hwnd_pbar, uninstall_flags);
		RemoveXenService(srcPath);

		advance_progress_bar(hwnd_parent, hwnd_pbar, uninstall_flags);
		update_xen_cleanup();

		PathAppend(srcPath, TEXT("uninstall.exe"));
		if (ok_to_remove) {
			advance_progress_bar(hwnd_parent, hwnd_pbar, uninstall_flags);
			UnInstallXen(uninstall_flags);
			StringCchCat(srcPath, MAX_PATH, TEXT(UNINSTALL_N));
		}
		else {
			/* Since we didn't actually uninstall, set the /u option. */
			StringCchCat(srcPath, MAX_PATH, TEXT(UNINSTALL_U));
		}
		advance_progress_bar(hwnd_parent, hwnd_pbar, uninstall_flags);
		destroy_progress_bar(hwnd_parent, hwnd_pbar);

		/* Setup the RunOnce. */
		StringCchLength( srcPath, MAX_PATH, &srcPathSize);
		if(ERROR_SUCCESS == RegOpenKeyEx(
				HKEY_LOCAL_MACHINE,
				TEXT(RKEY_S_RUNONCE),
				0,
				KEY_ALL_ACCESS,
				&hKey)) {
			RegSetValueEx(
					hKey,
					TEXT(RKEY_S_RUNONCE_VN),
					0,
					REG_SZ,
					(PBYTE)&srcPath,
					srcPathSize * sizeof(WCHAR));
			RegCloseKey(hKey);
		}

		/* Always reboot when uninstalling bucause the netowrk is lost. */
		rebooting = FALSE;
		if (uninstall_flags & AUTO_REBOOT_F) {
			rebooting = xen_shutdown_system(TEXT("xen uninstall"),
				REBOOT_TIMEOUT, TRUE);
		}
		if (!rebooting) {
			//CreateMessageBox(MBOX_TITLE, MBOX_FILE_CLEANUP_TEXT);
			if(SetupPromptReboot(NULL, NULL, FALSE) == -1) {
				OutputDebugString(TEXT("**** SetupPromptReboot FAILED ****\n"));
				retVal = GetLastError();
			}
		}
	}
	else if (uninstallPhase == UNINSTALL_PHASE_1) {
		OutputDebugString(TEXT("**** Calling UnInstallXen ****\n"));
		UnInstallXen(uninstall_flags);

		/* We don't have to reboot. Everything is uninstalled, but we will */
		/* setup the RunOnce. We will set the /n option. We just need */
		/* to protect from installing until we reboot gets to deleted files. */
		PathAppend(srcPath, TEXT("Novell\\XenDrv\\uninstall.exe"));
		StringCchCat(srcPath, MAX_PATH, TEXT(UNINSTALL_N));
		StringCchLength( srcPath, MAX_PATH, &srcPathSize);
		if(ERROR_SUCCESS == RegOpenKeyEx(
				HKEY_LOCAL_MACHINE,
				TEXT(RKEY_S_RUNONCE),
				0,
				KEY_ALL_ACCESS,
				&hKey)) {
			RegSetValueEx(
					hKey,
					TEXT(RKEY_S_RUNONCE_VN),
					0,
					REG_SZ,
					(PBYTE)&srcPath,
					srcPathSize * sizeof(WCHAR));
			RegCloseKey(hKey);
		}
		CreateMessageBox(MBOX_TITLE, MBOX_FILE_CLEANUP_TEXT);
	}

   return(0);
}

/******************************************************************************
	DisableXenDrivers
******************************************************************************/
static void
DisableXenDrivers(void)
{
	HKEY hKey;
	DWORD xenpvdInstalled = 0;

	if(ERROR_SUCCESS == RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_SYS_PVDRIVERS),
			0,
			KEY_ALL_ACCESS,
			&hKey)) {
		RegSetValueEx(
			hKey,
			TEXT(RKEY_SYS_PVDRIVERS_VN),
			0,
			REG_DWORD,
			(PBYTE)&xenpvdInstalled,
			sizeof(xenpvdInstalled));

			RegCloseKey(hKey);
	}
}

/******************************************************************************
	RemoveXenService
******************************************************************************/
static void
RemoveXenService(WCHAR *path)
{
	SetCurrentDirectory(path);

	/* Stop and remove the xensvc. */
	OutputDebugString(TEXT("**** Spawn xensvc remove from "));
	OutputDebugString(path);
	OutputDebugString(TEXT("  ****\n"));
	_spawnl(_P_WAIT, "xensvc", "xensvc", "remove", NULL);
}


/******************************************************************************
	CleanUpInstallFlag
******************************************************************************/
static void
CleanUpInstallFlag(void)
{
	HKEY hKey;
	DWORD xenpvdInstalled = 0;

	if(ERROR_SUCCESS == RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_S_NOVELL),
			0,
			KEY_ALL_ACCESS,
			&hKey)) {
		RegDeleteKey(hKey,	TEXT(RKEY_INSTALL_FLAG));

		RegCloseKey(hKey);
	}
}

/******************************************************************************
	TestInstallFlag

	Returns:	TRUE - Install Flag key is present 
				FALSE - Install Flag Key is not present
******************************************************************************/
static BOOL
TestInstallFlag(void)
{
	HKEY hKey;
	DWORD xenpvdInstalled = 0;

	if(ERROR_SUCCESS == RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_S_NOVELL_XENPVD),
			0,
			KEY_ALL_ACCESS,
			&hKey)) {
		return(TRUE);
	}
	return(FALSE);
}



/******************************************************************************
	UnInstallXen
******************************************************************************/

static DWORD
WINAPI
UnInstallXen(DWORD flags)
{
	DWORD retVal = ERROR_SUCCESS;
	DWORD retVal1 = ERROR_SUCCESS;
	HKEY hKey;
	HKEY hKey1;
	DWORD xenpvdInstalled;

	OutputDebugString(TEXT("**** ENTER UnInstallXen ****\n"));
	OutputDebugString(TEXT("**** Calling UnInstallDrivers ****\n"));

	retVal = UnInstallXenDrivers();

	OutputDebugString(TEXT("**** Returned From UnInstallDrivers ****\n"));

	// do these prior to Scan since it might not come back from scan
	CleanUpInstallFlag();
	del_xen_files(flags);
	delete_sys32_files(XENBUS_F | XENBLK_F | XENNET_F);

	/* Don't do ScanForHardwareChanges(); What's the benifit?  Can cause */
	/* system to crash. */
	remove_xen_reg_entries(TEXT(ENUM_SYSTEM), TEXT(XENBUS_MATCHING_ID));

	/* It seems best not to do this for xenblk and xennt. */
	/* Not doing xenblk sometimes keeps the system from prompting for */
	/* unknown hardware.  Not doing xennet keesp the system from asking */
	/* to reboot after a reinstall. */
	//remove_xen_reg_entries(TEXT(ENUM_PCI), TEXT(XENBLK_MATCHING_ID));
	//remove_xen_reg_entries(TEXT(ENUM_XEN), TEXT(XENNET_MATCHING_ID));

	OutputDebugString(TEXT("**** Exiting UnInstallXen ****\n"));

	return(retVal);
}

/******************************************************************************
	UnInstallXenDrivers
******************************************************************************/

static DWORD
WINAPI
UnInstallXenDrivers(void)
{
	DWORD retVal = ERROR_SUCCESS;
	DWORD numOfChars = 1024;
	TCHAR destInfPath[MAX_PATH];
	TCHAR srcPath[MAX_PATH];
	size_t srcPathSize = 0;
	BOOL    RebootRequired;

	OutputDebugString(TEXT("**** ENTER UnInstallXenDrivers ****\n"));

	if(SUCCEEDED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			srcPath))) {
		PathAppend(srcPath, TEXT("Novell\\XenDrv"));
		StringCchLength( srcPath, MAX_PATH, &srcPathSize);
	}
	else {
		OutputDebugString(TEXT("**** FAILED to get Path ****\n"));
		return (1);
	}


	OutputDebugString(TEXT("* Calling DriverPackageUninstall for XenBus *\n"));
	restore_sys32_files(XENBUS_F | XENBLK_F | XENNET_F);
	srcPath[srcPathSize] = '\0';
	numOfChars = 1024;
	PathAppend(srcPath, TEXT("xenbus"));
	SetCurrentDirectory(srcPath);
	retVal = (DWORD)_spawnl(_P_WAIT, "dpinst", "dpinst", "/q", "/d", "/u",
		XENBUS_INF, NULL);

	OutputDebugString(TEXT("* Calling DriverPackageUninstall for XenBlk *\n"));
	srcPath[srcPathSize] = '\0';
	SetCurrentDirectory(srcPath);
	numOfChars = 1024;
	retVal = (DWORD)_spawnl(_P_WAIT, "dpinst", "dpinst", "/q", "/d", "/u",
		XENBLK_INF, NULL);

	OutputDebugString(TEXT("* Calling DriverPackageUninstall for XenNet *\n"));
	srcPath[srcPathSize] = '\0';
	numOfChars = 1024;
	retVal = (DWORD)_spawnl(_P_WAIT, "dpinst", "dpinst", "/q", "/d", "/u",
		XENNET_INF, NULL);

	OutputDebugString(TEXT("**** Exiting UnInstallXenDrivers ****\n"));
    return(retVal);
}

/******************************************************************************
	CleanUpRegKeys
******************************************************************************/
static void
CleanUpRegKeys(void)
{
	HKEY hKey;
	DWORD	count=0;
	DWORD nextInstance=1; 
	DWORD size;

	OutputDebugString(TEXT("**** Entering CleanUpRegKeys ****\n"));

	// if key is not there nothing to do since the uninstall already cleaned them
	if(ERROR_SUCCESS == RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT("SYSTEM\\CurrentControlSet\\Services\\xenblk\\Enum"),
			0,
			KEY_ALL_ACCESS,
			&hKey)) {
		CleanUpRegKeyForService(&hKey);
		RegDeleteKey(hKey, NULL);
	}
	if(ERROR_SUCCESS == RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT("SYSTEM\\CurrentControlSet\\Services\\xenbus\\Enum"),
			0,
			KEY_ALL_ACCESS,
			&hKey)) {
		CleanUpRegKeyForService(&hKey);
		RegDeleteKey(hKey, NULL);
	}
	OutputDebugString(TEXT("**** Exiting CleanUpRegKeys ****\n"));
}

/******************************************************************************
	CleanUp
******************************************************************************/
static void
CleanUpRegKeyForService(HKEY *hKey)
{
	HKEY driverControlKey;
	HKEY enumKey;
	DWORD	count=0;
	DWORD nextInstance=1; 
	DWORD size;
	WCHAR enumKeyValueName[MAX_PATH];
	

	OutputDebugString(TEXT("**** Entering CleanUpRegKeyForService ****\n"));

	// locate enum key link starting at NextInstance - count. for count of enums
	//TODO: derive value name from NexInstace-count and then do for each enum
	// for now hard code value name for proof of concept.

	size = sizeof(enumKeyValueName);

	if (ERROR_SUCCESS == RegQueryValueEx(
			*hKey,
			TEXT("0"),
			NULL,
			NULL,
			(LPBYTE)&enumKeyValueName[0],
			(LPDWORD)&size)) {
		// get to driver control key
		if(ERROR_SUCCESS == RegOpenKeyEx(
				HKEY_LOCAL_MACHINE,
				TEXT("SYSTEM\\CurrentControlSet\\Enum"),
				0,
				KEY_ALL_ACCESS,
				&enumKey)) {
			OutputDebugString(TEXT("* SYSTEM\\CurrentControlSet\\Enum *\n"));
		
			if(ERROR_SUCCESS == RegOpenKeyEx(
					enumKey,
					enumKeyValueName,
					0,
					KEY_ALL_ACCESS,
					&driverControlKey)) {
				OutputDebugString(TEXT("* Opened enumKeyValueName *n"));
				// locate driver key in currentcontrolset/enum
				CleanUpDriverKey(&driverControlKey);
				RegDeleteKey(driverControlKey, NULL);
			}
			RegCloseKey(enumKey);
		}
	}
	OutputDebugString(TEXT("**** Exiting CleanUpRegKeyForService ****\n"));

}

/******************************************************************************
	CleanUpDriverKey
	hKey * driverControlKey - HKEY_LOCALMACHINE\SYSTEM\CurrentControlSet\<bus\pciid\value>
******************************************************************************/
static void
CleanUpDriverKey(HKEY *driverControlKey)
{
	WCHAR driverKeyValueName[MAX_PATH];
	HKEY driverClassKey;			  
	HKEY driverKey;			  
	DWORD size;
	

	OutputDebugString(TEXT("**** Entering CleanUpDriverKey ****\n"));

	// locate driver key in currentcontrolset/enum
	size = sizeof(driverKeyValueName);
	if (ERROR_SUCCESS == RegQueryValueEx(
			*driverControlKey,
			TEXT("Driver"),
			NULL,
			NULL,
			(LPBYTE)&driverKeyValueName[0],
			(LPDWORD)&size)) {
		OutputDebugString(TEXT("*** Success getting driverkeyvaluename ***\n"));
		// remove driver key
		if(ERROR_SUCCESS == RegOpenKeyEx(
				HKEY_LOCAL_MACHINE,
				TEXT("SYSTEM\\CurrentControlSet\\Class"),
				0,
				KEY_ALL_ACCESS,
				&driverClassKey)) {
			RegDeleteKey(driverClassKey, driverKeyValueName);
			RegCloseKey(driverClassKey);		
		}
	}
	OutputDebugString(TEXT("**** Exiting CleanUpDriverKey ****\n"));
}

#if 0
/******************************************************************************
	ScanForHardwareChanges
******************************************************************************/
static BOOL
ScanForHardwareChanges(void) 
{
	DEVINST     devInst;
	CONFIGRET   status;
    
	// 
	// Get the root devnode.
	// 
    
	status = CM_Locate_DevNode(&devInst, NULL, CM_LOCATE_DEVNODE_NORMAL);
	if (status != CR_SUCCESS) {
		return FALSE;
	}
    
	status = CM_Reenumerate_DevNode(devInst, 0);
	if (status != CR_SUCCESS) {
		return FALSE;
	}

	return TRUE;
}
#endif

static int
process_arglist(int argc, LPWSTR *argv, DWORD *uninstall_flags, DWORD *phase)
{
	int i;
	int cc;

	*uninstall_flags = 0;
	*phase = UNINSTALL_PHASE_0;
	cc = 1;
	for (i = 1; i < argc; i++) {
		if (_wcsnicmp(argv[i], TEXT(AUTO_REBOOT_SWITCH),
			lstrlen(TEXT(AUTO_REBOOT_SWITCH)))	== 0) {
			*uninstall_flags |= AUTO_REBOOT_F;
		}
		else if (_wcsnicmp(argv[i], TEXT(UNINSTALL_U_SWITCH),
			lstrlen(TEXT(UNINSTALL_U_SWITCH))) == 0) {
			*phase = UNINSTALL_PHASE_1;
		}
		else if (_wcsnicmp(argv[i], TEXT(UNINSTALL_N_SWITCH),
			lstrlen(TEXT(UNINSTALL_N_SWITCH))) == 0) {
			*phase = UNINSTALL_PHASE_2;
		}
		else if (_wcsnicmp(argv[i], TEXT(UNINSTALL_HELP),
			lstrlen(TEXT(UNINSTALL_HELP))) == 0) {
			*uninstall_flags = 0;
			*phase = 0;
			cc = 0;
			break;
		}
		else {
			*uninstall_flags = 0;
			*phase = 0;
			cc = 0;
			break;
		}
	}
	return cc;
}
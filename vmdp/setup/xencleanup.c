/*****************************************************************************
 *
 *   File Name:      xencleanup.c
 *   Created by:     KRA
 *   Date created:   12/09/08
 *
 *   %version:       8 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 09:40:24 2009 %
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

#ifdef XEN_SETUP_EXE
#include "xensetup.h"
#else
#include "xenuninstall.h"
#endif
#include <shlwapi.h>
#include <stdio.h> //for sprintf for debugging

extern HINSTANCE g_hInstance;

pv_info_t pv_info = {0};

void
clear_pv_info(void)
{
	memset(&pv_info, 0, sizeof(pv_info_t));
}

void
get_reinstall_info(HKEY hkey, TCHAR *matching_desc, TCHAR *reinstall)
{
	HKEY hkey_enum;
	TCHAR enum_n[MAX_REINSTALL_LEN];   
	TCHAR desc[MAX_DESC_LEN];   
	DWORD i;
	DWORD len;
	DWORD found;

	/* Enumerate all the keys in the GUID. */
	reinstall[0] = '\0';
	found = 0;
	len = MAX_REINSTALL_LEN;
	i = 0;
	while (!found && RegEnumKeyEx(hkey, i, enum_n, &len, 
			NULL, NULL, NULL, NULL)	== ERROR_SUCCESS) {
			/* Open the enumerate driver key. */
		if (RegOpenKeyEx(hkey,
				enum_n,
				0,
				KEY_ALL_ACCESS,
				&hkey_enum) == ERROR_SUCCESS ) {
			len = sizeof(desc);
			if (RegQueryValueEx(hkey_enum,
					TEXT("DeviceDesc"),
					NULL,
					NULL,
					(LPBYTE)desc,
					&len) == ERROR_SUCCESS) {
				if (_wcsnicmp(desc, matching_desc, lstrlen(matching_desc))
						== 0) {
					wcsncpy_s(reinstall, lstrlen(enum_n) + 1,
						enum_n, lstrlen(enum_n) + 1);
					found = 1;
				}
			}
			RegCloseKey(hkey_enum);
		}
		len = MAX_REINSTALL_LEN;
		i++;
	}
}

void
get_inf_info(HKEY hkey_class, TCHAR *guid, TCHAR *matching_id, TCHAR *inf)
{
	HKEY hkey_guid;
	HKEY hkey_drv;
	TCHAR key_name[MAX_DRV_LEN];   
	TCHAR reg_matching_id[MAX_MATCHING_ID_LEN];   
	DWORD i;
	DWORD len;
	DWORD found;

	/* Open the requested GUID key. */
	if (RegOpenKeyEx(hkey_class,
			guid,
			0,
			KEY_ALL_ACCESS,
			&hkey_guid)	== ERROR_SUCCESS ) {

		/* Enumerate all the keys in the GUID. */
		found = 0;
		len = MAX_DRV_LEN;
		i = 0;
		while (!found && RegEnumKeyEx(hkey_guid, i, key_name, &len, 
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
					if (lstrcmpi(reg_matching_id, matching_id) == 0) {
						found = 1;
						len = MAX_INF_LEN * sizeof(TCHAR);
						if (RegQueryValueEx(hkey_drv,
							 TEXT("InfPath"),
							 NULL,
							 NULL,
							 (LPBYTE)inf,
							 &len) != ERROR_SUCCESS) {
							inf[0] = '\0';
						}
					}
				}
				RegCloseKey(hkey_drv);
			}
			len = MAX_DRV_LEN;
			i++;
		}
		RegCloseKey(hkey_guid);
	}
}

static void
del_file(TCHAR *path, DWORD path_len, TCHAR *file)
{
	DWORD err;

	PathAppend(path, file);
	SetFileAttributes(path, FILE_ATTRIBUTE_NORMAL);
	if (!DeleteFile(path)) {
		err = GetLastError();
		if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND) {
			/* If the file is no there, don't mark for deletion. */
			MoveFileEx(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
		}
	}
	path[path_len] = '\0';
}

DWORD
del_xen_files(DWORD flags)
{

	TCHAR src_path[MAX_PATH];
	TCHAR cur_path[MAX_PATH];
	DWORD Err = NO_ERROR;
	DWORD delayed_remove = 0;
	size_t src_path_len = 0;

	OutputDebugString(TEXT("**** Enter DeleteXenFiles ****\n"));
	if (SUCCEEDED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			src_path))) {
		PathAppend(src_path, TEXT("Novell\\XenDrv"));
		StringCchLength( src_path, MAX_PATH, &src_path_len);
	}
	else {
		OutputDebugString(TEXT("** DeleteXenFiles - FAILED to get Path **\n"));
		return (1);
	}

	OutputDebugString(TEXT("**** Delete Files from the XenDrv dir. ****\n"));
	del_file(src_path, src_path_len, TEXT("xenbus\\xenbus.sys"));
	del_file(src_path, src_path_len, TEXT("xenbus\\xenbus.inf"));
	del_file(src_path, src_path_len, TEXT("xenbus\\xenbus.cat"));
	del_file(src_path, src_path_len, TEXT("xenbus\\dpinst.exe"));
	del_file(src_path, src_path_len, TEXT("xenbus\\difxapi.dll"));
	del_file(src_path, src_path_len, TEXT("setup.exe"));
	del_file(src_path, src_path_len, TEXT("uninstall.exe"));
	del_file(src_path, src_path_len, TEXT("xenblk.sys"));
	del_file(src_path, src_path_len, TEXT("xenblk.inf"));
	del_file(src_path, src_path_len, TEXT("xenblk.cat"));
	del_file(src_path, src_path_len, TEXT("xennet.sys"));
	del_file(src_path, src_path_len, TEXT("xennet.inf"));
	del_file(src_path, src_path_len, TEXT("xennet.cat"));
	del_file(src_path, src_path_len, TEXT("dpinst.xml"));
	del_file(src_path, src_path_len, TEXT("vmdpeula.txt"));
	del_file(src_path, src_path_len, TEXT("dpinst.exe"));
	del_file(src_path, src_path_len, TEXT("difxapi.dll"));
	del_file(src_path, src_path_len, TEXT("pvctrl.exe"));

	GetCurrentDirectory(sizeof(cur_path)/sizeof(cur_path[0]), cur_path);
	SetCurrentDirectory(src_path);
	_spawnl(_P_WAIT, "xensvc", "xensvc", "remove", NULL);
	SetCurrentDirectory(cur_path);
	del_file(src_path, src_path_len, TEXT("xensvc.exe"));

	/* Remove xenbus Directory */
	PathAppend(src_path, TEXT("xenbus"));
	if (!RemoveDirectory(src_path)) {
		/*
		Err = GetLastError();
		if (Err != ERROR_ALREADY_EXISTS) {
			OutputDebugString(TEXT("* DeleteXenFiles - rmdir Xenbus failed\n"));
		}
		else {
		*/
			OutputDebugString(TEXT("**** MoveFileEx for xenbus directory.\n"));
			MoveFileEx(src_path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
			delayed_remove = 1;
		//}
	}

	/* Remove XenDrv Directory */
	src_path[src_path_len] = '\0';
	if (!RemoveDirectory(src_path)) {
		/*
		Err = GetLastError();
		if (Err != ERROR_ALREADY_EXISTS) {
			OutputDebugString(TEXT("* DeleteXenFiles - rmdir XenDrv failed\n"));
		}
		else {
		*/
			OutputDebugString(TEXT("**** MoveFileEx for XenDrv directory.\n"));
			MoveFileEx( src_path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
			delayed_remove = 1;
		//}
	}

	if (SUCCEEDED(SHGetFolderPath(
			NULL, 
            CSIDL_SYSTEM, 
            NULL, 
            SHGFP_TYPE_CURRENT, 
            src_path))) {
		StringCchLength(src_path, MAX_PATH, &src_path_len);
		del_file(src_path, src_path_len, TEXT("drivers\\xenbus.sys"));
	}

	if (delayed_remove && !(flags & AUTO_REBOOT_F)) {
		CreateMessageBox(MBOX_TITLE, MBOX_FILE_CLEANUP_TEXT);
	}
	return 0;
}

void
del_bkup_files(void)
{
	TCHAR src_path[MAX_PATH];
	size_t src_path_len;

	if(FAILED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			src_path))) {
		return;
	}
	PathAppend(src_path, TEXT("Novell\\xenbkup"));
	StringCchLength(src_path, MAX_PATH, &src_path_len);

	del_file(src_path, src_path_len, TEXT(XENBUS_INF));
	del_file(src_path, src_path_len, TEXT(XENBUS_SYS));
	del_file(src_path, src_path_len, TEXT(XENBUS_CAT));

	del_file(src_path, src_path_len, TEXT(XENBLK_INF));
	del_file(src_path, src_path_len, TEXT(XENBLK_SYS));
	del_file(src_path, src_path_len, TEXT(XENBLK_CAT));

	del_file(src_path, src_path_len, TEXT(XENNET_INF));
	del_file(src_path, src_path_len, TEXT(XENNET_SYS));
	del_file(src_path, src_path_len, TEXT(XENNET_CAT));

	del_file(src_path, src_path_len, TEXT(XENSVC_EXE));
	del_file(src_path, src_path_len, TEXT(XENSETUP_EXE));
	del_file(src_path, src_path_len, TEXT(XENUNINST_EXE));
	del_file(src_path, src_path_len, TEXT("pvctrl.exe"));

	del_file(src_path, src_path_len, TEXT("dpinst.xml"));
	del_file(src_path, src_path_len, TEXT("dpinst.exe"));
	del_file(src_path, src_path_len, TEXT("difxapi.dll"));

	RemoveDirectory(src_path);
}

void
xenbus_delete(TCHAR *src_path)
{
	size_t src_path_len;

	StringCchLength(src_path, MAX_PATH, &src_path_len);

	del_file(src_path, src_path_len, TEXT(XENBUS_INF));
	del_file(src_path, src_path_len, TEXT(XENBUS_SYS));
	del_file(src_path, src_path_len, TEXT(XENBUS_CAT));
	del_file(src_path, src_path_len, TEXT(XENBUS_PNF));
}

static void
xenblk_delete(TCHAR *src_path)
{
	size_t src_path_len;

	StringCchLength(src_path, MAX_PATH, &src_path_len);

	del_file(src_path, src_path_len, TEXT(XENBLK_INF));
	del_file(src_path, src_path_len, TEXT(XENBLK_SYS));
	del_file(src_path, src_path_len, TEXT(XENBLK_CAT));
	del_file(src_path, src_path_len, TEXT(XENBLK_PNF));
}

static void
xennet_delete(TCHAR *src_path)
{
	size_t src_path_len;

	StringCchLength(src_path, MAX_PATH, &src_path_len);

	del_file(src_path, src_path_len, TEXT(XENNET_INF));
	del_file(src_path, src_path_len, TEXT(XENNET_SYS));
	del_file(src_path, src_path_len, TEXT(XENNET_CAT));
	del_file(src_path, src_path_len, TEXT(XENNET_PNF));
}

void
del_win_info(pv_drv_info_t *drv, void (* del_files)(TCHAR *))
{
	HKEY hkey;
	TCHAR src_path[MAX_PATH];
	size_t src_path_len;

	/* Delete old regeistry entries. */
	/*
	if (drv->difx[0] != '\0') {
		if (RegOpenKeyEx(
				HKEY_LOCAL_MACHINE,
				TEXT(RKEY_DRIVER_STORE),
				0,
				KEY_ALL_ACCESS,
				&hkey) == ERROR_SUCCESS ) {
			SHDeleteKey(hkey, drv->difx);
			RegCloseKey(hkey);
		}
	}
	*/

	if (drv->reinstall[0] != '\0') {
		if (RegOpenKeyEx(
				HKEY_LOCAL_MACHINE,
				TEXT(RKEY_REINSTALL),
				0,
				KEY_ALL_ACCESS,
				&hkey) == ERROR_SUCCESS ) {
			SHDeleteKey(hkey, drv->reinstall);
			RegCloseKey(hkey);
		}
	}

	/* Delete old files. */
	if (SUCCEEDED(SHGetFolderPath(
			NULL, 
            CSIDL_SYSTEM, 
            NULL, 
            SHGFP_TYPE_CURRENT, 
            src_path))) {
		StringCchLength(src_path, MAX_PATH, &src_path_len);

		/* Delete the DRVSTORE files. */
		/*
		if (drv->difx[0] != '\0') {
			PathAppend(src_path, TEXT("DRVSTORE"));
			PathAppend(src_path, drv->difx);
			del_files(src_path);
			RemoveDirectory(src_path);
		}
		*/

		/* Delete the ReinstallBackups files. */
		if (drv->reinstall[0] != '\0') {
			src_path[src_path_len] = '\0';
			PathAppend(src_path, TEXT("ReinstallBackups"));
			PathAppend(src_path, drv->reinstall);
			PathAppend(src_path, TEXT("DriverFiles"));
			del_files(src_path);
			RemoveDirectory(src_path);
			src_path[src_path_len] = '\0';
			PathAppend(src_path, TEXT("ReinstallBackups"));
			PathAppend(src_path, drv->reinstall);
			RemoveDirectory(src_path);
		}
	}
}


void
del_reg_pv_info(void)
{
	HKEY hkey;

	if (ERROR_SUCCESS == RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		TEXT(RKEY_S_NOVELL_XENPVD),
		0,
		KEY_ALL_ACCESS,
		&hkey))
	{
		RegDeleteValue(hkey, TEXT(XENBUS_INF));
		RegDeleteValue(hkey, TEXT(XENBLK_INF));
		RegDeleteValue(hkey, TEXT(XENNET_INF));
		RegCloseKey(hkey);
	}
}

void
get_saved_reg_pv_info(void)
{
	HKEY hkey;
	DWORD len;

	clear_pv_info();
	if (ERROR_SUCCESS == RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		TEXT(RKEY_S_NOVELL_XENPVD),
		0,
		KEY_ALL_ACCESS,
		&hkey))
	{
		len = MAX_INF_LEN * sizeof(TCHAR);
		RegQueryValueEx(hkey,
			TEXT(XENBUS_INF),
			NULL,
			NULL,
			(LPBYTE)pv_info.xenbus.inf,
			&len);
		len = MAX_INF_LEN * sizeof(TCHAR);
		RegQueryValueEx(hkey,
			TEXT(XENBLK_INF),
			NULL,
			NULL,
			(LPBYTE)pv_info.xenblk.inf,
			&len);
		len = MAX_INF_LEN * sizeof(TCHAR);
		RegQueryValueEx(hkey,
			TEXT(XENNET_INF),
			NULL,
			NULL,
			(LPBYTE)pv_info.xennet.inf,
			&len);

		RegCloseKey(hkey);
	}
}

static void
uninstall_bkup(pv_info_t *pv_info)
{
	HKEY hkey;
	TCHAR inf[MAX_INF_LEN];
	TCHAR src_path[MAX_PATH];
	size_t src_path_len;
	size_t pf_path_len;

	/* We will always delete the reinstall stuff. */
	if (RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_REINSTALL),
			0,
			KEY_ALL_ACCESS,
			&hkey) == ERROR_SUCCESS ) {
		get_reinstall_info(hkey, TEXT(XENBLK_DESC), pv_info->xenblk.reinstall);
		get_reinstall_info(hkey, TEXT(XENBUS_DESC), pv_info->xenbus.reinstall);
		get_reinstall_info(hkey, TEXT(XENNET_DESC), pv_info->xennet.reinstall);
		RegCloseKey(hkey);
	}

	/* Go get the new inf files used to install the drivers.  If the inf */
	/* file changed then go and delet the old inf file from the file system. */
	if(FAILED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			src_path))) {
		return;
	}
	if (RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_CLASS),
			0,
			KEY_ALL_ACCESS,
			&hkey) != ERROR_SUCCESS ) {
		return;
	}
	StringCchLength(src_path, MAX_PATH, &pf_path_len);
	PathAppend(src_path, TEXT("Novell\\xenbkup"));
	StringCchLength(src_path, MAX_PATH, &src_path_len);
	if (!SetCurrentDirectory(src_path))
		return;

	get_inf_info(hkey, TEXT(XENBUS_GUID), TEXT(XENBUS_MATCHING_ID), inf);
	if (_wcsnicmp(pv_info->xenbus.inf, inf, lstrlen(inf))) {
		_spawnl(_P_WAIT, "dpinst", "dpinst", "/q","/U", XENBUS_INF, NULL);
		del_win_info(&pv_info->xenbus, xenbus_delete);
	}

	get_inf_info(hkey, TEXT(XENBLK_GUID), TEXT(XENBLK_MATCHING_ID), inf);
	if (_wcsnicmp(pv_info->xenblk.inf, inf, lstrlen(inf))) {
		_spawnl(_P_WAIT, "dpinst", "dpinst", "/q","/U", XENBLK_INF, NULL);
		del_win_info(&pv_info->xenblk, xenblk_delete);
	}

	get_inf_info(hkey, TEXT(XENNET_GUID), TEXT(XENNET_MATCHING_ID), inf);
	if (_wcsnicmp(pv_info->xennet.inf, inf, lstrlen(inf))) {
		_spawnl(_P_WAIT, "dpinst", "dpinst", "/q","/U", XENNET_INF, NULL);
		del_win_info(&pv_info->xennet, xennet_delete);
	}
	RegCloseKey(hkey);
	src_path[pf_path_len] = '\0';
	SetCurrentDirectory(src_path);
}

void
update_xen_cleanup(void)
{
	get_saved_reg_pv_info();
	del_reg_pv_info();
	uninstall_bkup(&pv_info);
	del_bkup_files();
}

void
remove_xen_reg_entries(TCHAR *enumerator, TCHAR *match)
{
	HDEVINFO hdev;
	TCHAR buffer[MAX_PATH];
	SP_DEVINFO_DATA dev_info;
	DWORD idx = 0;
	DWORD data_type;
	DWORD buf_size;

	hdev = SetupDiGetClassDevs(NULL, enumerator, NULL, DIGCF_ALLCLASSES);
	if (hdev != INVALID_HANDLE_VALUE) {
		for ( ;; ) {
			memset( &dev_info, 0, sizeof(SP_DEVINFO_DATA) );
			dev_info.cbSize = sizeof( SP_DEVINFO_DATA );
			if (!SetupDiEnumDeviceInfo(hdev, idx++, &dev_info)) {
				if (GetLastError() == ERROR_NO_MORE_ITEMS)
					break;
			}

			buf_size = MAX_PATH * 2;
			if (SetupDiGetDeviceRegistryProperty(hdev,
					&dev_info,
					SPDRP_HARDWAREID,
					&data_type,
					(PBYTE)buffer,
					buf_size,
					&buf_size) ) {
				if (_wcsnicmp(buffer, match, lstrlen(match)) == 0) {
					SetupDiRemoveDevice(hdev, &dev_info);

					/* Keep going if the enumerator is XEN.  There */
					/* may be more than one VIF. */
					if (_wcsnicmp(enumerator, TEXT(ENUM_XEN),
						lstrlen(TEXT(ENUM_XEN))) != 0) {
						break;
					}
				}
			}
		}
		SetupDiDestroyDeviceInfoList(hdev);
	}
}

/******************************************************************************
	CreateMessageBox()
******************************************************************************/
void CreateMessageBox(UINT title_id, UINT text_id)
{
	HINSTANCE hInst;
	TCHAR msgTitle[128];
	TCHAR msgText[1024];
	
	hInst = GetModuleHandle(NULL);
	
	LoadString(hInst, title_id, msgTitle,
		sizeof(msgTitle)/sizeof(msgTitle[0]) - 1);
	LoadString(hInst, text_id, msgText, sizeof(msgText)/sizeof(msgText[0]) - 1);

	MessageBox(NULL, msgText, msgTitle, MB_OK);
}

/******************************************************************************
 CreateOkCancelMessageBox

 returns TRUE   - User selected OK
 returns FALSE  - User selected something other than ok
******************************************************************************/
BOOL
CreateOkCancelMessageBox(UINT title_id, UINT text_id)
{
	int mBoxRcode;
	HINSTANCE hInst;
	TCHAR msgTitle[128];
	TCHAR msgText[1024];
	
	hInst = GetModuleHandle(NULL);
	
	LoadString(hInst, title_id, msgTitle,
		sizeof(msgTitle)/sizeof(msgTitle[0]) - 1);
	LoadString(hInst, text_id, msgText, sizeof(msgText)/sizeof(msgText[0]) - 1);

	if (IDOK == MessageBox(NULL, msgText, msgTitle,	MB_OKCANCEL))
		return(TRUE);
	return(FALSE);	
}

BOOL
xen_shutdown_system(LPTSTR lpMsg, DWORD timeout, BOOL reboot)
{
	HANDLE tknh;				// handle to process token 
	TOKEN_PRIVILEGES tknp;		// pointer to token structure 
	BOOL ss_flag;				// system shutdown flag 
 
	/* Get the current process token handle so we can get shutdown  */
	/* privilege. */
	if (!OpenProcessToken(GetCurrentProcess(), 
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tknh)) {
		return FALSE; 
	}
 
	/* Get the LUID for shutdown privilege. */
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, 
		&tknp.Privileges[0].Luid); 
 
	tknp.PrivilegeCount = 1;
	tknp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
 
	/* Get shutdown privilege for this process. */
	AdjustTokenPrivileges(tknh, FALSE, &tknp, 0, 
		(PTOKEN_PRIVILEGES) NULL, 0); 
 
	/* Cannot test the return value of AdjustTokenPrivileges. */
 
	if (GetLastError() != ERROR_SUCCESS) {
		return FALSE; 
	}
 
	/* Display the shutdown dialog box and start the countdown. */
	ss_flag = InitiateSystemShutdownEx( 
		NULL,		// shut down local computer 
#if defined TARGET_OS_WinLH || defined TARGET_OS_Win7
		NULL, 		// message for user
#else
		lpMsg,		// message for user
#endif
		timeout,
		TRUE,		// force apps closed
		reboot,		// reboot after shutdown 
		SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER |
			SHTDN_REASON_FLAG_PLANNED);
 
	if (!ss_flag) {
		return FALSE; 
	}
 
	/* Disable shutdown privilege. */
	tknp.Privileges[0].Attributes = 0; 
	AdjustTokenPrivileges(tknh, FALSE, &tknp, 0, 
		(PTOKEN_PRIVILEGES) NULL, 0); 
 
	return TRUE; 
}

DWORD
xen_matches_os(void)
{
	OSVERSIONINFOEX osvi;
	SYSTEM_INFO si;
	DWORD setup_mj_ver;
	DWORD setup_mn_ver;
	DWORD setup_arch;

#if defined (TARGET_OS_Win2K)
	setup_mj_ver = 5;
	setup_mn_ver = 0;
#elif defined(TARGET_OS_WinXP)
	setup_mj_ver = 5;
	setup_mn_ver = 1;
#elif defined(TARGET_OS_WinNET)
	setup_mj_ver = 5;
	setup_mn_ver = 2;
#elif defined TARGET_OS_WinLH || defined TARGET_OS_Win7
	setup_mj_ver = 6;
	setup_mn_ver = 0;
#endif

#ifdef ARCH_x86
	setup_arch = PROCESSOR_ARCHITECTURE_INTEL;
#else
	setup_arch = PROCESSOR_ARCHITECTURE_AMD64;
#endif
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((OSVERSIONINFO *)&osvi);
#ifndef TARGET_OS_Win2K
	GetNativeSystemInfo(&si); 
#else
	GetSystemInfo(&si); 
#endif
	if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2
		&& osvi.wServicePackMajor == 0) {
		return MATCH_OS_UNSUPPORTED;
	}
	if (si.wProcessorArchitecture == setup_arch
			&& osvi.dwMajorVersion == setup_mj_ver) {
		if (osvi.dwMinorVersion == setup_mn_ver) {
			return MATCH_OS_SUCCESS;
		}
		else if (setup_mj_ver == 5) {
			if (setup_arch == PROCESSOR_ARCHITECTURE_AMD64
					&& setup_mn_ver == 2 && osvi.dwMinorVersion == 1) {
				/* 2003 64 builds run on XP 64. */
				return MATCH_OS_SUCCESS;
			}
		}
		else if (setup_mj_ver == 6) {
			/* 2008 runs on 2008, 2008 R2 and Win 7*/
			return MATCH_OS_SUCCESS;
		}
	}
	return MATCH_OS_MISMATCH;
}

DWORD
create_progress_bar(HWND *hwnd_parent, HWND *hwnd_pbar, DWORD intervals)
{
	TCHAR msgTitle[128];
	WNDCLASSEX wcx; 
	RECT rcClient;  // Client area of parent window 
	int cyVScroll;  // Height of scroll bar arrow 


	// Fill in the window class structure with parameters 
	// that describe the main window. 

	wcx.cbSize = sizeof(wcx);				// size of structure 
	wcx.style = CS_HREDRAW | CS_VREDRAW;	// redraw if size changes 
	wcx.lpfnWndProc = MainWndProc;			// points to window procedure 
	wcx.cbClsExtra = 0;						// no extra class memory 
	wcx.cbWndExtra = 0;						// no extra window memory 
	wcx.hInstance = g_hInstance;			// handle to instance 
	wcx.hIcon = LoadIcon(NULL, IDI_APPLICATION);	// predefined app. icon 
	wcx.hCursor = LoadCursor(NULL, IDC_ARROW);		// predefined arrow 
	wcx.hbrBackground = GetStockObject(LTGRAY_BRUSH);// white background brush 
	wcx.lpszMenuName =  TEXT("MainPBMenu");	// name of menu resource 
	wcx.lpszClassName = TEXT(PB_CLASS_NAME);// name of window class 
	wcx.hIconSm = NULL;
		/*
	wcx.hIconSm = LoadImage(g_hInstance,	// small class icon 
		MAKEINTRESOURCE(5),
		IMAGE_ICON, 
		GetSystemMetrics(SM_CXSMICON), 
		GetSystemMetrics(SM_CYSMICON), 
		LR_DEFAULTCOLOR); 
	*/

	if (!RegisterClassEx(&wcx)) {
		return 0;
	}

	LoadString(g_hInstance, MBOX_TITLE, msgTitle,
		sizeof(msgTitle)/sizeof(msgTitle[0]) - 1);

	*hwnd_parent = CreateWindowEx( 
		0,								// no extended styles           
		TEXT(PB_CLASS_NAME),	// class name                   
		(LPTSTR)msgTitle,
		WS_OVERLAPPED | WS_VISIBLE,
		CW_USEDEFAULT,					// default horizontal position  
		CW_USEDEFAULT,					// default vertical position    
		PB_WIN_WIDTH,
		PB_WIN_HEIGHT,
		(HWND)NULL,						// no parent or owner window    
		(HMENU)NULL,					// class menu used              
		g_hInstance,					// instance handle              
		NULL);							// no window creation data      
	if (!(*hwnd_parent)) {
		UnregisterClass(TEXT(PB_CLASS_NAME), g_hInstance);
		return 0;
	}

	ShowWindow(*hwnd_parent, SW_SHOWNORMAL);
	UpdateWindow(*hwnd_parent);

	InitCommonControls(); 
	GetClientRect(*hwnd_parent, &rcClient); 
	cyVScroll = GetSystemMetrics(SM_CYVSCROLL); 

	*hwnd_pbar = CreateWindowEx(
		0,
		PROGRESS_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		rcClient.left + cyVScroll,
		((rcClient.bottom - rcClient.top) - cyVScroll) / 2,
		rcClient.right - (cyVScroll * 2),
		cyVScroll, 
		*hwnd_parent,
		(HMENU)NULL,
		g_hInstance,
		NULL);
	if (!(*hwnd_pbar)) {
		DestroyWindow(*hwnd_parent);
		UnregisterClass(TEXT(PB_CLASS_NAME), g_hInstance);
		return 0;
	}
	SendMessage(*hwnd_pbar, PBM_SETRANGE, 0, MAKELPARAM(0, intervals));
	SendMessage(*hwnd_pbar, PBM_SETSTEP, (WPARAM) 1, 0); 
	return 1;
}

void
advance_progress_bar(HWND hwnd, HWND hwnd_pb, DWORD flags)
{
	if (flags & SHOW_PROGRESS_F) {
		//ShowWindow(hwnd, SW_SHOWNORMAL);
		UpdateWindow(hwnd);
		SendMessage(hwnd_pb, PBM_STEPIT, 0, 0);
	}
}

void
destroy_progress_bar(HWND hwnd_parent, HWND hwnd_pbar)
{
	if (hwnd_pbar) {
		DestroyWindow(hwnd_pbar);
	}
	if (hwnd_parent) {
		DestroyWindow(hwnd_parent);
		UnregisterClass(TEXT(PB_CLASS_NAME), g_hInstance);
	}
}

DWORD
cp_file(TCHAR *s, TCHAR *d, DWORD sl, DWORD dl, TCHAR *file)
{
	DWORD cc;

	PathAppend(s, file);
	PathAppend(d, file);
	cc = CopyFile(s, d, FALSE);
	SetFileAttributes(d, FILE_ATTRIBUTE_NORMAL);
	s[sl] = '\0';
	d[dl] = '\0';
	return cc;
}

void
restore_sys32_files(DWORD flags)
{
	TCHAR src[MAX_PATH];
	TCHAR dest[MAX_PATH];
	size_t src_len;
	size_t dest_len;

	if(FAILED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			src))) {
		return;
	}

	/* Get the complete path to system32. */
	if (FAILED(SHGetFolderPath(
			NULL, 
			CSIDL_SYSTEM, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			dest))) {
			return;
	}
	PathAppend(dest, TEXT("drivers"));
	StringCchLength(dest, MAX_PATH, &dest_len);

	PathAppend(src, TEXT("Novell\\XenDrv"));
	StringCchLength(src, MAX_PATH, &src_len);

	if (flags & XENBUS_F) {
		PathAppend(src, TEXT("xenbus"));
		cp_file(src, dest, src_len, dest_len, TEXT(XENBUS_SYS));
	}
	if (flags & XENBLK_F)
		cp_file(src, dest, src_len, dest_len, TEXT(XENBLK_SYS));
	if (flags & XENNET_F)
		cp_file(src, dest, src_len, dest_len, TEXT(XENNET_SYS));
}

void
delete_sys32_files(DWORD flags)
{
	TCHAR src[MAX_PATH];
	size_t src_len;

	/* Get the complete path to system32. */
	if (FAILED(SHGetFolderPath(
			NULL, 
			CSIDL_SYSTEM, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			src))) {
			return;
	}
	PathAppend(src, TEXT("drivers"));
	StringCchLength(src, MAX_PATH, &src_len);

	if (flags & XENBUS_F)
		del_file(src, src_len, TEXT(XENBUS_SYS));
	if (flags & XENBLK_F)
		del_file(src, src_len, TEXT(XENBLK_SYS));
	if (flags & XENNET_F)
		del_file(src, src_len, TEXT(XENNET_SYS));
}

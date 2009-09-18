/*****************************************************************************
 *
 *   File Name:      xenupdate.c
 *   Created by:     KRA
 *   Date created:   11/24/08
 *
 *   %version:       7 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 18:09:48 2009 %
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

#include "xensetup.h"
#include <shlwapi.h>

static void
get_pv_info(void)
{
	HKEY hkey;

	clear_pv_info();
	if (RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_CLASS),
			0,
			KEY_ALL_ACCESS,
			&hkey) == ERROR_SUCCESS ) {
		get_inf_info(hkey, TEXT(XENBLK_GUID), TEXT(XENBLK_MATCHING_ID),
			pv_info.xenblk.inf);
		get_inf_info(hkey, TEXT(XENBUS_GUID), TEXT(XENBUS_MATCHING_ID),
			pv_info.xenbus.inf);
		get_inf_info(hkey, TEXT(XENNET_GUID), TEXT(XENNET_MATCHING_ID),
			pv_info.xennet.inf);
		RegCloseKey(hkey);
	}

	if (RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_REINSTALL),
			0,
			KEY_ALL_ACCESS,
			&hkey) == ERROR_SUCCESS ) {
		get_reinstall_info(hkey, TEXT(XENBLK_DESC), pv_info.xenblk.reinstall);
		get_reinstall_info(hkey, TEXT(XENBUS_DESC), pv_info.xenbus.reinstall);
		get_reinstall_info(hkey, TEXT(XENNET_DESC), pv_info.xennet.reinstall);
		RegCloseKey(hkey);
	}
}

static void
save_reg_pv_drv_info(HKEY hkey, TCHAR *val_name, TCHAR *value)
{
	if (value) {
		RegSetValueEx(hkey,
			val_name,
			0,
			REG_SZ,
			(PBYTE)value,
			(lstrlen(value) * sizeof(WCHAR)) + 2);
	}
}

static void
save_reg_pv_info(void)
{
	HKEY hkey;

	if (ERROR_SUCCESS == RegCreateKeyEx(
		HKEY_LOCAL_MACHINE,
		TEXT(RKEY_S_NOVELL_XENPVD),
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hkey,
		NULL))
	{
		save_reg_pv_drv_info(hkey, TEXT(XENBUS_INF), pv_info.xenbus.inf);
		save_reg_pv_drv_info(hkey, TEXT(XENBLK_INF), pv_info.xenblk.inf);
		save_reg_pv_drv_info(hkey, TEXT(XENNET_INF), pv_info.xennet.inf);

		RegCloseKey(hkey);
	}
}

static void
save_bkup_files(TCHAR *src, DWORD src_len)
{
	TCHAR dest[MAX_PATH];
	size_t dest_len;
	size_t bus_len;

	if(FAILED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			dest))) {
		return;
	}

	PathAppend(dest, TEXT("Novell\\xenbkup"));
	StringCchLength(dest, MAX_PATH, &dest_len);

	if (!CreateDirectory(dest, NULL)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			return;
	}

	PathAppend(src, TEXT("xenbus"));
	StringCchLength(src, MAX_PATH, &bus_len);

	cp_file(src, dest, bus_len, dest_len, TEXT(XENBUS_INF));
	cp_file(src, dest, bus_len, dest_len, TEXT(XENBUS_SYS));
	cp_file(src, dest, bus_len, dest_len, TEXT(XENBUS_CAT));

	src[src_len] = '\0';
	cp_file(src, dest, src_len, dest_len, TEXT(XENBLK_INF));
	cp_file(src, dest, src_len, dest_len, TEXT(XENBLK_SYS));
	cp_file(src, dest, src_len, dest_len, TEXT(XENBLK_CAT));

	cp_file(src, dest, src_len, dest_len, TEXT(XENNET_INF));
	cp_file(src, dest, src_len, dest_len, TEXT(XENNET_SYS));
	cp_file(src, dest, src_len, dest_len, TEXT(XENNET_CAT));

	cp_file(src, dest, src_len, dest_len, TEXT(XENSVC_EXE));
	cp_file(src, dest, src_len, dest_len, TEXT(XENSETUP_EXE));
	cp_file(src, dest, src_len, dest_len, TEXT(XENUNINST_EXE));
	cp_file(src, dest, src_len, dest_len, TEXT("pvctrl.exe"));

	cp_file(src, dest, src_len, dest_len, TEXT("dpinst.xml"));
	cp_file(src, dest, src_len, dest_len, TEXT("dpinst.exe"));
	cp_file(src, dest, src_len, dest_len, TEXT("difxapi.dll"));

	src[src_len] = '\0';
}

static void
restore_bkup_files(void)
{
	TCHAR src[MAX_PATH];
	TCHAR dest[MAX_PATH];
	size_t src_len;
	size_t dest_len;
	size_t bus_len;

	if(FAILED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			src))) {
		return;
	}
	PathAppend(src, TEXT("Novell"));
	StringCchLength(src, MAX_PATH, &src_len);
	if (FAILED(StringCchCopy(dest, MAX_PATH, src))) {
		return;
	}

	PathAppend(src, TEXT("xenbkup"));
	PathAppend(dest, TEXT("XenDrv"));
	StringCchLength(src, MAX_PATH, &src_len);
	StringCchLength(dest, MAX_PATH, &dest_len);
	PathAppend(dest, TEXT("xenbus"));
	StringCchLength(dest, MAX_PATH, &bus_len);

	cp_file(src, dest, src_len, bus_len, TEXT(XENBUS_INF));
	cp_file(src, dest, src_len, bus_len, TEXT(XENBUS_SYS));
	cp_file(src, dest, src_len, bus_len, TEXT(XENBUS_CAT));

	dest[dest_len] = '\0';
	cp_file(src, dest, src_len, dest_len, TEXT(XENBLK_INF));
	cp_file(src, dest, src_len, dest_len, TEXT(XENBLK_SYS));
	cp_file(src, dest, src_len, dest_len, TEXT(XENBLK_CAT));

	cp_file(src, dest, src_len, dest_len, TEXT(XENNET_INF));
	cp_file(src, dest, src_len, dest_len, TEXT(XENNET_SYS));
	cp_file(src, dest, src_len, dest_len, TEXT(XENNET_CAT));

	cp_file(src, dest, src_len, dest_len, TEXT(XENSETUP_EXE));
	cp_file(src, dest, src_len, dest_len, TEXT(XENUNINST_EXE));
	cp_file(src, dest, src_len, dest_len, TEXT("pvctrl.exe"));

	SetCurrentDirectory(dest);
	_spawnl(_P_WAIT, "xensvc", "xensvc", "remove", NULL);
	cp_file(src, dest, src_len, dest_len, TEXT(XENSVC_EXE));
	_spawnl(_P_WAIT, "xensvc", "xensvc", "install", NULL);
}

DWORD
get_guid_drv_key(TCHAR *guid, TCHAR *matching_id, HKEY *hkey_drv)
{
	HKEY hkey_class;
	HKEY hkey_guid;
	TCHAR key_name[MAX_DRV_LEN];   
	TCHAR reg_matching_id[MAX_MATCHING_ID_LEN];   
	DWORD i;
	DWORD len;
	DWORD cc = 0;

	if (RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_CLASS),
			0,
			KEY_ALL_ACCESS,
			&hkey_class) != ERROR_SUCCESS ) {
		return cc;
	}

	/* Open the requested GUID key. */
	if (RegOpenKeyEx(hkey_class,
			guid,
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
					hkey_drv) == ERROR_SUCCESS ) {
				len = sizeof(reg_matching_id);
				if (RegQueryValueEx(*hkey_drv,
						TEXT("MatchingDeviceId"),
						NULL,
						NULL,
						(LPBYTE)reg_matching_id,
						&len) == ERROR_SUCCESS) {
					if (lstrcmpi(reg_matching_id, matching_id) == 0) {
						cc = 1;
						break;
					}
				}
				RegCloseKey(*hkey_drv);
			}
			len = MAX_DRV_LEN;
			i++;
		}
		RegCloseKey(hkey_guid);
	}
	RegCloseKey(hkey_class);
	return cc;
}

static void
set_inf_info(TCHAR *guid, TCHAR *matching_id, TCHAR *inf)
{
	HKEY hkey_class;
	HKEY hkey_guid;
	HKEY hkey_drv;
	TCHAR key_name[MAX_DRV_LEN];   
	TCHAR reg_matching_id[MAX_MATCHING_ID_LEN];   
	DWORD i;
	DWORD len;
	DWORD found;

	if (get_guid_drv_key(guid, matching_id, &hkey_drv)) {
		RegSetValueEx(hkey_drv,
			TEXT("InfPath"),
			0,
			REG_SZ,
			(PBYTE)inf,
			(lstrlen(inf) * sizeof(WCHAR)) + 2);
		RegCloseKey(hkey_drv);
	}
}

static void
undo_xenbus(void)
{
	HKEY hkey;

	set_inf_info(TEXT(XENBUS_GUID), TEXT(XENBUS_MATCHING_ID),
		pv_info.xenbus.inf);

	if (RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			TEXT(RKEY_REINSTALL),
			0,
			KEY_ALL_ACCESS,
			&hkey) != ERROR_SUCCESS ) {
		return;
	}
	get_reinstall_info(hkey, TEXT(XENBUS_DESC), pv_info.xenbus.reinstall);
	RegCloseKey(hkey);
	del_win_info(&pv_info.xenbus, xenbus_delete);
	_spawnl(_P_WAIT, "dpinst", "dpinst", "/q","/U", XENBUS_INF, NULL);
}

static void
undo_update(void)
{
	get_saved_reg_pv_info();

	/* Unfortunately, if you uninstall any files that were updeated, */
	/* the entire driver is uninstalled.  It does not go back to the */
	/* previously installed version.  So we will not uninstall, but */
	/* just leave the stuff lying around and put everything else back. */
	//uninstall_updated_files(&pv_info);
	//del_run_once(); RunOnce doesn't work.
	del_reg_pv_info();
	restore_bkup_files();
	restore_sys32_files(XENBUS_F | XENBLK_F | XENNET_F);
	del_bkup_files();
}

DWORD install_driver(DWORD stup_flags)
{
	if (stup_flags & EULA_ACCEPTED_F) {
		if (stup_flags & FORCE_INSTALL_F) {
			return (DWORD)_spawnl(_P_WAIT, "dpinst",
				"/q","/sw", "/sa", "/lm", "/d","/se", "/f", NULL);
		}
		return (DWORD)_spawnl(_P_WAIT, "dpinst",
			"/q","/sw", "/sa", "/lm", "/d","/se", NULL);
	}
	else {
		if (stup_flags & FORCE_INSTALL_F) {
			return (DWORD)_spawnl(_P_WAIT, "dpinst",
				"/q","/sw", "/sa", "/lm", "/d", "/f", NULL);
		}
		return (DWORD)_spawnl(_P_WAIT, "dpinst",
			"/q","/sw", "/sa", "/lm", "/d", NULL);
	}
}
static DWORD
can_update_xen(TCHAR *guid, TCHAR *matching_id)
{
	HKEY hkey_drv;
	TCHAR ver[MAX_MATCHING_ID_LEN];
	DWORD len;
	DWORD cc = 0;

	if (get_guid_drv_key(guid, matching_id, &hkey_drv)) {
		len = sizeof(ver);
		if (RegQueryValueEx(hkey_drv,
			 TEXT("DriverVersion"),
			 NULL,
			 NULL,
			 (LPBYTE)ver,
			 &len) == ERROR_SUCCESS) {
			/* Check the version of xenbus.  If it is */
			/* 1.1.3.2 or 1.1.15.1, it is too old and */
			/* we can't do the update. */
			if (lstrcmpi(ver, TEXT("1.1.15.1")) != 0 &&
				lstrcmpi(ver, TEXT("1.1.3.2")) != 0) {
				cc = 1;
			}
		}
		RegCloseKey(hkey_drv);
	}
	return cc;
}

static DWORD
get_pv_ver(TCHAR *path, DWORD len, TCHAR *file, DWORD *ms, DWORD *ls)
{
	VS_FIXEDFILEINFO *vs;
	BYTE *buf = NULL;
	DWORD fh;
	DWORD size;
	DWORD err = 0;

	PathAppend(path, file);

	if ((size = GetFileVersionInfoSize(path, &fh)) != 0) {
		if ((buf = (BYTE *)GlobalAlloc(0, size)) != NULL) {
			if (GetFileVersionInfo(path, fh, size, buf)) {
				if (VerQueryValue(buf, TEXT("\\"), &vs, &size) != 0) {
					*ms = vs->dwFileVersionMS;
					*ls = vs->dwFileVersionLS;
				}
				else
					err = 1;
			}
			else {
				err = GetLastError();
			}
		}
		else
			err = GetLastError();
	}
	else
		err = GetLastError();

	if (buf) {
		GlobalFree(buf);
	}

	path[len] = '\0';
	return err;
}

static void
get_pv_updates(LPCWSTR MediaRootDirectory, pv_update_t *pv_update)
{
	TCHAR o_path[MAX_PATH];
	TCHAR n_path[MAX_PATH];
	DWORD o_ms;
	DWORD o_ls;
	DWORD n_ms;
	DWORD n_ls;
	size_t o_len;
	size_t n_len;

	pv_update->cp_flags = 0;
	pv_update->xenbus = 0;
	pv_update->xenblk = 0;
	pv_update->xennet = 0;
	pv_update->xensvc = 0;

	if (FAILED(StringCchCopy(n_path, MAX_PATH, MediaRootDirectory))) {
		return;
	}
	StringCchLength(n_path, MAX_PATH, &n_len);

	if(FAILED(SHGetFolderPath(
			NULL, 
			CSIDL_PROGRAM_FILES, 
			NULL, 
			SHGFP_TYPE_CURRENT, 
			o_path))) {
		return;
	}
	PathAppend(o_path, TEXT("Novell\\XenDrv"));
	StringCchLength(o_path, MAX_PATH, &o_len);

	if (get_pv_ver(o_path, o_len, TEXT(XENBLK_SYS), &o_ms, &o_ls) == 0) {
		if (get_pv_ver(n_path, n_len, TEXT(XENBLK_SYS), &n_ms, &n_ls) == 0) {
			if (n_ms > o_ms || (n_ms == o_ms && n_ls > o_ls)) {
				pv_update->cp_flags |= CP_XENBLK_F;
				pv_update->xenblk = 1;
			}
		}
	}

	if (get_pv_ver(o_path, o_len, TEXT(XENNET_SYS), &o_ms, &o_ls) == 0) {
		if (get_pv_ver(n_path, n_len, TEXT(XENNET_SYS), &n_ms, &n_ls) == 0) {
			if (n_ms > o_ms || (n_ms == o_ms && n_ls > o_ls)) {
				pv_update->cp_flags |= CP_XENNET_F;
				pv_update->xennet = 1;
			}
		}
	}

	if (get_pv_ver(o_path, o_len, TEXT(XENSVC_EXE), &o_ms, &o_ls) == 0) {
		if (get_pv_ver(n_path, n_len, TEXT(XENSVC_EXE), &n_ms, &n_ls) == 0) {
			if (n_ms > o_ms || (n_ms == o_ms && n_ls > o_ls)) {
				pv_update->cp_flags |= CP_XENSVC_F;
				pv_update->xensvc = 1;
			}
		}
	}

	PathAppend(o_path, TEXT("xenbus"));
	StringCchLength(o_path, MAX_PATH, &o_len);

	if (get_pv_ver(o_path, o_len, TEXT(XENBUS_SYS), &o_ms, &o_ls) == 0) {
		if (get_pv_ver(n_path, n_len, TEXT(XENBUS_SYS), &n_ms, &n_ls) == 0) {
			if (n_ms > o_ms || (n_ms == o_ms && n_ls > o_ls)) {
				pv_update->cp_flags |= CP_XENBUS_F;
				pv_update->xenbus = 1;
			}
		}
	}
}

DWORD
update_xen(LPCWSTR MediaRootDirectory, DWORD DirPathLength,
	TCHAR *src_path, size_t src_path_len, DWORD setup_flags,
	HWND hwnd_parent, HWND hwnd_pbar, BOOL *reboot)
{
	pv_update_t pv_update;
	DWORD err;
	DWORD installed_xenbus;

	if (can_update_xen(TEXT(XENBUS_GUID), TEXT(XENBUS_MATCHING_ID)) == 0) {
		CreateMessageBox(MBOX_TITLE, MBOX_UPDATE_TEXT);
		return 1;
	}

	if (setup_flags & FORCE_INSTALL_F) {
		pv_update.cp_flags = CP_ALL_F;
		pv_update.xenbus = 1;
		pv_update.xenblk = 1;
		pv_update.xennet = 1;
		pv_update.xensvc = 1;
	}
	else {

		get_pv_updates(MediaRootDirectory, &pv_update);
		if (pv_update.cp_flags == 0)
			return 1;
	}

	update_xen_cleanup();
	advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);

	/* Save off the current registry driver info. */
	get_pv_info();
	save_reg_pv_info();
	save_bkup_files(src_path, src_path_len);
	//set_run_once(); RunOnce doesn't work.

	if (pv_update.xensvc) {
		/* Stop the xensvc so that the new file can be copied over the old. */
		/* Because we stopped the service, we can't start it again without */
		/* a reboot. */
		*reboot = TRUE;
		SetCurrentDirectory(src_path);
		_spawnl(_P_WAIT, "xensvc", "xensvc", "remove", NULL);
		advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);
	}

	CopyXenFiles(MediaRootDirectory, DirPathLength, pv_update.cp_flags);
	advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);

	/* Install Xenbus if needed. */
	err = 0;
	installed_xenbus = 0;
	if (pv_update.xenbus) {
		PathAppend(src_path, TEXT("xenbus"));
		SetCurrentDirectory(src_path);

		/* We always supress the eula for xenbus. */
		err = install_driver(setup_flags | EULA_ACCEPTED_F | FORCE_INSTALL_F);
		if (err != (DWORD) -1 && !(err & 0x80000000)) {
			advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);
			if (err & 0x40000000) {
				OutputDebugString(TEXT("**** Reboot Required ***\n"));
			}

			installed_xenbus = err & 0xff ? 1 : 0;
			if (installed_xenbus )
				*reboot = TRUE;
			err = 0;

			/* Go back to the xendrv dir. */
			src_path[src_path_len] = '\0';
		}
		else {
			OutputDebugString(TEXT("Update spawn dpinst FAILED for xenbus\n"));
		}
	}

	SetCurrentDirectory(src_path);

	if (pv_update.xensvc && err == 0) {
		_spawnl(_P_WAIT, "xensvc", "xensvc", "install", NULL);
	}

	if ((pv_update.xenblk || pv_update.xennet) && err == 0) {
		/* If dpinst asks for a reboot, we will never come back so */
		/* do the cleanup here even though that leave a big hole. */
		cleanup_setup();

		/* Install/update both xenblk and xennet based on the setup flags. */
		err = install_driver(setup_flags | FORCE_INSTALL_F);
		if (err != (DWORD) -1 && !(err & 0x80000000)) {
			advance_progress_bar(hwnd_parent, hwnd_pbar, setup_flags);
			if (err & 0x40000000) {
				*reboot = TRUE;
				OutputDebugString(TEXT("**** Reboot Required ***\n"));
			}

			/* The user selected not to reboot or it was not required. */
			/* RunOnce doesn't work, try to do cleanup now. */
			/* Even if reboot is selected, we don't want to chance any */
			/* portion of update_xen_cleanup from running in case it */
			/* does not finish before the process is killed. */
			//update_xen_cleanup();
			err = 0;
		}	
		else {
			if (installed_xenbus) {
				PathAppend(src_path, TEXT("xenbus"));
				SetCurrentDirectory(src_path);
				undo_xenbus();
				src_path[src_path_len] = '\0';
				SetCurrentDirectory(src_path);
			}
			OutputDebugString(TEXT("Update spawn FAILED: xenblk xennet\n"));
		}
	}

	if (err)
		undo_update();

	return err;
}

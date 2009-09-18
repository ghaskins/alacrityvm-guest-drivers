/*****************************************************************************
 *
 *   File Name:      xencleanup.h
 *   Created by:     kra
 *   Date created:   05/12/09
 *
 *   %version:       2 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 09:51:41 2009 %
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
#ifndef _XENCLEANUP_H_
#define _XENCLEANUP_H_

#define MATCH_OS_SUCCESS		0
#define MATCH_OS_MISMATCH		1
#define MATCH_OS_UNSUPPORTED	2

typedef struct _pv_drv_info_s
{
	TCHAR inf[MAX_INF_LEN];
	TCHAR reinstall[MAX_REINSTALL_LEN];
} pv_drv_info_t;

typedef struct _pv_info_s
{
	pv_drv_info_t xenbus;
	pv_drv_info_t xenblk;
	pv_drv_info_t xennet;
} pv_info_t;

//
// xencleanup
//
extern pv_info_t pv_info;
void clear_pv_info(void);
void get_reinstall_info(HKEY hkey, TCHAR *matching_desc, TCHAR *reinstall);
void get_inf_info(HKEY hkey_class, TCHAR *guid, TCHAR *matching_id, TCHAR *inf);
void del_bkup_files(void);
void xenbus_delete(TCHAR *src_path);
void del_file(TCHAR *path, DWORD path_len, TCHAR *file);
DWORD del_xen_files(DWORD);
void del_win_info(pv_drv_info_t *drv, void (* del_files)(TCHAR *));
void del_reg_pv_info(void);
void get_saved_reg_pv_info(void);
void uninstall_bkup(pv_info_t *pv_info);
void update_xen_cleanup(void);
void remove_xen_reg_entries(TCHAR *enumerator, TCHAR *match);
void CreateMessageBox(UINT title_id, UINT text_id);
BOOL CreateOkCancelMessageBox(UINT title_id, UINT text_id);
BOOL xen_shutdown_system(LPTSTR lpMsg, DWORD timeout, BOOL reboot);
DWORD xen_matches_os(void);
DWORD create_progress_bar(HWND *hwnd_parent, HWND *hwnd_pbar, DWORD intervals);
void advance_progress_bar(HWND hwnd, HWND hwnd_pb, DWORD flags);
void destroy_progress_bar(HWND hwnd_parent, HWND hwnd_pbar);
DWORD cp_file(TCHAR *s, TCHAR *d, DWORD sl, DWORD dl, TCHAR *file);
void restore_sys32_files(DWORD flags);
void delete_sys32_files(DWORD flags);

#endif

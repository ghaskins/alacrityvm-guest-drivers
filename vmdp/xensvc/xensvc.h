/*****************************************************************************
 *
 *   File Name:      xensvc.h
 *
 *   %version:       2 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Sep 08 10:11:50 2008 %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2008 Unpublished Work of Novell, Inc. All Rights Reserved.
 *
 * THIS IS AN UNPUBLISHED WORK OF NOVELL, INC.  IT CONTAINS NOVELL'S
 * CONFIDENTIAL, PROPRIETARY, AND TRADE SECRET INFORMATION.  NOVELL
 * RESTRICTS THIS WORK TO NOVELL EMPLOYEES WHO NEED THE WORK TO PERFORM
 * THEIR ASSIGNMENTS AND TO THIRD PARTIES AUTHORIZED BY NOVELL IN WRITING.
 * THIS WORK MAY NOT BE USED, COPIED, DISTRIBUTED, DISCLOSED, ADAPTED,
 * PERFORMED, DISPLAYED, COLLECTED, COMPILED, OR LINKED WITHOUT NOVELL'S
 * PRIOR WRITTEN CONSENT.  USE OR EXPLOITATION OF THIS WORK WITHOUT
 * AUTHORIZATION COULD SUBJECT THE PERPETRATOR TO CRIMINAL AND  CIVIL
 * LIABILITY.
 *****************************************************************************/

#ifndef _XENSVC_H_
#define _XENSVC_H_

#include <windows.h>
#include <winioctl.h>
#include <tchar.h>
#include <strsafe.h>
#include <shellapi.h>
#include <winpv_defs.h>

#define XENSVC_SHUTDOWN_NAME TEXT("XenShutdownSvc")
#define XENSVC_SHUTDOWN_DISPLAY_NAME TEXT("Xen Shutdown Service")

#define XENSVC_SHUTDOWN_TIMEOUT 30

#define MBOX_XENSVC_TITLE		4001
#define MBOX_XENSVC_TEXT		4002

#ifdef DBG
#define DBG_OUTPUT OutputDebugString
#else
#define DBG_OUTPUT
#endif

extern SERVICE_STATUS	g_xensvc_shutdown_status; 

VOID xensvc_report_status(SERVICE_STATUS_HANDLE,
	SERVICE_STATUS *,
	DWORD,
	DWORD,
	DWORD);
VOID xensvc_report_event(LPCWSTR, LPTSTR);

/* xensvc_shutdown.c */
VOID WINAPI xensvc_shutdown_main(DWORD, LPTSTR *); 

#endif	// _XENSVC_H_

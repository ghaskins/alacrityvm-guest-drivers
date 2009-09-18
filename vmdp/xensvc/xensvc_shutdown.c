/*****************************************************************************
 *
 *   File Name:      xensvc_shutdown.c
 *
 *   %version:       2 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Sep 08 10:11:57 2008 %
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
#include "xensvc.h"

static SERVICE_STATUS_HANDLE	g_xensvc_shutdown_status_handle; 
static HANDLE					g_xensvc_shutdown_stop_event_handle = NULL;
static HANDLE					g_xensvc_shutdown_wait_handle = NULL;


static VOID xensvc_shutdown_init(DWORD, LPTSTR *); 
static DWORD WINAPI xensvc_shutdown_wait_thread(LPVOID param);
//static void xensvc_shutdown_wait_callback(void *context, BOOLEAN timedout);
static VOID WINAPI xensvc_shutdown_ctrl_handler(DWORD); 
static BOOL xensvc_shutdown_system(LPTSTR lpMsg, BOOL reboot);

/*
* Purpose: 
*   Entry point for the service
*
* Parameters:
*   dwArgc - Number of arguments in the lpszArgv array
*   lpszArgv - Array of strings. The first string is the name of
*     the service and subsequent strings are passed by the process
*     that called the StartService function to start the service.
* 
* Return value:
*   None.
*/
VOID WINAPI
xensvc_shutdown_main( DWORD dwArgc, LPTSTR *lpszArgv )
{
	DBG_OUTPUT(TEXT("==> xensvc_shutdown_main ****\n"));
	g_xensvc_shutdown_status_handle = RegisterServiceCtrlHandler(
		XENSVC_SHUTDOWN_NAME,
		xensvc_shutdown_ctrl_handler);

	if( !g_xensvc_shutdown_status_handle ) { 
		xensvc_report_event(XENSVC_SHUTDOWN_NAME,
			TEXT("RegisterServiceCtrlHandler")); 
		return; 
	} 

	// These SERVICE_STATUS members remain as set here
	g_xensvc_shutdown_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
	g_xensvc_shutdown_status.dwServiceSpecificExitCode = 0;    

	// Report initial status to the SCM
	xensvc_report_status(g_xensvc_shutdown_status_handle,
		&g_xensvc_shutdown_status,
		SERVICE_START_PENDING,
		NO_ERROR,
		3000);

	// Perform service-specific initialization and work.
	xensvc_shutdown_init(dwArgc, lpszArgv);
	DBG_OUTPUT(TEXT("<== xensvc_shutdown_main ****\n"));
}

/*
* Purpose: 
*   The service code
*
* Parameters:
*   dwArgc - Number of arguments in the lpszArgv array
*   lpszArgv - Array of strings. The first string is the name of
*     the service and subsequent strings are passed by the process
*     that called the StartService function to start the service.
* 
* Return value:
*   None
*/
static VOID
xensvc_shutdown_init( DWORD dwArgc, LPTSTR *lpszArgv)
{
	DWORD shutdown;
	DWORD cbdata;
	HKEY hkey;

	/*   Be sure to periodically call xensvc_report_status() with  */
	/*   SERVICE_START_PENDING. If initialization fails, call */
	/*   xensvc_report_status with SERVICE_STOPPED. */

	/* Create an event. The control handler function, */
	/* xensvc_shutdown_ctrl_handler, signals this event when it */
	/* receives the stop control code. */
	DBG_OUTPUT(TEXT("==> xensvc_shutdown_init ****\n"));
	g_xensvc_shutdown_stop_event_handle = CreateEvent(
		NULL,		// default security attributes
		TRUE,		// manual reset event
		FALSE,		// not signaled
		NULL);		// no name

	if (g_xensvc_shutdown_stop_event_handle == NULL) {
		xensvc_report_status(g_xensvc_shutdown_status_handle,
			&g_xensvc_shutdown_status,
			SERVICE_STOPPED,
			NO_ERROR,
			0);
		return;
	}

	/* Report running status when initialization is complete. */
	xensvc_report_status(g_xensvc_shutdown_status_handle,
		&g_xensvc_shutdown_status,
		SERVICE_RUNNING,
		NO_ERROR,
		0);

	g_xensvc_shutdown_wait_handle = CreateThread(
		NULL,
		0,
		xensvc_shutdown_wait_thread,
		NULL,
		0,
		NULL);
	if (g_xensvc_shutdown_wait_handle) {
		CloseHandle(g_xensvc_shutdown_wait_handle);
	}
	else
		return;

	/* RegisterWaitForSingleObject doesn't work.*/

	DBG_OUTPUT(TEXT("    RegOpenKeyEx ****\n"));
	if (ERROR_SUCCESS == RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		TEXT(XENBUS_HKLM_DEVICE_KEY),
		0,
		KEY_ALL_ACCESS,
		&hkey)) 
	{
		/* We need to do something here so that the first */
		/* RegNotifyChangeKeyValue will catch the first */
		/* change to the registry. */
		RegQueryValueEx(
			hkey,
			TEXT(XENBUS_SHUTDOWN_STR),
			NULL,
			NULL,
			(PBYTE)&shutdown,
			&cbdata);
		while (1) {
			/* Block until the registry is changed. */
			DBG_OUTPUT(TEXT("    RegNotifyChangeKeyValue ****\n"));
			if (ERROR_SUCCESS == RegNotifyChangeKeyValue(
				hkey,
				FALSE,
				REG_LEGAL_CHANGE_FILTER,
				NULL,
				FALSE)) {
				/* Read the value to see if we are to shutdown. */
				DBG_OUTPUT(TEXT("    RegQueryValueEx ****\n"));
				if (ERROR_SUCCESS == RegQueryValueEx(
					hkey,
					TEXT(XENBUS_SHUTDOWN_STR),
					NULL,
					NULL,
					(PBYTE)&shutdown,
					&cbdata)) {
					if (shutdown == XENBUS_REG_SHUTDOWN_VALUE) {
						DBG_OUTPUT(TEXT("    XENBUS_REG_SHUTDOWN_VALUE ***\n"));
						xensvc_shutdown_system(TEXT("xm shutdown"), FALSE);
					}
					else if (shutdown == XENBUS_REG_REBOOT_VALUE) {
						DBG_OUTPUT(TEXT("    XENBUS_REG_REBOOT_VALUE ***\n"));
						xensvc_shutdown_system(TEXT("xm reboot"), TRUE);
					}

					/* Clear the value incase of subsequent cancel. */
					DBG_OUTPUT(TEXT("    RegSetValueEx ***\n"));
					shutdown = 0;
					RegSetValueEx(
						hkey,
						TEXT(XENBUS_SHUTDOWN_STR),
						0,
						REG_DWORD,
						(PBYTE)&shutdown,
						sizeof(shutdown));
				}

			}
			else
				break;
		}
		RegCloseKey(hkey);
	}
	DBG_OUTPUT(TEXT("<== xensvc_shutdown_init ****\n"));
}

static DWORD WINAPI
xensvc_shutdown_wait_thread(LPVOID param)
{
	DBG_OUTPUT(TEXT("==> xensvc_shutdown_wait_thread ****\n"));
	WaitForSingleObject(g_xensvc_shutdown_stop_event_handle, INFINITE);
	xensvc_report_status(g_xensvc_shutdown_status_handle,
		&g_xensvc_shutdown_status,
		SERVICE_STOPPED,
		NO_ERROR,
		0);
	DBG_OUTPUT(TEXT("<== xensvc_shutdown_wait_thread ****\n"));
	return 0;
}

/*
* Purpose: 
*   Called by SCM whenever a control code is sent to the service
*   using the ControlService function.
*
* Parameters:
*   dwCtrl - control code
* 
* Return value:
*   None
*/
static VOID WINAPI
xensvc_shutdown_ctrl_handler(DWORD dwCtrl)
{
	DBG_OUTPUT(TEXT("==> xensvc_shutdown_ctrl_handler ****\n"));
	switch(dwCtrl) {  
		case SERVICE_CONTROL_STOP: 
			xensvc_report_status(g_xensvc_shutdown_status_handle,
				&g_xensvc_shutdown_status, 
				SERVICE_STOP_PENDING,
				NO_ERROR,
				0);

			/* Signal the service to stop. */
			SetEvent(g_xensvc_shutdown_stop_event_handle);
			return;
 
		case SERVICE_CONTROL_INTERROGATE: 
			/* Fall through to send current status. */
			break; 
		default: 
			break;
	} 

	xensvc_report_status(g_xensvc_shutdown_status_handle,
		&g_xensvc_shutdown_status,
		g_xensvc_shutdown_status.dwCurrentState,
		NO_ERROR,
		0);
	DBG_OUTPUT(TEXT("<== xensvc_shutdown_ctrl_handler ****\n"));
}

static BOOL
xensvc_shutdown_system(LPTSTR lpMsg, BOOL reboot)
{
	HANDLE tknh;				// handle to process token 
	TOKEN_PRIVILEGES tknp;		// pointer to token structure 
	BOOL ss_flag;				// system shutdown flag 
 
	/* Get the current process token handle so we can get shutdown  */
	/* privilege. */
	DBG_OUTPUT(TEXT("==> xensvc_shutdown_system ****\n"));
	if (!OpenProcessToken(GetCurrentProcess(), 
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tknh)) {
		xensvc_report_event(XENSVC_SHUTDOWN_NAME,
			TEXT("xensvc failed OpenProcessToken")); 
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
		xensvc_report_event(XENSVC_SHUTDOWN_NAME,
			TEXT("xensvc failed AdjustTokenPrivileges")); 
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
		XENSVC_SHUTDOWN_TIMEOUT,
		TRUE,		// force apps closed
		reboot,		// reboot after shutdown 
		SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER |
			SHTDN_REASON_FLAG_PLANNED);
 
	if (!ss_flag) {
		xensvc_report_event(XENSVC_SHUTDOWN_NAME,
			TEXT("xensvc failed InitiateSystemShutdown")); 
		return FALSE; 
	}
 
	/* Disable shutdown privilege. */
	tknp.Privileges[0].Attributes = 0; 
	AdjustTokenPrivileges(tknh, FALSE, &tknp, 0, 
		(PTOKEN_PRIVILEGES) NULL, 0); 
 
	DBG_OUTPUT(TEXT("<== xensvc_shutdown_system ****\n"));
	return TRUE; 
}

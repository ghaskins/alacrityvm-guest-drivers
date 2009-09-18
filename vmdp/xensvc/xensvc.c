/*****************************************************************************
 *
 *   File Name:      xensvc.c
 *
 *   %version:       2 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Sep 08 10:09:59 2008 %
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

SERVICE_STATUS				g_xensvc_shutdown_status; 

static TCHAR				g_xensvc_err[MAX_PATH];

static LPCWSTR g_xensvc_name[] = {
	XENSVC_SHUTDOWN_NAME,
};
static DWORD g_xensvc_number = sizeof(g_xensvc_name) / sizeof(LPCWSTR);

static LPCWSTR g_xensvc_display_name[] = {
	XENSVC_SHUTDOWN_DISPLAY_NAME,
};

static SERVICE_STATUS *g_xensvc_status[] = {
	&g_xensvc_shutdown_status,
}; 

static void xensvc_usage(void);
static VOID xensvc_install(void);
static VOID xensvc_remove(void);
static LPTSTR xensvc_get_last_error_text(LPTSTR lpszBuf, DWORD dwSize);

/*
* Purpose: 
*   Entry point for the process
*
* Parameters:
*   None
* 
* Return value:
*   None
*/
int __cdecl
wmain()
{ 
	xenbus_register_shutdown_event_t ioctl;
    ULONG ulReturnedLength;
	ULONG notify;
	HKEY hkey;

	/* Add any additional services for the process to this table. */
	SERVICE_TABLE_ENTRY dispatch_table[] = { 
		{XENSVC_SHUTDOWN_NAME, (LPSERVICE_MAIN_FUNCTION)xensvc_shutdown_main}, 
		{NULL, NULL}
	}; 
	LPWSTR *wargv;
	int argc;

	DBG_OUTPUT(TEXT("==> wmain ****\n"));

	wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if( NULL == wargv ) {
		DBG_OUTPUT(TEXT("    CommandLineToArgvW filed ****\n"));
		return 0;
	}

	DBG_OUTPUT(TEXT("    checking argc ****\n"));
	if (argc > 1) {
		if (_wcsicmp(wargv[1], TEXT("install")) == 0 ) {
			DBG_OUTPUT(TEXT("    calling xensvc_install ****\n"));
			xensvc_install();
		}
		else if (_wcsicmp(wargv[1], TEXT("remove")) == 0 ) {
			DBG_OUTPUT(TEXT("    calling xensvc_remove ****\n"));
			xensvc_remove();
		}
		else {
			xensvc_usage();
		}
		LocalFree(wargv);
		return 1;
	}
	LocalFree(wargv);
 
	/* Open the registr and write that we want to be notified */
	/* when a xm shutdown or reboot is requested. */
	if(ERROR_SUCCESS != RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		TEXT(XENBUS_HKLM_DEVICE_KEY),
		0,
		KEY_ALL_ACCESS,
		&hkey)) 
	{
		DBG_OUTPUT(TEXT("    failed to open device key ****\n"));
		return 0;
	}
	notify = XENBUS_WANTS_SHUTDOWN_NOTIFICATION;
	RegSetValueEx(
		hkey,
		TEXT(XENBUS_SHUTDOWN_NOTIFICATION_STR),
		0,
		REG_DWORD,
		(PBYTE)&notify,
		sizeof(notify));

	RegCloseKey(hkey);

	/* This call returns when the service has stopped. */
	/* The process should simply terminate when the call returns. */
	DBG_OUTPUT(TEXT("    StartServiceCtrlDispatcher ****\n"));
	if (!StartServiceCtrlDispatcher(dispatch_table)) { 
		DBG_OUTPUT(TEXT("    StartServiceCtrlDispatcher failed ****\n"));
		return 0;
	}	 
	DBG_OUTPUT(TEXT("    wmain CloseHandle ****\n"));
	DBG_OUTPUT(TEXT("<== wmain ****\n"));
	return 1;
} 

void
xensvc_usage(void)
{
	HINSTANCE hinst;
	TCHAR title[16];
	TCHAR msg[128];
	
	hinst = GetModuleHandle(NULL);
	
	LoadString(hinst, MBOX_XENSVC_TITLE, title,
		sizeof(title)/sizeof(title[0]) - 1);
	LoadString(hinst, MBOX_XENSVC_TEXT, msg, sizeof(msg)/sizeof(msg[0]) - 1);

	MessageBox(NULL, msg, title, MB_OK);
}

static VOID
xensvc_install()
{
	SC_HANDLE sc_mgr;
	SC_HANDLE sc_svc;
	TCHAR sz_path[MAX_PATH];
	DWORD i;

	DBG_OUTPUT(TEXT("==> xensvc_install ****\n"));
	if(!GetModuleFileName(NULL, sz_path, MAX_PATH)) {
		DBG_OUTPUT(TEXT("    GetModuleFileName failed ****\n"));
		return;
	}

	/* Get a handle to the SCM database. */
	DBG_OUTPUT(TEXT("    OpenSCManager ****\n"));
	sc_mgr = OpenSCManager( 
		NULL,						// local computer
		NULL,						// ServicesActive database 
		SC_MANAGER_ALL_ACCESS);		// full access rights 
	
	if (NULL == sc_mgr) {
		DBG_OUTPUT(TEXT("    OpenSCManager failed ****\n"));
		return;
	}

	for (i = 0; i < g_xensvc_number; i++) {
		DBG_OUTPUT(TEXT("    CreateService ****\n\t"));
		DBG_OUTPUT(g_xensvc_name[i]);
		sc_svc = CreateService( 
			sc_mgr,						// SCM database 
			g_xensvc_name[i],			// name of service 
			g_xensvc_display_name[i],	// service name to display 
			SERVICE_ALL_ACCESS,			// desired access 
			SERVICE_WIN32_OWN_PROCESS,	// service type 
			SERVICE_AUTO_START,			// start type 
			SERVICE_ERROR_NORMAL,		// error control type 
			sz_path,					// path to service's binary 
			NULL,						// no load ordering group 
			NULL,						// no tag identifier 
			NULL,						// no dependencies 
			NULL,						// LocalSystem account 
			NULL);						// no password 
 
		if (sc_svc == NULL) {
			DBG_OUTPUT(TEXT("\n    CreateService failed ****\n"));
		}
		else {
			DBG_OUTPUT(TEXT("\n    CreateService succeeded ****\n"));
			CloseServiceHandle(sc_svc); 
		}
	}

	CloseServiceHandle(sc_mgr);
	DBG_OUTPUT(TEXT("<== xensvc_install ****\n"));
}

static void
xensvc_remove()
{
	SC_HANDLE   sc_svc;
	SC_HANDLE   sc_mgr;
	DWORD		i;
	ULONG		notify;
	HKEY		hkey;

	DBG_OUTPUT(TEXT("==> xensvc_remove ****\n"));
	sc_mgr = OpenSCManager(
		NULL,					// machine (NULL == local)
		NULL,					// database (NULL == default)
		SC_MANAGER_CONNECT);	// access required

	if (sc_mgr) {
		for (i = 0; i < g_xensvc_number; i++) {
			sc_svc = OpenService(sc_mgr, g_xensvc_name[i],
				DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);

			if (sc_svc) {
				/* try to stop the service */
				if (ControlService(sc_svc, SERVICE_CONTROL_STOP,
					g_xensvc_status[i])) {
					DBG_OUTPUT(g_xensvc_name[i]);
					Sleep(1000);

					while (QueryServiceStatus(sc_svc, g_xensvc_status[i])) {
						if (g_xensvc_status[i]->dwCurrentState ==
								SERVICE_STOP_PENDING){
							DBG_OUTPUT(TEXT("."));
							Sleep( 1000 );
						}
						else
							break;
					}

					if (g_xensvc_status[i]->dwCurrentState == SERVICE_STOPPED) {
						DBG_OUTPUT(TEXT("\n    stopped.\n"));
					}
					else {
						DBG_OUTPUT(TEXT("\n    failed to stop.\n"));
					}
				}

				/* now remove the service */
				if (DeleteService(sc_svc)) {
					DBG_OUTPUT(TEXT("    Service deleted.\n"));
				}
				else {
					DBG_OUTPUT(TEXT("    DeleteService failed\n"));
				}

				CloseServiceHandle(sc_svc);

				/* We no longer want shutdown notifications. */
				if(ERROR_SUCCESS == RegOpenKeyEx(
					HKEY_LOCAL_MACHINE,
					TEXT(XENBUS_HKLM_DEVICE_KEY),
					0,
					KEY_ALL_ACCESS,
					&hkey)) 
				{
					notify = XENBUS_NO_SHUTDOWN_NOTIFICATION;
					RegSetValueEx(
						hkey,
						TEXT(XENBUS_SHUTDOWN_NOTIFICATION_STR),
						0,
						REG_DWORD,
						(PBYTE)&notify,
						sizeof(notify));

					RegCloseKey(hkey);
				}
			}
			else {
				DBG_OUTPUT(TEXT("    OpenService failed\n"));
			}
		}

		CloseServiceHandle(sc_mgr);
	}
	else {
		DBG_OUTPUT(TEXT("OpenSCManager failed\n"));
	}

	DBG_OUTPUT(TEXT("<== xensvc_remove ****\n"));
}

static LPTSTR
xensvc_get_last_error_text(LPTSTR lpszBuf, DWORD dwSize)
{
	DWORD dwRet;
	LPTSTR lpszTemp = NULL;

	DBG_OUTPUT(TEXT("==> xensvc_get_last_error_text ****\n"));
	dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL,
		GetLastError(),
		LANG_NEUTRAL,
		(LPTSTR)&lpszTemp,
		0,
		NULL);

	/* supplied buffer is not long enough */
	if (!dwRet || ( (long)dwSize < (long)dwRet+14))
		lpszBuf[0] = TEXT('\0');
	else {
		if (NULL != lpszTemp) {
			/* remove cr and newline character */
			lpszTemp[lstrlen(lpszTemp)-2] = TEXT('\0');
			_stprintf_s(lpszBuf, dwSize, TEXT("%s (0x%x)"),
				lpszTemp, GetLastError());
		}
	}

	if (NULL != lpszTemp)
		LocalFree((HLOCAL) lpszTemp);

	DBG_OUTPUT(TEXT("<== xensvc_get_last_error_text ****\n"));
	return lpszBuf;
}

/*
* Purpose: 
*   Sets the current service status and reports it to the SCM.
*
* Parameters:
*   dwCurrentState - The current state (see SERVICE_STATUS)
*   dwWin32ExitCode - The system error code
*   dwWaitHint - Estimated time for pending operation, 
*     in milliseconds
* 
* Return value:
*   None
*/
VOID
xensvc_report_status(SERVICE_STATUS_HANDLE ss_handle,
	SERVICE_STATUS *status,
	DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	DBG_OUTPUT(TEXT("==> xensvc_report_status ****\n"));
	status->dwCurrentState = dwCurrentState;
	status->dwWin32ExitCode = dwWin32ExitCode;
	status->dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		status->dwControlsAccepted = 0;
	else
		status->dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED))
		status->dwCheckPoint = 0;
	else
		status->dwCheckPoint = dwCheckPoint++;

	DBG_OUTPUT(TEXT("     SetServiceStatusn"));
	SetServiceStatus(ss_handle, status);
	DBG_OUTPUT(TEXT("<== xensvc_report_status ****\n"));
}

/*
* Purpose: 
*   Logs messages to the event log
*
* Parameters:
*   szFunction - name of function that failed
* 
* Return value:
*   None
*
* Remarks:
*   The service must have an entry in the Application event log.
*/
VOID
xensvc_report_event(LPCWSTR service, LPTSTR szFunction) 
{ 
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];
	TCHAR Buffer[80];

	DBG_OUTPUT(TEXT("==> xensvc_report_event ****\n"));

	hEventSource = RegisterEventSource(NULL, service);
	if( NULL != hEventSource ) {
		StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"),
			szFunction, GetLastError());

		lpszStrings[0] = service;
		lpszStrings[1] = Buffer;

		DBG_OUTPUT(szFunction);
		ReportEvent(hEventSource,	// event log handle
			EVENTLOG_ERROR_TYPE,	// event type
			0,						// event category
			0, //SVC_ERROR,         // event identifier
			NULL,					// no security identifier
			2,						// size of lpszStrings array
			0,						// no binary data
			lpszStrings,			// array of strings
			NULL);					// no binary data

		DeregisterEventSource(hEventSource);
	}
	DBG_OUTPUT(TEXT("<== xensvc_report_event ****\n"));
}

/**********************************************************************
*
*	Header %name:	win_commonkey.h %
*	Instance:		platform_1
*	Description:	
*	%created_by:	kallan %
*	%date_created:	Tue May 12 12:09:08 2009 %
*
**********************************************************************/
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

#ifndef _platform_1_win_commonkey_h_H
#define _platform_1_win_commonkey_h_H

// FILE TO KEEP keys common between setup and unistall!



// Install Flag key defines
#define RKEY_S_NOVELL	"SOFTWARE\\Novell"
#define RKEY_INSTALL_FLAG "XenPVD"
#define RKEY_S_NOVELL_XENPVD "SOFTWARE\\Novell\\XenPVD"

// Run Once registry keys
#define RKEY_S_RUNONCE "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
#define RKEY_S_RUNONCE_EX "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnceEx"
#define RKEY_S_RUNONCE_VN "XenUnInstall"
#define RKEY_S_RUNONCE_UPDATE_VN "XenUpdate"

// Use PV driver keys
#define RKEY_SYS_PVDRIVERS "SYSTEM\\CurrentControlSet\\Services\\xenbus\\Parameters\\Device"
#define RKEY_SYS_PVDRIVERS_VN "use_pv_drivers"

#define RKEY_PROC_ID "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"
#define RKEY_PROC_ID_NAME "Identifier"

#define XENBUS_GUID "{4d36e97d-e325-11ce-bfc1-08002be10318}"
#define XENBLK_GUID "{4D36E97B-E325-11CE-BFC1-08002BE10318}"
#define XENNET_GUID "{4d36e972-e325-11ce-bfc1-08002be10318}"

#define XENBUS_MATCHING_ID "root\\xenbus"
#define XENBLK_MATCHING_ID "pci\\ven_5853&dev_0001"
#define XENNET_MATCHING_ID "xen\\type_vif"

#define ENUM_SYSTEM "Root\\SYSTEM"
#define ENUM_PCI "PCI"
#define ENUM_XEN "XEN"

#define XENSETUP_EXE "setup.exe"
#define XENUNINST_EXE "uninstall.exe"

#define XENBUS_INF "xenbus.inf"
#define XENBLK_INF "xenblk.inf"
#define XENNET_INF "xennet.inf"

#define XENBUS_SYS "xenbus.sys"
#define XENBLK_SYS "xenblk.sys"
#define XENNET_SYS "xennet.sys"

#define XENBUS_CAT "xenbus.cat"
#define XENBLK_CAT "xenblk.cat"
#define XENNET_CAT "xennet.cat"

#define XENBUS_DFX "xenbus.dfx"
#define XENBLK_DFX "xenblk.dfx"
#define XENNET_DFX "xennet.dfx"

#define XENBUS_RST "xenbus.rst"
#define XENBLK_RST "xenblk.rst"
#define XENNET_RST "xennet.rst"

#define XENBUS_PNF "xenbus.PNF"
#define XENBLK_PNF "xenblk.PNF"
#define XENNET_PNF "xennet.PNF"

#define XENSVC_EXE "xensvc.exe"

#define XENBUS_DESC "SUSE Bus"
#define XENBLK_DESC "SUSE Block"
#define XENNET_DESC "SUSE Network"

#define RKEY_CLASS "SYSTEM\\CurrentControlSet\\Control\\Class"
#define RKEY_REINSTALL "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Reinstall"
#define RKEY_DRIVER_STORE "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DIFx\\DriverStore"

#define MAX_INF_LEN						16
#define MAX_DIFX_LEN					64
#define MAX_REINSTALL_LEN				8
#define MAX_DRV_LEN						8
#define MAX_MATCHING_ID_LEN				48
#define MAX_INF_FILE_LEN				24
#define MAX_DESC_LEN					128

#define XENBUS_F						1
#define XENBLK_F						2
#define XENNET_F						4

#endif

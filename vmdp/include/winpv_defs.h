/*****************************************************************************
 *
 *   File Name:      winpv_devs.h
 *
 *   %version:       6 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jun 15 08:41:44 2009 %
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

#ifndef _WINPV_DEFS_H
#define _WINPV_DEFS_H

#define XEN_INT_NOT_XEN 0x0
#define XEN_INT_DISK	0x1
#define XEN_INT_OTHER	0x2
#define XEN_INT_LAN		0x2
#define XEN_INT_XS		0x4

#define XENBUS_DEVICE_NAME_WSTR			L"\\Device\\XenBus"
#define XENBUS_SYMBOLIC_NAME_WSTR		L"\\DosDevices\\XenBus"
#define XENBUS_SERVICE_NAME_STRING		TEXT("\\\\.\\XenBus")
#define SYSTEM_START_OPTIONS_LEN 		512
#define SAFE_BOOT_WSTR					L"SAFEBOOT"
#define SYSTEM_START_OPTIONS_WSTR		L"SystemStartOptions"
#define USE_PV_DRIVERS_WSTR				L"use_pv_drivers"
#define XENBUS_DEVICE_KEY_WSTR			L"xenbus\\Parameters\\Device"
#define XENBUS_HKLM_DEVICE_KEY			"SYSTEM\\CurrentControlSet\\Services\\xenbus\\Parameters\\Device"

#define XENBUS_FULL_DEVICE_KEY_WSTR				L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\xenbus\\Parameters\\Device"
#define XENBUS_SHUTDOWN_WSTR					L"shutdown"
#define XENBUS_SHUTDOWN_STR						"shutdown"
#define XENBUS_SHUTDOWN_NOTIFICATION_WSTR		L"shutdown_notification"
#define XENBUS_SHUTDOWN_NOTIFICATION_STR		"shutdown_notification"

#define XENBUS_NO_SHUTDOWN_NOTIFICATION			0
#define XENBUS_WANTS_SHUTDOWN_NOTIFICATION		1
#define XENBUS_UNDEFINED_SHUTDOWN_NOTIFICATION	(uint32_t)(-1)

#define XENBUS_REG_SHUTDOWN_VALUE				1
#define XENBUS_REG_REBOOT_VALUE					2

#define XENBUS_PROBE_PV_DISK					0x001
#define XENBUS_PROBE_PV_NET						0x002

/* Xenblk will control all disks unless any of the following are set. */
/* If set, xenblk will not controly the type of disk specified. */
#define XENBUS_PROBE_PV_OVERRIDE_FILE_DISK		0x004
#define XENBUS_PROBE_PV_OVERRIDE_ALL_PHY_DISK	0x008
#define XENBUS_PROBE_PV_OVERRIDE_ALL_BY_DISK	0x010
#define XENBUS_PROBE_PV_OVERRIDE_BY_ID_DISK		0x020
#define XENBUS_PROBE_PV_OVERRIDE_BY_NAME_DISK	0x040
#define XENBUS_PROBE_PV_OVERRIDE_BY_PATH_DISK	0x080
#define XENBUS_PROBE_PV_OVERRIDE_BY_UUID_DISK	0x100

/* Xenblk will control all disks but IOEMU disks marked as hd<x> disk. */
#define XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK		0x200
#define XENBUS_PROBE_PV_DISK_NET_NO_IOEMU		\
	(XENBUS_PROBE_PV_DISK						\
		| XENBUS_PROBE_PV_NET					\
		| XENBUS_PROBE_PV_OVERRIDE_IOEMU_DISK)

#define XENBUS_SHOULD_NOT_CREATE_SYMBOLIC_LINK	0x80000000

#define XENBUS_PROBE_PV_OVERRIDE_DISK_MASK		\
	(XENBUS_PROBE_PV_OVERRIDE_FILE_DISK			\
	| XENBUS_PROBE_PV_OVERRIDE_ALL_PHY_DISK		\
	| XENBUS_PROBE_PV_OVERRIDE_ALL_BY_DISK		\
	| XENBUS_PROBE_PV_OVERRIDE_BY_ID_DISK		\
	| XENBUS_PROBE_PV_OVERRIDE_BY_NAME_DISK		\
	| XENBUS_PROBE_PV_OVERRIDE_BY_PATH_DISK		\
	| XENBUS_PROBE_PV_OVERRIDE_BY_UUID_DISK)

#define XENBUS_PV_ALL_PORTOFFSET		4
#define XENBUS_PV_SPECIFIC_PORTOFFSET	8
#define XENBUS_PV_PORTOFFSET_DISK_VALUE	1
#define XENBUS_PV_PORTOFFSET_NET_VALUE	2
#define XENBUS_PV_PORTOFFSET_ALL_VALUE	1

#define XENBLK_MAXIMUM_TARGETS 			32

#define XENBUS_NORMAL_INIT				1
#define XENBUS_RESUME_INIT				2
#define XENBUS_CRASHDUMP_INIT			4

#define OP_MODE_NORMAL			1
#define OP_MODE_HIBERNATE		2
#define OP_MODE_CRASHDUMP		4
#define OP_MODE_SHUTTING_DOWN	8
#define OP_MODE_DISCONNECTED	0x10

/* Match to xen/public/sched.h */
#define XENBUS_SHUTDOWN		0  /* Domain exited normally. Clean up and kill. */
#define XENBUS_REBOOT		1  /* Clean up, kill, and then restart.          */
#define XENBUS_SUSPEND		2  /* Clean up, save suspend info, kill.         */
#define XENBUS_CRASH		3  /* Tell controller we've crashed.             */
#define XENBUS_HALT			4
#define XENBUS_DEBUG_DUMP	5

typedef struct xenbus_register_shutdown_event_s
{
    LIST_ENTRY list;
	void *irp;
	unsigned long shutdown_type;
} xenbus_register_shutdown_event_t;

#define IOCTL_XENBUS_REGISTER_SHUTDOWN_EVENT \
   CTL_CODE( FILE_DEVICE_VMBUS, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS )
#endif

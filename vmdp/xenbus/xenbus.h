/*****************************************************************************
 *
 *   File Name:      xenbus.h
 *   Created by:     
 *   Date created:   
 *
 *   %version:       41 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jul 06 15:31:21 2009 %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2006-2009 Unpublished Work of Novell, Inc. All Rights Reserved.
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

#ifndef _XENBUSDRV_H
#define _XENBUSDRV_H

#include <ntddk.h>
#include <wdmsec.h>
#include <initguid.h>
#include "guid.h"

#define NTSTRSAFE_NO_DEPRECATE
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#define __XEN_INTERFACE_VERSION__ 0x00030204
#define __BUSDRV__
#include <asm/win_compat.h>
#include <asm/win_hypervisor.h>
#include <xen/public/win_xen.h>
#include <xen/public/version.h>
#include <xen/public/memory.h>
#include <win_xeninfo.h>
#include <win_features.h>
#include <win_gnttab.h>
#include <win_evtchn.h>
#include <win_hvm.h>
#include <win_xenbus.h>
#include "xen_support.h"
#include "xenbus_comms.h"
#include "xenbus_ver.h"


#ifdef DBG
#define DPRINTK(_X_) //XENBUS_PRINTK _X_
#define DPR_INIT(_X_) XENBUS_PRINTK _X_
//#define DPR_EARLY(_X_) DbgPrint _X_
#define DPR_EARLY(_X_) XENBUS_PRINTK _X_

#define DPRINT(_X_)
//#define DPR_WAIT(_X_) if (DBG_WAIT) XENBUS_PRINTK _X_
//#define DPR_XS(_X_) if (DBG_XS) XENBUS_PRINTK _X_
#define DPR_WAIT(_X_)
#define DPR_XS(_X_)
#define DPR_PNP(_X_)
#else
#define DPRINT(_X_)
#define DPR_INIT(_X_)
#define DPR_WAIT(_X_)
#define DPR_XS(_X_)
#define DPR_PNP(_X_)
#define DPR_EARLY(_X_)
#endif

/* Always print errors. */
#define DBG_PRINT(_X_) XENBUS_PRINTK _X_
#define XENBUS_PRINTK_PORT		0xe9

#define XENBUS_POOL_TAG (ULONG) 'neXp'

#define XB_EVENT	0x1
#define XS_LIST		0x2
#define XS_REQUEST	0x4

#define X_CMP		0x1
#define X_GNT		0x2
#define X_DPC		0x4
#define X_RPL		0x8
#define X_WAT		0x10
#define X_XSL		0x20
#define X_WEL		0x40

#define GNTTAB_SUSPEND_F				0x001
#define XEN_INFO_CLEANUP_F				0x002
#define XEN_STORE_INTERFACE_NULL_F		0x004
#define XENBUS_RELEASE_DEVICE_F			0x008
#define INITIALIZE_HYPERCALL_PAGE_F		0x010
#define XEN_INFO_INIT_F					0x020
#define GNTTAB_INIT_F					0x040
#define EVTCHN_INIT_F					0x080
#define XEN_FINISH_INIT_F				0x100
#define GNTTAB_FINISH_INIT_F			0x200
#define XS_INIT_F						0x400
#define XB_COMMS_INIT_F					0x800
#define READ_REPLY_F					0x10000
#define XB_WRITE_F						0x20000
#define EVTCHN_F						0x40000

//#define XENBUG_TRACE_FLAGS 1
#if defined XENBUG_TRACE_FLAGS || defined DBG
#define XENBUS_INIT_FLAG(_F, _V)		((_F) = (_V))
#define XENBUS_SET_FLAG(_F, _V)			((_F) |= (_V))
#define XENBUS_CLEAR_FLAG(_F, _V)		((_F) &= ~(_V))
#else
#define XENBUS_INIT_FLAG(_F, _V)
#define XENBUS_SET_FLAG(_F, _V)
#define XENBUS_CLEAR_FLAG(_F, _V)
#endif

#define NR_RESERVED_ENTRIES 8

#define PRINTF_BUFFER_SIZE 4096

typedef enum _PNP_STATE {

    NotStarted = 0,         // Not started yet
    Started,                // Device has received the START_DEVICE IRP
    StopPending,            // Device has received the QUERY_STOP IRP
    Stopped,                // Device has received the STOP_DEVICE IRP
    RemovePending,          // Device has received the QUERY_REMOVE IRP
    SurpriseRemovePending,  // Device has received the SURPRISE_REMOVE IRP
    Deleted,                // Device has received the REMOVE_DEVICE IRP
    UnKnown                 // Unknown state

} PNP_STATE;

typedef struct _COMMON_DEVICE_EXTENSION
{
    BOOLEAN IsFdo;
    PDEVICE_OBJECT Self;

    PNP_STATE pnpstate;
    DEVICE_POWER_STATE devpower;
    SYSTEM_POWER_STATE syspower;

} COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;

typedef enum _XENBUS_DEVICE_ORIGIN
{
    alloced,
    created,
} XENBUS_DEVICE_ORIGIN;

/* child PDOs device extension */
typedef struct _PDO_DEVICE_EXTENSION
{
    COMMON_DEVICE_EXTENSION;

    PDEVICE_OBJECT ParentFdo;

    PCHAR Nodename;

    UNICODE_STRING HardwareIDs;

    XENBUS_DEVICE_TYPE Type;
    XENBUS_DEVICE_SUBTYPE subtype;
	XENBUS_DEVICE_ORIGIN origin;

    PCHAR BackendID;
    PCHAR Otherend;
	void *frontend_dev;
	void *controller;
	uint32_t (*ioctl)(void *, pv_ioctl_t);

    LIST_ENTRY Link;

    BOOLEAN Present;
    BOOLEAN ReportedMissing;
    UCHAR Reserved[2];

    ULONG InterfaceRefCount;
    ULONG PagingPathCount;
    ULONG DumpPathCount;
    ULONG HibernationPathCount;
    KEVENT PathCountEvent;

} PDO_DEVICE_EXTENSION, *PPDO_DEVICE_EXTENSION;

/* FDO device extension as function driver */
typedef struct _FDO_DEVICE_EXTENSION
{
    COMMON_DEVICE_EXTENSION;

	uint32_t sig;
    PDEVICE_OBJECT Pdo;
    PDEVICE_OBJECT LowerDevice;
    UNICODE_STRING ifname;
    IO_REMOVE_LOCK RemoveLock;
	//KDPC dpc;
    FAST_MUTEX Mutex;
	SYSTEM_POWER_STATE power_state;
    LIST_ENTRY ListOfPDOs;

    ULONG NumPDOs;
	uint64_t mmio;
	uint8_t *mem;
	uint32_t mmiolen;
	uint32_t device_irq;
	grant_ref_t *gnttab_list;
	grant_ref_t *gnttab_free_head;
	int *gnttab_free_count;
	void *info;
    LIST_ENTRY shutdown_requests;
	KSPIN_LOCK qlock;
	PIRP irp;
	PIO_WORKITEM item;
	void *disk_controller;
	uint32_t (*disk_ioctl)(void *, pv_ioctl_t);
#ifdef XENBUS_HAS_DEVICE
    PUCHAR PortBase;
    ULONG NumPorts;
    BOOLEAN MappedPort;
#endif
} FDO_DEVICE_EXTENSION, *PFDO_DEVICE_EXTENSION;

struct xs_stored_msg
{
    LIST_ENTRY list;
    struct xsd_sockmsg hdr;
    union
    {
        /* Queued replies */
        struct
        {
            char *body;
        } reply;

        /* Queued watch events */
        struct
        {
            struct xenbus_watch *handle;
            char **vec;
            unsigned int vec_size;
        } watch;
    } u;
};

#ifdef XENBUS_HAS_DEVICE
extern PKINTERRUPT DriverInterruptObj;
#endif
extern PDEVICE_OBJECT gfdo;
extern PFDO_DEVICE_EXTENSION gfdx;
extern shared_info_t *shared_info_area;
extern uint32_t use_pv_drivers;
extern grant_ref_t g_gnttab_list[MAX_NR_GRANT_ENTRIES];
extern grant_ref_t g_gnttab_free_head;
extern int g_gnttab_free_count;
extern void *info[XENBLK_MAXIMUM_TARGETS];
extern KSPIN_LOCK xenbus_print_lock;
extern struct xenbus_watch vbd_watch;
#if defined XENBUG_TRACE_FLAGS || defined DBG
extern uint32_t rtrace;
extern uint32_t xenbus_locks;
#endif
#ifdef DBG
extern uint32_t evt_print;
#endif


#define PDX_TO_FDX(_pdx)                        \
    ((PFDO_DEVICE_EXTENSION) (_pdx->ParentFdo->DeviceExtension))


#ifdef XENBUS_HAS_DEVICE
BOOLEAN
XenbusOnInterrupt(
  IN PKINTERRUPT InterruptObject,
  IN PFDO_DEVICE_EXTENSION fdx
  );
#else
#define FDOSetResources(_fdx, _raw,	_translated) STATUS_SUCCESS
#endif

/* function device subdispatch routines */

NTSTATUS
FDO_Pnp(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  );

NTSTATUS
PDO_Pnp(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  );
PPDO_DEVICE_EXTENSION
xenbus_find_pdx_from_nodename(
	PFDO_DEVICE_EXTENSION fdx,
	char *nodename
	);

NTSTATUS
XenbusInitializePDO(
  PDEVICE_OBJECT fdo,
  const char *type,
  char * multiID
  );

NTSTATUS
XenbusDestroyPDO(
  PDEVICE_OBJECT pdo
  );

NTSTATUS
set_callback_irq(int irq);

VOID
gnttab_suspend(void);

NTSTATUS
gnttab_init(void);

NTSTATUS
gnttab_finish_init(PDEVICE_OBJECT fdo, uint32_t reason);

void
evtchn_remove_queue_dpc(void);

VOID
evtchn_init(void);

#ifdef XENBUS_HAS_DEVICE
BOOLEAN EvtchnISR(void *context);
#else
ULONG EvtchnISR(void);
#endif

KDEFERRED_ROUTINE xenbus_invalidate_relations;

void
xb_read_msg(void);

void
xenbus_watch(IN PDEVICE_OBJECT DeviceObject, PDEVICE_OBJECT fdo);

IO_WORKITEM_ROUTINE xenbus_watch_work;

KDEFERRED_ROUTINE XenbusDpcRoutine;

void
xenbus_copy_fdx(PFDO_DEVICE_EXTENSION dfdx, PFDO_DEVICE_EXTENSION sfdx);

NTSTATUS
xenbus_ioctl(PFDO_DEVICE_EXTENSION fdx, PIRP Irp);

NTSTATUS
xenbus_open_device_key(HANDLE *registryKey);

void xenbus_shutdown_setup(uint32_t *shutdown, uint32_t *notify);

#endif

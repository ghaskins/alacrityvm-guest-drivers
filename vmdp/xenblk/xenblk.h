/*****************************************************************************
 *
 *   File Name:      xenblk.h
 *   Created by:     KRA
 *   Date created:   12-07-06
 *
 *   %version:       45 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 16:23:23 2009 %
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

#ifndef _XENBLK_H_
#define _XENBLK_H_

#include <ntddk.h>
#include <ntdddisk.h>
#ifdef XENBLK_STORPORT
#include <storport.h>
#else
#include <scsi.h>
//#include <srb.h>
#endif
#include "xenblk_ver.h"

#define __XEN_INTERFACE_VERSION__ 0x00030202
#include <asm/win_compat.h>
#include <xen/public/win_xen.h>
#include <xen/public/grant_table.h>
#ifdef ARCH_x86_64
#pragma pack(8)
#else
#pragma pack(4)
#endif
#include <xen/public/io/blkif.h>
#pragma pack()
#include <xen/public/io/protocols.h>
#include <win_gnttab.h>
#include <win_xenbus.h>
#include <win_evtchn.h>
#include <win_maddr.h>

#define NT_DEVICE_NAME				L"\\Device\\XenBlk"
#define DOS_DEVICE_NAME				L"\\DosDevices\\"
#define XENBLK_DESIGNATOR_STR		"Xen Virtual Block Device"

#define XENBLK_TAG_GENERAL			'GneX'  // "XenG" - generic tag

#define XENBLK_MEDIA_TYPE			FixedMedia //0xF8

#define FLAG_LINK_CREATED			0x00000001

#define RMLOCK_TAG					'LneX'
#define RMLOCK_MAXIMUM				1	// Max minutes system allows lock to be held
#define RMLOCK_HIGHWATER			10	// Max number of irps holding lock at one time

#define REQUEST_ALLOCED				0x1
#define VIRTUAL_ADDR_ALLOCED		0x2
#define XENBLK_MAX_SGL_ELEMENTS		32

#define BLK_RING_SIZE __WIN_RING_SIZE((blkif_sring_t *)0, PAGE_SIZE)

#ifndef unlikely
#define unlikely
#endif
#ifndef inline
#define inline __inline
#endif

#ifdef DBG
#define DPRINTK(_X_) XENBUS_PRINTK _X_
#define DPR_INIT(_X_) XENBUS_PRINTK _X_
#define DPR_INTERRUPT(_X_)
#define DPR_EARLY(_X_) DbgPrint _X_
#define DPR_IO(_X_)
#define DPRINT(_X_)
#define DPR_MM(_X_)
#define DPR_TRC(_X_)
#else
#define DPRINTK(_X_)
#define DPR_IO(_X_)
#define DPR_INIT(_X_)
#define DPR_INTERRUPT(_X_)
#define DPR_EARLY(_X_)
#define DPRINT(_X_)
#define DPR_MM(_X_)
#define DPR_TRC(_X_)
#endif

#define CRASHDUMP_LEVEL			IPI_LEVEL

/* Always print errors. */
#define DBG_PRINT(_X_) XENBUS_PRINTK _X_

#define BLKIF_STATE_DISCONNECTED 0
#define BLKIF_STATE_CONNECTED    1
#define BLKIF_STATE_SUSPENDED    2

#define WORKING					0x001
#define PENDINGSTOP				0x002
#define PENDINGREMOVE			0x004
#define SURPRISEREMOVED			0x008
#define REMOVED					0x010
#define REMOVING				0x020
#define RESTARTING				0x040
#define RESTARTED				0x080
#define INITIALIZING			0x100
#define STOPPED					0x200

#ifdef XENBLK_REQUEST_VERIFIER
#define PAGE_ROUND_UP			2
#else
#define PAGE_ROUND_UP			1
#endif

#ifdef XENBLK_STORPORT
#define XENBLK_LOCK_HANDLE STOR_LOCK_HANDLE
#else
#define XENBLK_LOCK_HANDLE XEN_LOCK_HANDLE
#endif

#ifdef DBG
#define BLK_SIO_L				1
#define BLK_INT_L				2
#define BLK_BLK_L				4
#define BLK_ID_L				8
#define BLK_GET_L				0x10
#define BLK_ADD_L				0x20
#define BLK_CON_L				0x40
#define BLK_FRE_L				0x80
#define BLK_IZE_L				0x100
#define BLK_STI_L				0x200
#define BLK_RBUS_L				0x400
#define BLK_ISR_L				0x800
#define BLK_RDPC_L				0x1000
#define BLK_ACTR_L				0x2000
#define BLK_RSU_L				0x4000

extern uint32_t should_print_int;
#define SHOULD_PRINT_INT 5
#define XENBLK_SET_FLAG(_F, _V)			InterlockedOr(&(_F), (_V))
#define XENBLK_CLEAR_FLAG(_F, _V)		InterlockedAnd(&(_F), ~(_V))
#define XENBLK_ZERO_VALUE(_V)			_V = 0
#define XENBLK_INC(_V)					InterlockedIncrement(&(_V))
#define XENBLK_DEC(_V)					InterlockedDecrement(&(_V))
#else
#define XENBLK_SET_FLAG(_F, _V)
#define XENBLK_CLEAR_FLAG(_F, _V)
#define XENBLK_ZERO_VALUE(_V)
#define XENBLK_INC(_V)
#define XENBLK_DEC(_V)
#endif

#ifndef XENBLK_STORPORT
typedef PHYSICAL_ADDRESS STOR_PHYSICAL_ADDRESS;
typedef struct _STOR_SCATTER_GATHER_ELEMENT {
    STOR_PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Length;
    ULONG_PTR Reserved;
} STOR_SCATTER_GATHER_ELEMENT;

typedef struct _STOR_SCATTER_GATHER_LIST {
    ULONG NumberOfElements;
    ULONG_PTR Reserved;
    STOR_SCATTER_GATHER_ELEMENT List[1];
} STOR_SCATTER_GATHER_LIST;
#endif

typedef struct _xenblk_srb_extension {
	uint8_t *va;
    STOR_PHYSICAL_ADDRESS pa;
	STOR_SCATTER_GATHER_LIST *sgl;
	STOR_SCATTER_GATHER_LIST *sys_sgl;
	uint8_t working_sgl_buf[sizeof(STOR_SCATTER_GATHER_LIST) +
		(sizeof(STOR_SCATTER_GATHER_ELEMENT) * XENBLK_MAX_SGL_ELEMENTS)];
	STOR_SCATTER_GATHER_LIST *working_sgl;
#ifdef XENBLK_STORPORT
	void *sa[XENBLK_MAX_SGL_ELEMENTS];
#else
	void *sa[1];
	STOR_SCATTER_GATHER_LIST scsi_sgl;
#endif
	struct _xenblk_srb_extension *next;
	SCSI_REQUEST_BLOCK *srb;
	uint32_t use_cnt;
	uint16_t status;
} xenblk_srb_extension;

struct blk_shadow {
	blkif_request_t req;
	void *request;
	blkif_request_t *bret;
	unsigned long frame[BLKIF_MAX_SEGMENTS_PER_REQUEST];
	xenblk_srb_extension *srb_ext;
#ifdef DBG
	uint32_t seq;
#endif
};

struct blk_req_ring_el {
	uint64_t addr[XENBLK_MAX_SGL_ELEMENTS];
	uint32_t len[XENBLK_MAX_SGL_ELEMENTS];
	uint32_t tlen;
	uint16_t rw;
	uint16_t num_el;
	uint16_t disk;
	uint16_t alloced;
};

struct blk_req_ring {
	struct blk_req_ring_el ring[BLK_RING_SIZE];
	uint32_t prod;
};

struct blk_mm_ring_el {
	void *vaddr;
#ifdef XENBLK_STORPORT
	uint32_t mapped_elements;
	void *mapped_addr[XENBLK_MAX_SGL_ELEMENTS];
	unsigned long mapped_len[XENBLK_MAX_SGL_ELEMENTS];
#endif
};

struct blk_mm_ring {
	struct blk_mm_ring_el ring[BLK_RING_SIZE];
	unsigned long cons;
	unsigned long prod;
};

typedef struct _XENBLK_DEVICE_EXTENSION {
	struct blkfront_info **info;		// xenbus has the array
	//struct blkfront_info *oldinfo[32];
	uint64_t		mmio;
	uint32_t		mmio_len;
	void			*mem;
	void			*port;
	uint32_t		DevState;			// Current device state
	uint32_t		op_mode;			// operation mode e.g. OP_MODE_NORMAL
	ULONG			vector;
	KIRQL			irql;
    XENBLK_LOCK_HANDLE lh;
	KDPC			restart_dpc;
#ifndef XENBLK_STORPORT
	KSPIN_LOCK		dev_lock;
	KDPC			rwdpc;
#endif
#ifdef DBG
	uint32_t		xenblk_locks;
	uint32_t		cpu_locks;
	uint32_t		alloc_cnt_i;
	uint32_t		alloc_cnt_s;
	uint32_t		alloc_cnt_v;
	struct blk_req_ring req;
#endif
} XENBLK_DEVICE_EXTENSION, *PXENBLK_DEVICE_EXTENSION;

/*
 * We have one of these per vbd, whether ide, scsi or 'other'.  They
 * hang in private_data off the gendisk structure. We may end up
 * putting all kinds of interesting stuff here :-)
 */
struct blkfront_info
{
	XENBLK_DEVICE_EXTENSION *xbdev;
	blkif_vdev_t handle;
	int connected;
	int ring_ref;
	blkif_front_ring_t ring;
	unsigned int evtchn, irq;
	struct xlbd_major_info *mi;
	LIST_ENTRY rq;
#if !defined XENBLK_STORPORT || defined XENBUS_HAS_DEVICE
	KSPIN_LOCK lock;
#endif
	struct xenbus_watch watch;
	struct gnttab_free_callback callback;
	struct blk_shadow shadow[BLK_RING_SIZE];
	uint32_t id[BLK_RING_SIZE];
	unsigned long shadow_free;
	unsigned long sector_size;
	uint64_t sectors;
	unsigned int binfo;
	char *nodename;
	char *otherend;
	domid_t otherend_id;
	uint32_t has_interrupt;
	struct blk_mm_ring mm;
	xenblk_srb_extension *hsrb_ext;
	xenblk_srb_extension *tsrb_ext;

#if 0
	/**
	 * The number of people holding this device open.  We won't allow a
	 * hot-unplug unless this is 0.
	 */
	int users;
#endif
#ifdef DBG
	uint32_t queued_srb_ext;
	uint32_t xenblk_locks;
	uint32_t cpu_locks;
	uint32_t req;
	uint32_t seq;
	uint32_t cseq;
#endif
};


static __inline void
xenblk_add_tail(struct blkfront_info *info, xenblk_srb_extension *srb_ext)
{
	if (info->hsrb_ext == NULL) {
		info->hsrb_ext = srb_ext;
		info->tsrb_ext = srb_ext;
	}
	else {
		info->tsrb_ext->next = srb_ext;
		info->tsrb_ext = srb_ext;
	}
}

static __inline void
xenblk_build_alloced_sgl(uint8_t *va, ULONG tlen,
	STOR_SCATTER_GATHER_LIST *sgl)
{
    STOR_PHYSICAL_ADDRESS pa;
    STOR_PHYSICAL_ADDRESS spa;
	ULONG len;
	uint32_t i;

	DPR_MM(("xenblk_build_alloced_sgl of len %d\n", tlen));
	i = 0;
	while (tlen) {
		spa.QuadPart = __pa(va); 
		sgl->List[i].PhysicalAddress.QuadPart = spa.QuadPart;
		len =  PAGE_SIZE < tlen ? PAGE_SIZE : tlen;
		while (len < tlen) {
			pa.QuadPart = __pa(va + len);
			if (spa.QuadPart + len != pa.QuadPart) {
				break;
			}
			len += len + PAGE_SIZE < tlen ? PAGE_SIZE : tlen - len;
		}
		DPR_MM(("xenblk_build_alloced_sgl [%d] len %d\n", i, len));
		sgl->List[i].Length = len;
		va += len;
		tlen -= len;
		i++;
	}
	DPR_MM(("xenblk_build_alloced_sgl num elements %d\n", i));
	sgl->NumberOfElements = i;
}

#ifdef DBG
static __inline void
xenblk_print_save_req(struct blk_req_ring *req)
{
	uint32_t i;
	uint32_t j;
	uint32_t k;

	DBG_PRINT(("\nBLK_RING_SIZE is %d\n", BLK_RING_SIZE));
	i = req->prod & (BLK_RING_SIZE - 1);
	for (j = 0; j < BLK_RING_SIZE; j++) {
		DBG_PRINT(("%3d Disk %d, op %x, total len %d, elements %d, alloced %d\n",
			i,
			req->ring[i].disk,
			req->ring[i].rw,
			req->ring[i].tlen,
			req->ring[i].num_el,
			req->ring[i].alloced));
		for (k = 0; k < req->ring[i].num_el; k++) {
			DBG_PRINT(("\telemet %d, addr %x, len 0x%x\n", k,
				(uint32_t)req->ring[i].addr[k],
				req->ring[i].len[k]));
		}
		i = (i + 1) & (BLK_RING_SIZE - 1);
	}
	DBG_PRINT(("End Disk request dump.\n"));
}

static __inline void
xenblk_save_req(struct blkfront_info *info,
	SCSI_REQUEST_BLOCK *srb,
	xenblk_srb_extension *srb_ext)
{
	struct blk_req_ring *req;
	unsigned long idx;
	uint32_t i;

	req = &info->xbdev->req;
	idx = req->prod & (BLK_RING_SIZE - 1);

	req->ring[idx].rw = srb->Cdb[0];
	req->ring[idx].num_el = (uint16_t)srb_ext->sys_sgl->NumberOfElements;
	req->ring[idx].tlen = srb->DataTransferLength;
	req->ring[idx].disk = srb->TargetId;
	req->ring[idx].alloced = srb_ext->va ? 1 : 0;
	for (i = 0; i < srb_ext->sys_sgl->NumberOfElements; i++) {
		req->ring[idx].len[i] = srb_ext->sys_sgl->List[i].Length;
		req->ring[idx].addr[i] =
			srb_ext->sys_sgl->List[i].PhysicalAddress.QuadPart;
	}
	req->prod++;
}

static __inline void
xenblk_print_cur_req(struct blkfront_info *info, SCSI_REQUEST_BLOCK *srb)
{
	xenblk_srb_extension *srb_ext;
	struct blk_req_ring *req;
	unsigned long idx;
	uint32_t i;

	if (!srb) {
		return;
	}

	srb_ext = (xenblk_srb_extension *)srb->SrbExtension;
	DBG_PRINT(("rDisk %d, op %x, total len %d, elements %d, alloced %d\n",
		srb->TargetId,
		srb->Cdb[0],
		srb->DataTransferLength,
		srb_ext->sys_sgl->NumberOfElements,
		srb_ext->va ? 1 : 0));
	for (i = 0; i < srb_ext->sys_sgl->NumberOfElements; i++) {
		DBG_PRINT(("\telemet %d, addr %x, len 0x%x\n", i,
			(uint32_t)srb_ext->sys_sgl->List[i].PhysicalAddress.QuadPart,
			srb_ext->sys_sgl->List[i].Length));
	}
}
#else
#define xenblk_print_save_req(_req)
#define xenblk_save_req(_info, _srb, _srb_ext)
#define xenblk_print_cur_req(_info, _srb)
#define XenBlkVerifySGL(_srb_ext, _DataTransferLength)
#endif

#ifdef XENBLK_STORPORT
/***************************** STOR PORT *******************************/
typedef uint64_t xenblk_addr_t;
#define xenblk_pause StorPortPause
#define xenblk_resume StorPortResume
#define xenblk_request_complete StorPortNotification
#define xenblk_notification StorPortNotification
#define	xenblk_next_request(_next, _dev_ext)
#define	xenblk_request_timer_call StorPortNotification
#define xenblk_complete_all_requests StorPortCompleteRequest
#define xenblk_get_physical_address StorPortGetPhysicalAddress
#define xenblk_get_device_base StorPortGetDeviceBase
#define xenblk_build_sgl StorPortGetScatterGatherList
#define xenblk_write_port_ulong StorPortWritePortUlong
#define xenblk_acquire_spinlock(_dext, _plock, _ltype, _lctx, _plhndl)	\
	StorPortAcquireSpinLock((_dext), (_ltype), (_lctx), (_plhndl))
#define xenblk_release_spinlock(_dext, _plock, _lhndl)	\
	StorPortReleaseSpinLock((_dext), &(_lhndl))
#define storport_acquire_spinlock(_dext, _plock, _ltype, _lctx, _plhndl)	\
	StorPortAcquireSpinLock((_dext), (_ltype), (_lctx), (_plhndl))
#define storport_xenblk_release_spinlock(_dext, _plock, _lhndl)	\
	StorPortReleaseSpinLock((_dext), &(_lhndl))

#define scsiport_acquire_spinlock(_plock, _plhndl)
#define scsiport_release_spinlock(_plock, _lhndl)

static __inline void
xenblk_map_system_sgl(SCSI_REQUEST_BLOCK *srb, MEMORY_CACHING_TYPE cache_type)
{
	xenblk_srb_extension *srb_ext;
	uint32_t i;

#ifdef DBG
	KIRQL irql;

	irql = KeGetCurrentIrql();
	if (irql > DISPATCH_LEVEL && irql < HIGH_LEVEL) {
		DPR_INIT(("*** xenblk_map_system_sgl at irql %d *** \n", irql));
	}
#endif
	srb_ext = (xenblk_srb_extension *)srb->SrbExtension;
	ASSERT(srb_ext->sys_sgl->NumberOfElements <= XENBLK_MAX_SGL_ELEMENTS);
	for (i = 0; i < srb_ext->sys_sgl->NumberOfElements; i++) {
		srb_ext->sa[i] = MmMapIoSpace(
			srb_ext->sys_sgl->List[i].PhysicalAddress,
			srb_ext->sys_sgl->List[i].Length,
			cache_type);
		if (srb_ext->sa[i] == NULL) {
			DBG_PRINT(("xenblk_map_system_sgl: MmMapIoSpace failed.\n"));
		}
		DPR_MM(("\tMmMapIoSpace addr = %p, paddr = %lx, len = %x\n",
			srb_ext->sa[i],
			(uint32_t)srb_ext->sys_sgl->List[0].PhysicalAddress.QuadPart,
			srb_ext->sys_sgl->List[0].Length));
	}

	srb_ext->working_sgl = (STOR_SCATTER_GATHER_LIST *)
		srb_ext->working_sgl_buf;

	xenblk_build_alloced_sgl(srb_ext->va, srb->DataTransferLength,
		srb_ext->working_sgl);

	srb_ext->sgl = srb_ext->working_sgl;
}

static __inline void
xenblk_unmap_system_address(void *sa[], STOR_SCATTER_GATHER_LIST *sys_sgl)
{
	uint32_t i;

#ifdef DBG
	KIRQL irql;

	irql = KeGetCurrentIrql();
	if (irql > DISPATCH_LEVEL && irql < HIGH_LEVEL) {
		DPR_INIT(("*** xenblk_unmap_system_address at irql %d *** \n", irql));
	}
#endif
	for (i = 0; i < sys_sgl->NumberOfElements; i++) {
		MmUnmapIoSpace(sa[i], sys_sgl->List[i].Length);
		sa[i] = NULL;
	}
}

static __inline void
xenblk_unmap_system_addresses(struct blkfront_info *info)
{
	struct blk_mm_ring *mm;
	unsigned long idx;
	uint32_t i;

#ifdef DBG
	KIRQL irql;

	irql = KeGetCurrentIrql();
	if (irql > DISPATCH_LEVEL && irql < HIGH_LEVEL) {
		DPR_INIT(("*** xenblk_unmap_system_addresses at irql %d *** \n", irql));
	}
#endif
	mm = &info->mm;
	while (mm->cons != mm->prod) {
		idx = mm->cons & (BLK_RING_SIZE - 1);
		for (i = 0; i < mm->ring[idx].mapped_elements; i++) {
			if (mm->ring[idx].mapped_addr[i]) {
				DPR_MM(("mm umap: idx %d, i %d, irql %d, len %lx, va %p, sa %p\n",
					idx, i, KeGetCurrentIrql(), mm->ring[idx].mapped_len[i],
					mm->ring[idx].vaddr,
					mm->ring[idx].mapped_addr[i]));
				MmUnmapIoSpace(mm->ring[idx].mapped_addr[i],
					mm->ring[idx].mapped_len[i]);
				mm->ring[idx].mapped_addr[i] = NULL;
			}
		}
		DPR_MM(("mm ExFreePool addr %p\n", mm->ring[idx].vaddr));
		if (mm->ring[idx].vaddr) {
			ExFreePool(mm->ring[idx].vaddr);
			XENBLK_DEC(info->xbdev->alloc_cnt_v);
			mm->ring[idx].vaddr = NULL;
		}
		mm->cons++;
	}
}

static __inline void
xenblk_save_system_address(struct blkfront_info *info,
	xenblk_srb_extension *srb_ext)
{
	struct blk_mm_ring *mm;
	unsigned long idx;
	uint32_t i;

	mm = &info->mm;
	idx = mm->prod & (BLK_RING_SIZE - 1);
#ifdef DBG
	if (mm->ring[idx].vaddr != NULL) {
		DBG_PRINT(("xenblk_save_system_address: vaddr is null %p\n",
			mm->ring[idx].vaddr));
	}
#endif
	mm->ring[idx].vaddr = srb_ext->va;
	mm->ring[idx].mapped_elements = srb_ext->sys_sgl->NumberOfElements;
	for (i = 0; i < srb_ext->sys_sgl->NumberOfElements; i++) {
		mm->ring[idx].mapped_addr[i] = srb_ext->sa[i];
		mm->ring[idx].mapped_len[i] = srb_ext->sys_sgl->List[i].Length;
		DPR_MM(("mm save: idx %d, i %d, irql %d, len %lx, va %p, sa %p\n",
			idx, i, KeGetCurrentIrql(), mm->ring[idx].mapped_len[i],
			mm->ring[idx].vaddr,
			mm->ring[idx].mapped_addr[i]));
	}
	mm->prod++;
}

static __inline unsigned long
xenblk_get_system_address(XENBLK_DEVICE_EXTENSION *dev_ext,
	SCSI_REQUEST_BLOCK *srb,
	void **sa)
{
	STOR_PHYSICAL_ADDRESS paddr;
	ULONG len;

	paddr = StorPortGetPhysicalAddress(dev_ext, srb, srb->DataBuffer, &len);
	*sa = StorPortGetVirtualAddress(dev_ext, paddr);
		//StorPortGetPhysicalAddress(dev_ext, srb, srb->DataBuffer, &len));
	DPR_MM(("\tsa = %p, PA = %x %x, DLen = %lx len = %lx.\n",
			 *sa,
			 (uint32_t)(paddr.QuadPart >> 32),
			 (uint32_t)paddr.QuadPart,
		srb->DataTransferLength, len));
	return *sa ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static __inline void *
xenblk_get_virtual_address(XENBLK_DEVICE_EXTENSION *dev_ext,
	SCSI_REQUEST_BLOCK *Srb,
	STOR_PHYSICAL_ADDRESS paddr)
{
	return StorPortGetVirtualAddress(dev_ext, paddr);
}

static __inline xenblk_addr_t
xenblk_get_buffer_addr(SCSI_REQUEST_BLOCK *srb, xenblk_srb_extension *srb_ext)
{
	return srb_ext->sgl->List[0].PhysicalAddress.QuadPart;
}

static __inline unsigned long
xenblk_buffer_mfn(XENBLK_DEVICE_EXTENSION *dev_ext, SCSI_REQUEST_BLOCK *srb,
	xenblk_srb_extension *srb_ext, xenblk_addr_t addr)
{
	return (pfn_to_mfn((unsigned long)(addr >> PAGE_SHIFT)));
}

#else
/***************************** SCSI MINIPORT *******************************/
typedef uintptr_t xenblk_addr_t;
#define xenblk_pause(_dev_ext, _pause_val)
#define xenblk_resume(dev_ext)
#define xenblk_request_complete ScsiPortNotification
#define xenblk_notification ScsiPortNotification
#define	xenblk_next_request ScsiPortNotification
#define	xenblk_request_timer_call ScsiPortNotification
#define xenblk_complete_all_requests ScsiPortCompleteRequest
#define xenblk_get_physical_address ScsiPortGetPhysicalAddress
#define xenblk_get_device_base ScsiPortGetDeviceBase
#define xenblk_write_port_ulong(_dext, _port, _val)						\
		ScsiPortWritePortUlong((_port), (_val))
#define xenblk_acquire_spinlock(_dext, _plock, _ltype, _lctx, _plhndl)	\
	XenAcquireSpinLock((_plock), (_plhndl))
#define xenblk_release_spinlock(_dext, _plock, _lhndl)	\
	XenReleaseSpinLock((_plock), (_lhndl));

#define scsiport_acquire_spinlock(_plock, _plhndl)	\
	XenAcquireSpinLock((_plock), (_plhndl))
#define scsiport_release_spinlock(_plock, _lhndl)	\
	XenReleaseSpinLock((_plock), (_lhndl));

#define xenblk_unmap_system_address(_addr, _len)
#define StorPortEnablePassiveInitialization(_dev, _foo)
#define storport_acquire_spinlock(_dext, _plock, _ltype, _lctx, _plhndl)
#define storport_xenblk_release_spinlock(_dext, _plock, _lhndl)

static __inline void
xenblk_unmap_system_addresses(struct blkfront_info *info)
{
	struct blk_mm_ring *mm;

	mm = &info->mm;
	while (mm->cons != mm->prod) {
		ExFreePool(mm->ring[mm->cons & (BLK_RING_SIZE - 1)].vaddr);
		XENBLK_DEC(info->xbdev->alloc_cnt_v);
		mm->cons++;
	}
}

static __inline void
xenblk_save_system_address(struct blkfront_info *info,
	xenblk_srb_extension *srb_ext)
{
	struct blk_mm_ring *mm;

	mm = &info->mm;
	mm->ring[mm->prod & (BLK_RING_SIZE - 1)].vaddr = srb_ext->va;
	mm->prod++;
}

static __inline void
xenblk_map_system_sgl(SCSI_REQUEST_BLOCK *srb, MEMORY_CACHING_TYPE cache_type)
{
	xenblk_srb_extension *srb_ext;

	srb_ext = (xenblk_srb_extension *)srb->SrbExtension;
	srb_ext->sa[0] = srb->DataBuffer;
	
	srb_ext->working_sgl = (STOR_SCATTER_GATHER_LIST *)
		srb_ext->working_sgl_buf;

	xenblk_build_alloced_sgl(srb_ext->va, srb->DataTransferLength,
		srb_ext->working_sgl);

	srb_ext->sgl = srb_ext->working_sgl;
}

static __inline void *
xenblk_get_virtual_address(XENBLK_DEVICE_EXTENSION *dev_ext,
	SCSI_REQUEST_BLOCK *Srb,
	STOR_PHYSICAL_ADDRESS paddr)
{
	return Srb->DataBuffer;
}

static __inline STOR_SCATTER_GATHER_LIST *
xenblk_build_sgl(XENBLK_DEVICE_EXTENSION *dev_ext,
	SCSI_REQUEST_BLOCK *Srb)
{
	xenblk_srb_extension *srb_ext;
	ULONG len;

	srb_ext = (xenblk_srb_extension *)Srb->SrbExtension;
	srb_ext->scsi_sgl.NumberOfElements = 1;
	srb_ext->scsi_sgl.List[0].Length = Srb->DataTransferLength;
	srb_ext->scsi_sgl.List[0].PhysicalAddress = 
		xenblk_get_physical_address(dev_ext, Srb, Srb->DataBuffer, &len);
	return &srb_ext->scsi_sgl;
}

static __inline xenblk_addr_t
xenblk_get_buffer_addr(SCSI_REQUEST_BLOCK *srb, xenblk_srb_extension *srb_ext)
{
	if (srb_ext->va == NULL)
		return (xenblk_addr_t)srb->DataBuffer;
	return (xenblk_addr_t)srb_ext->va;
}

static __inline unsigned long
scsi_virt_to_mfn(XENBLK_DEVICE_EXTENSION *dev_ext, SCSI_REQUEST_BLOCK *srb,
	void *vaddr, unsigned long *len)
{
    PHYSICAL_ADDRESS paddr;

	paddr = xenblk_get_physical_address(dev_ext, srb, vaddr, len);
	DPRINT(("\tscsi_virt_to_mfn = %x\n", (uint32_t)paddr.QuadPart));
	return (pfn_to_mfn((unsigned long)(paddr.QuadPart >> PAGE_SHIFT)));
}

static __inline unsigned long
xenblk_buffer_mfn(XENBLK_DEVICE_EXTENSION *dev_ext, SCSI_REQUEST_BLOCK *srb,
	xenblk_srb_extension *srb_ext, xenblk_addr_t addr)
{
	unsigned long buffer_mfn;
	unsigned long phys_len;

	if (srb_ext->va == NULL) {
		buffer_mfn = scsi_virt_to_mfn(
			dev_ext, srb, (void *)addr, &phys_len);
		if (buffer_mfn == 0) {
			DPRINT(("\tscsi_virt_to_mfn returned 0\n"));
			buffer_mfn = virt_to_mfn(addr);
		}
	}
	else {
		buffer_mfn = virt_to_mfn(addr);
		if (buffer_mfn == 0) {
			DPRINT(("\tvirt_to_mfn returned 0\n"));
			buffer_mfn = scsi_virt_to_mfn(
				dev_ext, NULL, (void *)addr, &phys_len);
		}
	}
	return buffer_mfn;
}


#endif


void XenBlkFreeResource(struct blkfront_info *info, uint32_t info_idx,
	XENBUS_RELEASE_ACTION action);
void XenBlkFreeAllResources(XENBLK_DEVICE_EXTENSION *dev_ext,
	XENBUS_RELEASE_ACTION action);
NTSTATUS blkfront_probe(struct blkfront_info *info);
NTSTATUS do_blkif_request(struct blkfront_info *info, SCSI_REQUEST_BLOCK *srb);
uint32_t blkif_complete_int(struct blkfront_info *info);
void blkif_completion(struct blk_shadow *s);
void blkif_quiesce(struct blkfront_info *info);
void blkif_disconnect_backend(XENBLK_DEVICE_EXTENSION *dev_ext);
void XenBlkDebugDump(XENBLK_DEVICE_EXTENSION *dev_ext);

static __inline void
xenblk_cp_from_sa(void *sa[], STOR_SCATTER_GATHER_LIST *sys_sgl, uint8_t *va)
{
	uint32_t i;

	for (i = 0; i < sys_sgl->NumberOfElements; i++) {
		RtlCopyMemory(va, sa[i], sys_sgl->List[i].Length);
		va += sys_sgl->List[i].Length;
	}
}

static __inline void
xenblk_cp_to_sa(void *sa[], STOR_SCATTER_GATHER_LIST *sys_sgl, uint8_t *va)
{
	uint32_t i;

	for (i = 0; i < sys_sgl->NumberOfElements; i++) {
		DPR_MM(("   xenblk_cp_to_sa: sa[%d] %p, va %p, len %d\n",
				i, sa[i], va, sys_sgl->List[i].Length));
		RtlCopyMemory(sa[i], va, sys_sgl->List[i].Length);
		va += sys_sgl->List[i].Length;
	}
}

static __inline uint64_t
xenblk_strtou64(
  const char *ptr, char **endptr, int radix
  )
{
    unsigned char ch;
    unsigned int i;
    uint64_t res = 0;
    const char *p;
    int valid = 1;

    if (ptr == NULL)
        return 0;

    for (p=ptr; *p!='\0' && valid; p++) {
        ch = *p;
        switch (ch) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            i = ch - '0';
            break;

        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            if (radix == 16)
                i = ch - 'A' + 10;
            else {
                valid = 0;
                p--;
            }
            break;

        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            if (radix == 16)
                i = ch - 'a' + 10;
            else {
                valid = 0;
                p--;
            }
            break;

        default:
            valid = 0;
            p--;
            break;
        }
        if (valid)
            res = res*radix + i;
    }
    if (endptr)
        *endptr = (char*) p;
    return res;
}
#endif	// _XENBLK_H_

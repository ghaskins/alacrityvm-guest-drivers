/*****************************************************************************
 *
 *   File Name:      xenblkfront.c
 *   Created by:     KRA
 *   Date created:   12-14-06
 *
 *   %version:       35 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 15 15:12:49 2009 %
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

#include <stdio.h>
#include <ntddk.h>
#include "xenblk.h"

#ifdef DBG
extern uint32_t srbs_seen;
extern uint32_t srbs_returned;
extern uint32_t io_srbs_seen;
extern uint32_t io_srbs_returned;
extern uint32_t sio_srbs_seen;
extern uint32_t sio_srbs_returned;
#endif
#define MAXIMUM_OUTSTANDING_BLOCK_REQS \
    (BLKIF_MAX_SEGMENTS_PER_REQUEST * BLK_RING_SIZE)
#define GRANT_INVALID_REF	0

static void backend_changed(struct xenbus_watch *watch,
	const char **vec, unsigned int len);
static void connect(struct blkfront_info *);
static void blkfront_closing(struct blkfront_info *info);
static int blkfront_remove(XENBLK_DEVICE_EXTENSION *);
static int talk_to_backend(struct blkfront_info *);
static int setup_blkring(struct blkfront_info *);

static void kick_pending_request_queues(struct blkfront_info *);

static KDEFERRED_ROUTINE blkif_int;
static void blkif_restart_queue(IN PDEVICE_OBJECT DevObject, IN PVOID Context);
static void blkif_free(struct blkfront_info *, int);
static void blkif_completion(struct blk_shadow *s);
#if 0
static void blkif_recover(struct blkfront_info *);
#endif

/**
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures and the ring buffer for communication with the backend, and
 * inform the backend of the appropriate details for those.  Switch to
 * Initialised state.
 */
NTSTATUS blkfront_probe(struct blkfront_info *info)
{
	char *buf;
    enum xenbus_state backend_state;
	int err, i;

	DPR_INIT(("blkfront_probe: in\n"));

	info->nodename = xenbus_get_nodename_from_dev(info);
	info->otherend = xenbus_get_otherend_from_dev(info);
	info->otherend_id = (domid_t)xenblk_strtou64(
		xenbus_get_backendid_from_dev(info),
			NULL, 10);
	DPR_INIT(("  info->nodename = %s\n", info->nodename));
	DPR_INIT(("  info->otherend = %s, id = %d\n", info->otherend,
		 info->otherend_id));

	info->connected = BLKIF_STATE_DISCONNECTED;
	info->sector_size = 0;
	info->sectors = 0;
	info->binfo = 0;
    InitializeListHead(&info->rq);
#ifndef XENBLK_STORPORT
	KeInitializeSpinLock(&info->lock);
#endif

	info->shadow_free = 0;
	for (i = 0; i < BLK_RING_SIZE; i++) {
		info->shadow[i].req.id = i+1;
		info->shadow[i].req.nr_segments = 0;
		info->shadow[i].request = NULL;
		//info->shadow[i].mapped_va = NULL;
	}
	info->shadow[BLK_RING_SIZE-1].req.id = 0x0fffffff;

	info->handle =(uint16_t)xenblk_strtou64(
		strrchr(info->nodename,'/') + 1, NULL, 10);

	InitializeListHead(&info->watch.list);
	info->watch.callback = backend_changed;
	info->watch.node = info->otherend;
	info->watch.flags = XBWF_new_thread;
	info->watch.context = info;
	info->mm.cons = 0;
	info->mm.prod = 0;

	err = talk_to_backend(info);
	if (err) {
		return err;
	}

	for (i = 0; i < 1000; i++) {
		DPR_INIT(("      blkfront_probe %s: reading state, irql %x, cpu %x\n",
			info->nodename, KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
		buf = xenbus_read(XBT_NIL, info->otherend, "state", NULL);
		if (buf == NULL){
			xenbus_printf(XBT_NIL, info->nodename, "reading state", "%p", buf);
			DBG_PRINT(("	blkfront_probe: failed to read state.\n"));
			continue;
		}

		backend_state = (enum xenbus_state)xenblk_strtou64(buf, NULL, 10);
		xenbus_free_string(buf);

		if (backend_state == XenbusStateConnected) {
			connect(info);
			break;
		}

		/* We don't actually want to wait any time because we may be */
		/* at greater than PASSIVE_LEVEL. */
		DPR_INIT(("      blkfront_probe: waiting for connect\n"));
	}

	err = register_xenbus_watch(&info->watch);
	if (err) {
		DBG_PRINT(("blkfront_probe: register_xenbus_watch returned 0x%x.\n",
			err));
		if (err != -EEXIST) {
			return err;
		}
	}

	DPR_INIT(("blkfront_probe: finished\n"));
	return STATUS_SUCCESS;
}

#if 0

/**
 * We are reconnecting to the backend, due to a suspend/resume, or a backend
 * driver restart.  We tear down our blkif structure and recreate it, but
 * leave the device-layer structures intact so that this is transparent to the
 * rest of the kernel.
 */
static int blkfront_resume(struct xenbus_device *dev)
{
	struct blkfront_info *info = dev->dev.driver_data;
	int err;

	DPRINT(("blkfront_resume: %s\n", dev->nodename));

	blkif_free(info, 1);

	err = talk_to_backend(info);
	if (!err)
		blkif_recover(info);

	return err;
}
#endif


static int talking_to_backend = 0;
/* Common code used when first setting up, and when resuming. */
static int talk_to_backend(struct blkfront_info *info)
{
	const char *message = NULL;
	struct xenbus_transaction xbt;
	int err;

	/* Create shared ring, alloc event channel. */
	DPR_INIT(("talk_to_backend: irql %x, cpu %x, locks %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber(), info->xenblk_locks));
	err = setup_blkring(info);
	if (err)
		goto out;
	DPR_INIT(("talk_to_backend: ring_ref %u, evtchn %u\n",
		info->ring_ref, info->evtchn));

again:
	err = xenbus_transaction_start(&xbt);
	if (err) {
		DBG_PRINT(("talk_to_backend: xenbus_transaction_start failed\n"));
		xenbus_printf(xbt, info->nodename, "starting transaction","%d", err);
		goto destroy_blkring;
	}

	talking_to_backend++;
	DPR_INIT(("talk_to_backend: ring_ref %u\n", info->ring_ref));
	err = xenbus_printf(xbt, info->nodename, "ring-ref","%u", info->ring_ref);
	if (err) {
		message = "writing ring-ref";
		goto abort_transaction;
	}
	DPR_INIT(("talk_to_backend: evtchn %u\n", info->evtchn));
	err = xenbus_printf(xbt, info->nodename, "event-channel", "%u",
		info->evtchn);
	if (err) {
		message = "writing event-channel";
		goto abort_transaction;
	}
	DPR_INIT(("talk_to_backend: protocol %s\n", XEN_IO_PROTO_ABI_NATIVE));
	err = xenbus_printf(xbt, info->nodename, "protocol", "%s",
		XEN_IO_PROTO_ABI_NATIVE);
	if (err) {
		message = "writing protocol";
		goto abort_transaction;
	}

	err = xenbus_transaction_end(xbt, 0);
	if (err) {
		if (err == -EAGAIN)
			goto again;
		xenbus_printf(xbt, info->nodename, "completing transaction","%d", err);
		goto destroy_blkring;
	}

	xenbus_switch_state(info->nodename, XenbusStateInitialised);

	DPR_INIT(("talk_to_backend finished: irql %x, cpu %x, locks %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber(), info->xenblk_locks));
	talking_to_backend--;
	return 0;

 abort_transaction:
	xenbus_transaction_end(xbt, 1);
	if (message)
		xenbus_printf(xbt, info->nodename, message,"%s", err);
 destroy_blkring:
	blkif_free(info, 0);
 out:
	talking_to_backend--;
 DPR_INIT(("talk_to_backend error: irql %x, cpu %x, locks %x\n",
	 KeGetCurrentIrql(), KeGetCurrentProcessorNumber(), info->xenblk_locks));
	return err;
}


#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
static int setup_blkring(struct blkfront_info *info)
{
	blkif_sring_t *sring;
	int err;

	info->ring_ref = GRANT_INVALID_REF;

	sring = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, XENBLK_TAG_GENERAL);
	if (!sring) {
		xenbus_printf(XBT_NIL, info->nodename, "allocating shared ring","%x",
			-ENOMEM);
		return -ENOMEM;
	}
	DPR_INIT(("setup_blkring alloc sring **.\n"));
	XENBLK_INC(info->xbdev->alloc_cnt_s);
	SHARED_RING_INIT(sring);
	WIN_FRONT_RING_INIT(&info->ring, sring, PAGE_SIZE);

	DPRINT(("kra sring_t %x, req %x, rsp %x, ring %x.\n",
		sizeof(blkif_sring_t), sizeof(blkif_request_t),
		sizeof(blkif_response_t), __WIN_RING_SIZE(sring, PAGE_SIZE)));
	DPRINT(("kra off ring %x, h %x, id %x, s %x, seg %x\n",
		offsetof(blkif_sring_t, ring),
		offsetof(blkif_request_t, handle),
		offsetof(blkif_request_t, id),
		offsetof(blkif_request_t, sector_number),
		offsetof(blkif_request_t, seg)));
	DPRINT(("setup_blkring: sring = %x, mfn ring.sring = %x, mfn sring = %x\n",
		sring, virt_to_mfn(info->ring.sring), virt_to_mfn(sring)));

	err = xenbus_grant_ring(info->otherend_id, virt_to_mfn(info->ring.sring));
	if (err < 0) {
		DPR_INIT(("setup_blkring: xenbus_grant_ring failed\n"));
		ExFreePool(sring);
		info->ring.sring = NULL;
		XENBLK_DEC(info->xbdev->alloc_cnt_s);
		goto fail;
	}
	info->ring_ref = err;
	DPR_INIT(("setup_blkring: info->ring_ref = %x for id %d\n", info->ring_ref,
		info->otherend_id));

	err = xenbus_alloc_evtchn(info->otherend_id, &info->evtchn);
	if (err) {
		DPR_INIT(("setup_blkring: xenbus_alloc_evtchn failed\n"));
		goto fail;
	}
	DPRINT(("setup_blkring: info->evtchn = %d, %x\n", info->evtchn, blkif_int));

	err = register_dpc_to_evtchn(info->evtchn, blkif_int, info,
#ifdef XENBUS_HAS_DEVICE
		NULL);
#else
		&info->has_interrupt);
#endif
	if (err < 0) {
		xenbus_printf(XBT_NIL, info->nodename,
			"bind_evtchn_to_irqhandler failed","%x", err);
		DPRINTK(("register_dpc_to_evtchn failed %x\n", err));
		goto fail;
	}
	info->irq = info->evtchn;

	DPRINT(("returning from setup blkring: ring_ref %u, evtchn %u\n",
		info->ring_ref, info->evtchn));
	return 0;
fail:
	blkif_free(info, 0);
	return err;
}


/*
 * Callback received when the backend's state changes.
 */
static void backend_changed(struct xenbus_watch *watch,
	const char **vec, unsigned int len)
{
	struct blkfront_info *info = (struct blkfront_info *)watch->context;
	struct block_device *bd;
	char *buf;
    XENBLK_LOCK_HANDLE lh;
    enum xenbus_state backend_state;
	xenbus_release_device_t release_data;
	uint32_t i;

	DPR_INIT(("blkfront:backend_changed: node %s.\n", vec[0]));
	if (talking_to_backend) {
		DPRINT(("backend_changed called while talking to backend.\n"));
	}

	buf = xenbus_read(XBT_NIL, info->otherend, "state", NULL);
	if (buf == NULL){
		xenbus_printf(XBT_NIL, info->nodename, "reading state", "%x", buf);
		DBG_PRINT(("blkfront:backend_changed failed to read state from\n"));
		DBG_PRINT(("         %s.\n", info->otherend));
		if (info->connected != BLKIF_STATE_DISCONNECTED)
			return;
		backend_state = XenbusStateClosed;
	}
	else {
		backend_state = (enum xenbus_state)xenblk_strtou64(buf, NULL, 10);
		xenbus_free_string(buf);
		DPR_INIT(("blkfront:backend_changed to state %d.\n", backend_state));
	}

	switch (backend_state) {
	case XenbusStateInitialising:
	case XenbusStateInitWait:
	case XenbusStateInitialised:
	case XenbusStateUnknown:
		break;
	case XenbusStateClosed:
		DPR_INIT(("blkfront:backend_changed to state closed.\n"));
		unregister_xenbus_watch(&info->watch);
		if (info->irq) {
			DPR_INIT(("      unregister_dpc_from_evtchn %d.\n",
				info->evtchn));
			unregister_dpc_from_evtchn(info->evtchn);
			info->evtchn = info->irq = 0;
		}
		for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
			if (info->xbdev->info[i] == info) {
				XenBlkFreeResource(info, i, RELEASE_REMOVE);
				break;
			}
		}
		break;

	case XenbusStateConnected:
		DPR_INIT(("backend_changed %p: connect irql = %d, cpu = %d, locks %x\n",
			backend_changed, KeGetCurrentIrql(),
			KeGetCurrentProcessorNumber(), info->xenblk_locks));
		connect(info);
#ifdef DBG
		//XenBlkDebugDump(info->xbdev);
#endif
		break;

	case XenbusStateClosing:
		DPR_INIT(("blkfront:backend_changed is closing.\n"));
		xenblk_acquire_spinlock(info->xbdev, &info->lock, InterruptLock,
			NULL, &lh);
		DPR_INIT(("blkfront:backend_changed is closing - blkif_quiesce.\n"));
		blkif_quiesce(info);
		info->connected = BLKIF_STATE_DISCONNECTED;
		xenblk_release_spinlock(info->xbdev, &info->lock, lh);
		DPR_INIT(("blkfront:backend_changed is closing - switch to closed.\n"));
		xenbus_switch_state(info->nodename, XenbusStateClosed);
		break;
	}
	DPR_INIT(("backend_changed %p: OUT irql = %d, cpu = %d, locks %x\n",
		backend_changed, KeGetCurrentIrql(),
		KeGetCurrentProcessorNumber(), info->xenblk_locks));
}


/* ** Connection ** */


/*
 * Invoked when the backend is finally 'ready' (and has told produced
 * the details about the physical device - #sectors, size, etc).
 */
static void connect(struct blkfront_info *info)
{
    XENBLK_LOCK_HANDLE lh;
	int err;
	char *buf;

	xenblk_acquire_spinlock(info->xbdev, &info->lock, InterruptLock, NULL, &lh);
	XENBLK_SET_FLAG(info->xenblk_locks, (BLK_CON_L | BLK_INT_L));
	XENBLK_SET_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
	DPR_INIT(("blkfront.c:connect: %p, irql %x, cpu %x IN\n",
		info, KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	if ((info->connected == BLKIF_STATE_CONNECTED) ||
	    (info->connected == BLKIF_STATE_SUSPENDED) ) {
		DPR_INIT(("blkfront.c:connect: %p OUT already connected\n", info));
		XENBLK_CLEAR_FLAG(info->xenblk_locks, (BLK_CON_L | BLK_INT_L));
		XENBLK_CLEAR_FLAG(info->cpu_locks,(1 << KeGetCurrentProcessorNumber()));
		xenblk_release_spinlock(info->xbdev, &info->lock, lh);
		return;
	}

	XENBLK_CLEAR_FLAG(info->xenblk_locks, (BLK_CON_L | BLK_INT_L));
	XENBLK_CLEAR_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
	xenblk_release_spinlock(info->xbdev, &info->lock, lh);

	DPR_INIT(("blkfront.c:connect:%s sectors.\n", info->otherend));

	buf = xenbus_read(XBT_NIL, info->otherend, "sectors", NULL);
	if (buf == NULL){
		xenbus_printf(XBT_NIL, info->nodename, "reading sectors", "%x",
			buf);
		DPR_INIT(("blkfront.c:connect: failed to read sectors\n"));
	}

	info->sectors = xenblk_strtou64(buf, NULL, 10);
	xenbus_free_string(buf);

	DPR_INIT(("blkfront.c:connect:%s info.\n", info->otherend));
	buf = xenbus_read(XBT_NIL, info->otherend, "info", NULL);
	if (buf == NULL) {
		xenbus_printf(XBT_NIL, info->nodename, "reading info", "%x",
			buf);
		DPR_INIT(("blkfront.c:connect: failed to read info\n"));
	}

	info->binfo = (unsigned int)xenblk_strtou64(buf, NULL, 10);
	xenbus_free_string(buf);

	DPR_INIT(("blkfront.c:connect:%s sector-size.\n", info->otherend));
	buf = xenbus_read(XBT_NIL, info->otherend, "sector-size", NULL);
	if (buf == NULL) {
		xenbus_printf(XBT_NIL, info->nodename, "reading sector-size",
			"%x", buf);
		DPR_INIT(("blkfront.c:connect: failed to read sector-size\n"));
	}

	info->sector_size = (unsigned long)xenblk_strtou64(buf, NULL, 10);
	xenbus_free_string(buf);

	DPR_INIT(("blkfront: sctrs 0x%x%08x, sz 0x%x, lst sctr 0x%x, binfo %x\n",
		(uint32_t)(info->sectors >> 32),
		(uint32_t)info->sectors,
		info->sector_size,
		(uint32_t)(info->sectors) - 1,
		info->binfo));

	(void)xenbus_switch_state(info->nodename, XenbusStateConnected);

	info->connected = BLKIF_STATE_CONNECTED;

	/* Kick pending requests. */
	kick_pending_request_queues(info);
	DPR_INIT(("blkfront.c:connect: OUT\n"));
}

#if 0
/**
 * Handle the change of state of the backend to Closing.  We must delete our
 * device-layer structures now, to ensure that writes are flushed through to
 * the backend.  Once is this done, we can switch to Closed in
 * acknowledgement.
 */
static void blkfront_closing(struct blkfront_info *info)
{
    XENBLK_LOCK_HANDLE lh;
	unsigned long flags;

	DPRINT(("blkfront_closing: %s removed\n", info->nodename));

	if (info->rq == NULL)
		return;

	xenblk_acquire_spinlock(info->xbdev, &info->lock, InterruptLock, NULL, &lh);
	XENBLK_SET_FLAG(info->xenblk_locks, BLK_INT_L);
	XENBLK_SET_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
	/* No more blkif_request(). */
	blk_stop_queue(info->rq);
	/* No more gnttab callback work. */
	gnttab_cancel_free_callback(&info->callback);
	XENBLK_CLEAR_FLAG(info->xbdev, BLK_INT_L);
	XENBLK_CLEAR_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
	xenblk_release_spinlock(info->xbdev, &info->lock, lh);

	/* Flush gnttab callback work. Must be done with no locks held. */
	flush_scheduled_work();

	xlvbd_del(info);

	xenbus_frontend_closed(info);
}


static int blkfront_remove(struct xenbus_device *dev)
{
	struct blkfront_info *info = dev->dev.driver_data;

	DPRINT(("blkfront_remove: %s removed\n", dev->nodename));

	blkif_free(info, 0);

	kfree(info);

	return 0;
}
#endif


static inline int GET_ID_FROM_FREELIST(
	struct blkfront_info *info)
{
	unsigned long free;

#ifdef DBG
	if (info->xenblk_locks & BLK_ID_L) {
		DPR_INIT(("GET_ID_FROM_FREELIST already set: irql %x, cpu %x, xlocks %x on 0x%x\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
			info->xenblk_locks,	info->cpu_locks));
	}
#endif

	XENBLK_SET_FLAG(info->xenblk_locks, (BLK_ID_L | BLK_GET_L));
	XENBLK_SET_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));

	free = info->shadow_free;
	ASSERT(free < BLK_RING_SIZE);
	info->shadow_free = (unsigned long)info->shadow[free].req.id;
	info->shadow[free].req.id = 0x0fffffee; /* debug */

	XENBLK_CLEAR_FLAG(info->xenblk_locks, (BLK_ID_L | BLK_GET_L));
	XENBLK_CLEAR_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));

	return free;
}

static inline void ADD_ID_TO_FREELIST(
	struct blkfront_info *info, unsigned long id)
{
#ifdef DBG
	if (info->xenblk_locks & BLK_ID_L) {
		DPR_INIT(("ADD_ID_TO_FREELIST already set: irql %x, cpu %x, xlocks %x on 0x%x\n",
			KeGetCurrentIrql(), KeGetCurrentProcessorNumber(),
			info->xenblk_locks,	info->cpu_locks));
	}
#endif

	XENBLK_SET_FLAG(info->xenblk_locks, (BLK_ID_L | BLK_ADD_L));
	XENBLK_SET_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));

	info->shadow[id].req.id  = info->shadow_free;
	info->shadow[id].request = NULL;
	//info->shadow[id].mapped_va = NULL;
	info->shadow_free = id;

	XENBLK_CLEAR_FLAG(info->xenblk_locks, (BLK_ID_L | BLK_ADD_L));
	XENBLK_CLEAR_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
}


static void flush_enabled(struct blkfront_info *info)
{
	DPRINT(("flush_enabled - IN irql = %d\n", KeGetCurrentIrql()));
	notify_remote_via_irq(info->irq);
	DPRINT(("flush_enabled - OUT irql = %d\n", KeGetCurrentIrql()));
}

static inline void flush_requests(struct blkfront_info *info)
{
	int notify;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&info->ring, notify);

	if (notify) {
		notify_remote_via_irq(info->irq);
	}
}

static void kick_pending_request_queues(struct blkfront_info *info)
{
	if (!RING_FULL(&info->ring)) {
		/* Re-enable calldowns. */
		//kra todo
		//blk_start_queue(info->rq);
		/* Kick things off immediately. */
		//do_blkif_request(info, NULL);
	}
}

static void blkif_restart_queue(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context)
{
	struct blkfront_info *info = (struct blkfront_info *)Context;

	if (info->connected == BLKIF_STATE_CONNECTED)
		kick_pending_request_queues(info);
}

static void blkif_restart_queue_callback(void *arg)
{
	struct blkfront_info *info = (struct blkfront_info *)arg;

	xenblk_request_timer_call(RequestTimerCall, info->xbdev,
		blkif_restart_queue, 100);
}

#if 0
int blkif_open(struct inode *inode, struct file *filep)
{
	struct blkfront_info *info = inode->i_bdev->bd_disk->private_data;
	info->users++;
	return 0;
}


int blkif_release(struct inode *inode, struct file *filep)
{
	struct blkfront_info *info = inode->i_bdev->bd_disk->private_data;
	info->users--;
	if (info->users == 0) {
		/* Check whether we have been instructed to close.  We will
		   have ignored this request initially, as the device was
		   still mounted. */
		enum xenbus_state state = xenbus_read_driver_state(info->otherend);

		if (state == XenbusStateClosing)
			blkfront_closing(info);
	}
	return 0;
}


int blkif_ioctl(struct inode *inode, struct file *filep,
		unsigned command, unsigned long argument)
{
	int i;

	DPRINTK_IOCTL("command: 0x%x, argument: 0x%lx, dev: 0x%04x\n",
		      command, (long)argument, inode->i_rdev);

	switch (command) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	case HDIO_GETGEO: {
		struct block_device *bd = inode->i_bdev;
		struct hd_geometry geo;
		int ret;

                if (!argument)
                        return -EINVAL;

		geo.start = get_start_sect(bd);
		ret = blkif_getgeo(bd, &geo);
		if (ret)
			return ret;

		if (copy_to_user((struct hd_geometry __user *)argument, &geo,
				 sizeof(geo)))
                        return -EFAULT;

                return 0;
	}
#endif
	case CDROMMULTISESSION:
		DPRINT(("FIXME: support multisession CDs later\n"));
		for (i = 0; i < sizeof(struct cdrom_multisession); i++)
			if (put_user(0, (char __user *)(argument + i)))
				return -EFAULT;
		return 0;

	default:
		/*printk(KERN_ALERT "ioctl %08x not supported by Xen blkdev\n",
		  command);*/
		return -EINVAL; /* same return as native Linux */
	}

	return 0;
}


int blkif_getgeo(struct block_device *bd, struct hd_geometry *hg)
{
	/* We don't have real geometry info, but let's at least return
	   values consistent with the size of the device */
	sector_t nsect = get_capacity(bd->bd_disk);
	sector_t cylinders = nsect;

	hg->heads = 0xff;
	hg->sectors = 0x3f;
	sector_div(cylinders, hg->heads * hg->sectors);
	hg->cylinders = cylinders;
	if ((sector_t)(hg->cylinders + 1) * hg->heads * hg->sectors < nsect)
		hg->cylinders = 0xffff;
	return 0;
}
#endif

#ifdef DBG
static int dumpit = 0;
static void
DumpPacket(uint64_t disk_offset, PUCHAR currentAddress, uint32_t len)
{
	uint32_t i;
	int j;

	if (!currentAddress) {
		return;
	}
	/*
	if (dumpit > 29) {
		return;
	}
	dumpit++;
	*/
	/*
	len = len > 4096 ? 4096 : len;

	for (i = 0; i < len; i += 512 ) {
		DPRINT(("%3x: ",i));
		for (j = 0; i + j < len && j < 16; j++) {
			DPRINT(("%2x ", currentAddress[i+j]));
		}
		DPRINT(("\n" ));
	}
	*/
	len = len > 512 ? 512 : len;

	//DPR_INIT(("Dumpping packe for disk_offset %x, address %x\n",
	//		(uint32_t)disk_offset, currentAddress));
	for (i = 0; i < len; ) {
		DBG_PRINT(("%3x: ",i));
		for (j = 0; i < len && j < 16; j++, i++) {
			DBG_PRINT(("%2x ", currentAddress[i]));
		}
		DBG_PRINT(("\n" ));
	}
}
#endif


/*
 * do_blkif_request
 *
 */
NTSTATUS do_blkif_request(struct blkfront_info *info, SCSI_REQUEST_BLOCK *srb)
{
	uint64_t disk_offset;
	xenblk_addr_t addr;
	xenblk_srb_extension *srb_ext;
	blkif_request_t *ring_req;
	STOR_SCATTER_GATHER_LIST *sgl;
	uint32_t *ids;
	ULONG sidx;
	ULONG i;
	unsigned long buffer_mfn;
	unsigned long len;
	unsigned long id;
	unsigned long page_offset;
	unsigned long remaining_bytes;
	unsigned int starting_req_prod_pvt;
	unsigned int fsect;
	unsigned int lsect;
    XENBLK_LOCK_HANDLE lh;
	grant_ref_t gref_head;
	int idx;
	int ref;
	int num_pages;
	int num_segs;
	int num_ring_req;

	if (info->connected != BLKIF_STATE_CONNECTED) {
		DBG_PRINT(("\tblkif_queue_request - OUT, not connected \n"));
		return 1;
	}

	srb_ext = (xenblk_srb_extension *)srb->SrbExtension;
	srb_ext->next = NULL;
	srb_ext->srb = srb;
	srb_ext->use_cnt = 0;
	DPR_TRC(("      blkif_queue_request - IN srb %p, srb_ext %p, va %p\n",
			 srb, srb_ext, srb_ext->va));

	/* Figure out how many pages this request will take. */
	num_pages = 0;
	page_offset = 0;
	for (i = 0; i < srb_ext->sgl->NumberOfElements; i++) {
		if (((uint32_t)srb_ext->sgl->List[i].PhysicalAddress.QuadPart &
			(PAGE_SIZE - 1)) == 0) {
			num_pages += ((srb_ext->sgl->List[i].Length - 1) >>
				(PAGE_SHIFT)) + 1;
		}
		else {
			num_pages++; /* we have at least one page */
			page_offset =
				(unsigned long)srb_ext->sgl->List[i].PhysicalAddress.QuadPart &
					(PAGE_SIZE - 1);
			len = srb_ext->sgl->List[i].Length;
			remaining_bytes = len > PAGE_SIZE - page_offset ?
				len - (PAGE_SIZE - page_offset) : 0;
			if (remaining_bytes) {
				num_pages += ((remaining_bytes - 1) >> (PAGE_SHIFT)) + 1;
			}
		}
	}

	num_ring_req = ((num_pages - 1) / BLKIF_MAX_SEGMENTS_PER_REQUEST) + 1;

	if (srb->Cdb[0] < SCSIOP_READ16) {
		disk_offset = ((uint64_t)((uint32_t)(
			((PCDB)srb->Cdb)->CDB10.LogicalBlockByte3
			| ((PCDB)srb->Cdb)->CDB10.LogicalBlockByte2 << 8
			| ((PCDB)srb->Cdb)->CDB10.LogicalBlockByte1 << 16
			| ((PCDB)srb->Cdb)->CDB10.LogicalBlockByte0 << 24)));
	}
	else {
		REVERSE_BYTES_QUAD(&disk_offset, ((PCDB)srb->Cdb)->CDB16.LogicalBlock);
		DPR_TRC(("\tREV: %02x%02x%02x%02x%02x%02x%02x%02x, %x%08x.\n",
			((PCDB)srb->Cdb)->CDB16.LogicalBlock[0],
			((PCDB)srb->Cdb)->CDB16.LogicalBlock[1],
			((PCDB)srb->Cdb)->CDB16.LogicalBlock[2],
			((PCDB)srb->Cdb)->CDB16.LogicalBlock[3],
			((PCDB)srb->Cdb)->CDB16.LogicalBlock[4],
			((PCDB)srb->Cdb)->CDB16.LogicalBlock[5],
			((PCDB)srb->Cdb)->CDB16.LogicalBlock[6],
			((PCDB)srb->Cdb)->CDB16.LogicalBlock[7],
			(uint32_t)(disk_offset >> 32),
			(uint32_t)disk_offset));
	}
	idx = 0;

	//xenblk_acquire_spinlock(info->xbdev,&info->lock,InterruptLock, NULL,&lh);
	scsiport_acquire_spinlock(&info->lock, &lh);
	starting_req_prod_pvt = info->ring.req_prod_pvt;
	ids = info->id;

	if ((int)(RING_FREE_REQUESTS(&info->ring)) < num_ring_req) {
		DBG_PRINT(("\tblkif_queue_request - OUT, free requests = %x, required = %x, num_pages %d\n",
			(int)(RING_FREE_REQUESTS(&info->ring)), num_ring_req, num_pages));
		//xenblk_release_spinlock(info->xbdev, &info->lock, lh);
		scsiport_release_spinlock(&info->lock, lh);
		return 1;
	}

	sgl = srb_ext->sgl;
	len = sgl->List[0].Length;
	addr = xenblk_get_buffer_addr(srb, srb_ext);
	sidx = 1;

	for (; num_ring_req; num_ring_req--) {
		if (num_pages <= BLKIF_MAX_SEGMENTS_PER_REQUEST) {
			num_segs = num_pages;
			num_pages = 0;
		}
		else {
			num_segs = BLKIF_MAX_SEGMENTS_PER_REQUEST;
			num_pages -= BLKIF_MAX_SEGMENTS_PER_REQUEST;
		}

		if (gnttab_alloc_grant_references((uint16_t)num_segs,&gref_head) < 0) {

			/* Undo everything that we've done so far. */
			DBG_PRINT(("\tblkif_queue_request - g_a_g_refs failed\n"));
			info->ring.req_prod_pvt = starting_req_prod_pvt;
			for (; idx; idx--) {
				blkif_completion(&info->shadow[ids[idx]]);
				ADD_ID_TO_FREELIST(info, ids[idx]);
			}
			gnttab_request_free_callback(
				&info->callback,
				blkif_restart_queue_callback,
				info,
				(uint16_t)num_segs);
			DBG_PRINT(("\tblkif_queue_request - OUT, g_a_g_refs failed\n"));
			//xenblk_release_spinlock(info->xbdev, &info->lock, lh);
			scsiport_release_spinlock(&info->lock, lh);
			return 1;
		}

		/* Fill out a communications ring structure. */
		ring_req = RING_GET_REQUEST(&info->ring, info->ring.req_prod_pvt);

		/* scsi already has a spinlock.  Just do it for storport. */
		storport_acquire_spinlock(info->xbdev, &info->lock,
			InterruptLock, NULL, &lh);
		id = GET_ID_FROM_FREELIST(info);
		storport_xenblk_release_spinlock(info->xbdev, &info->lock, lh);
		blkif_completion(&info->shadow[id]);

		ids[idx++] = id;
		ring_req->id = id;
		ring_req->operation =
			(srb->Cdb[0] == SCSIOP_WRITE || srb->Cdb[0] == SCSIOP_WRITE16) ?
			BLKIF_OP_WRITE : BLKIF_OP_READ;
			//BLKIF_OP_WRITE_BARRIER : BLKIF_OP_READ;
		ring_req->sector_number = disk_offset;
		ring_req->handle = info->handle;

		for (ring_req->nr_segments = 0; num_segs; num_segs--) {
			ref = gnttab_claim_grant_reference(&gref_head);
			ASSERT(ref != -1);

			buffer_mfn = xenblk_buffer_mfn(info->xbdev, srb, srb_ext, addr);
			ring_req->seg[ring_req->nr_segments].gref = ref;

			/* Check for page/sector alignment. */
			if (((uint32_t)addr & (PAGE_SIZE - 1)) == 0) {
				fsect = 0;
				lsect = len >= PAGE_SIZE ? 7 : (uint8_t)((len >> 9) - 1);

				len = len >= PAGE_SIZE ? len - PAGE_SIZE : 0;
				if (len) {
					addr += PAGE_SIZE;
				}
				else {
					if (sidx < sgl->NumberOfElements) {
						len = sgl->List[sidx].Length;
						addr = (xenblk_addr_t)
							sgl->List[sidx].PhysicalAddress.QuadPart;
						sidx++;
					}
				}
			}
			else {
				unsigned long len_adj;

				page_offset = (unsigned long)addr & (PAGE_SIZE - 1);
				fsect = ((uint32_t)addr & (PAGE_SIZE - 1)) >> 9;
				lsect = page_offset + len >= PAGE_SIZE ? 7 :
					(uint8_t)(((page_offset + len) >> 9) - 1);
				DPRINT(("    addr = %x, len = %lx  pages = %d, n_segs = %d\n",
					(uint32_t)addr, len, num_pages, num_segs));
				DPRINT(("    first sect = %d, last sect = %d\n",fsect,lsect));

				len_adj = len >= PAGE_SIZE - page_offset ?
					PAGE_SIZE - page_offset : len;
				len -= len_adj;
				if (len) {
					addr += len_adj;
				}
				else {
					if (sidx < sgl->NumberOfElements) {
						len = sgl->List[sidx].Length;
						addr = (xenblk_addr_t)
							sgl->List[sidx].PhysicalAddress.QuadPart;
						sidx++;
					}
				}
			}
			ring_req->seg[ring_req->nr_segments].first_sect = (uint8_t)fsect;
			ring_req->seg[ring_req->nr_segments].last_sect = (uint8_t)lsect;
			disk_offset += ((lsect - fsect) + 1);

			gnttab_grant_foreign_access_ref(
				ref,
				info->otherend_id,
				buffer_mfn,
				ring_req->operation & 1);

			info->shadow[id].frame[ring_req->nr_segments] =
				mfn_to_pfn(buffer_mfn);

			ring_req->nr_segments++;
		}

		info->ring.req_prod_pvt++;

		/* Keep a private copy so we can reissue requests when recovering. */
		info->shadow[id].req = *ring_req;
		gnttab_free_grant_references(gref_head);

#ifdef DBG
		info->shadow[id].seq = info->seq;
		InterlockedIncrement(&info->seq);
#endif
		info->shadow[id].srb_ext = srb_ext;
		srb_ext->use_cnt++;
	}

	info->shadow[id].request = srb;
#ifdef DBG
	if (srb->Cdb[0] >= SCSIOP_READ16) {
		DPR_TRC(("Submitting the SCSIOP 16 request %x.\n", srb->Cdb[0]));
	}
	storport_acquire_spinlock(info->xbdev, &info->lock,
		InterruptLock, NULL, &lh);
	if (info->req) {
		DBG_PRINT(("Queued requests = %d, seq %d, cseq %d.\n", info->req,
			info->shadow[id].seq, info->cseq));
	}
	InterlockedIncrement(&info->req);
	storport_xenblk_release_spinlock(info->xbdev, &info->lock, lh);
#endif

	/* Check if there are virtual and system addresses that need to be */
	/* freed and unmapped now that we are at DPC time. */
	xenblk_unmap_system_addresses(info);

	flush_requests(info);
	//xenblk_release_spinlock(info->xbdev, &info->lock, lh);
	scsiport_release_spinlock(&info->lock, lh);
	DPR_TRC(("      blkif_queue_request - OUT srb %p, srb_ext %p, va %p\n",
			 srb, srb_ext, srb_ext->va));
	XENBLK_INC(io_srbs_seen);
	return STATUS_SUCCESS;
}

static void
XenBlkCompleteRequest(struct blkfront_info *info, SCSI_REQUEST_BLOCK *srb,
	unsigned int status)
{
	xenblk_srb_extension *srb_ext;
	unsigned int len;
#ifdef XENBLK_REQUEST_VERIFIER
	uint32_t *pu32;
#endif

	DPR_TRC(("    XenBlkCompleteRequest - in \n"));
	srb_ext = (xenblk_srb_extension *)srb->SrbExtension;

#ifdef DBG
	if (srb->Cdb[0] >= SCSIOP_READ16) {
		DPR_TRC(("Start completing the SCSIOP 16 request %x.\n", srb->Cdb[0]));
	}
#endif
	if (srb_ext->va) {
		if (srb->Cdb[0] == SCSIOP_READ || srb->Cdb[0] == SCSIOP_READ16) {
			DPR_MM((" xenblk_cp_to_sa: srb %p, ext %p, va %p, sa %p\n",
					srb, srb_ext, srb_ext->va, srb_ext->sa));
			xenblk_cp_to_sa(srb_ext->sa, srb_ext->sys_sgl, srb_ext->va);
			DPR_MM(("\tRtlCopyMemory done.\n"));
#ifdef XENBLK_REQUEST_VERIFIER
			pu32 = (uint32_t *)(srb_ext->va + srb->DataTransferLength);
			for (len = 0; len < PAGE_SIZE; len += 4) {
				if (*pu32 != 0xabababab) {
					DBG_PRINT(("** Overwrite at %x: %x.\n", len, *pu32));
				}
			}
#endif
		}

		/* Save the virtual and system addresses so that they can be */
		/* freed and unmapped at DPC time rather than at interrupt time. */
		xenblk_save_system_address(info, srb_ext);
		DPR_MM(("\tabout to complete %p.\n", srb));
	}
#ifdef XENBLK_DBG_MAP_SGL_ONLY
	else {
		if (srb->Cdb[0] == SCSIOP_READ || srb->Cdb[0] == SCSIOP_READ16) {
			xenblk_save_system_address(info, srb_ext);
		}
	}
#endif
	xenblk_save_req(info, srb, srb_ext);

	if (status == BLKIF_RSP_OKAY) {
		srb->SrbStatus = SRB_STATUS_SUCCESS;
	}
	else {
		uint64_t disk_offset;

		srb->SrbStatus = SRB_STATUS_ERROR;

		if (srb->Cdb[0] < SCSIOP_READ16) {
			disk_offset = ((uint64_t)((uint32_t)(
				((PCDB)srb->Cdb)->CDB10.LogicalBlockByte3
				| ((PCDB)srb->Cdb)->CDB10.LogicalBlockByte2 << 8
				| ((PCDB)srb->Cdb)->CDB10.LogicalBlockByte1 << 16
				| ((PCDB)srb->Cdb)->CDB10.LogicalBlockByte0 << 24)));
		}
		else {
			REVERSE_BYTES_QUAD(&disk_offset,
				((PCDB)srb->Cdb)->CDB16.LogicalBlock);
		}
		DBG_PRINT(("\tXenBlkCompleteRequest: error status %x\n", status));
		DBG_PRINT(("\top %x, I/O len 0x%x, disk offset 0x%x%08x\n",
			srb->Cdb[0],
			srb->DataTransferLength,
			(uint32_t)(disk_offset >> 32),
			(uint32_t)disk_offset));
	}

#ifdef DBG
	if (should_print_int < SHOULD_PRINT_INT) {
		DPR_INIT(("\tdev = %x, srb = %p, status = %x, ss = %x \n",
			info->xbdev, srb, srb->SrbStatus, srb->ScsiStatus));
	}

	if (srb->Cdb[0] >= SCSIOP_READ16) {
		DPR_TRC(("Done completing the SCSIOP 16 request %x.\n", srb->Cdb[0]));
	}
#endif
	xenblk_next_request(NextRequest, info->xbdev);
	XENBLK_INC(srbs_returned);
	XENBLK_INC(io_srbs_returned);
	XENBLK_INC(sio_srbs_returned);
	xenblk_request_complete(RequestComplete, info->xbdev, srb);
	DPR_TRC(("    XenBlkCompleteRequest - out \n"));
}

uint32_t blkif_complete_int(struct blkfront_info *info)
{
    //XENBLK_LOCK_HANDLE lh;
	SCSI_REQUEST_BLOCK *srb;
	blkif_response_t *bret;
	RING_IDX i, rp;
	unsigned long id;
	uint32_t did_work = 0;
	int more_to_do = 1;
	uint16_t status;
#ifdef DBG
	int outoforder = 0;
#endif

	DPR_TRC(("  blkif_complete_int - IN irql = %d\n", KeGetCurrentIrql()));
	//xenblk_acquire_spinlock(info->xbdev,&info->lock,InterruptLock,NULL, &lh);

	if (info->connected) {
		while (more_to_do) {
			rp = info->ring.sring->rsp_prod;
			rmb(); /* Ensure we see queued responses up to 'rp'. */

			for (i = info->ring.rsp_cons; i != rp; i++) {
				bret = RING_GET_RESPONSE(&info->ring, i);
				id = (unsigned long)bret->id;

				/*
				blkif_completion(&info->shadow[id]);
					is done right after GET_ID_FROM_FREE_LIST 
				*/

#ifdef DBG
				if (info->shadow[id].seq > info->cseq) {
					DBG_PRINT(("XENBLK: sequence, %x - %x: req %p, status %x\n",
						info->shadow[id].seq, info->cseq,
						info->shadow[id].request, bret->status));
					xenblk_print_cur_req(info,
						(SCSI_REQUEST_BLOCK *)info->shadow[id].request);

				}
				InterlockedIncrement(&info->cseq);
#endif
				info->shadow[id].srb_ext->use_cnt--;

				if (info->shadow[id].request) {
#ifdef DBG
					if (info->shadow[id].srb_ext->use_cnt) {
						DBG_PRINT(("XENBLK: srb %p  use count %x\n",
							info->shadow[id].request,
							info->shadow[id].srb_ext->use_cnt));
						outoforder = 1;
						info->queued_srb_ext++;
					}
					InterlockedDecrement(&info->req);
#endif
					info->shadow[id].srb_ext->status = bret->status;
					xenblk_add_tail(info, info->shadow[id].srb_ext);
				}

				ADD_ID_TO_FREELIST(info, id);
				did_work++;
			}

			info->ring.rsp_cons = i;

			if (i != info->ring.req_prod_pvt) {
				RING_FINAL_CHECK_FOR_RESPONSES(&info->ring, more_to_do);
			} else {
				info->ring.sring->rsp_event = i + 1;
				more_to_do = 0;
			}
		}

		while (info->hsrb_ext) {
			if (info->hsrb_ext->use_cnt == 0) {
#ifdef DBG
				if (outoforder) {
					DBG_PRINT(("XENBLK: completing sequenced srb %p\n",
						info->hsrb_ext->srb));
					info->queued_srb_ext--;
				}
				else if (info->queued_srb_ext) {
					DBG_PRINT(("XENBLK: completing queued, srb %p\n",
						info->hsrb_ext->srb));
					info->queued_srb_ext--;
				}
#endif
				srb = info->hsrb_ext->srb;
				status = info->hsrb_ext->status;
				info->hsrb_ext = info->hsrb_ext->next;
				XenBlkCompleteRequest(info, srb, status);
			}
			else {
				break;
			}
		}
	}

	//xenblk_release_spinlock(info->xbdev, &info->lock, lh);
	//kick_pending_request_queues(info);
	DPR_TRC(("  blkif_complete_int - OUT\n"));
	return did_work;
}

void
blkif_quiesce(struct blkfront_info *info)
{
	uint32_t j;

#ifdef DBG
	should_print_int = 0;
#endif
	while (info->ring.rsp_cons != info->ring.req_prod_pvt) {
		DPR_INIT(("blkif_quiesce outstanding reqs %p: %x %x \n",
			info, info->ring.rsp_cons, info->ring.req_prod_pvt));
		blkif_complete_int(info);
	}

	/* Clear out any grants that may still be around. */
	DPR_INIT(("blkif_quiesce: doing shadow completion\n"));
	for (j = 0; j < BLK_RING_SIZE; j++) {
		blkif_completion(&info->shadow[j]);
	}
#ifdef DBG
	if (srbs_seen != srbs_returned) {
		DBG_PRINT(("Q: srbs_seen = %x, srbs_returned = %x, diff %d.\n",
			srbs_seen, srbs_returned, srbs_seen - srbs_returned));
	}
	if (sio_srbs_seen != sio_srbs_returned) {
		DBG_PRINT(("Q: sio_srbs_seen = %x, sio_srbs_returned = %x, diff %d.\n",
			sio_srbs_seen, sio_srbs_returned, sio_srbs_seen - sio_srbs_returned));
	}
	if (io_srbs_seen != io_srbs_returned) {
		DBG_PRINT(("Q: io_srbs_seen = %x, io_srbs_returned = %x, diff %d.\n",
			io_srbs_seen, io_srbs_returned, io_srbs_seen - io_srbs_returned));
	}
#endif
}

void
blkif_disconnect_backend(XENBLK_DEVICE_EXTENSION *dev_ext)
{
	uint32_t i;
	uint32_t j;
	char *buf;
	enum xenbus_state backend_state;
	struct blkfront_info *info;

	DPR_INIT(("blkif_disconnect_backend: IN.\n"));
	for (i = 0; i < XENBLK_MAXIMUM_TARGETS; i++) {
		info = dev_ext->info[i];
		if (info) {
			/* Since we are doing the disconnect, unregister the watch so */
			/* we wont get a callback after we have freed resources. */
			unregister_xenbus_watch(&info->watch);
			if (info->irq) {
				DPR_INIT(("      unregister_dpc_from_evtchn %d.\n",
					info->evtchn));
				unregister_dpc_from_evtchn(info->evtchn);
				info->evtchn = info->irq = 0;
			}
			blkif_quiesce(info);

			DPR_INIT(("      switching to closeing: %s.\n", info->nodename));
			xenbus_switch_state(info->nodename, XenbusStateClosing);
			for (j = 0; j < 1000; j++) {
				buf = xenbus_read(XBT_NIL, info->otherend, "state", NULL);
				if (buf) {
					backend_state = (enum xenbus_state)
						xenblk_strtou64(buf, NULL, 10);
					xenbus_free_string(buf);
					if (backend_state == XenbusStateClosing) {
						DPR_INIT(("      back end state is closing.\n"));
						break;
					}
				}
			}

			DPR_INIT(("      switching to closed: %s.\n", info->nodename));
			xenbus_switch_state(info->nodename, XenbusStateClosed);
			for (j = 0; j < 1000; j++) {
				buf = xenbus_read(XBT_NIL, info->otherend, "state", NULL);
				if (buf) {
					backend_state = (enum xenbus_state)
						xenblk_strtou64(buf, NULL, 10);
					xenbus_free_string(buf);
					if (backend_state == XenbusStateClosed) {
						DPR_INIT(("      back end state is closed.\n"));
						break;
					}
				}
			}
			if (info->ring_ref) {
				gnttab_end_foreign_access(info->ring_ref, 0);
			}
		}
	}
	XenBlkFreeAllResources(dev_ext, RELEASE_ONLY);
	DPR_INIT(("blkif_disconnect_backend: alloc_cnt i %d, s %d, v %d OUT.\n",
			  dev_ext->alloc_cnt_i,
			  dev_ext->alloc_cnt_s,
			  dev_ext->alloc_cnt_v
			  ));
	dev_ext->op_mode = OP_MODE_DISCONNECTED;
}

static void blkif_int(PKDPC dpc, PVOID dcontext, PVOID sa1, PVOID sa2)
{
	struct blkfront_info *info = (struct blkfront_info *)dcontext;
#ifdef XENBUS_HAS_DEVICE
	KIRQL oldirql;

	DPRINT(("blkif_int calling blkif_complete_int\n"));
	KeAcquireSpinLock(&info->lock, &oldirql);
	blkif_complete_int(info);
	KeReleaseSpinLock(&info->lock, oldirql);
#else
	blkif_complete_int(info);
#endif
}

static void blkif_free(struct blkfront_info *info, int suspend)
{
    XENBLK_LOCK_HANDLE lh;

	/* Prevent new requests being issued until we fix things up. */
	xenblk_acquire_spinlock(info->xbdev, &info->lock, InterruptLock, NULL, &lh);
	XENBLK_SET_FLAG(info->xenblk_locks, (BLK_FRE_L | BLK_INT_L));
	XENBLK_SET_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
	info->connected = suspend ?
		BLKIF_STATE_SUSPENDED : BLKIF_STATE_DISCONNECTED;
	/* No more blkif_request(). */
	//kra todo
	//if (info->rq)
	//	blk_stop_queue(info->rq);
	/* No more gnttab callback work. */
	gnttab_cancel_free_callback(&info->callback);
	XENBLK_CLEAR_FLAG(info->xenblk_locks, (BLK_FRE_L | BLK_INT_L));
	XENBLK_CLEAR_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
	xenblk_release_spinlock(info->xbdev, &info->lock, lh);

	/* Flush gnttab callback work. Must be done with no locks held. */
	//kra todo
	//flush_scheduled_work();

	/* Free resources associated with old device channel. */
	if (info->ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(info->ring_ref, 0);
					  //(unsigned long)info->ring.sring);
		info->ring_ref = GRANT_INVALID_REF;
		info->ring.sring = NULL;
	}
	if (info->irq)
		unregister_dpc_from_evtchn(info->evtchn);
		//unbind_from_irqhandler(info->irq, info);
	info->evtchn = info->irq = 0;

}

static void blkif_completion(struct blk_shadow *s)
{
	int i;

	for (i = 0; i < s->req.nr_segments; i++) {
#ifdef DBG
/*
		if (should_print_int < SHOULD_PRINT_INT) {
			DPR_INIT(("blkif_completion: i = %d, gref = %x.\n",
					  i, s->req.seg[i].gref));
		}
*/
#endif
		gnttab_end_foreign_access(s->req.seg[i].gref, 0);
#ifdef DBG
		if (should_print_int < SHOULD_PRINT_INT) {
			DPR_INIT(("blkif_completion: gnttab_end_foreign_access back: i = %d, gref = %x.\n",
				i, s->req.seg[i].gref));
		}
#endif
	}
	s->req.nr_segments = 0;
#ifdef DBG
/*
		if (should_print_int < SHOULD_PRINT_INT) {
			DPR_INIT(("blkif_completion: OUT\n"));
		}
*/
#endif
}

#if 0
static void blkif_recover(struct blkfront_info *info)
{
    XENBLK_LOCK_HANDLE lh;
	int i;
	blkif_request_t *req;
	struct blk_shadow *copy;
	int j;

	/* Stage 1: Make a safe copy of the shadow state. */
	copy = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, XENBLK_TAG_GENERAL);
	if (!copy) {
		return;
	}
	RtlCopyMemory(copy, info->shadow, sizeof(info->shadow));

	/* Stage 2: Set up free list. */
	RtlZeroMemory(&info->shadow, sizeof(info->shadow));
	for (i = 0; i < BLK_RING_SIZE; i++)
		info->shadow[i].req.id = i+1;
	info->shadow_free = info->ring.req_prod_pvt;
	info->shadow[BLK_RING_SIZE-1].req.id = 0x0fffffff;

	/* Stage 3: Find pending requests and requeue them. */
	for (i = 0; i < BLK_RING_SIZE; i++) {
		/* Not in use? */
		if (copy[i].request == NULL)
			continue;

		/* Grab a request slot and copy shadow state into it. */
		req = RING_GET_REQUEST(
			&info->ring, info->ring.req_prod_pvt);
		*req = copy[i].req;

		/* We get a new request id, and must reset the shadow state. */
		req->id = GET_ID_FROM_FREELIST(info);
		blkif_completion(&info->shadow[req->id]);
		RtlCopyMemory(&info->shadow[req->id], &copy[i], sizeof(copy[i]));

		/* Rewrite any grant references invalidated by susp/resume. */
		for (j = 0; j < req->nr_segments; j++)
			gnttab_grant_foreign_access_ref(
				req->seg[j].gref,
				info->otherend_id,
				pfn_to_mfn(info->shadow[req->id].frame[j]),
				req->operation & 1);
		
				//rq_data_dir(
				//	(struct request *)
				//	info->shadow[req->id].request));
		info->shadow[req->id].req = *req;

		info->ring.req_prod_pvt++;
	}

	ExFreePool(copy);

	xenbus_switch_state(info->nodename, XenbusStateConnected);

	xenblk_acquire_spinlock(info->xbdev, &info->lock, InterruptLock, NULL, &lh);
	XENBLK_SET_FLAG(info->xenblk_locks, BLK_INT_L);
	XENBLK_SET_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));

	/* Now safe for us to use the shared ring */
	info->connected = BLKIF_STATE_CONNECTED;

	/* Send off requeued requests */
	flush_enabled(info);

	/* Kick any other new requests queued since we resumed */
	//kick_pending_request_queues(info);

	XENBLK_CLEAR_FLAG(info->xenblk_locks, BLK_INT_L);
	XENBLK_CLEAR_FLAG(info->cpu_locks, (1 << KeGetCurrentProcessorNumber()));
	xenblk_release_spinlock(info->xbdev, &info->lock, lh);
	kick_pending_request_queues(info);
}
#endif


/* ** Driver Registration ** */


static void xlblk_exit(void)
{
	//return xenbus_unregister_driver(&blkfront);
}

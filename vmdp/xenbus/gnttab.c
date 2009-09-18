/*****************************************************************************
 *
 *   File Name:      gnttab.c
 *
 *   %version:       17 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jan 21 11:36:16 2009 %
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

#include "xenbus.h"
grant_ref_t g_gnttab_list[MAX_NR_GRANT_ENTRIES];
grant_ref_t g_gnttab_free_head;
int g_gnttab_free_count;

static grant_ref_t *gnttab_list;
static int *gnttab_free_count;
static grant_ref_t *gnttab_free_head;

static KSPIN_LOCK gnttab_list_lock;

static struct grant_entry *shared = NULL;

static struct gnttab_free_callback *gnttab_free_callback_list;

static int gntlock = 0;

#define GnttabAcquireSpinLock(_l, _h) 									\
{																		\
	if (KeGetCurrentIrql() <= DISPATCH_LEVEL) {							\
		XenAcquireSpinLock((_l), (_h));									\
		gntlock++;														\
	}																	\
}

#define GnttabReleaseSpinLock(_l, _h)									\
{																		\
	if (gntlock) {														\
		gntlock--;														\
		XenReleaseSpinLock((_l), (_h));									\
	}																	\
}

static int get_free_entries(int count)
{
    int ref;
    grant_ref_t head;
	XEN_LOCK_HANDLE	lh;

    XenAcquireSpinLock(&gnttab_list_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_GNT);

    if (*gnttab_free_count < count) {
		XENBUS_CLEAR_FLAG(xenbus_locks, X_GNT);
        XenReleaseSpinLock(&gnttab_list_lock, lh);
        return -1;
    }

    ref = head = *gnttab_free_head;
    *gnttab_free_count -= count;
    while (count-- > 1)
        head = gnttab_list[head];
    *gnttab_free_head = gnttab_list[head];
    gnttab_list[head] = gGNTTAB_LIST_END;

	XENBUS_CLEAR_FLAG(xenbus_locks, X_GNT);
	XenReleaseSpinLock(&gnttab_list_lock, lh);
    return ref;
}

#define get_free_entry() get_free_entries(1)

static void do_free_callbacks(void)
{
    struct gnttab_free_callback *callback, *next;

    callback = gnttab_free_callback_list;
    gnttab_free_callback_list = NULL;

    while (callback != NULL) {
        next = callback->next;
        if (*gnttab_free_count >= callback->count) {
            callback->next = NULL;
            callback->fn(callback->arg);
        } else {
            callback->next = gnttab_free_callback_list;
            gnttab_free_callback_list = callback;
        }
        callback = next;
    }
}

static __inline void check_free_callbacks(void)
{
    if (gnttab_free_callback_list)
        do_free_callbacks();
}

static void put_free_entry(grant_ref_t ref)
{
    XEN_LOCK_HANDLE lh;

    XenAcquireSpinLock(&gnttab_list_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_GNT);

    gnttab_list[ref] = *gnttab_free_head;
    *gnttab_free_head = ref;
    (*gnttab_free_count)++;
    check_free_callbacks();

	XENBUS_CLEAR_FLAG(xenbus_locks, X_GNT);
	XenReleaseSpinLock(&gnttab_list_lock, lh);
}

/*
 * Public grant-issuing interface functions
 */

/*
 * Typical windows way of usint memory barriers is to set up a mutex protected
 * section. The compiler will generate mbs to prevent from reordering. However,
 * since we are writing driver exclusively for Windows 2003 server, here the
 * Windows 2003 only KeMemoryBarrier() is used. This may be subjected to change
 * in future.
 *
 * DDK support routines InterlockedCompareExchange and InterlockedExchange has
 * only 32bit and 64bit version, we are introducing our own 16bit version for
 * grant_entry.flags access.
 */

int gnttab_grant_foreign_access(domid_t domid, unsigned long frame,
                                int readonly)
{
    int ref;

    if ((ref = get_free_entry()) == -1)
        return -1;

    shared[ref].frame = frame;
    shared[ref].domid = domid;
    KeMemoryBarrier();
    InterlockedExchange16(
      &shared[ref].flags,
      GTF_permit_access | (readonly ? GTF_readonly : 0));

    return ref;
}

void gnttab_grant_foreign_access_ref(grant_ref_t ref, domid_t domid,
                                     unsigned long frame, int readonly)
{
    shared[ref].frame = frame;
    shared[ref].domid = domid;
    KeMemoryBarrier();
    InterlockedExchange16(
      &shared[ref].flags,
      GTF_permit_access | (readonly ? GTF_readonly : 0));
}


int gnttab_query_foreign_access(grant_ref_t ref)
{
    u16 nflags;

    nflags = shared[ref].flags;

    return (nflags & (GTF_reading|GTF_writing));
}

uint16_t gnttab_query_foreign_access_flags(grant_ref_t ref)
{
    return shared[ref].flags;
}


int gnttab_end_foreign_access_ref(grant_ref_t ref, int readonly)
{
	uint32_t cnt = 0;
    u16 flags, nflags;

	//DPR_INIT(("gnttab_end_foreign_access_ref %d.\n", ref));
    nflags = shared[ref].flags;
    do {
        if ((flags = nflags) & (GTF_reading|GTF_writing)) {
            DBG_PRINT(("XENBUS gnttab_end_foreign_access_ref: ref %x, flags %x\n",
				ref, nflags));
			return 0;
        }
		cnt++;
		if (cnt > 1000) {
			DPR_INIT(("gnttab_end_foreign_access_ref %d, f %x, nf %x.\n", ref, flags, nflags));
		}
    } while ((nflags = InterlockedCompareExchange16(&shared[ref].flags, 0, flags))
             != flags);

    return 1;
}

void gnttab_end_foreign_access(grant_ref_t ref, int readonly)
{

	//DPR_INIT(("gnttab_end_foreign_access %d.\n", ref));
    if (gnttab_end_foreign_access_ref(ref, readonly)) {
        put_free_entry(ref);
    }
    else {
        /* XXX This needs to be fixed so that the ref and page are
           placed on a list to be freed up later. */
        DBG_PRINT(("XENBUS: leaking g.e. and page still in use!\n"));
    }
}

#if 0
int gnttab_grant_foreign_transfer(domid_t domid, unsigned long pfn)
{
    int ref;

    if ((ref = get_free_entry()) == -1)
        return -1;
    gnttab_grant_foreign_transfer_ref(ref, domid, pfn);

    return ref;
}

void gnttab_grant_foreign_transfer_ref(grant_ref_t ref, domid_t domid,
                                       unsigned long pfn)
{
    shared[ref].frame = pfn;
    shared[ref].domid = domid;
    KeMemoryBarrier();
    InterlockedExchange16(
      &shared[ref].flags,
      GTF_accept_transfer);
}

unsigned long gnttab_end_foreign_transfer_ref(grant_ref_t ref)
{
    unsigned long frame;
    u16           flags;

    /*
     * If a transfer is not even yet started, try to reclaim the grant
     * reference and return failure (== 0).
     */
    while (!((flags = shared[ref].flags) & GTF_transfer_committed)) {
        if (InterlockedCompareExchange16(&shared[ref].flags, 0, flags) == flags)
            return 0;
        //cpu_relax();
    }

    /* If a transfer is in progress then wait until it is completed. */
    while (!(flags & GTF_transfer_completed)) {
        flags = shared[ref].flags;
        //cpu_relax();
    }

    /* Read the frame number /after/ reading completion status. */
    KeMemoryBarrier();
    frame = shared[ref].frame;
    //BUG_ON(frame == 0);

    return frame;
}

unsigned long gnttab_end_foreign_transfer(grant_ref_t ref)
{
    unsigned long frame = gnttab_end_foreign_transfer_ref(ref);
    put_free_entry(ref);
    return frame;
}
#endif

void gnttab_free_grant_reference(grant_ref_t ref)
{
    put_free_entry(ref);
}

void gnttab_free_grant_references(grant_ref_t head)
{
    grant_ref_t ref;
    XEN_LOCK_HANDLE lh;
    int count = 1;

    if (head == gGNTTAB_LIST_END)
        return;

    XenAcquireSpinLock(&gnttab_list_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_GNT);
    ref = head;
    while (gnttab_list[ref] != gGNTTAB_LIST_END) {
        ref = gnttab_list[ref];
        count++;
#ifdef DBG
		if ((unsigned int)count > gGNTTAB_LIST_END) {
			DBG_PRINT(("gnttab_free_grant_references: stuck in while.\n"));
		}
#endif
    }
    gnttab_list[ref] = *gnttab_free_head;
    *gnttab_free_head = head;
    *gnttab_free_count += count;
    check_free_callbacks();
	XENBUS_CLEAR_FLAG(xenbus_locks, X_GNT);
	XenReleaseSpinLock(&gnttab_list_lock, lh);
}

int gnttab_alloc_grant_references(u16 count, grant_ref_t *head)
{
    int h = get_free_entries(count);

	//DPR_INIT(("gnttab_alloc_grant_references shared = %x.\n", shared));
    if (h == -1)
        return -1;

    *head = h;

    return 0;
}

int gnttab_empty_grant_references(const grant_ref_t *private_head)
{
    return (*private_head == gGNTTAB_LIST_END);
}

int gnttab_claim_grant_reference(grant_ref_t *private_head)
{
    grant_ref_t g = *private_head;
    if (g == gGNTTAB_LIST_END)
        return -1;
    *private_head = gnttab_list[g];
    return g;
}

void gnttab_release_grant_reference(grant_ref_t *private_head,
                                    grant_ref_t release)
{
    gnttab_list[release] = *private_head;
    *private_head = release;
}

void gnttab_request_free_callback(struct gnttab_free_callback *callback,
                                  void (*fn)(void *), void *arg, u16 count)
{
    XEN_LOCK_HANDLE lh;
    XenAcquireSpinLock(&gnttab_list_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_GNT);
    if (callback->next)
        goto out;
    callback->fn = fn;
    callback->arg = arg;
    callback->count = count;
    callback->next = gnttab_free_callback_list;
    gnttab_free_callback_list = callback;
    check_free_callbacks();
out:
	XENBUS_CLEAR_FLAG(xenbus_locks, X_GNT);
	XenReleaseSpinLock(&gnttab_list_lock, lh);
}

void gnttab_cancel_free_callback(struct gnttab_free_callback *callback)
{
    struct gnttab_free_callback **pcb;
    XEN_LOCK_HANDLE lh;

    XenAcquireSpinLock(&gnttab_list_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_GNT);
    for (pcb = &gnttab_free_callback_list; *pcb; pcb = &(*pcb)->next) {
        if (*pcb == callback) {
            *pcb = callback->next;
            break;
        }
    }
	XENBUS_CLEAR_FLAG(xenbus_locks, X_GNT);
	XenReleaseSpinLock(&gnttab_list_lock, lh);
}

static NTSTATUS gnttab_resume(void)
{
    PHYSICAL_ADDRESS addr;
    unsigned long frame;
    int32_t x;
    struct xen_add_to_physmap_compat xatp;

    DPR_INIT(("XENBUS: gnttab_resume - IN\n"));

    if (alloc_xen_mmio(PAGE_SIZE * gNR_GRANT_FRAMES, &addr.QuadPart)
		!= STATUS_SUCCESS)
		return STATUS_INSUFFICIENT_RESOURCES;

    frame = (ULONG) (addr.QuadPart >> PAGE_SHIFT);

	/* By looping in reverse order, the grant table is only expanced once. */
    for (x = gNR_GRANT_FRAMES - 1; x >= 0;  x--) {
        xatp.domid = DOMID_SELF;
        xatp.idx = x;
        xatp.space = XENMAPSPACE_grant_table;
        xatp.gpfn = frame + x;
        if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp) != 0) {
            DBG_PRINT(("XENBUS: grant table hypercall failed.\n"));
            return STATUS_UNSUCCESSFUL;
        }
    }

    DPR_INIT(("XENBUS: gnttab_resume - OUT\n"));
    return STATUS_SUCCESS;
}

VOID gnttab_suspend(void)
{
	shared = NULL;
}

NTSTATUS gnttab_init(void)
{
    NTSTATUS status;

    DPR_INIT(("XENBUS: gnttab_init - IN\n"));

	KeInitializeSpinLock(&gnttab_list_lock);
    status = gnttab_resume();
    if (!NT_SUCCESS(status))
        return status;

    DPR_INIT(("XENBUS: gnttab_init - OUT, success\n"));
    return STATUS_SUCCESS;
}

NTSTATUS gnttab_finish_init(PDEVICE_OBJECT fdo, uint32_t reason)
{
    PFDO_DEVICE_EXTENSION fdx;
    uint32_t i;

    DPR_INIT(("XENBUS: gnttab_finish_init - IN\n"));

    if (alloc_xen_shared_mem(PAGE_SIZE * gNR_GRANT_FRAMES, &shared)
		!= STATUS_SUCCESS)
		return STATUS_UNSUCCESSFUL;

	fdx = (PFDO_DEVICE_EXTENSION) fdo->DeviceExtension;
	gnttab_list = fdx->gnttab_list;
	gnttab_free_count = fdx->gnttab_free_count;
	gnttab_free_head = fdx->gnttab_free_head;

	if (reason != OP_MODE_CRASHDUMP) {
		/* Only do this if we are not doing a crash dump. */
		for (i = NR_RESERVED_ENTRIES; i < gNR_GRANT_ENTRIES; i++)
			gnttab_list[i] = i + 1;
		*gnttab_free_count = gNR_GRANT_ENTRIES - NR_RESERVED_ENTRIES;
		*gnttab_free_head  = NR_RESERVED_ENTRIES;

		memset(shared, 0, PAGE_SIZE * gNR_GRANT_FRAMES);
		KeMemoryBarrier();
	}

    DPR_INIT(("XENBUS: gnttab_finish_init l = %p, h = %p:%d- OUT\n",
			  gnttab_list, &gnttab_free_head, gnttab_free_head));
    return STATUS_SUCCESS;
}

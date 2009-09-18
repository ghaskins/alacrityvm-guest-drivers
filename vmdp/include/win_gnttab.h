/*****************************************************************************
 *
 *   File Name:      win_gnttab.h
 *
 *   %version:       7 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon May 12 16:11:38 2008 %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2006-2007 Unpublished Work of Novell, Inc. All Rights Reserved.
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

#ifndef _GNTTAB_H
#define _GNTTAB_H

#include <xen/public/grant_table.h>
#include <win_features.h>
#include <asm/win_hypervisor.h>

#define MIN_NR_GRANT_FRAMES 4
#define MAX_NR_GRANT_FRAMES 10

#define MIN_NR_GRANT_ENTRIES    \
	(MIN_NR_GRANT_FRAMES * PAGE_SIZE / sizeof(struct grant_entry))

#define MAX_NR_GRANT_ENTRIES    \
	(MAX_NR_GRANT_FRAMES * PAGE_SIZE / sizeof(struct grant_entry))

struct gnttab_free_callback
{
    struct gnttab_free_callback *next;
    void (*fn)(void *);
    void *arg;
    u16 count;
};

DLLEXPORT int
gnttab_grant_foreign_access(domid_t domid, unsigned long frame,
                                int readonly);

/*
 * End access through the given grant reference, iff the grant entry is no
 * longer in use.  Return 1 if the grant entry was freed, 0 if it is still in
 * use.
 */
DLLEXPORT int
gnttab_end_foreign_access_ref(grant_ref_t ref, int readonly);

DLLEXPORT void
gnttab_end_foreign_access(grant_ref_t ref, int readonly);

DLLEXPORT int
gnttab_grant_foreign_transfer(domid_t domid, unsigned long pfn);

DLLEXPORT unsigned long
gnttab_end_foreign_transfer_ref(grant_ref_t ref);
DLLEXPORT unsigned long
gnttab_end_foreign_transfer(grant_ref_t ref);

DLLEXPORT int
gnttab_query_foreign_access(grant_ref_t ref);

DLLEXPORT uint16_t
gnttab_query_foreign_access_flags(grant_ref_t ref);

/*
 * operations on reserved batches of grant references
 */
DLLEXPORT int
gnttab_alloc_grant_references(u16 count, grant_ref_t *pprivate_head);

DLLEXPORT void
gnttab_free_grant_reference(grant_ref_t ref);

DLLEXPORT void
gnttab_free_grant_references(grant_ref_t head);

DLLEXPORT int
gnttab_empty_grant_references(const grant_ref_t *pprivate_head);

DLLEXPORT int
gnttab_claim_grant_reference(grant_ref_t *pprivate_head);

DLLEXPORT void
gnttab_release_grant_reference(grant_ref_t *private_head,
                                    grant_ref_t release);

DLLEXPORT void
gnttab_request_free_callback(struct gnttab_free_callback *callback,
                                  void (*fn)(void *), void *arg, u16 count);
DLLEXPORT void
gnttab_cancel_free_callback(struct gnttab_free_callback *callback);

DLLEXPORT void
gnttab_grant_foreign_access_ref(grant_ref_t ref, domid_t domid,
                                     unsigned long frame, int readonly);

DLLEXPORT void
gnttab_grant_foreign_transfer_ref(grant_ref_t, domid_t domid,
                                       unsigned long pfn);

#define gnttab_map_vaddr(map) ((void *)(map.host_virt_addr))


static __inline void
gnttab_set_map_op(struct gnttab_map_grant_ref *map, unsigned long addr,
                  uint32_t flags, grant_ref_t ref, domid_t domid)
{
    PHYSICAL_ADDRESS physaddr;

    if (flags & GNTMAP_contains_pte) {
        map->host_addr = addr;
    }
    else if (xen_feature(XENFEAT_auto_translated_physmap)) {
        physaddr = MmGetPhysicalAddress((void *)((ULONG_PTR)addr));
        map->host_addr = physaddr.QuadPart;
    }
    else {
        map->host_addr = addr;
    }

    map->flags = flags;
    map->ref = ref;
    map->dom = domid;
}

static __inline void
gnttab_set_unmap_op(struct gnttab_unmap_grant_ref *unmap, unsigned long addr,
                    uint32_t flags, grant_handle_t handle)
{
    PHYSICAL_ADDRESS physaddr;

    if (flags & GNTMAP_contains_pte) {
        unmap->host_addr = addr;
    }
    else if (xen_feature(XENFEAT_auto_translated_physmap)) {
        physaddr = MmGetPhysicalAddress((void *)((ULONG_PTR)addr));
        unmap->host_addr = physaddr.QuadPart;
    }
    else {
        unmap->host_addr = addr;
    }

    unmap->handle = handle;
    unmap->dev_bus_addr = 0;
}

#endif

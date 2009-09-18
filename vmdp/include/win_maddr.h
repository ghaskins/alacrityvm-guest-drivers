/*****************************************************************************
 *
 *   File Name:      win_maddr.h
 *
 *   %version:       5 %
 *   %derived_by:    kallan %
 *   %date_modified: Fri Feb 08 16:01:49 2008 %
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

#ifndef _WIN_MADDR_H
#define _WIN_MADDR_H

#include <win_features.h>

NTKERNELAPI
PHYSICAL_ADDRESS
MmGetPhysicalAddress (
    IN PVOID BaseAddress
    );


#define INVALID_P2M_ENTRY (~0UL)

static __inline unsigned long long
__pa(PVOID vaddr)
{
    PHYSICAL_ADDRESS paddr;
    paddr = MmGetPhysicalAddress(vaddr);
    return paddr.QuadPart;
}

static __inline unsigned long
pfn_to_mfn(unsigned long pfn)
{
#ifdef DBG
    if (!xen_feature(XENFEAT_auto_translated_physmap))
        KdPrint(("XENFEAT_auto_translated_physmap not set!\n"));
#endif
    return pfn;
}

static __inline unsigned long
mfn_to_pfn(unsigned long mfn)
{
#ifdef DBG
    if (!xen_feature(XENFEAT_auto_translated_physmap))
        KdPrint(("XENFEAT_auto_translated_physmap not set!\n"));
#endif
    return mfn;
}

typedef unsigned long paddr_t;
typedef unsigned long maddr_t;

#ifndef PAGE_MASK
# define PAGE_MASK (~((1 << PAGE_SHIFT) - 1))
#endif

static __inline maddr_t
phys_to_machine(paddr_t phys)
{
    maddr_t machine = pfn_to_mfn(phys >> PAGE_SHIFT);
    machine = (machine << PAGE_SHIFT) | (phys & ~PAGE_MASK);
    return machine;
}

#define virt_to_machine(v)  (phys_to_machine((paddr_t)(__pa((PVOID)v))))
#define virt_to_mfn(v)      (pfn_to_mfn((unsigned long)(__pa((PVOID)v) >> PAGE_SHIFT)))
#define phys_to_mfn(p)      ((unsigned long)((p) >> PAGE_SHIFT))

#endif

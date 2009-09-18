/*****************************************************************************
 *
 *   File Name:      win_hvm.h
 *
 *   %version:       3 %
 *   %derived_by:    %
 *   %date_modified: %
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

#ifndef _HVM_H
#define _HVM_H


#include <xen/public/hvm/params.h>
#include <asm/win_hypervisor.h>

static __inline uint64_t
hvm_get_parameter(int idx)
{
    struct xen_hvm_param xhv;
    int r;

    xhv.domid = DOMID_SELF;
    xhv.index = idx;

    r = HYPERVISOR_hvm_op(HVMOP_get_param, &xhv);
    if (r < 0) {
        DbgPrint("XENBUS: cannot get hvm parameter %d: %d.\n",
                 idx, r);
        return 0;
    }
    return xhv.value;
}

#endif
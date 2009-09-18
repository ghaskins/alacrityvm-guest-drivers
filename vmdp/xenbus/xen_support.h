/*****************************************************************************
 *
 *   File Name:      xen_support.h
 *
 *   %version:       6 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu May 22 15:21:13 2008 %
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

#ifndef _XENSUPPORT_H
#define _XENSUPPORT_H

NTSTATUS alloc_xen_mmio(unsigned long len, uint64_t *phys_addr);
NTSTATUS alloc_xen_shared_mem(uint32_t len, void **addr);

void xen_info_cleanup(void);

char *kasprintf(size_t len, const char *fmt, ...);

extern uint32_t gNR_GRANT_FRAMES;
extern uint32_t gNR_GRANT_ENTRIES;
extern uint32_t gGNTTAB_LIST_END;


NTSTATUS InitializeHypercallPage(VOID);

#endif

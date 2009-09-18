/*****************************************************************************
 *
 *   File Name:      xenbus_comms.h
 *
 *   %version:       5 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Apr 14 15:02:36 2008 %
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

#ifndef _XENBUS_COMMS_H
#define _XENBUS_COMMS_H

NTSTATUS xs_finish_init(PDEVICE_OBJECT fdo, uint32_t reason);
NTSTATUS xs_init(uint32_t reason);

extern int xen_store_evtchn;
extern struct xenstore_domain_interface *xen_store_interface;

VOID xs_cleanup(void);

NTSTATUS xenbus_probe_init(PDEVICE_OBJECT fdo, uint32_t reason);

#endif
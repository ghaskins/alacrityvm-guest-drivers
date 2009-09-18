/*****************************************************************************
 *
 *   File Name:      win_evtchn.h
 *
 *   %version:       6 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Oct 01 15:31:47 2008 %
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

#ifndef _EVTCHN_H
#define _EVTCHN_H

#include <xen/public/event_channel.h>
#include <win_xeninfo.h>

DLLEXPORT void mask_evtchn(int port);
DLLEXPORT void unmask_evtchn(int port);
DLLEXPORT void notify_remote_via_irq(int irq);
DLLEXPORT void unbind_evtchn_from_irq(unsigned int evtchn);

DLLEXPORT NTSTATUS
register_dpc_to_evtchn(
  ULONG evtchn,
  PKDEFERRED_ROUTINE dpcroutine,
  PVOID dpccontext,
  uint32_t *int_indication
  );

DLLEXPORT VOID
unregister_dpc_from_evtchn(ULONG evtchn);

DLLEXPORT void force_evtchn_callback(void);

static __inline int
notify_remote_via_evtchn(int port)
{
    struct evtchn_send send;

    send.port = port;
    return HYPERVISOR_event_channel_op(EVTCHNOP_send, &send);
}

#endif

/*****************************************************************************
 *
 *   File Name:      sendrecv.c
 *
 *   %version:       23 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jul 02 10:07:51 2009 %
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

#include <ndis.h>
#include "miniport.h"

VOID
VNIFInterruptDpc(
  IN PKDPC Dpc,
  IN PVOID DeferredContext,
  IN PVOID SystemArgument1,
  IN PVOID SystemArgument2)
{
    PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) DeferredContext;
	int did_work = 0;
	LARGE_INTEGER li;

    DPRINT(("VNIF: Interrupt Dpc - IN.\n"));

	//timer_should_call_isr = 0;

	if (!adapter) {
		DBG_PRINT(("VNIFInterruptDpc: adapter is null.\n"));
		return;
	}
#ifdef DBG
	if (KeGetCurrentIrql() > 2) {
		DPR_INIT(("VNIFInterruptDpc irql = %d\n", KeGetCurrentIrql()));
	}
	if (adapter->dbg_print_cnt < 5) {
		DPR_INIT(("VNIFInterruptDpc: dbg prnt %p, %s %d.\n",
			adapter, adapter->Nodename, adapter->dbg_print_cnt));
		adapter->dbg_print_cnt++;
	}
#endif

    did_work = VNIFCheckSendCompletion(adapter);


	if (RING_HAS_UNCONSUMED_RESPONSES(&adapter->rx)) {
		VNIFReceivePackets(adapter);
		did_work++;
	}

	/* Xenbus will mask the evtchn before scheduling the DPC. */
	/* Unmask here to allow xen to inject more interrupts. */
	//unmask_evtchn(adapter->evtchn);
	DPRINT(("VNIF: Interrupt Dpc - OUT.\n"));
}

uint32_t
xennet_should_complete_packet(PVNIF_ADAPTER adapter, PUCHAR dest, UINT len)
{
	PUCHAR mac;
	uint32_t i, j;

    if (!VNIF_IS_READY(adapter)) {
		adapter->in_discards++;
		return VNIF_RECEIVE_DISCARD;
	}

	DPRINT(("should complete %x.\n", dest[0x14]));
	/* Is this a directed packet? */
	if ((dest[0] & 0x01) == 0) {
#ifdef NDIS60_MINIPORT
		adapter->ifHCInUcastPkts++;
		adapter->ifHCInUcastOctets += len;
#endif
		if (adapter->PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {
			return VNIF_RECEIVE_COMPLETE;
		}
		if (adapter->PacketFilter & NDIS_PACKET_TYPE_DIRECTED) {
			mac = adapter->CurrentAddress;
			for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++) {
				if (mac[i] != dest[i]) {
					adapter->in_discards++;
					return VNIF_RECEIVE_DISCARD;
				}
			}
			return VNIF_RECEIVE_COMPLETE;
		}
		adapter->in_discards++;
		return VNIF_RECEIVE_DISCARD;
	}

	/* Must be a broadcast or multicast. */
	if (ETH_IS_BROADCAST(dest)) {
#ifdef NDIS60_MINIPORT
		adapter->ifHCInBroadcastPkts++;
		adapter->ifHCInBroadcastOctets += len;
#endif
		if (adapter->PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {
			return VNIF_RECEIVE_COMPLETE;
		}
		if (adapter->PacketFilter & NDIS_PACKET_TYPE_BROADCAST) {
			return VNIF_RECEIVE_COMPLETE;
		}
		adapter->in_discards++;
		return VNIF_RECEIVE_DISCARD;
	}

	/* Must be a multicast */
#ifdef NDIS60_MINIPORT
	adapter->ifHCInMulticastPkts++;
	adapter->ifHCInMulticastOctets += len;
#endif
	if (adapter->PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {
		return VNIF_RECEIVE_COMPLETE;
	}
	if (adapter->PacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) {
		return VNIF_RECEIVE_COMPLETE;
	}
	if ((adapter->PacketFilter & NDIS_PACKET_TYPE_MULTICAST) == 0) {
		adapter->in_discards++;
		return VNIF_RECEIVE_DISCARD;
	}

	/* Now we have to search the multicast list to see if there is a match. */
	for (i = 0; i < adapter->ulMCListSize; i++) {
		mac = &adapter->MCList[i][0];
		for (j = 0; j < ETH_LENGTH_OF_ADDRESS; j++) {
			if (mac[j] != dest[j]) {
				break;
			}
		}
		if (j == ETH_LENGTH_OF_ADDRESS) {
			return VNIF_RECEIVE_COMPLETE;
		}
	}
	adapter->in_discards++;
	return VNIF_RECEIVE_DISCARD;
}

#define XENNET_ADD_RCB_TO_RING(_adapter, _rcb)								\
{																			\
    RING_IDX req_prod;														\
    netif_rx_request_t *req;												\
																			\
	req_prod = _adapter->rx.req_prod_pvt;									\
    req = RING_GET_REQUEST(&_adapter->rx, req_prod);						\
																			\
    req->gref = _rcb->grant_rx_ref;											\
    req->id = (UINT16) _rcb->index;											\
    KeMemoryBarrier();														\
																			\
	DPRINT(("Put rcb %x back on the ring %x.\n", _rcb->index, req_prod));	\
    _adapter->rx.req_prod_pvt = req_prod + 1;								\
    RING_PUSH_REQUESTS(&_adapter->rx);										\
}

/* Assumes Adapter->RecvLock is held. */
void
xennet_add_rcb_to_ring(PVNIF_ADAPTER adapter)
{
	RCB *rcb;

	DPRINT(("xennet_add_rcb_to_ring.\n"));

	if ((adapter->rx.req_prod_pvt - adapter->rx.rsp_cons)
			== NET_RX_RING_SIZE) {
		DPR_INIT(("xennet_add_rcb_to_ring: ring is already full.\n"));
		return;
	}
	rcb = (RCB *) RemoveHeadList(&adapter->RecvFreeList);
	if (rcb == (RCB *)&adapter->RecvFreeList) {
		DPR_INIT(("out: nBusyRecv %d, receives to process %d\n", 
			adapter->nBusyRecv,
			adapter->rx.sring->rsp_prod - adapter->rx.rsp_cons));
		DPR_INIT(("out: req_prod_pvt 0x%x, rsp_cons 0x%x\n", 
			adapter->rx.req_prod_pvt, adapter->rx.rsp_cons));
		DPR_INIT(("out: sring: req_prod 0x%x, rsp_prod 0x%x.\n",
				   adapter->rx.sring->req_prod, adapter->rx.sring->rsp_prod));
		DPR_INIT(("out: sring: req_event 0x%x, rsp_event 0x%x.\n",
			adapter->rx.sring->req_event, adapter->rx.sring->rsp_event));
		return;
	}

	XENNET_ADD_RCB_TO_RING(adapter, rcb);

	DPRINT(("xennet_add_rcb_to_ring: end.\n"));
}

/* Assumes Adapter->RecvLock is held. */
void
xennet_return_rcb(PVNIF_ADAPTER adapter, RCB *rcb)
{
	DPRINT(("xennet_return_rcb: %x.\n", rcb->index));
	if (adapter->num_rcb <= NET_RX_RING_SIZE) {
		XENNET_ADD_RCB_TO_RING(adapter, rcb);
	}
	else {
		DPRINT(("xennet_return_rcb: xennet_add_rcb_to_ring.\n"));
		InsertTailList(&adapter->RecvFreeList, &rcb->list);
		xennet_add_rcb_to_ring(adapter);
	}
}

void
xennet_drop_rcb(PVNIF_ADAPTER adapter, RCB *rcb, int status)
{
	if (status == NETIF_RSP_NULL) {
		DBG_PRINT(("VNIF: receive null status.\n"));
	}
	else if (status == NETIF_RSP_ERROR) {
		adapter->ifInErrors++;
	}
	else if (status == NETIF_RSP_DROPPED) {
		adapter->in_no_buffers++;
	}

	DBG_PRINT(("VNIF: receive errors = %d, dropped = %d, status = %x, idx %x.\n",
		 adapter->ifInErrors, adapter->in_no_buffers, status, rcb->index));
	xennet_return_rcb(adapter, rcb);
}

VOID
VNIFFreeQueuedRecvPackets(PVNIF_ADAPTER Adapter)
{
}

VOID
VNIFReceiveTimerDpc(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) FunctionContext;

	DPR_INIT(("VNIFReceiveTimerDpc \n"));
	if (!IsListEmpty(&adapter->RecvToProcess)) {
		DPR_INIT(("VNIFReceiveTimerDpc calling VNIFReceivePackets\n"));
		VNIFReceivePackets(adapter);
	}
}

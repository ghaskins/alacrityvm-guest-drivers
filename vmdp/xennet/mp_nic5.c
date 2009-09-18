/*****************************************************************************
 *
 *   File Name:      mp_nic5.c
 *
 *   %version:       10 %
 *   %derived_by:    kallan %
 *   %date_modified: Wed Jul 08 09:04:52 2009 %
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

#include <ndis.h>
#include "miniport.h"

#if 0
static uint32_t CHKSUM_WRAP = 0;

DEBUG_DUMP_CHKSUM(
	char type, uint16_t f, uint8_t *pkt, uint32_t len)
{
	if (len > 0x33) {
		if (pkt[0xc] == 8 && pkt[0xd] == 0 && pkt[0x17] == 6) {
			KdPrint(("%x%ci%02x%02xt%02x%02x ", f, type,
				pkt[0x18], pkt[0x19], pkt[0x32], pkt[0x33]));
			CHKSUM_WRAP++;
			if (!(CHKSUM_WRAP % 6)) {
				KdPrint(("\n"));
			}
			if (type == 'X') {
				//pkt[0x32] = 0;
				//pkt[0x33] = 0;
				pkt[0x33]++;
			}
		}
	}
}
#endif

#ifdef XENNET_COPY_TX
static INT
VNIFCopyPacket(TCB *tcb, PNDIS_PACKET Packet)
{
    PNDIS_BUFFER   CurrentBuffer;
    PVOID          VirtualAddress;
    UINT           CurrentLength;
    UINT           BytesToCopy;
    UINT           BytesCopied = 0;
    UINT           BufferCount;
    UINT           PacketLength;
    UINT           DestBufferSize = VNIF_BUFFER_SIZE;
    PUCHAR         pDest;

    tcb->orig_send_packet = Packet;

    pDest = tcb->data;


    NdisQueryPacket(
      Packet,
		NULL,
      &BufferCount,
      &CurrentBuffer,
      &PacketLength);

    ASSERT(PacketLength <= VNIF_BUFFER_SIZE);

    while(CurrentBuffer && DestBufferSize) {
        NdisQueryBufferSafe(
          CurrentBuffer,
          &VirtualAddress,
          &CurrentLength,
          NormalPagePriority);

        if(VirtualAddress == NULL) {
            return -1;
        }

        CurrentLength = min(CurrentLength, DestBufferSize);

        if(CurrentLength) {
            NdisMoveMemory(pDest, VirtualAddress, CurrentLength);
            BytesCopied += CurrentLength;
            DestBufferSize -= CurrentLength;
            pDest += CurrentLength;
        }

        NdisGetNextBuffer(
          CurrentBuffer,
          &CurrentBuffer);
    }

    if(BytesCopied < ETH_MIN_PACKET_SIZE) {
        //
        // This would be the case if the packet size is less than
        // ETH_MIN_PACKET_SIZE
        //
        NdisZeroMemory(pDest, ETH_MIN_PACKET_SIZE - BytesCopied);
        BytesCopied = ETH_MIN_PACKET_SIZE;
    }

    return BytesCopied;
}

void
MPSendPackets(
  PVNIF_ADAPTER adapter,
  PPNDIS_PACKET PacketArray,
  UINT NumberOfPackets)

/* We failed the whole list, make sure we don't advance pvt. */
{
	LIST_ENTRY discard_list;
	LIST_ENTRY *list_packet;
	NDIS_PACKET *packet;
#ifdef VNIF_TX_CHECKSUM_ENABLED
	PNDIS_TCP_IP_CHECKSUM_PACKET_INFO packetinfo;
#endif
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    TCB *tcb;
    int len;
    RING_IDX i;
    struct netif_tx_request *tx;
    int notify;
	uint16_t flags;

	DPRINT(("VNIF: VNIFSendPackets IN.\n"));
	if (adapter == NULL) {
		DBG_PRINT(("VNIFSendPackets: received null adapter\n"));
		return;
	}
#ifdef DBG
	if (KeGetCurrentIrql() > 2) {
		DPR_INIT(("VNIFSendPackets irql = %d\n", KeGetCurrentIrql()));
	}
	if (adapter->dbg_print_cnt < 5) {
		DPR_INIT(("VNIFSendPackets: starting send, dbg prnt %p, %s %d\n",
			adapter, adapter->Nodename, adapter->dbg_print_cnt));
		adapter->dbg_print_cnt++;
	}
#endif

    /* send the packets */
    NdisAcquireSpinLock(&adapter->SendLock);

	if (VNIF_IS_READY(adapter)) {
		NdisInitializeListHead(&discard_list);
		for (i = 0; i < NumberOfPackets; i++) {
			InsertTailList(&adapter->SendWaitList,
				(PLIST_ENTRY)PacketArray[i]->MiniportReserved);
		}
		if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_SEND_IN_PROGRESS)) {
			NdisReleaseSpinLock(&adapter->SendLock);
			return;
		}
		if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_SEND_SLOWDOWN)) {
			DPR_INIT(("Too many receives outstanding %x, queuing sends.\n",
				adapter->nBusyRecv));
			NdisReleaseSpinLock(&adapter->SendLock);
			return;
		}

		VNIF_SET_FLAG(adapter, VNF_ADAPTER_SEND_IN_PROGRESS);

		i = adapter->tx.req_prod_pvt;

		while (!IsListEmpty(&adapter->SendFreeList)) {
			list_packet = RemoveHeadList(&adapter->SendWaitList);
			if (list_packet != &adapter->SendWaitList)
			{
				packet = (NDIS_PACKET *)CONTAINING_RECORD(list_packet,
					NDIS_PACKET, MiniportReserved);
			}
			else {
				/* No more packets on the WaitList or in PacketArray. */
				break;
			}

			flags = NETTXF_data_validated;
#ifdef VNIF_TX_CHECKSUM_ENABLED
			if (adapter->cur_tx_tasks) {
				packetinfo = (PNDIS_TCP_IP_CHECKSUM_PACKET_INFO)(
					&(NDIS_PER_PACKET_INFO_FROM_PACKET(
						packet, TcpIpChecksumPacketInfo)));
				if ((*(uintptr_t *)&packetinfo->Value) & 0x1c) {
					if (packetinfo->Transmit.NdisPacketChecksumV4) {
						if (packetinfo->Transmit.NdisPacketTcpChecksum) {
							if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_TCP) {
								DPR_CHKSUM(("tcp: flags to valid and blank\n"));
								flags |=
									NETTXF_data_validated | NETTXF_csum_blank;
							}
							else {
								DPR_INIT(("discard tcp: c %x i %p\n",
									adapter->cur_tx_tasks,packetinfo->Value));
								InsertTailList(&discard_list, 
									(PLIST_ENTRY)packet->MiniportReserved);
								continue;
							}
						}
						if (packetinfo->Transmit.NdisPacketUdpChecksum) {
							if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP) {
								DPR_CHKSUM(("udp: flags to valid and blank\n"));
								flags |=
									NETTXF_data_validated | NETTXF_csum_blank;
							}
							else {
								DPR_INIT(("discard udp: c %x i %p\n",
									adapter->cur_tx_tasks,packetinfo->Value));
								InsertTailList(&discard_list, 
									(PLIST_ENTRY)packet->MiniportReserved);
								continue;
							}
						}
						if (packetinfo->Transmit.NdisPacketIpChecksum) {
							if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_IP) {
								DPR_CHKSUM(("ip: flags to valid and blank\n"));
								flags |=
									NETTXF_data_validated | NETTXF_csum_blank;
							}
							else {
								DPR_INIT(("discard ip: c %x i %p\n",
									adapter->cur_tx_tasks,packetinfo->Value));
								InsertTailList(&discard_list, 
									(PLIST_ENTRY)packet->MiniportReserved);
								continue;
							}
						}
					}
					else {
						/* IPV6 packet */
						DPR_INIT(("discard ipv6: c %x i %p\n",
							adapter->cur_tx_tasks,packetinfo->Value));
						/* Packet wants checksum, but it's not supported. */
						InsertTailList(&discard_list, 
							(PLIST_ENTRY)packet->MiniportReserved);
						continue;
					}
				}
#ifdef DBG
				if (packetinfo->Value & 0xfffe) {
					DPR_CHKSUM(("TX check value %p.\n", packetinfo->Value));
				}
#endif
			}
#endif
			tcb = (TCB *) RemoveHeadList(&adapter->SendFreeList);
			if ((len = VNIFCopyPacket(tcb, packet)) < 0) {
				DBG_PRINT(("VNIF: copy pakcet fail.\n"));
				InsertHeadList(&adapter->SendFreeList, &tcb->list);
				NdisMSendComplete( adapter->AdapterHandle,
					packet,
					NDIS_STATUS_RESOURCES);
				DBG_PRINT(("Copy packet failed.\n"));
				continue;
			}
		
			adapter->nBusySend++;

			tx = RING_GET_REQUEST(&adapter->tx, i);
			tx->id = (uint16_t) tcb->index;
			tx->gref = tcb->grant_tx_ref;
			xennet_save_req(&adapter->txlist, tcb, i);
			tx->offset = (uint16_t) BYTE_OFFSET(tcb->data);
			tx->size = (uint16_t) len;

			/* TCP/IP transport has already computed checksum for us */
			tx->flags = flags;
			i++;
			//DEBUG_DUMP_CHKSUM('X', tx->flags, tcb->data, len);
		}

		adapter->tx.req_prod_pvt = i;

		RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&adapter->tx, notify);
		if (notify) {
			notify_remote_via_evtchn(adapter->evtchn);
		}

		VNIF_CLEAR_FLAG(adapter, VNF_ADAPTER_SEND_IN_PROGRESS);
		NdisReleaseSpinLock(&adapter->SendLock);

		while (!IsListEmpty(&discard_list)) {
			list_packet = RemoveHeadList(&discard_list);
			packet = (NDIS_PACKET *)CONTAINING_RECORD(list_packet,
				NDIS_PACKET, MiniportReserved);
			DPR_INIT(("VNIFSendPackets discarding packet %p.\n", packet));
			NdisMSendComplete(
				adapter->AdapterHandle,
				packet,
				NDIS_STATUS_FAILURE);
		}
	}
	else {
		DPR_INIT(("VNIFSendPackets not ready %x.\n", adapter->Flags));
		NdisReleaseSpinLock(&adapter->SendLock);
		if (VNIF_TEST_FLAG(adapter, VNF_RESET_IN_PROGRESS))
			status = NDIS_STATUS_RESET_IN_PROGRESS;
		else if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_NO_LINK))
			status = NDIS_STATUS_NO_CABLE;
		else
			status = NDIS_STATUS_FAILURE;
		for (i = 0; i < NumberOfPackets; i++)
		{
			DPR_INIT(("VNIFSendPackets completing packet %p.\n",
				PacketArray[i]));
			NdisMSendComplete(
				adapter->AdapterHandle,
				PacketArray[i],
				status);
		}
	}

    //KRA VNIFCheckSendCompletion(adapter);


	DPRINT(("VNIF: VNIFSendPackets OUT.\n"));
}
#else // TX fragments.

void
MPSendPackets(
  PVNIF_ADAPTER adapter,
  PPNDIS_PACKET PacketArray,
  UINT NumberOfPackets)
{
	LIST_ENTRY *wait_packet;
	NDIS_PACKET *packet;
	SCATTER_GATHER_LIST *pFragList;
	SCATTER_GATHER_ELEMENT *el, *el_end;
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	unsigned long mfn;
    uint32_t len;
	uint32_t packet_len;
    RING_IDX i;
    struct netif_tx_request *tx;
    int notify;
    UINT cnt;
	xen_ulong_t id;
	int pkt_origin;
	uint16_t offset;

	DPRINT(("VNIF: VNIFSendPackets IN.\n"));
#ifdef DBG
	if (KeGetCurrentIrql() > 2) {
		KdPrint(("VNIFSendPackets irql = %d\n", KeGetCurrentIrql()));
	}
#endif
	if (adapter == NULL) {
		DBG_PRINT(("VNIFSendPackets: received null adapter\n"));
		return;
	}

    /* send the packets */
    NdisAcquireSpinLock(&adapter->SendLock);

	if (VNIF_IS_READY(adapter)) {
		i = adapter->tx.req_prod_pvt;
		cnt = 0;
		while (!RING_FULL(&adapter->tx)) {
			packet = NULL;
			wait_packet = RemoveHeadList(&adapter->SendWaitList);
			if (wait_packet != &adapter->SendWaitList)
			{
				packet = (NDIS_PACKET *)CONTAINING_RECORD(wait_packet,
					NDIS_PACKET, MiniportReserved);
				pkt_origin = 2;
				DBG_PRINT(("Removed packet form wait list %p: cid %p.\n",
					packet, NdisGetPacketCancelId(packet)));
			}
			else if (cnt < NumberOfPackets) {
				packet = PacketArray[cnt];
				//DBG_PRINT(("Using PacketArray[%x] pkt %p.\n", cnt, packet));
				cnt++;
				pkt_origin = 1;
			}
			else {
				/* No more packets on the WaitList or in PacketArray. */
				break;
			}

			pFragList = NDIS_PER_PACKET_INFO_FROM_PACKET(packet, 
				ScatterGatherListPacketInfo);
			NdisQueryPacketLength(packet, &packet_len);
#ifdef DBG
			if (pFragList == NULL) {
				DBG_PRINT(("Frag list == NULL %p, len = %x.\n",
					packet, packet_len));
				continue;
			}
#endif

			/* Assume each fragment spans a page. */
			if (RING_FREE_REQUESTS(&adapter->tx) <
				(pFragList->NumberOfElements << 1)){
				DBG_PRINT(("Not enough tx free requests.\n"));
				/* New packets go on the tail, old go back on the head. */
				if (pkt_origin == 1) {
					InsertTailList(
						&adapter->SendWaitList,
						(PLIST_ENTRY)packet->MiniportReserved);
				}
				else {
					InsertHeadList(
						&adapter->SendWaitList,
						(PLIST_ENTRY)packet->MiniportReserved);
				}
				break;
			}

			packet->MiniportReserved[0] = 0;
			el = pFragList->Elements;
			el_end = el + pFragList->NumberOfElements;

			/* Setup for first fragment. */
			len = el->Length;
			id = adapter->tx_id_alloc_head;
			adapter->tx_id_alloc_head = (xen_ulong_t)(adapter->tx_packets[id]);

			adapter->tx_packets[id] = NULL; //skb;

			tx = RING_GET_REQUEST(&adapter->tx, i);
			i++;
			tx->id = (uint16_t)id;

			mfn = phys_to_mfn(el->Address.QuadPart);
			gnttab_grant_foreign_access_ref(adapter->grant_tx_ref[id],
				adapter->BackendID, mfn, GNTMAP_readonly);

			tx->gref = adapter->grant_tx_ref[id];
			offset = (uint16_t)(
				(unsigned long)el->Address.QuadPart & ~PAGE_MASK);
			tx->offset = offset;

			/* First fragment gets total size and validated. */
			tx->size = (uint16_t)(packet_len >= ETH_MIN_PACKET_SIZE ?
				packet_len : ETH_MIN_PACKET_SIZE);
			tx->flags = NETTXF_data_validated;

			/* Check if the first fragment spans a page. */
			while (len > (uint32_t)(PAGE_SIZE - offset)) {
				DPRINT(("First frag crosses page, len %x, offset %x.\n",
					len,offset));
				/* Fix up the previous element. */
				tx->flags |= NETTXF_more_data;
				len -= PAGE_SIZE - offset;
				offset = 0;

				/* Start the next element. */
				id = adapter->tx_id_alloc_head;
				adapter->tx_id_alloc_head = (xen_ulong_t)
					(adapter->tx_packets[id]);

				adapter->tx_packets[id] = NULL; //skb;
				tx = RING_GET_REQUEST(&adapter->tx, i);
				i++;
				tx->id = (uint16_t)id;

				mfn++;
				gnttab_grant_foreign_access_ref(adapter->grant_tx_ref[id],
					adapter->BackendID, mfn, GNTMAP_readonly);

				tx->gref = adapter->grant_tx_ref[id];
				tx->offset = 0;
				tx->flags = 0;
				if (len <= PAGE_SIZE) {
					tx->size = (uint16_t)len;
					break;
				}
				else {
					tx->size = PAGE_SIZE;
				}
			}

			/* Start the next fragment if needed. */
			el++;
			while (el < el_end)
			{
				tx->flags |= NETTXF_more_data;

				len = el->Length;
				id = adapter->tx_id_alloc_head;
				adapter->tx_id_alloc_head = (xen_ulong_t)
					(adapter->tx_packets[id]);

				adapter->tx_packets[id] = NULL; //skb;

				tx = RING_GET_REQUEST(&adapter->tx, i);
				i++;
				tx->id = (uint16_t)id;

				mfn = phys_to_mfn(el->Address.QuadPart);
				gnttab_grant_foreign_access_ref(adapter->grant_tx_ref[id],
					adapter->BackendID, mfn, GNTMAP_readonly);

				tx->gref = adapter->grant_tx_ref[id];
				offset = (uint16_t)(
					(unsigned long)el->Address.QuadPart & ~PAGE_MASK);
				tx->offset = offset;
				tx->size = (uint16_t)len;
				tx->flags = 0;
				while (len > (uint32_t)(PAGE_SIZE - offset)) {
					DPRINT(("Another frag crosses page, len %x, offset %x.\n",
						len,offset));
					tx->size = PAGE_SIZE - offset;
					tx->flags |= NETTXF_more_data;
					len -= tx->size;
					offset = 0;

					id = adapter->tx_id_alloc_head;
					adapter->tx_id_alloc_head = (xen_ulong_t)
						(adapter->tx_packets[id]);

					adapter->tx_packets[id] = NULL; //skb;
					tx = RING_GET_REQUEST(&adapter->tx, i);
					i++;
					tx->id = (uint16_t)id;

					mfn++;
					gnttab_grant_foreign_access_ref(adapter->grant_tx_ref[id],
						adapter->BackendID, mfn, GNTMAP_readonly);

					tx->gref = adapter->grant_tx_ref[id];
					tx->offset = 0;
					tx->size = (uint16_t)len;
					tx->flags = 0;
				}
				el++;
			}
			if (packet_len < ETH_MIN_PACKET_SIZE) {
				tx->flags |= NETTXF_more_data;

				id = adapter->tx_id_alloc_head;
				adapter->tx_id_alloc_head = (xen_ulong_t)
					(adapter->tx_packets[id]);

				tx = RING_GET_REQUEST(&adapter->tx, i);
				i++;
				tx->id = (uint16_t)id;

				tx->gref = adapter->zero_ref;
				tx->offset = (uint16_t)adapter->zero_offset;
				tx->size = (uint16_t)(ETH_MIN_PACKET_SIZE - packet_len);
				tx->flags = 0;
				packet->MiniportReserved[0] = XENNET_ZERO_FRAGMENT;
			}

			adapter->tx_packets[id] = packet; /* Only save skb on last frag. */
			adapter->nBusySend++;
			DPRINT(("txx packet = %p, id = %x, len %x len %x, frags %x.\n",
				packet, id, packet_len, len, pFragList->NumberOfElements));
		}

		adapter->tx.req_prod_pvt = i;

		RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&adapter->tx, notify);
		if (notify) {
			notify_remote_via_evtchn(adapter->evtchn);
		}

		for (; cnt < NumberOfPackets; cnt++) {
			/* Save any packets not processed. */
			DBG_PRINT(("Queueing up initial packets on wait list %p: cid %p.\n",
				PacketArray[cnt], NdisGetPacketCancelId(PacketArray[cnt])));
			InsertTailList(
			  &adapter->SendWaitList,
			  (PLIST_ENTRY)PacketArray[cnt]->MiniportReserved);
		}

		NdisReleaseSpinLock(&adapter->SendLock);
	}
	else {
		DPR_INIT(("VNIFSendPackets not ready %x.\n", adapter->Flags));
		NdisReleaseSpinLock(&adapter->SendLock);
		if (VNIF_TEST_FLAG(adapter, VNF_RESET_IN_PROGRESS))
			status = NDIS_STATUS_RESET_IN_PROGRESS;
		else if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_NO_LINK))
			status = NDIS_STATUS_NO_CABLE;
		else
			status = NDIS_STATUS_FAILURE;
		for (cnt = 0; cnt < NumberOfPackets; cnt++)
		{
			DPR_INIT(("VNIFSendPackets completing packet %p.\n",
				PacketArray[cnt]));
			NdisMSendComplete(
				adapter->AdapterHandle,
				PacketArray[cnt],
				status);
		}
	}

    //KRA VNIFCheckSendCompletion(adapter);


	DPRINT(("VNIF: VNIFSendPackets OUT.\n"));
}
#endif

int
VNIFCheckSendCompletion(PVNIF_ADAPTER adapter)
{
    /* this function is to be called within the dpc
     * to free those completed send packets.
     */
	struct netif_tx_response *txrsp;
    PNDIS_PACKET TxPacketArray[VNIF_MAX_BUSY_SENDS];
#ifdef XENNET_COPY_TX
    TCB *pkt;
#else
	NDIS_PACKET *pkt;
#endif
    RING_IDX cons, prod;
    uint32_t id;
	uint32_t cnt;
	int did_work = 0;

    DPRINT(("VNIF: VNIFCheckSendCompletion IN.\n"));
#ifdef DBG
	if (KeGetCurrentIrql() > 2) {
		KdPrint(("VNIFCheckSendCompletion irql = %d\n", KeGetCurrentIrql()));
	}
#endif

    DPRINT(("VNIF: VNIF Check Send Completion.\n"));
    NdisDprAcquireSpinLock(&adapter->SendLock);
	cnt = 0;
    do {
        prod = adapter->tx.sring->rsp_prod;
        KeMemoryBarrier();

        for (cons = adapter->tx.rsp_cons;
			cons != prod && cnt < VNIF_MAX_BUSY_SENDS;
			cons++) {

            txrsp = RING_GET_RESPONSE(&adapter->tx, cons);
            if (txrsp->status == NETIF_RSP_NULL) {
				DPR_INIT(("VNIFCheckSendCompletion: tx NETIF_RSP_NULL %x\n",
					txrsp->id));
                continue;
            }

			id = txrsp->id;
#ifdef XENNET_COPY_TX
			pkt = adapter->TCBArray[id];
			TxPacketArray[cnt] = pkt->orig_send_packet;
			xennet_clear_req(&adapter->txlist, pkt);
			pkt->orig_send_packet = NULL;
			InsertHeadList(&adapter->SendFreeList, &pkt->list);
#else
			pkt = adapter->tx_packets[id];
			DPRINT(("txr packet = %p, id = %x.\n", pkt, id));

			adapter->tx_packets[id] = (NDIS_PACKET *)adapter->tx_id_alloc_head;
			adapter->tx_id_alloc_head = id;

			if (pkt == NULL) {
				if (gnttab_end_foreign_access_ref(
					adapter->grant_tx_ref[id], GNTMAP_readonly) == 0) {
					DBG_PRINT(("VNIFCheckSendCompletion: grant in use!\n"));
				}
				continue;
			}

			if (pkt->MiniportReserved[0] != XENNET_ZERO_FRAGMENT) {
				if (gnttab_end_foreign_access_ref(
						adapter->grant_tx_ref[id], GNTMAP_readonly) == 0) {
					DBG_PRINT(("Last fragment: grant still in use!\n"));
				}
			}

			DPRINT(("txr packet = %p\n", pkt));
			TxPacketArray[cnt] = pkt;
#endif
			cnt++;
			if (txrsp->status == NETIF_RSP_OKAY) {
				adapter->GoodTransmits++;
			}
			else{
				if (txrsp->status == NETIF_RSP_ERROR) {
					adapter->ifOutErrors++;
				}
				if (txrsp->status == NETIF_RSP_DROPPED) {
					adapter->ifOutDiscards++;
				}
				DBG_PRINT(("VNIF: send errs %d, drped %d, pkt %p, id %x.\n",
					adapter->ifOutErrors, adapter->ifOutDiscards, pkt, id));
			}
			adapter->nBusySend--;

			/* we don't release grant_ref until the device unload. */

#ifdef DBG
			if (!VNIF_IS_READY(adapter)) {
				DPR_INIT(("VNIFCheckSendCompletion: not ready %p\n", pkt));
			}
#endif
        }
        adapter->tx.rsp_cons = prod;

        /*
         * Set a new event, then check for race with update of tx_cons.
         * Note that it is essential to schedule a callback, no matter
         * how few buffers are pending. Even if there is space in the
         * transmit ring, higher layers may be blocked because too much
         * data is outstanding: in such cases notification from Xen is
         * likely to be the only kick that we'll get.
         */
        adapter->tx.sring->rsp_event =
            prod + ((adapter->tx.sring->req_prod - prod) >> 1) + 1;
        KeMemoryBarrier();
		did_work++;
    } while ((cons == prod) && (prod != adapter->tx.sring->rsp_prod));

	NdisDprReleaseSpinLock(&adapter->SendLock);

	/* Now that we've released the spinlock, complete all the sends. */
	for (cons = 0; cons < cnt; cons++) {
		NdisMSendComplete(
			adapter->AdapterHandle,
			TxPacketArray[cons],
			NDIS_STATUS_SUCCESS);
	}

    if (VNIF_IS_READY(adapter)) {
		/* See if we can send any packets that we may have queued. */
		if (!IsListEmpty(&adapter->SendWaitList)) {
			MPSendPackets(adapter, NULL, 0);
		}
	}
#ifdef DBG
	else {
		DPR_INIT(("VNIFCheckSendCompletion: adapter not ready, reset, halt, etc. %p\n",
				  adapter));
	}
#endif
    DPRINT(("VNIF: VNIFCheckSendCompletion OUT.\n"));
	return did_work;

    /* TODO: should try to wake up WaitList */
}

/*
 * VNIFReceivePackets does the following:
 * 1. update rx_ring consumer pointer.
 * 2. remove the corresponding RCB out of free list
 * 3. indicate all these packets.
 *
 * Later when returning the packets:
 * 1. reinit the packets, reinsert into free list.
 * 2. put the grant_ref back into rx_ring.req
 */
#ifdef DBG
static LARGE_INTEGER longest = {0};
static LARGE_INTEGER start_long = {0};
static LARGE_INTEGER end_long = {0};
static uint32_t last_idx = 0;
#endif

VOID
VNIFReceivePackets(IN PVNIF_ADAPTER adapter)
{
    PNDIS_PACKET RecvPacketArray[NET_RX_RING_SIZE];
	PNDIS_TCP_IP_CHECKSUM_PACKET_INFO info;
    netif_rx_request_t *req;
    struct netif_rx_response *rx;
    grant_ref_t i, ref;
    ULONG cnt;
    RING_IDX rp;
	RING_IDX old;
    RCB *rcb;
    UINT len, offset, blen;
    PNDIS_PACKET packet;
	int more_to_do;
	uint32_t j;

	DPRINT(("VNIFReceivePackets: start.\n"));
    VNIF_INC_REF(adapter);

#ifdef DBG
	if (KeGetCurrentIrql() > 2) {
		DBG_PRINT(("VNIFReceivePackets irql = %d\n", KeGetCurrentIrql()));
	}

	if ((adapter->rx.sring->rsp_prod - adapter->rx.rsp_cons)
				>= (NET_RX_RING_SIZE - 5)
			|| (adapter->rx.sring->rsp_prod == adapter->rx.sring->req_prod)
			|| ((adapter->rx.sring->rsp_prod - adapter->rx.rsp_cons)
				+ adapter->nBusyRecv) >= (NET_RX_RING_SIZE - 5)) {
		DBG_PRINT(("VNIFReceivePackets: nBusyRecv %d, receives to process %d\n", 
			adapter->nBusyRecv,
			adapter->rx.sring->rsp_prod - adapter->rx.rsp_cons));
		DBG_PRINT(("VNIFReceivePackets: req_prod_pvt 0x%x, rsp_cons 0x%x\n", 
			adapter->rx.req_prod_pvt, adapter->rx.rsp_cons));
		DBG_PRINT(("VNIFReceivePackets: sring: req_prod 0x%x, rsp_prod 0x%x.\n",
				   adapter->rx.sring->req_prod, adapter->rx.sring->rsp_prod));
		DBG_PRINT(("VNIFReceivePackets: sring: req_event 0x%x, rsp_event 0x%x.\n",
			adapter->rx.sring->req_event, adapter->rx.sring->rsp_event));
	}
#endif

    NdisDprAcquireSpinLock(&adapter->RecvLock);

	cnt = 0;
	old = adapter->rx.sring->req_prod;
    do {
		rp = adapter->rx.sring->rsp_prod;
		KeMemoryBarrier();

		for (j = 0, i = adapter->rx.rsp_cons; i != rp; i++, j++) {
			rx = RING_GET_RESPONSE(&adapter->rx, i);
			rcb = adapter->RCBArray[rx->id];
			rcb->len = rx->status;
			rcb->flags = rx->flags;
			InsertTailList(&adapter->RecvToProcess, &rcb->list);
		}

		adapter->rx.rsp_cons = rp;

		while (cnt < adapter->rcv_limit
				&& !IsListEmpty(&adapter->RecvToProcess)) {
			rcb = (RCB *) RemoveHeadList(&adapter->RecvToProcess);
			len = rcb->len;
			if (len > NETIF_RSP_NULL) {
				if ((ref = rcb->grant_rx_ref) != GRANT_INVALID_REF) {
					if (xennet_should_complete_packet(adapter, rcb->page, len)){
						if (len < 60) {
							NdisZeroMemory(rcb->page + len, 60 - len);
						}

						NdisAdjustBufferLength(rcb->buffer, len);

						packet = rcb->packet;

						/*
						 * If backend set data_validated or csum_blank flag,
						 * we set the packet valid, override TCP/IP transport
						 * validation against checksum. This is not only more
						 * efficient, but necessary, in case backend sends its
						 * local packet with wrong csum.
						 */
						info = (PNDIS_TCP_IP_CHECKSUM_PACKET_INFO) (
								&(NDIS_PER_PACKET_INFO_FROM_PACKET(
								packet,
								TcpIpChecksumPacketInfo)));
						if (rcb->page[VNIF_PACKET_OFFSET_IP]
								== VNIF_PACKET_TYPE_IP
							&& adapter->cur_rx_tasks
							&& (!(rcb->page[VNIF_IP_FLAGS_BYTE] & 1)
								&& !rcb->page[VNIF_IP_FRAG_OFFSET_BYTE])
							&& (rcb->flags &
								(NETRXF_data_validated | NETRXF_csum_blank))) {

							info->Receive.NdisPacketIpChecksumSucceeded =
								(adapter->cur_rx_tasks &
									VNIF_CHKSUM_IPV4_IP) ? 1 : 0;

							if (rcb->page[VNIF_PACKET_OFFSET_TCP_UDP]
									== VNIF_PACKET_TYPE_TCP) {
								info->Receive.NdisPacketTcpChecksumSucceeded =
									(adapter->cur_rx_tasks &
										VNIF_CHKSUM_IPV4_TCP) ? 1 : 0;
								info->Receive.NdisPacketUdpChecksumSucceeded =0;
							}
							else if (rcb->page[VNIF_PACKET_OFFSET_TCP_UDP]
									== VNIF_PACKET_TYPE_UDP) {
								info->Receive.NdisPacketTcpChecksumSucceeded =0;
								info->Receive.NdisPacketUdpChecksumSucceeded =
									(adapter->cur_rx_tasks &
										VNIF_CHKSUM_IPV4_UDP) ? 1 : 0;
							}
							DPR_CHKSUM(("Checksum: rx success: %x f %x %x.\n",
								rcb->page[0x14], rcb->flags,
								adapter->cur_rx_tasks));
						}
						else {
							info->Value = 0;
#ifdef DBG
							if (rcb->flags & NETRXF_csum_blank
								&& !adapter->cur_rx_tasks) {
								/* We need to calculate our own checksum.*/
								DBG_PRINT(("Blank no tasks? flag %x, rx %x.\n",
									rcb->flags, adapter->cur_rx_tasks));
							}
#endif
						}

						adapter->GoodReceives++;
						NDIS_SET_PACKET_STATUS(packet, NDIS_STATUS_SUCCESS);
						RecvPacketArray[cnt] = packet;
						cnt++;
						VNIFInterlockedIncrement(adapter->nBusyRecv);
						DPRINT(("Receiveing rcb %x.\n", rcb->index));
						if (adapter->num_rcb > NET_RX_RING_SIZE) {
							DPRINT(("VRcvPkts: xennet_add_rcb_to_ring.\n"));
							xennet_add_rcb_to_ring(adapter);
						}
					}
					else {
						xennet_return_rcb(adapter, rcb);
					}
				}
				else {
					/*
					 * This definitely indicates a bug, either in this driver
					 * or in the backend driver. In future this should flag the
					 * bad situation to the system controller to reboot the
					 * backed.
					 */
					DBG_PRINT(("VNIF: GRANT_INVALID_REF for rcb %d.\n",
						rcb->index));
					xennet_return_rcb(adapter, rcb);
					continue;
				}
			}
			else {
				xennet_drop_rcb(adapter, rcb, rcb->len);
				continue;
			}
		}
		DPRINT(("VNIF: VNIF Received Packets = %d.\n", cnt));
		RING_FINAL_CHECK_FOR_RESPONSES(&adapter->rx, more_to_do);
	} while (more_to_do && cnt < adapter->rcv_limit);

	if ((uint32_t)adapter->nBusyRecv > adapter->tx_throttle_start) {
		VNIF_SET_FLAG(adapter, VNF_ADAPTER_SEND_SLOWDOWN);
		DPR_INIT(("VNIFReceivePackets: nBusyRecv %d, slowing down sends\n", 
			adapter->nBusyRecv));
	}

	if (!IsListEmpty(&adapter->RecvToProcess)) {
		LARGE_INTEGER li;

		li.QuadPart = 500;
		VNIF_SET_TIMER(adapter->rcv_timer, li);
	}

	/* Holding the lock over the call hangs the machine.*/
    NdisDprReleaseSpinLock(&adapter->RecvLock);
    if (cnt != 0) {
#ifdef DBG
		if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_HALT_IN_PROGRESS)) {
			DPR_INIT(("VNIFReceivePackets: halting, indicating %d packets.\n",
				cnt));
		}
		DPRINT(("VNIFReceivePackets: NdisMIndicateReceivePacket.\n"));

		if (start_long.QuadPart == 0)
			KeQueryTickCount(&start_long);
#endif

        NdisMIndicateReceivePacket(
          adapter->AdapterHandle,
          RecvPacketArray,
          cnt);
	}

	if (adapter->rx.sring->req_event > old) {
        notify_remote_via_evtchn(adapter->evtchn);
	}

    VNIF_DEC_REF(adapter);
	DPRINT(("VNIFReceivePackets: end.\n"));
}

VOID
MPReturnPacket(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN PNDIS_PACKET Packet)
{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER)MiniportAdapterContext;
	RCB *rcb;
	RING_IDX req_prod;
	netif_rx_request_t *req;
#ifdef DBG
	LARGE_INTEGER cur;

	if (KeGetCurrentIrql() > 2) {
		DBG_PRINT(("xennet_return_packet irql = %d\n", KeGetCurrentIrql()));
	}
	if (*((uint32_t *)&Packet->MiniportReserved[sizeof(void *)])
		!= 0x44badd55) {
		DBG_PRINT(("Reserved not working %x.\n",
			*((uint32_t *)&Packet->MiniportReserved[sizeof(void *)])));
	}
	if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_HALT_IN_PROGRESS)) {
		KdPrint(("xennet_return_packet: halting, waiting for %d packets.\n",
			adapter->nBusyRecv));
	}

	KeQueryTickCount(&end_long);
	cur.QuadPart = end_long.QuadPart - start_long.QuadPart;
	if (cur.QuadPart > longest.QuadPart) {
		longest.QuadPart = cur.QuadPart;
	}
	start_long.QuadPart = 0;
	if ((adapter->rx.sring->rsp_prod - adapter->rx.rsp_cons)
				>= (NET_RX_RING_SIZE - 5)
			|| (adapter->rx.sring->rsp_prod == adapter->rx.sring->req_prod)
			|| ((adapter->rx.sring->rsp_prod - adapter->rx.rsp_cons)
				+ adapter->nBusyRecv) >= (NET_RX_RING_SIZE - 5)) {
		DBG_PRINT(("MPReturnPacket: nBusyRecv %d, receives to process %d\n", 
			adapter->nBusyRecv,
			adapter->rx.sring->rsp_prod - adapter->rx.rsp_cons));
		DBG_PRINT(("MPReturnPacket: req_prod_pvt 0x%x, rsp_cons 0x%x\n", 
			adapter->rx.req_prod_pvt, adapter->rx.rsp_cons));
		DBG_PRINT(("MPReturnPacket: sring: req_prod 0x%x, rsp_prod 0x%x.\n",
				   adapter->rx.sring->req_prod, adapter->rx.sring->rsp_prod));
		DBG_PRINT(("MPReturnPacket: sring: req_event 0x%x, rsp_event 0x%x.\n",
			adapter->rx.sring->req_event, adapter->rx.sring->rsp_event));
		DBG_PRINT(("Longest return packet time %x, cur time %x.\n",
				   (uint32_t)longest.QuadPart, (uint32_t) cur.QuadPart));
	}
#endif
	DPRINT(("xennet_return_packet.\n"));

	NdisDprAcquireSpinLock(&adapter->RecvLock);

	rcb = *((PRCB *) Packet->MiniportReserved);
	xennet_return_rcb(adapter, rcb);

	VNIFInterlockedDecrement(adapter->nBusyRecv);

	if ((uint32_t)adapter->nBusyRecv <= adapter->tx_throttle_stop) {
		if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_SEND_SLOWDOWN)) {
			VNIF_CLEAR_FLAG(adapter, VNF_ADAPTER_SEND_SLOWDOWN);
			DPR_INIT(("MPReturnPacket: nBusyRecv %d, clearing send slowdown\n", 
				adapter->nBusyRecv));
			if (VNIF_IS_READY(adapter)) {
				// See if we can send any packets that we may have queued. 
				if (!IsListEmpty(&adapter->SendWaitList)) {
					MPSendPackets(adapter, NULL, 0);
				}
			}
		}
	}

	NdisDprReleaseSpinLock(&adapter->RecvLock);
	DPRINT(("xennet_return_packet: end.\n"));
}

VOID
VNIFFreeQueuedSendPackets(PVNIF_ADAPTER adapter, NDIS_STATUS status)
{
	PLIST_ENTRY		pEntry;
	PNDIS_PACKET	Packet;

#ifdef DBG
	if (KeGetCurrentIrql() > 2) {
		KdPrint(("VNIFFreeQueuedSendPackets irql = %d\n", KeGetCurrentIrql()));
	}
#endif

	while(TRUE) {
		pEntry = (PLIST_ENTRY) NdisInterlockedRemoveHeadList(
			&adapter->SendWaitList,
			&adapter->SendLock);
		if(!pEntry) {
			break;
		}

		Packet = CONTAINING_RECORD(pEntry, NDIS_PACKET, MiniportReserved);
		DPR_INIT(("VNIFFreeQueuedSendPackets %p \n", Packet));
		NdisMSendComplete(
			adapter->AdapterHandle,
			Packet,
			status);

	}
}


void
VNIFIndicateLinkStatus(PVNIF_ADAPTER adapter, uint32_t status)
{
	NdisMIndicateStatus(adapter->AdapterHandle,
		(status ?
			NDIS_STATUS_MEDIA_CONNECT : NDIS_STATUS_MEDIA_DISCONNECT),          
		(PVOID)0, 0);

	NdisMIndicateStatusComplete(adapter->AdapterHandle);
}

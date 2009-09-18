/*****************************************************************************
 *
 *   File Name:      mp_nic6.c
 *
 *   %version:       11 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jul 06 14:23:51 2009 %
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

void
dump_packet(TCB *pkt)
{
	uint8_t *data;
	uint32_t i;

	DBG_PRINT(("dump_packek ref %x, len %x\n",pkt->grant_tx_ref, pkt->len));
	for (data = pkt->data, i = 0; i + 16 < pkt->len; i += 16) {
		DBG_PRINT(("%03x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				   i,
				  data[i + 0],
				  data[i + 1],
				  data[i + 2],
				  data[i + 3],
				  data[i + 4],
				  data[i + 5],
				  data[i + 6],
				  data[i + 7],
				  data[i + 8],
				  data[i + 9],
				  data[i + 10],
				  data[i + 11],
				  data[i + 12],
				  data[i + 13],
				  data[i + 14],
				  data[i + 15]));
	}
	DBG_PRINT(("%03x:", i));
	for (; i < pkt->len; i++) {
		DBG_PRINT((" %02x", data[i]));
	}
	DBG_PRINT(("\n"));
}
#endif

#ifdef XENNET_COPY_TX
static uint32_t 
VNIFCopyNetBuffer(PNET_BUFFER nb, TCB *tcb, uint32_t priority)
{
    ULONG          cur_len;
    PUCHAR         pSrc;
    PUCHAR         pDest;
    ULONG          bytes_copied = 0;
    ULONG          offset;
    PMDL           cur_mdl;
    ULONG          data_len;
	uint32_t loop_cnt;
	uint32_t priority_len;
	uint32_t mac_len;
    
    DPRINT(("--> MpCopyNetBuffer\n"));
    
    pDest = tcb->data;
    cur_mdl = NET_BUFFER_FIRST_MDL(nb);
    offset = NET_BUFFER_DATA_OFFSET(nb);
    data_len = NET_BUFFER_DATA_LENGTH(nb);
    
	priority_len = P8021_BYTE_LEN;
	mac_len = P8021_MAC_LEN;
	loop_cnt = 0;
	cur_len = 0;
    while (cur_mdl && data_len > 0) {
		DPRINT(("NdisQueryMdl: cnt %d, mdl %p, cl %x, dl %x, off %x, dest %p\n",
				  loop_cnt, cur_mdl, cur_len, data_len, offset, pDest));
        NdisQueryMdl(cur_mdl, &pSrc, &cur_len, NormalPagePriority);
		DPRINT(("              src %p, cl %x\n", pSrc, cur_len));
        if (pSrc == NULL) {
            bytes_copied = 0;
            break;
        }
        /*  Current buffer length is greater than the offset to the buffer. */
        if (cur_len > offset) { 
            pSrc += offset;
            cur_len -= offset;

            if (cur_len > data_len) {
                cur_len = data_len;
            }
            data_len -= cur_len;
			DPRINT(("              src %p, dest %p cl %x\n",
					  pSrc, pDest, cur_len));
			/* Make sure this is a priority packet and not a Vlan packet. */
			if (priority && (priority & P8021_HOST_MASK) == 0
				&& priority_len) {
				if (cur_len >= mac_len) {
            		NdisMoveMemory(pDest, pSrc, mac_len);
					bytes_copied += mac_len;
					cur_len -= mac_len;
					pDest += mac_len;
					pSrc += mac_len;
					*(uint16_t *)pDest = P8021_TPID_TYPE;
					pDest += 2;
					*(uint16_t *)pDest =
						(uint16_t)(priority << P8021_BIT_SHIFT);
					pDest += 2;
					mac_len = 0;
					priority_len = 0;
            		bytes_copied += P8021_BYTE_LEN;
				}
				else {
					mac_len -= cur_len;
				}
			}
            NdisMoveMemory(pDest, pSrc, cur_len);
            bytes_copied += cur_len;

            pDest += cur_len;
            offset = 0;
        }
        else {
			DPRINT(("MpCopyNetBuffer: CurrLenght <= offset\n"));
            offset -= cur_len;
        }
		DPRINT(("              NdisGetNextMdl\n"));
        NdisGetNextMdl(cur_mdl, &cur_mdl);
		loop_cnt++;
    
    }
	DPRINT(("              Done copy\n"));
    if ((bytes_copied != 0) && (bytes_copied < ETH_MIN_PACKET_SIZE)) {
        NdisZeroMemory(pDest, ETH_MIN_PACKET_SIZE - bytes_copied);
        bytes_copied = ETH_MIN_PACKET_SIZE;
    }
    
    DPRINT(("<-- MpCopyNetBuffer\n"));
#ifdef DBG
	if (priority) {
		DPR_INIT(("VNIFCopyNetBuffer 802.1p = %x, len = %d, %d\n",
				  priority, NET_BUFFER_DATA_LENGTH(nb), bytes_copied));
	}
#endif
    return bytes_copied;
}
#endif

VOID
MpProcessSGList(
    IN  PDEVICE_OBJECT          pDO,
    IN  PVOID                   pIrp,
    IN  PSCATTER_GATHER_LIST    pSGList,
    IN  PVOID                   Context
    )
/*++
Routine Description:

    Process  SG list for an NDIS packet or a NetBuffer by submitting the physical addresses
    in SG list to NIC's DMA engine and issuing command n hardware.
    
Arguments:

    pDO:  Ignore this parameter
    
    pIrp: Ignore this parameter
    
    pSGList: A pointer to Scatter Gather list built for the NDIS packet or NetBuffer passed
             to NdisMAllocNetBufferList. This is not necessarily
             the same ScatterGatherListBuffer passed to NdisMAllocNetBufferSGList
             
    Context: The context passed to NdisMAllocNetBufferList. Here is 
             a pointer to MP_TCB
             
Return Value:

     None

    NOTE: called at DISPATCH level
--*/
{
#if 0
    PMP_TCB             tcb = (PMP_TCB)Context;
    PMP_ADAPTER         adapter = tcb->adapter;
    PMP_FRAG_LIST       pFragList = (PMP_FRAG_LIST)pSGList;
    NDIS_STATUS         status;
    MP_FRAG_LIST        FragList;
    BOOLEAN             fSendComplete = FALSE;
    ULONG               bytes_copied;
    PNET_BUFFER_LIST    nb_list;
    
    DPRINT( ("--> MpProcessSGList\n"));

    VNIF_ACQUIRE_SPIN_LOCK(&adapter->SendLock, TRUE);

    //
    // Save SG list that we got from NDIS. This is not necessarily the
    // same SG list buffer we passed to NdisMAllocNetBufferSGList
    //
    tcb->sg_list = pSGList;
        
    if (!VNIF_TEST_FLAG(tcb, fMP_TCB_IN_USE))
    {
        
        //
        // Before this callback function is called, reset happened and aborted
        // all the sends.
        // Call ndis to free resouces allocated for a SG list
        //
        VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, TRUE);

        NdisMFreeNetBufferSGList(
                            adapter->NdisMiniportDmaHandle,
                            tcb->sg_list,
                            tcb->nb);
        
    }
    else
    {
        if (pSGList->NumberOfElements > NIC_MAX_PHYS_BUF_COUNT)
        {
            //
            // the driver needs to do the local copy
            // 
            
            bytes_copied = MpCopyNetBuffer(tcb->nb, tcb->MpTxBuf);
                        
            //
            // MpCopyNetBuffer may return 0 if system resources are low or exhausted
            //
            if (bytes_copied == 0)
            {
                           
                DPRINT( ("Copy NetBuffer NDIS_STATUS_RESOURCES, NetBuffer= "PTR_FORMAT"\n", tcb->nb));
                nb_list = tcb->nb_list;
                NET_BUFFER_LIST_STATUS(nb_list) = NDIS_STATUS_RESOURCES;
                
                VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list)--;
                
                if (VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list) == 0)
                {
                    fSendComplete = TRUE;
                }
             
                VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, TRUE);
                NdisMFreeNetBufferSGList(
                    adapter->NdisMiniportDmaHandle,
                    tcb->sg_list,
                    tcb->nb);

                if (fSendComplete)
                {

                    NET_BUFFER_LIST_NEXT_NBL(nb_list) = NULL;
        
                    NdisMSendNetBufferListsComplete(
                        VNIF_GET_ADAPTER_HANDLE(adapter),
                        nb_list,
                        NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL);   
                }
             
             }
             else
             {
                VNIF_SET_FLAG(tcb, fMP_TCB_USE_LOCAL_BUF);

                //
                // Set up the frag list, only one fragment after it's coalesced
                //
                pFragList = &FragList;
                pFragList->NumberOfElements = 1;
                pFragList->Elements[0].Address = tcb->MpTxBuf->BufferPa;
                pFragList->Elements[0].Length = (bytes_copied >= NIC_MIN_PACKET_SIZE) ?
                                                bytes_copied : NIC_MIN_PACKET_SIZE;
                
                status = NICSendNetBuffer(adapter, tcb, pFragList);
                VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, TRUE);
             }
        }
        else
        {
            status = NICSendNetBuffer(adapter, tcb, pFragList);
            VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, TRUE);
        }
    }

    DPRINT( ("<-- MpProcessSGList\n"));
#endif
}

__inline PNET_BUFFER_LIST 
MP_FREE_SEND_NET_BUFFER(PVNIF_ADAPTER adapter, TCB *tcb)
{
    
	PNET_BUFFER         nb;
	PNET_BUFFER_LIST    nb_list;
	//BOOLEAN             fWaitForMapping = FALSE;

	//ASSERT(MP_TEST_FLAG(tcb, fMP_TCB_IN_USE));
    
	//fWaitForMapping = MP_TEST_FLAG(tcb, fMP_TCB_WAIT_FOR_MAPPING);
    
	nb = tcb->nb;
	nb_list = tcb->nb_list;
	tcb->nb = NULL;

	// Update the list before releasing the Spin-Lock 
	// to avoid race condition on the Adapter send list
	//adapter->CurrSendHead = adapter->CurrSendHead->Next;
	adapter->nBusySend--;
	//MP_CLEAR_FLAGS(tcb);
	ASSERT(adapter->nBusySend >= 0);

	//if (nb && !fWaitForMapping)
#ifndef XENNET_COPY_TX
	if (nb)
	{
		//
		// Call ndis to free resouces allocated for a SG list
		//
		NdisDprReleaseSpinLock(&adapter->SendLock);

		NdisMFreeNetBufferSGList(
			adapter->NdisMiniportDmaHandle,
			tcb->sg_list,
			nb);
		NdisDprAcquireSpinLock(&adapter->SendLock);
	}
#endif
	InsertHeadList(&adapter->SendFreeList, &tcb->list);
    
	//
	// SendLock is hold
	//
	if (nb_list)
	{
		VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list)--;
		if (VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list) == 0)
		{
			DPRINT(("Completing NetBufferList= %p\n", nb_list));
			NET_BUFFER_LIST_NEXT_NBL(nb_list) = NULL;
			return nb_list;
		}
	}

	return NULL;
    
}

NDIS_STATUS
VNIFSendNetBufferList(PVNIF_ADAPTER adapter,
	PNET_BUFFER_LIST nb_list,
	BOOLEAN bFromQueue,
	BOOLEAN dispatch_level)
{
	NDIS_STATUS		status = NDIS_STATUS_PENDING;
	NDIS_STATUS		send_status;
	TCB				*tcb = NULL;
	ULONG			bytes_copied;
	PNET_BUFFER		nb_to_send;
	PNET_BUFFER		nb;
    struct netif_tx_request *tx;
    uint32_t		len;
    int				notify;
    RING_IDX		i;
    PNDIS_NET_BUFFER_LIST_8021Q_INFO p8021;
#ifdef VNIF_TX_CHECKSUM_ENABLED
	PNDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO info;
#endif
	uint16_t flags;
	BOOLEAN			bSendNetBuffer = FALSE;
    
	DPRINT(("--> VNIFSendNetBufferList, NetBufferList = %p\n", nb_list));

#ifdef VNIF_TX_CHECKSUM_ENABLED
	info =
		(PNDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO) (
			&(NET_BUFFER_LIST_INFO(
			nb_list,
			TcpIpChecksumNetBufferListInfo)));
#ifdef DBG
	if (info->Transmit.TcpChecksum || info->Transmit.UdpChecksum ||
		info->Transmit.IpHeaderChecksum) {
		DPR_CHKSUM(("Rqst TX Checksum: v4 %x, v6 %x, tcp %x, udp %x, ip %x.\n",
			info->Transmit.IsIPv4,
			info->Transmit.IsIPv6,
			info->Transmit.TcpChecksum,
			info->Transmit.UdpChecksum,
			info->Transmit.IpHeaderChecksum));

	}
#endif
#endif

	p8021 =
		(PNDIS_NET_BUFFER_LIST_8021Q_INFO) (
			&(NET_BUFFER_LIST_INFO(
			nb_list,
			Ieee8021QNetBufferListInfo)));
    
	send_status = NDIS_STATUS_SUCCESS;

	if (bFromQueue) {
		nb_to_send = VNIF_GET_NET_BUFFER_LIST_NEXT_SEND(nb_list);
		VNIF_GET_NET_BUFFER_LIST_NEXT_SEND(nb_list) = NULL;
	}
	else {
		nb_to_send = NET_BUFFER_LIST_FIRST_NB(nb_list);
	}

	i = adapter->tx.req_prod_pvt;
	for (;  nb_to_send != NULL; nb_to_send = NET_BUFFER_NEXT_NB(nb_to_send)) {
		bSendNetBuffer = FALSE;

		/* Do we have any free tcbs? */
		if (IsListEmpty(&adapter->SendFreeList)) {
			ASSERT(nb_to_send != NULL);

			/* Save off the buffer we are currently trying to send. */
			VNIF_GET_NET_BUFFER_LIST_NEXT_SEND(nb_list) = nb_to_send;
			if (!bFromQueue) {
				/* We didn't process nb_list form the queue, so put it on. */
				InsertHeadQueue(&adapter->SendWaitQueue,
					VNIF_GET_NET_BUFFER_LIST_LINK(nb_list));
				adapter->nWaitSend++;
				
			}
			adapter->SendingNetBufferList = NULL;
			status = NDIS_STATUS_RESOURCES;
			break;
		}



		flags = NETTXF_data_validated;
#ifdef VNIF_TX_CHECKSUM_ENABLED
		if (adapter->cur_tx_tasks) {
			if ((*(uintptr_t *)&info->Value) & 0x1c) {
				if (info->Transmit.IsIPv4) {
					if (info->Transmit.TcpChecksum) {
						if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_TCP) {
							DPR_CHKSUM(("tcp: flags to valid and blank\n"));
							flags |= NETTXF_csum_blank;
						}
						else {
							DBG_PRINT(("tcp: mark checksum as failure\n"));
							send_status = NDIS_STATUS_FAILURE;
							break;
						}
					}
					if (info->Transmit.UdpChecksum) {
						if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP) {
							DPR_CHKSUM(("udp: flags to valid and blank\n"));
							flags |= NETTXF_csum_blank;
						}
						else {
							DBG_PRINT(("udp: mark checksum as failure\n"));
							send_status = NDIS_STATUS_FAILURE;
							break;
						}
					}
					if (info->Transmit.IpHeaderChecksum) {
						if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_IP) {
							DPR_CHKSUM(("ip: flags to valid and blank\n"));
							flags |= NETTXF_csum_blank;
						}
						else {
							DBG_PRINT(("ip: mark checksum as failure\n"));
							send_status = NDIS_STATUS_FAILURE;
							break;
						}
					}
				}
				else {
					/* IPV6 packet */
					DBG_PRINT(("ipv6: mark checksum as failure\n"));
					/* Packet wants checksum, but it's not supported. */
					send_status = NDIS_STATUS_FAILURE;
					break;
				}
			}
		}
#ifdef DBG
		if (info->Value) {
			DPR_CHKSUM(("TX check value %p.\n", info->Value));
		}
#endif
#endif




		tcb = (TCB *)RemoveHeadList(&adapter->SendFreeList);
        
		tcb->adapter = adapter;
		tcb->nb = nb_to_send;
		tcb->nb_list = nb_list;
    
		adapter->nBusySend++;
		//ASSERT(adapter->nBusySend <= adapter->NumTcb);

#ifdef XENNET_COPY_TX
		len = VNIFCopyNetBuffer(nb_to_send, tcb,
			(uint32_t)(*(uintptr_t *)&p8021->Value));
		if (len) {
#ifdef XENNET_TRACK_TX
			tcb->len = len;
#endif
			tx = RING_GET_REQUEST(&adapter->tx, i);
			tx->id = (uint16_t)tcb->index;
			tx->gref = tcb->grant_tx_ref;
			xennet_save_req(&adapter->txlist, tcb, i);
			tx->offset = (uint16_t)BYTE_OFFSET(tcb->data);
			tx->size = (uint16_t)len;

			/* TCP/IP transport has already computed checksum for us */
			tx->flags = flags; //NETTXF_data_validated;
			i++;
		}
		else {
			DBG_PRINT(("VNIFSendNetBufferList: VNIFCopyNetBuffer failed.\n"));
			send_status = NDIS_STATUS_RESOURCES;
		}
#else
		ASSERT(VNIF_TEST_FLAG(adapter, fMP_ADAPTER_SCATTER_GATHER));
		//
		// NOTE: net Buffer has to use new DMA APIs
		// 
		//
		// The function is called at DISPATCH level
		//
		VNIF_SET_FLAG(tcb, fMP_TCB_IN_USE);
		VNIF_SET_FLAG(tcb, fMP_TCB_WAIT_FOR_MAPPING);
            
            
		VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, dispatch_level);

		send_status = NdisMAllocateNetBufferSGList(
			adapter->NdisMiniportDmaHandle,
			nb_to_send,
			tcb,
			NDIS_SG_LIST_WRITE_TO_DEVICE,
			tcb->ScatterGatherListBuffer,
			adapter->ScatterGatherListSize);
		/* MpProcessSGList gets called by NdisMAllocateNetBufferSGList */

		VNIF_ACQUIRE_SPIN_LOCK(&adapter->SendLock, dispatch_level);            
#endif
            
		if (NDIS_STATUS_SUCCESS != send_status) {
			DBG_PRINT(("NetBuffer can't be mapped, nb = %p\n", nb_to_send));

			InsertHeadList(&adapter->SendFreeList, &tcb->list);
			adapter->nBusySend--;

			//VNIF_CLEAR_FLAGS(tcb);
			tcb->nb = NULL;
			tcb->nb_list = NULL;

			send_status = NDIS_STATUS_RESOURCES;
			break;
 
		}

	}

	/* All the NetBuffers in the NetBufferList has been processed, */
	/* If the NetBufferList is in queue now, dequeue it. */
	if (nb_to_send == NULL) {
		if (bFromQueue) {
			RemoveHeadQueue(&adapter->SendWaitQueue);
			adapter->nWaitSend--;
		}
		adapter->SendingNetBufferList = NULL;
	}

	//
	// As far as the miniport knows, the NetBufferList has been sent out.
	// Complete the NetBufferList now.  Error case.
	//
	/* This only happens if the first net buffer fials. */
	if (NDIS_STATUS_SUCCESS != send_status) {
		for (; nb_to_send != NULL;
			nb_to_send = NET_BUFFER_NEXT_NB(nb_to_send)) {
			VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list)--;
		}
		DBG_PRINT(("VNIFSendNetBufferList: failed, should complete, cnt %x.\n",
			VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list)	));

		NET_BUFFER_LIST_STATUS(nb_list) = send_status;
		if (VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list) == 0) {
        
			VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, dispatch_level);

			NET_BUFFER_LIST_NEXT_NBL(nb_list) = NULL;
			DBG_PRINT(("VNIFSendNetBufferList: failed, now completing.\n"));
			NdisMSendNetBufferListsComplete(
				adapter->AdapterHandle,
				nb_list,
				NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL);

			VNIF_ACQUIRE_SPIN_LOCK(&adapter->SendLock, dispatch_level);
		}
		else {
			DBG_PRINT(("VNIFSendNetBufferList: failed, not completing %x.\n",
				VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list)));
		}
	}
	adapter->tx.req_prod_pvt = i;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&adapter->tx, notify);
	if (notify) {
		notify_remote_via_evtchn(adapter->evtchn);
	}
    
	DPRINT( ("<-- VNIFSendNetBufferList\n"));
	return status;

}

void 
MPSendNetBufferLists(
	PVNIF_ADAPTER adapter,
	PNET_BUFFER_LIST nb_list,
	NDIS_PORT_NUMBER PortNumber,
	ULONG SendFlags)
{

	NDIS_STATUS status = NDIS_STATUS_PENDING;
	UINT nb_cnt = 0;
	PNET_BUFFER nb;
	PNET_BUFFER_LIST cur_nb_list;
	PNET_BUFFER_LIST next_nb_list;
	BOOLEAN dispatch_level;
    
	DPRINT(("VNIF: VNIFSendPackets IN.\n"));
#ifdef DBG
	if (adapter == NULL) {
		DBG_PRINT(("VNIFSendPackets: received null adapter\n"));
		return;
	}
	if (adapter->dbg_print_cnt < 5) {
		DPR_INIT(("VNIFSendPackets: starting send, dbg prnt %p, %s %d\n",
			adapter, adapter->Nodename, adapter->dbg_print_cnt));
		adapter->dbg_print_cnt++;
	}
#endif

	dispatch_level = NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags);
	do {
		VNIF_ACQUIRE_SPIN_LOCK(&adapter->SendLock, dispatch_level);

		if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_PAUSING|VNF_ADAPTER_PAUSED)) {
			status =  NDIS_STATUS_PAUSED;
			DBG_PRINT(("MPSendNetBufferLists: adapter paused %x.\n",
				adapter->Flags));
			VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, dispatch_level);
			break;
		}

		//
		// Is this adapter ready for sending?
		// 
		if (VNIF_IS_NOT_READY(adapter)) {
			//
			// there  is link
			//
			if (!VNIF_TEST_FLAG(adapter, VNF_ADAPTER_NO_LINK)) {
				//
				// Insert Net Buffer List into the queue
				// 
				for (cur_nb_list = nb_list;
					cur_nb_list != NULL;
					cur_nb_list = next_nb_list) {
					next_nb_list = NET_BUFFER_LIST_NEXT_NBL(cur_nb_list);
					
					nb_cnt = 0;
					for (nb = NET_BUFFER_LIST_FIRST_NB(cur_nb_list);
						nb != NULL; 
						nb = NET_BUFFER_NEXT_NB(nb)) {
						nb_cnt++;
					}
					ASSERT(nb_cnt > 0);
					VNIF_GET_NET_BUFFER_LIST_REF_COUNT(cur_nb_list) = nb_cnt;
					VNIF_GET_NET_BUFFER_LIST_NEXT_SEND(cur_nb_list) =
						NET_BUFFER_LIST_FIRST_NB(cur_nb_list);
					NET_BUFFER_LIST_STATUS(cur_nb_list) = NDIS_STATUS_SUCCESS;
					InsertTailQueue(&adapter->SendWaitQueue, 
						VNIF_GET_NET_BUFFER_LIST_LINK(cur_nb_list));
					adapter->nWaitSend++;
					DBG_PRINT(("MPSendNetBufferLists: link detected - queue nb_list %p, nb_cnt %x\n", cur_nb_list, nb_cnt));
				}
				VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, dispatch_level);
				break;
			}
			
			//
			// adapter is not ready and there is not link
			//
			status = VNIF_GET_STATUS_FROM_FLAGS(adapter);
			
			DBG_PRINT(("MPSendNetBufferLists: adapter not ready, f = %x, s = %x.\n",
				adapter->Flags, status));
			VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, dispatch_level);
			break;
		}
		
		// adapter is ready, send this net buffer list, in this case, 
		// we always return pending     
		for (cur_nb_list = nb_list;
			cur_nb_list != NULL;
			cur_nb_list = next_nb_list) {
			next_nb_list = NET_BUFFER_LIST_NEXT_NBL(cur_nb_list);
			nb_cnt = 0;
			for (nb = NET_BUFFER_LIST_FIRST_NB(cur_nb_list);
				nb != NULL; 
				nb = NET_BUFFER_NEXT_NB(nb)) {
				nb_cnt++;
			}
			ASSERT(nb_cnt > 0);
			VNIF_GET_NET_BUFFER_LIST_REF_COUNT(cur_nb_list) = nb_cnt;
			//
			// queue is not empty or tcb is not available, or another
			// thread is sending a NetBufferList.
			//
			if (!IsQueueEmpty(&adapter->SendWaitQueue) || 
				IsListEmpty(&adapter->SendFreeList) ||
				adapter->SendingNetBufferList != NULL) {
				//
				// The first net buffer is the buffer to send
				//
				VNIF_GET_NET_BUFFER_LIST_NEXT_SEND(cur_nb_list) =
					NET_BUFFER_LIST_FIRST_NB(cur_nb_list);
				NET_BUFFER_LIST_STATUS(cur_nb_list) = NDIS_STATUS_SUCCESS;
				InsertTailQueue(&adapter->SendWaitQueue, 
					VNIF_GET_NET_BUFFER_LIST_LINK(cur_nb_list));
				adapter->nWaitSend++;
			}
			else {
				/* Do the actual work of sending. */
				adapter->SendingNetBufferList = cur_nb_list;
				NET_BUFFER_LIST_STATUS(cur_nb_list) = NDIS_STATUS_SUCCESS;
				VNIFSendNetBufferList(adapter, cur_nb_list,
					FALSE, dispatch_level);
			}
		}
		
		VNIF_RELEASE_SPIN_LOCK(&adapter->SendLock, dispatch_level);
	}
	while (FALSE);

	if (status != NDIS_STATUS_PENDING) {
		ULONG SendCompleteFlags = 0;
        
		for (cur_nb_list = nb_list;
			cur_nb_list != NULL;
			cur_nb_list = next_nb_list) {
			next_nb_list = NET_BUFFER_LIST_NEXT_NBL(cur_nb_list);
			NET_BUFFER_LIST_STATUS(cur_nb_list) = status;
		}

		if (NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags)) {
			NDIS_SET_SEND_COMPLETE_FLAG(SendCompleteFlags,
				NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL);
		}
                  
		DBG_PRINT(("MPSendNetBufferLists: error, complete nb_list %x.\n",
			SendCompleteFlags));
		NdisMSendNetBufferListsComplete(adapter->AdapterHandle,
			nb_list,
			SendCompleteFlags);   
	}
	DPRINT( ("<==== MPSendNetBufferLists\n"));
}

__inline 
MP_SEND_STATS(PVNIF_ADAPTER adapter, PNET_BUFFER NetBuffer, int16_t status)
{
	PUCHAR  EthHeader;
	ULONG   Length;
	PMDL    Mdl = NET_BUFFER_CURRENT_MDL(NetBuffer);

	NdisQueryMdl(Mdl, &EthHeader, &Length, NormalPagePriority);
	if (EthHeader != NULL)
	{
		EthHeader += NET_BUFFER_CURRENT_MDL_OFFSET(NetBuffer);
		if (ETH_IS_BROADCAST(EthHeader))
		{
			adapter->ifHCOutBroadcastPkts++;
			adapter->ifHCOutBroadcastOctets +=NET_BUFFER_DATA_LENGTH(NetBuffer);
		}
		else if (ETH_IS_MULTICAST(EthHeader))
		{
			adapter->ifHCOutMulticastPkts++;
			adapter->ifHCOutMulticastOctets +=NET_BUFFER_DATA_LENGTH(NetBuffer);
		}
		else
		{
			adapter->ifHCOutUcastPkts++;
			adapter->ifHCOutUcastOctets += NET_BUFFER_DATA_LENGTH(NetBuffer);
		}
	}
	if (status != NETIF_RSP_OKAY) {
		if (status == NETIF_RSP_ERROR) {
			adapter->ifOutErrors++;
		}
		if (status == NETIF_RSP_DROPPED) {
			adapter->ifOutDiscards++;
		}
		DBG_PRINT(("VNIF: send errs %d, drped %d.\n",
			adapter->ifOutErrors, adapter->ifOutDiscards));
	}
}

NDIS_STATUS
VNIFCheckSendCompletion(PVNIF_ADAPTER adapter)
{
    NDIS_STATUS         status = NDIS_STATUS_SUCCESS;
    PNET_BUFFER_LIST    nb_list;
    PNET_BUFFER_LIST    last_nb_list = NULL;
    PNET_BUFFER_LIST    complete_nb_lists = NULL;
	PQUEUE_ENTRY pEntry; 
	struct netif_tx_response *txrsp;
#ifdef XENNET_COPY_TX
    TCB *pkt;
#else
	NDIS_PACKET *pkt;
    PNDIS_PACKET TxPacketArray[VNIF_MAX_BUSY_SENDS];
#endif
    RING_IDX cons, prod;
    uint32_t id;
	uint32_t cnt;
	int did_work = 0;

    DPRINT(("---> MpHandleSendInterrupt\n"));
    NdisDprAcquireSpinLock(&adapter->SendLock);

    //
    // Any packets being sent? Any packet waiting in the send queue?
    //
    if (adapter->nBusySend == 0 &&
        IsQueueEmpty(&adapter->SendWaitQueue)) {
        DPRINT(("<--- MpHandleSendInterrupt\n"));
		NdisDprReleaseSpinLock(&adapter->SendLock);
        return status;
    }

    //
    // Check the first TCB on the send list
    //
	cnt = 0;
    do { //(adapter->nBusySend > 0)
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
			xennet_clear_req(&adapter->txlist, pkt);
#ifdef DBG
			if (!VNIF_IS_READY(adapter)) {
				DPR_INIT(("VNIFCheckSendCompletion: not ready %p\n", pkt));
			}
#endif
			MP_SEND_STATS(adapter, pkt->nb, txrsp->status);

			nb_list = MP_FREE_SEND_NET_BUFFER(adapter, pkt);
			if (nb_list != NULL) {
				NET_BUFFER_LIST_STATUS(nb_list) = NDIS_STATUS_SUCCESS;
				if (complete_nb_lists == NULL) {
					complete_nb_lists = nb_list;
				}
				else {
					NET_BUFFER_LIST_NEXT_NBL(last_nb_list) = nb_list;
				}
				NET_BUFFER_LIST_NEXT_NBL(nb_list) = NULL;
				last_nb_list = nb_list;
			}
#else
			pkt = adapter->tx_packets[id];
#ifdef DBG
			if (!VNIF_IS_READY(adapter)) {
				DPR_INIT(("VNIFCheckSendCompletion: not ready %p\n", pkt));
			}
#endif
			DPRINT(("txr packet = %p, id = %x.\n", pkt, id));

			adapter->tx_packets[id] = (NDIS_PACKET *)adapter->tx_id_alloc_head;
			adapter->tx_id_alloc_head = id;

			if (pkt == NULL) {
				if (gnttab_end_foreign_access_ref(
						adapter->grant_tx_ref[id], GNTMAP_readonly)
					== 0) {
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

    /* Complete the NET_BUFFER_LISTs */
    if (complete_nb_lists != NULL)
    {
        NdisDprReleaseSpinLock(&adapter->SendLock);
        
        NdisMSendNetBufferListsComplete(
                adapter->AdapterHandle,
                complete_nb_lists,
                NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL);
        NdisDprAcquireSpinLock(&adapter->SendLock);
    }

    //
    // If we queued any transmits because we didn't have any TCBs earlier,
    // dequeue and send those packets now, as long as we have free TCBs.
    //
    if (VNIF_IS_READY(adapter))
    {
        while (!IsQueueEmpty(&adapter->SendWaitQueue) &&
			!IsListEmpty(&adapter->SendFreeList) &&
            adapter->SendingNetBufferList == NULL) {
            
            //
            // We cannot remove it now, we just need to get the head
            //
            pEntry = GetHeadQueue(&adapter->SendWaitQueue);
            ASSERT(pEntry);
            nb_list = VNIF_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK (pEntry);
            adapter->SendingNetBufferList = nb_list;
            
            status = VNIFSendNetBufferList(adapter, nb_list, TRUE, TRUE);
            if (status != NDIS_STATUS_SUCCESS &&
				status != NDIS_STATUS_PENDING) {
                break;
            }
        }
    }

    
    DPRINT(("<--- MpHandleSendInterrupt\n"));
    NdisDprReleaseSpinLock(&adapter->SendLock);
    return status;
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
VOID
VNIFReceivePackets(PVNIF_ADAPTER adapter)
{
	PNDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO info;
    PNDIS_NET_BUFFER_LIST_8021Q_INFO p8021;
	PNET_BUFFER nb;
	PNET_BUFFER_LIST nb_list = NULL; 
	PNET_BUFFER_LIST prev_nb_list = NULL;
    netif_rx_request_t *req;
    struct netif_rx_response *rx;
	uint32_t rcv_flags;
    grant_ref_t i, ref;
	UINT nb_list_cnt;
    RING_IDX rp;
	RING_IDX old;
    RCB *rcb;
    UINT len, offset;
	int more_to_do;
	int j, k;

	DPRINT(("VNIFReceivePackets: start.\n"));

	if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_PAUSING | VNF_ADAPTER_PAUSED)) {
		return;
	}

    VNIF_INC_REF(adapter);

#ifdef DBG
	if (KeGetCurrentIrql() > 2) {
		DPR_INIT(("VNIFReceivePackets irql = %d\n", KeGetCurrentIrql()));
	}
#endif

    NdisDprAcquireSpinLock(&adapter->RecvLock);
	nb_list_cnt = 0;
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

		while (nb_list_cnt < adapter->rcv_limit
				&& !IsListEmpty(&adapter->RecvToProcess)) {
			rcb = (RCB *) RemoveHeadList(&adapter->RecvToProcess);
			len = rcb->len;
			if (len > NETIF_RSP_NULL) {
				if ((ref = rcb->grant_rx_ref) != GRANT_INVALID_REF) {
					/* xennet_should_complete_packet also updates the stats. */
					rcv_flags = xennet_should_complete_packet(
						adapter, rcb->page, len);
					if (rcv_flags) {
						if (len < 60) {
							NdisZeroMemory(rcb->page + len, 60 - len);
						}

						nb = NET_BUFFER_LIST_FIRST_NB(rcb->nb_list);

						/* Make sure it's a priority and not a Vlan packet. */
						if (*(uint16_t *)&rcb->page[P8021_TPID_BYTE]
								== P8021_TPID_TYPE
							&& (rcb->page[P8021_TCI_BYTE]
								& P8021_NETWORK_MASK) == 0
							&& rcb->page[P8021_VLAN_BYTE] == 0) {
							p8021 =
								(PNDIS_NET_BUFFER_LIST_8021Q_INFO) (
									&(NET_BUFFER_LIST_INFO(
									rcb->nb_list,
									Ieee8021QNetBufferListInfo)));
							(*(uintptr_t *)&p8021->Value) =
								rcb->page[P8021_TCI_BYTE] >> P8021_BIT_SHIFT;
							DPR_INIT(("802.1.p page[14] = %x, pri = %x.\n",
								rcb->page[14], p8021->TagHeader.UserPriority));
							
							/* Copy the MAC header to fill in the space */
							/* occupied by the priority bytes. */
							for (k = P8021_VLAN_BYTE, j = SRC_ADDR_END_BYTE;
								j >= 0; k--, j--) {
								rcb->page[k] = rcb->page[j];
							}
							/* Don't worry about the offset, just adjust the */
							/* mdl start address.  Fix the address when the */
							/* packet is returned. */
							(uint8_t *)rcb->mdl->MappedSystemVa +=
								P8021_BYTE_LEN;
							(uint8_t *)rcb->mdl->StartVa += P8021_BYTE_LEN;
							/*
							for (k = 12, j = 16; k < (int)len; k++, j++) {
								rcb->page[k] = rcb->page[j];
							}
							*/
							len -= P8021_BYTE_LEN;
							DPR_INIT(("  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
									  rcb->page[0],
									  rcb->page[1],
									  rcb->page[2],
									  rcb->page[3],
									  rcb->page[4],
									  rcb->page[5],
									  rcb->page[6],
									  rcb->page[7],
									  rcb->page[8],
									  rcb->page[9],
									  rcb->page[10],
									  rcb->page[11],
									  rcb->page[12],
									  rcb->page[13],
									  rcb->page[14],
									  rcb->page[15],
									  rcb->page[16],
									  rcb->page[17]));
						}
						NET_BUFFER_DATA_LENGTH(nb) = len;
						NdisAdjustMdlLength(rcb->mdl, len);

						nb_list_cnt++;
						if (nb_list == NULL) {
							nb_list = rcb->nb_list;
						}
						else {
							NET_BUFFER_LIST_NEXT_NBL(prev_nb_list) =
								rcb->nb_list;
						}
						NET_BUFFER_LIST_NEXT_NBL(rcb->nb_list) = NULL;
						VNIF_CLEAR_FLAG(rcb->nb_list,
							(NET_BUFFER_LIST_FLAGS(rcb->nb_list)
							 & NBL_FLAGS_MINIPORT_RESERVED));
						prev_nb_list = rcb->nb_list;

						/*
						 * If backend set data_validated or csum_blank flag,
						 * we set the packet valid, override TCP/IP transport
						 * validation against checksum. This is not only more
						 * efficient, but necessary, in case backend sends its
						 * local packet with wrong csum.
						 */
						info =
							(PNDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO) (
								&(NET_BUFFER_LIST_INFO(
								nb_list,
								TcpIpChecksumNetBufferListInfo)));

						if (rcb->page[VNIF_PACKET_OFFSET_IP]
								== VNIF_PACKET_TYPE_IP
							&& adapter->cur_rx_tasks
							&& (!(rcb->page[VNIF_IP_FLAGS_BYTE] & 1)
								&& !rcb->page[VNIF_IP_FRAG_OFFSET_BYTE])
							&& (rcb->flags &
								(NETRXF_data_validated | NETRXF_csum_blank))) {

							info->Receive.IpChecksumSucceeded =
								(adapter->cur_rx_tasks &
									VNIF_CHKSUM_IPV4_IP) ? 1 : 0;

							if (rcb->page[VNIF_PACKET_OFFSET_TCP_UDP]
									== VNIF_PACKET_TYPE_TCP) {
								info->Receive.TcpChecksumSucceeded = 
									(adapter->cur_rx_tasks &
										VNIF_CHKSUM_IPV4_TCP) ? 1 : 0;
								info->Receive.UdpChecksumSucceeded = 0;
							}
							else if (rcb->page[VNIF_PACKET_OFFSET_TCP_UDP]
									== VNIF_PACKET_TYPE_UDP) {
								info->Receive.TcpChecksumSucceeded = 0;
								info->Receive.UdpChecksumSucceeded =
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
								DBG_PRINT(("Need to calc RX checksum?.\n"));
							}
#endif
						}
						//DEBUG_DUMP_CHKSUM('R', rcb->flags, rcb->page, len);

						VNIFInterlockedIncrement(adapter->nBusyRecv);
						DPRINT(("Receiveing rcb %x.\n", rcb->index));
						if (adapter->num_rcb > NET_RX_RING_SIZE)
							xennet_add_rcb_to_ring(adapter);
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
		DPRINT(("VNIF: VNIF Received Packets = %d.\n", nb_list_cnt));
		RING_FINAL_CHECK_FOR_RESPONSES(&adapter->rx, more_to_do);
	} while (more_to_do && nb_list_cnt < VNIF_MAX_BUSY_RECVS);

	if (!IsListEmpty(&adapter->RecvToProcess)) {
		LARGE_INTEGER li;

		li.QuadPart = 500;
		VNIF_SET_TIMER(adapter->rcv_timer, li);
	}

    NdisDprReleaseSpinLock(&adapter->RecvLock);

	if (nb_list_cnt) {
		//adapter->PoMgmt.OutstandingRecv += nb_list_cnt;

		NdisMIndicateReceiveNetBufferLists(
			adapter->AdapterHandle,
			nb_list,
			NDIS_DEFAULT_PORT_NUMBER,
			nb_list_cnt,
			NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL); 
	}

	if (adapter->rx.sring->req_event > old) {
        notify_remote_via_evtchn(adapter->evtchn);
	}

    VNIF_DEC_REF(adapter);
	DPRINT(("VNIFReceivePackets: end.\n"));
}

VOID 
MPReturnNetBufferLists(NDIS_HANDLE MiniportAdapterContext,
	PNET_BUFFER_LIST    NetBufferLists,
	ULONG               ReturnFlags)
{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER)MiniportAdapterContext;
    PNDIS_NET_BUFFER_LIST_8021Q_INFO p8021;
	RCB *rcb;
	PNET_BUFFER_LIST nb_list;
	PNET_BUFFER_LIST next_nb_list;
	BOOLEAN dispatch_level;

	DPRINT(("====> MPReturnNetBufferLists\n"));

	dispatch_level = NDIS_TEST_RETURN_AT_DISPATCH_LEVEL(ReturnFlags);
	VNIF_ACQUIRE_SPIN_LOCK(&adapter->RecvLock, dispatch_level);
    
	for (nb_list = NetBufferLists; nb_list != NULL; nb_list = next_nb_list) {
		next_nb_list = NET_BUFFER_LIST_NEXT_NBL(nb_list);
		rcb = VNIF_GET_NET_BUFFER_LIST_RFD(nb_list);

		/* In case of priority packet, put everything back the way it was. */
		rcb->mdl->MappedSystemVa = rcb->page;
		rcb->mdl->StartVa = rcb->page;
		p8021 =
			(PNDIS_NET_BUFFER_LIST_8021Q_INFO) (
				&(NET_BUFFER_LIST_INFO(
				rcb->nb_list,
				Ieee8021QNetBufferListInfo)));
		p8021->Value = 0;

		xennet_return_rcb(adapter, rcb);

		VNIFInterlockedDecrement(adapter->nBusyRecv);
	}

	VNIF_RELEASE_SPIN_LOCK(&adapter->RecvLock, dispatch_level);
    
	DPRINT(("<==== MPReturnNetBufferLists\n"));
}

VOID 
VNIFFreeQueuedSendPackets(PVNIF_ADAPTER adapter, NDIS_STATUS status)
{
	PQUEUE_ENTRY entry;
	PNET_BUFFER_LIST nb_list;
	PNET_BUFFER_LIST nb_list_to_complete = NULL;
	PNET_BUFFER_LIST last_nb_list = NULL;
	PNET_BUFFER nb;

	DPRINT(("--> VNIFFreeQueuedSendPackets\n"));

	NdisAcquireSpinLock(&adapter->SendLock);
	while (!IsQueueEmpty(&adapter->SendWaitQueue)) {
		entry = RemoveHeadQueue(&adapter->SendWaitQueue); 
		ASSERT(entry);
        
		adapter->nWaitSend--;

		nb_list = VNIF_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK(entry);

		NET_BUFFER_LIST_STATUS(nb_list) = status;
		//
		// The sendLock is held
		// 
		nb = VNIF_GET_NET_BUFFER_LIST_NEXT_SEND(nb_list);
        
		for (; nb != NULL; nb = NET_BUFFER_NEXT_NB(nb)) {
			VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list)--;
		}
		//
		// If Ref count goes to 0, then complete it.
		// Otherwise, Send interrupt DPC would complete it later
		// 
		if (VNIF_GET_NET_BUFFER_LIST_REF_COUNT(nb_list) == 0) {
			if (nb_list_to_complete == NULL) {
				nb_list_to_complete = nb_list;
			}
			else {
				NET_BUFFER_LIST_NEXT_NBL(last_nb_list) = nb_list;
			}
			NET_BUFFER_LIST_NEXT_NBL(nb_list) = NULL;
			last_nb_list = nb_list;

		}
	}

	if (nb_list_to_complete != NULL)
	{
		NdisReleaseSpinLock(&adapter->SendLock);
		NdisMSendNetBufferListsComplete(
			adapter->AdapterHandle,
			nb_list_to_complete,
			NDIS_STATUS_SEND_ABORTED);   
	}
	else
		NdisReleaseSpinLock(&adapter->SendLock);
    
	DPRINT(("<-- VNIFFreeQueuedSendPackets\n"));

}

void
VNIFIndicateLinkStatus(PVNIF_ADAPTER adapter, uint32_t status)
{
    
	NDIS_LINK_STATE                LinkState;
	NDIS_STATUS_INDICATION         StatusIndication;

	DPRINT(("--> VNIFIndicateLinkStatus: %x\n", status));
	NdisZeroMemory(&LinkState, sizeof(NDIS_LINK_STATE));
	NdisZeroMemory(&StatusIndication, sizeof(NDIS_STATUS_INDICATION));
    
	LinkState.Header.Revision = NDIS_LINK_STATE_REVISION_1;
	LinkState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	LinkState.Header.Size = sizeof(NDIS_LINK_STATE);
    
	if (status) {
		LinkState.MediaConnectState = MediaConnectStateConnected;
		LinkState.MediaDuplexState = MediaDuplexStateFull;
		LinkState.XmitLinkSpeed = 1000 * 1000000;
	}
	else {
		LinkState.MediaConnectState = MediaConnectStateDisconnected;
		LinkState.MediaDuplexState = MediaDuplexStateUnknown;
		LinkState.XmitLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
	}
   
	StatusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
	StatusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
	StatusIndication.Header.Size = sizeof(NDIS_STATUS_INDICATION);
	StatusIndication.SourceHandle = adapter->AdapterHandle;
	StatusIndication.StatusCode = NDIS_STATUS_LINK_STATE;
	StatusIndication.StatusBuffer = &LinkState;
	StatusIndication.StatusBufferSize = sizeof(LinkState);
	
	NdisMIndicateStatusEx(adapter->AdapterHandle, &StatusIndication);
	DPRINT(("<-- VNIFIndicateLinkStatus\n"));
}

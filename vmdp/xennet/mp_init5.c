/*****************************************************************************
 *
 *   File Name:      init5.c
 *
 *   %version:       9 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jul 02 17:24:44 2009 %
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


static void
VNIFSetOffloadDefaults(PVNIF_ADAPTER adapter)
{
	DPR_INIT(("VNIFSetOffloadDefaults\n"));
	adapter->hw_chksum_task.V4Transmit.IpOptionsSupported = 0;
#ifdef VNIF_TX_CHECKSUM_ENABLED
	adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 1;
	adapter->hw_chksum_task.V4Transmit.TcpChecksum = 1;
	adapter->cur_tx_tasks = VNIF_CHKSUM_IPV4_TCP;
#else
	adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 0;
	adapter->hw_chksum_task.V4Transmit.TcpChecksum = 0;
	adapter->cur_tx_tasks = 0;
#endif
	adapter->hw_chksum_task.V4Transmit.UdpChecksum = 0;
	adapter->hw_chksum_task.V4Transmit.IpChecksum = 0;

	adapter->hw_chksum_task.V4Receive.IpOptionsSupported = 0;
	adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 1;
	adapter->hw_chksum_task.V4Receive.TcpChecksum = 1;
	adapter->hw_chksum_task.V4Receive.UdpChecksum = 0;
	adapter->hw_chksum_task.V4Receive.IpChecksum = 0;
	adapter->cur_rx_tasks = VNIF_CHKSUM_IPV4_TCP;

	adapter->hw_chksum_task.V6Transmit.IpOptionsSupported = 0;
	adapter->hw_chksum_task.V6Transmit.TcpOptionsSupported = 0;
	adapter->hw_chksum_task.V6Transmit.TcpChecksum = 0;
	adapter->hw_chksum_task.V6Transmit.UdpChecksum = 0;

	adapter->hw_chksum_task.V6Receive.IpOptionsSupported = 0;
	adapter->hw_chksum_task.V6Receive.TcpOptionsSupported = 0;
	adapter->hw_chksum_task.V6Receive.TcpChecksum = 0;
	adapter->hw_chksum_task.V6Receive.UdpChecksum = 0;
}

/* Used to override defaults as set through the NIC's properties.  The */
/* hardware now says that it only supports these latests settings. */
void
VNIFInitChksumOffload(PVNIF_ADAPTER adapter, uint32_t chksum_type,
	uint32_t chksum_value)
{
	DPR_INIT(("VNIFInitChksumOffload type %x, value %x: tx %x rx %x\n",
		chksum_type, chksum_value,
		adapter->cur_tx_tasks, adapter->cur_rx_tasks));

	if (chksum_type == VNIF_CHKSUM_IPV4_TCP) {
		switch (chksum_value) {
			case VNIF_CHKSUM_DISABLED:
				DPR_INIT(("Setting TCP off\n"));
				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_TCP;
				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_TCP;
				adapter->hw_chksum_task.V4Transmit.TcpChecksum = 0;
				adapter->hw_chksum_task.V4Receive.TcpChecksum = 0;

				if (!(adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP)) {
					adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 0;
				}
				if (!(adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_UDP)) {
					adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 0;
				}
				break;

			case VNIF_CHKSUM_TX:
				DPR_INIT(("Setting TCP Tx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_TCP;
				adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Transmit.TcpChecksum = 1;

				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_TCP;
				adapter->hw_chksum_task.V4Receive.TcpChecksum = 0;
				if (!(adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_UDP)) {
					adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 0;
				}
				break;

			case VNIF_CHKSUM_RX:
				DPR_INIT(("Setting TCP Rx\n"));
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_TCP;
				adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Receive.TcpChecksum = 1;

				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_TCP;
				adapter->hw_chksum_task.V4Transmit.TcpChecksum = 0;
				if (!(adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP)) {
					adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 0;
				}
				break;

			case VNIF_CHKSUM_TXRX:
				DPR_INIT(("Setting TCP Tx Rx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_TCP;
				adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Transmit.TcpChecksum = 1;

				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_TCP;
				adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Receive.TcpChecksum = 1;
				break;
			default:
				DPR_INIT(("Unknown TCP chksum value %x\n", chksum_value));
				break;
		}
	}
	else if (chksum_type == VNIF_CHKSUM_IPV4_UDP) {
		switch (chksum_value) {
			case VNIF_CHKSUM_DISABLED:
				DPR_INIT(("Setting UDP off\n"));
				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_UDP;
				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_UDP;
				adapter->hw_chksum_task.V4Transmit.UdpChecksum = 0;
				adapter->hw_chksum_task.V4Receive.UdpChecksum = 0;

				if (!(adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP)) {
					adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 0;
				}
				if (!(adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_UDP)) {
					adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 0;
				}
				break;

			case VNIF_CHKSUM_TX:
				DPR_INIT(("Setting UDP Tx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_UDP;
				adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Transmit.UdpChecksum = 1;

				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_UDP;
				adapter->hw_chksum_task.V4Receive.UdpChecksum = 0;
				if (!(adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_UDP)) {
					adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 0;
				}
				break;

			case VNIF_CHKSUM_RX:
				DPR_INIT(("Setting UDP Rx\n"));
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_UDP;
				adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Receive.UdpChecksum = 1;

				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_UDP;
				adapter->hw_chksum_task.V4Transmit.UdpChecksum = 0;
				if (!(adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP)) {
					adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 0;
				}
				break;

			case VNIF_CHKSUM_TXRX:
				DPR_INIT(("Setting UDP Tx Rx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_UDP;
				adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Transmit.UdpChecksum = 1;

				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_UDP;
				adapter->hw_chksum_task.V4Receive.TcpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Receive.UdpChecksum = 1;
				break;
			default:
				DPR_INIT(("Unknown UDP chksum value %x\n", chksum_value));
				break;
		}
	}
	else { /* IP */
		switch (chksum_value) {
			case VNIF_CHKSUM_DISABLED:
				DPR_INIT(("Setting IP off\n"));
				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_IP;
				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_IP;
				adapter->hw_chksum_task.V4Transmit.IpChecksum = 0;
				adapter->hw_chksum_task.V4Transmit.IpOptionsSupported = 0;
				adapter->hw_chksum_task.V4Receive.IpOptionsSupported = 0;
				adapter->hw_chksum_task.V4Receive.IpChecksum = 0;
				break;

			case VNIF_CHKSUM_TX:
				DPR_INIT(("Setting IP Tx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_IP;
				adapter->hw_chksum_task.V4Transmit.IpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Transmit.IpChecksum = 1;

				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_IP;
				adapter->hw_chksum_task.V4Receive.IpChecksum = 0;
				adapter->hw_chksum_task.V4Receive.IpOptionsSupported = 0;
				break;

			case VNIF_CHKSUM_RX:
				DPR_INIT(("Setting IP Rx\n"));
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_IP;
				adapter->hw_chksum_task.V4Receive.IpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Receive.IpChecksum = 1;

				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_IP;
				adapter->hw_chksum_task.V4Transmit.IpChecksum = 0;
				adapter->hw_chksum_task.V4Transmit.IpOptionsSupported = 0;
				break;

			case VNIF_CHKSUM_TXRX:
				DPR_INIT(("Setting IP Tx Rx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_IP;
				adapter->hw_chksum_task.V4Transmit.IpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Transmit.IpChecksum = 1;

				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_IP;
				adapter->hw_chksum_task.V4Receive.IpOptionsSupported = 1;
				adapter->hw_chksum_task.V4Receive.IpChecksum = 1;
				break;
			default:
				DPR_INIT(("Unknown IP chksum value %x\n", chksum_value));
				break;
		}
	}
	DPR_INIT(("VNIFInitChksumOffload resulting tx %x rx %x\n",
		adapter->cur_tx_tasks, adapter->cur_rx_tasks));
}

NDIS_STATUS
VNIFInitialize(PVNIF_ADAPTER adapter,
	PNDIS_MEDIUM MediumArray,
	UINT MediumArraySize,
	PUINT SelectedMediumIndex,
	NDIS_HANDLE WrapperConfigurationContext)
{
	NDIS_STATUS status;
	uint32_t i;

	for (i = 0; i < MediumArraySize; i++) {
		if (MediumArray[i] == VNIF_MEDIA_TYPE) {
			*SelectedMediumIndex = i;
			break;
		}
	}

	if (i == MediumArraySize) {
		return NDIS_STATUS_UNSUPPORTED_MEDIA;
	}

	/* Check for any overrides */
	adapter->WrapperContext = WrapperConfigurationContext;
	VNIFSetOffloadDefaults(adapter);
	VNIFReadRegParameters(adapter);

	NdisMSetAttributesEx(
		adapter->AdapterHandle,
		(NDIS_HANDLE) adapter,
		0,
		NDIS_ATTRIBUTE_DESERIALIZE
		| NDIS_ATTRIBUTE_BUS_MASTER
		| NDIS_ATTRIBUTE_USES_SAFE_BUFFER_APIS,
		NdisInterfaceInternal);

#ifdef XENNET_SG_DMA
	status = NdisMInitializeScatterGatherDma(
		adapter->AdapterHandle,
		TRUE,
		ETH_MAX_PACKET_SIZE);
	if (status != NDIS_STATUS_SUCCESS) {
		DPR_INIT(("VNIF: VNIFInitialize - init scatter gather failed %x.\n",
			status));
#ifndef XENNET_COPY_TX
		return status;
#endif
	}
#endif

#ifdef NDIS50_MINIPORT
	//
	// Register a shutdown handler for NDIS50 or earlier miniports
	// For NDIS51 miniports, set AdapterShutdownHandler.
	//
	NdisMRegisterAdapterShutdownHandler(
		adapter->AdapterHandle,
		(PVOID) adapter,
		(ADAPTER_SHUTDOWN_HANDLER) MPShutdown);
#endif
	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
VNIFSetupNdisAdapterEx(PVNIF_ADAPTER adapter)
{
	NdisAllocateSpinLock(&adapter->Lock);
	NdisInitializeTimer(&adapter->ResetTimer,
		VNIFResetCompleteTimerDpc, adapter);
	NdisInitializeTimer(&adapter->rcv_timer,
		VNIFReceiveTimerDpc, adapter);
	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
VNIFSetupNdisAdapterRx(PVNIF_ADAPTER adapter)
{
	PNDIS_PACKET packet;
	PNDIS_BUFFER buffer;
	RCB *rcb;
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	grant_ref_t ref;
	uint32_t i;

	DPR_INIT(("VNIF: VNIFNdisSetupNdisAdapter - IN\n"));
	do {
		/* Pre-allocate packet pool and buffer pool for recveive. */
		NdisAllocatePacketPool(
			&status,
			&adapter->recv_pool,
			adapter->num_rcb,
			PROTOCOL_RESERVED_SIZE_IN_PACKET);
		if(status != NDIS_STATUS_SUCCESS) {
			DBG_PRINT(("VNIF: NdisAllocatePacketPool failed\n"));
			break;
		}

		NdisAllocateBufferPool(
			&status,
			&adapter->RecvBufferPoolHandle,
			adapter->num_rcb);
		if(status != NDIS_STATUS_SUCCESS) {
			DBG_PRINT(("VNIF: NdisAllocateBufferPool for recv buffer failed\n"));
			break;
		}

		/*
		 * We have to initialize all of RCBs before receiving any data. The RCB
		 * is the control block for a single packet data structure. And we
		 * should pre-allocate the buffer and memory for receive. Because ring
		 * buffer is not initialized at the moment, putting RCB grant reference
		 * onto rx_ring.req is deferred.
		 */
		for (i = 0; i < adapter->num_rcb; i++) {
			VNIF_ALLOCATE_MEMORY(
				rcb,
				sizeof(RCB),
				VNIF_POOL_TAG,
				NdisMiniportDriverHandle,
				NormalPoolPriority);
			if(rcb == NULL) {
				DBG_PRINT(("VNIF: fail to allocate memory for RCBs.\n"));
				status = STATUS_NO_MEMORY;
				break;
			}
			NdisZeroMemory(rcb, sizeof(RCB));
			adapter->RCBArray[i] = rcb;
			rcb->adapter = adapter;
			rcb->index = i;

			/*
			 * there used to be a bytes header option in xenstore for
			 * receive page but now it is hardwired to 0.
			 */

			NdisAllocatePacket(
				&status,
				&packet,
				adapter->recv_pool);
			if(status != NDIS_STATUS_SUCCESS) {
				DBG_PRINT(("VNIF: NdisAllocatePacket failed\n"));
				break;
			}
			NDIS_SET_PACKET_HEADER_SIZE(packet, ETH_HEADER_SIZE);
			rcb->packet = packet;
			*((RCB **)packet->MiniportReserved) = rcb;
			*((uint32_t *)&packet->MiniportReserved[sizeof(void *)]) =
				0x44badd55;

			VNIF_ALLOCATE_MEMORY(
				rcb->page,
				PAGE_SIZE,
				VNIF_POOL_TAG,
				NdisMiniportDriverHandle,
				NormalPoolPriority);
			if(rcb->page == NULL) {
				DBG_PRINT(("VNIF: fail to allocate receive pages.\n"));
				status = STATUS_NO_MEMORY;
				break;
			}

			NdisAllocateBuffer(
				&status,
				&buffer,
				adapter->RecvBufferPoolHandle,
				(PVOID)(rcb->page),
				PAGE_SIZE);
			if (status != NDIS_STATUS_SUCCESS) {
				DBG_PRINT(("VNIF: NdisAllocateBuffer failed.\n"));
				break;
			}
			rcb->buffer = buffer;
			NdisChainBufferAtBack(packet, buffer);

			rcb->grant_rx_ref = GRANT_INVALID_REF;
		}

		if (status != NDIS_STATUS_SUCCESS)
			break;		/* Get out of the do while. */

	} while(FALSE);

	DPR_INIT(("VNIF: VNIFNdisSetupNdisAdapter - OUT\n"));
	return status;
}

VOID
VNIFFreeAdapterRx(PVNIF_ADAPTER adapter)
{
	RCB *rcb;
	uint32_t i;

	for(i = 0; i < adapter->num_rcb; i++) {
		rcb = adapter->RCBArray[i];
		if (!rcb)
			continue;

		if (rcb->packet) {
			NdisFreePacket(rcb->packet);
		}

		if (rcb->buffer) {
			NdisFreeBuffer(rcb->buffer);
		}

		if (rcb->page) {
			NdisFreeMemory(rcb->page, PAGE_SIZE, 0);
		}
		NdisFreeMemory(adapter->RCBArray[i], sizeof(RCB), 0);
	}

	DPR_INIT(("VNIF: VNIFFreeAdapter packet stuff\n"));
	if(adapter->recv_pool) {
		NdisFreePacketPool(adapter->recv_pool);
	}
	if(adapter->RecvBufferPoolHandle) {
		NdisFreeBufferPool(adapter->RecvBufferPoolHandle);
	}
}

VOID
VNIFFreeAdapterEx(PVNIF_ADAPTER adapter)
{
}

NDIS_STATUS
VNIFNdisOpenConfiguration(PVNIF_ADAPTER adapter, NDIS_HANDLE *config_handle)
{
	NDIS_STATUS status;

	DPR_INIT(("VNIFNdisOpenConfiguration: irql = %d IN\n",KeGetCurrentIrql()));
	NdisOpenConfiguration(&status, config_handle, adapter->WrapperContext);
	return status;
}

/*****************************************************************************
 *
 *   File Name:      init.c
 *
 *   %version:       25 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jul 27 16:40:07 2009 %
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

static NDIS_STRING reg_tcp_chksum_name =
	NDIS_STRING_CONST("*TCPChecksumOffloadIPv4");
static NDIS_STRING reg_numrfd_name =
	NDIS_STRING_CONST("NumRcb");
static NDIS_STRING reg_rcv_limit_name =
	NDIS_STRING_CONST("RcvLimit");

#ifndef NDIS60_MINIPORT
static NDIS_STRING reg_tx_throttle_start_name =
	NDIS_STRING_CONST("TxThrottleStart");
static NDIS_STRING reg_tx_throttle_stop_name =
	NDIS_STRING_CONST("TxThrottleStop");
#endif

static NDIS_STATUS
VNIFSetupNdisAdapterTx(PVNIF_ADAPTER adapter)
{
#ifdef XENNET_COPY_TX
	TCB *tcb;
#endif
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	uint32_t i;

	do {
#ifdef XENNET_COPY_TX
		/*
		* Allocate for each TCB, because sizeof(TCB) is less than PAGE_SIZE,
		* it will not cross page boundary.
		*/
		for (i = 0; i < VNIF_MAX_BUSY_SENDS; i++) {
			VNIF_ALLOCATE_MEMORY(
				tcb,
				sizeof(TCB),
				VNIF_POOL_TAG,
				NdisMiniportDriverHandle,
				NormalPoolPriority);
			if(tcb == NULL) {
				DBG_PRINT(("VNIF: Failed to allocate memory for TCB's\n"));
				status = STATUS_NO_MEMORY;
				break;
			}
			NdisZeroMemory(tcb, sizeof(TCB));
			adapter->TCBArray[i] = tcb;
			tcb->index = i;
			tcb->grant_tx_ref = GRANT_INVALID_REF;
		}
#else
		VNIF_ALLOCATE_MEMORY(
			adapter->zero_data,
			ETH_MIN_PACKET_SIZE,
			VNIF_POOL_TAG,
			NdisMiniportDriverHandle,
			NormalPoolPriority);
		if(adapter->zero_data == NULL) {
			DBG_PRINT(("VNIF: Failed to allocate memory for zero_data\n"));
			status = STATUS_NO_MEMORY;
			break;
		}
		NdisZeroMemory(adapter->zero_data, ETH_MIN_PACKET_SIZE);

		for (i = 0; i < VNIF_MAX_BUSY_SENDS; i++) {
			adapter->grant_tx_ref[i] = GRANT_INVALID_REF;
		}
#endif
	} while (FALSE);
	return status;
}

VOID
VNIFFreeAdapterTx(PVNIF_ADAPTER adapter)
{
	uint32_t i;

	/*
	* Free all the resources we allocated for send.
	*/
#ifdef XENNET_COPY_TX
	DPR_INIT(("VNIF: VNIFFreeAdapter NdisFreeMemory\n"));
	for (i = 0; i < VNIF_MAX_BUSY_SENDS; i++) {
		if (adapter->TCBArray[i]) {
			NdisFreeMemory(adapter->TCBArray[i], sizeof(TCB), 0);
		}
	}
#else
	NdisFreeMemory(adapter->zero_data, ETH_MIN_PACKET_SIZE, 0);
#endif
}

NDIS_STATUS
VNIFSetupNdisAdapter(PVNIF_ADAPTER adapter)
{
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	void *ptr;

	DPR_INIT(("VNIF: VNIFSetupNdisAdapter - IN\n"));
	do {
		NdisInitializeListHead(&adapter->List);
		NdisInitializeListHead(&adapter->SendWaitList);
		NdisInitializeListHead(&adapter->SendFreeList);
		NdisInitializeListHead(&adapter->RecvFreeList);
		NdisInitializeListHead(&adapter->RecvToProcess);

		NdisAllocateSpinLock(&adapter->RecvLock);
		NdisAllocateSpinLock(&adapter->SendLock);

		/* initialize event first, so VNIF_DEC_REF can use it. */
		NdisInitializeEvent(&adapter->RemoveEvent);

		adapter->gref_tx_head = GRANT_INVALID_REF;
		adapter->gref_rx_head = GRANT_INVALID_REF;

		/* the link speed is set */
		adapter->ulLinkSpeed = VNIF_LINK_SPEED;

		ptr = NULL;
		VNIF_ALLOCATE_MEMORY(
			ptr,
			sizeof(RCB *) * adapter->num_rcb,
			VNIF_POOL_TAG,
			NdisMiniportDriverHandle,
			NormalPoolPriority);
		if (ptr == NULL) {
			status = STATUS_NO_MEMORY;
			break;
		}
		NdisZeroMemory(ptr, sizeof(RCB *) * adapter->num_rcb);
		adapter->RCBArray = (RCB **)ptr;

		status = VNIFSetupNdisAdapterEx(adapter);
		if(status != NDIS_STATUS_SUCCESS) {
			break;
		}

		status = VNIFSetupNdisAdapterRx(adapter);
		if(status != NDIS_STATUS_SUCCESS) {
			VNIFFreeAdapterEx(adapter);
			VNIFFreeAdapterRx(adapter);
			break;
		}

		status = VNIFSetupNdisAdapterTx(adapter);
		if(status != NDIS_STATUS_SUCCESS) {
			VNIFFreeAdapterEx(adapter);
			VNIFFreeAdapterRx(adapter);
			VNIFFreeAdapterTx(adapter);
			break;
		}

		if (adapter->oid_buffer) {
			switch (adapter->oid) {
				case OID_802_3_PERMANENT_ADDRESS:
					ETH_COPY_NETWORK_ADDRESS(
						adapter->oid_buffer,
						adapter->PermanentAddress);
					break;
				case OID_802_3_CURRENT_ADDRESS:
					ETH_COPY_NETWORK_ADDRESS(
						adapter->oid_buffer,
						adapter->CurrentAddress);
					break;
			}
			adapter->oid = 0;
			adapter->oid_buffer = NULL;
			VNIFOidRequestComplete(adapter);
		}
	} while (FALSE);
	if (status != NDIS_STATUS_SUCCESS) {
		NdisFreeSpinLock(&adapter->RecvLock);
		NdisFreeSpinLock(&adapter->SendLock);
	}
	DPR_INIT(("VNIF: VNIFSetupNdisAdapter - OUT\n"));
	return status;
}

VOID
VNIFFreeAdapter(PVNIF_ADAPTER adapter)
{
	uint32_t i;

	DPR_INIT(("VNIF: VNIFFreeAdapter- IN\n"));

	ASSERT(adapter);
	ASSERT(!adapter->RefCount);
	ASSERT(IsListEmpty(&adapter->SendWaitList));

	VNIFFreeAdapterEx(adapter);
	VNIFFreeAdapterRx(adapter);
	VNIFFreeAdapterTx(adapter);
	VNIFFreeXenAdapter(adapter)
	NdisFreeSpinLock(&adapter->SendLock);
	NdisFreeSpinLock(&adapter->RecvLock);
	NdisFreeMemory(adapter, sizeof(VNIF_ADAPTER), 0);

	DPR_INIT(("VNIF: VNIFFreeAdapter- OUT\n"));
}

VOID
VNIFAttachAdapter(PVNIF_ADAPTER adapter)
{
	NdisInterlockedInsertTailList(
		&Global.AdapterList,
		&adapter->List,
		&Global.Lock);
}

VOID
VNIFDetachAdapter(PVNIF_ADAPTER adapter)
{
	NdisAcquireSpinLock(&Global.Lock);
	RemoveEntryList(&adapter->List);
	NdisReleaseSpinLock(&Global.Lock);
}

NDIS_STATUS
VNIFReadRegParameters(PVNIF_ADAPTER adapter)
{
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	NDIS_HANDLE config_handle;
    NDIS_CONFIGURATION_PARAMETER reg_value;
    PNDIS_CONFIGURATION_PARAMETER returned_value;
    WCHAR wbuffer[16] = {0};
	PUCHAR net_addr;
	UINT length;

	status = VNIFNdisOpenConfiguration(adapter, &config_handle);

	DPR_INIT(("VNIF: VNIFReadRegParameters back from NdisOpenConfiguration\n"));
	if(status != NDIS_STATUS_SUCCESS) {
		DBG_PRINT(("VNIF: NdisOpenConfiguration failed\n"));
		ETH_COPY_NETWORK_ADDRESS(adapter->CurrentAddress,
			adapter->PermanentAddress);
		return NDIS_STATUS_FAILURE;
	}

	//
	// Read NetworkAddress registry value and use it as the current address
	// if there is a software configurable NetworkAddress specified in
	// the registry.
	//
	NdisReadNetworkAddress(&status, &net_addr, &length, config_handle);
	DPR_INIT(("VNIF: VNIFReadRegParameters back from NdisReadNetworkAddress\n"));

	if((status == NDIS_STATUS_SUCCESS) && (length == ETH_LENGTH_OF_ADDRESS)) {
		if (ETH_IS_MULTICAST(net_addr) || ETH_IS_BROADCAST(net_addr)) {
			/* cannot assign a multicast address as a mac address. */
			ETH_COPY_NETWORK_ADDRESS(adapter->CurrentAddress,
				adapter->PermanentAddress);
		}
		else {
			ETH_COPY_NETWORK_ADDRESS(adapter->CurrentAddress, net_addr);
		}
	}
	else {
		ETH_COPY_NETWORK_ADDRESS(adapter->CurrentAddress,
			adapter->PermanentAddress);
	}

	DPR_INIT(("VNIF: Permanent Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
			  adapter->PermanentAddress[0],
			  adapter->PermanentAddress[1],
			  adapter->PermanentAddress[2],
			  adapter->PermanentAddress[3],
			  adapter->PermanentAddress[4],
			  adapter->PermanentAddress[5]));

	DPR_INIT(("VNIF: Current Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
			  adapter->CurrentAddress[0],
			  adapter->CurrentAddress[1],
			  adapter->CurrentAddress[2],
			  adapter->CurrentAddress[3],
			  adapter->CurrentAddress[4],
			  adapter->CurrentAddress[5]));

	NdisReadConfiguration(
		&status,
		&returned_value,
		config_handle,
		&reg_tcp_chksum_name,
		NdisParameterInteger);
	DPR_INIT(("VNIF: NdisReadConfiguration TCP %x\n", status));
	if (status == NDIS_STATUS_SUCCESS) {
		VNIFInitChksumOffload(adapter, VNIF_CHKSUM_IPV4_TCP,
			returned_value->ParameterData.IntegerData);
	}
	else {
		adapter->cur_tx_tasks = 0;
		adapter->cur_rx_tasks = 0;
	}

	NdisReadConfiguration(
		&status,
		&returned_value,
		config_handle,
		&reg_numrfd_name,
		NdisParameterInteger);
	DPR_INIT(("VNIF: NdisReadConfiguration NumRcb %d\n",
		returned_value->ParameterData.IntegerData));
	if (status == NDIS_STATUS_SUCCESS) {
		adapter->num_rcb = returned_value->ParameterData.IntegerData;
		if (adapter->num_rcb > VNIF_MAX_NUM_RCBS)
			adapter->num_rcb = NET_RX_RING_SIZE;
	}
	else {
		adapter->num_rcb = NET_RX_RING_SIZE;
		status = NDIS_STATUS_SUCCESS;
	}

	NdisReadConfiguration(
		&status,
		&returned_value,
		config_handle,
		&reg_rcv_limit_name,
		NdisParameterInteger);
	DPR_INIT(("VNIF: NdisReadConfiguration rcv limit %d\n",
		returned_value->ParameterData.IntegerData));
	if (status == NDIS_STATUS_SUCCESS) {
		adapter->rcv_limit = returned_value->ParameterData.IntegerData;
		if (adapter->rcv_limit > NET_RX_RING_SIZE ||
			adapter->rcv_limit < VNIF_MIN_RCV_LIMIT)
			adapter->rcv_limit = NET_RX_RING_SIZE;
	}
	else {
		adapter->rcv_limit = VNIF_MAX_BUSY_RECVS;
		status = NDIS_STATUS_SUCCESS;
	}

#ifndef NDIS60_MINIPORT
	NdisReadConfiguration(
		&status,
		&returned_value,
		config_handle,
		&reg_tx_throttle_start_name,
		NdisParameterInteger);
	DPR_INIT(("VNIF: NdisReadConfiguration tx_throttle_start %d\n",
		returned_value->ParameterData.IntegerData));
	if (status == NDIS_STATUS_SUCCESS) {
		adapter->tx_throttle_start = returned_value->ParameterData.IntegerData;
		if (adapter->tx_throttle_start > adapter->num_rcb)
			adapter->tx_throttle_start = adapter->num_rcb;
	}
	else {
		adapter->tx_throttle_start = adapter->num_rcb;
		status = NDIS_STATUS_SUCCESS;
	}
	DPR_INIT(("VNIF: tx_throttle_start %d\n", adapter->tx_throttle_start));

	NdisReadConfiguration(
		&status,
		&returned_value,
		config_handle,
		&reg_tx_throttle_stop_name,
		NdisParameterInteger);
	DPR_INIT(("VNIF: NdisReadConfiguration tx_throttle_stop %d\n",
		returned_value->ParameterData.IntegerData));
	if (status == NDIS_STATUS_SUCCESS) {
		if (returned_value->ParameterData.IntegerData <=
				adapter->tx_throttle_start)
			adapter->tx_throttle_stop =
				returned_value->ParameterData.IntegerData;
		else {
			/* It makes no sense to have the stop value greater than the */
			/* start value.  Set the stop to the start and then write */
			/* the value back to the registry so that NIC configuration */
			/* will accurately reflect what is being used. */
			adapter->tx_throttle_stop = adapter->tx_throttle_start;
			reg_value.ParameterType = NdisParameterString;
			reg_value.ParameterData.StringData.Length = 0;
			reg_value.ParameterData.StringData.MaximumLength = sizeof(wbuffer);
			reg_value.ParameterData.StringData.Buffer = wbuffer;
			RtlIntegerToUnicodeString(adapter->tx_throttle_start, 10,
				&reg_value.ParameterData.StringData);
			NdisWriteConfiguration(
				&status,
				config_handle,
				&reg_tx_throttle_stop_name,
				&reg_value);
		}
	} else {
		adapter->tx_throttle_stop = adapter->tx_throttle_start;
		status = NDIS_STATUS_SUCCESS;
	}
	DPR_INIT(("VNIF: tx_throttle_stop %d\n", adapter->tx_throttle_stop));
#endif

	//
	// Close the configuration registry
	//
	NdisCloseConfiguration(config_handle);

	DPR_INIT(("VNIF: VNIFReadRegParameters - OUT\n"));

	return status;
}

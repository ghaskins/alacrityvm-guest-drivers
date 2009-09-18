/*****************************************************************************
 *
 *   File Name:      oid.c
 *
 *   %version:       15 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jun 25 08:55:22 2009 %
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

#ifndef NDIS60_MINIPORT
static NDIS_TASK_OFFLOAD OffloadTasks[] = {
    {
        NDIS_TASK_OFFLOAD_VERSION,
        sizeof(NDIS_TASK_OFFLOAD),
        TcpIpChecksumNdisTask,
        0,
        sizeof(NDIS_TASK_TCP_IP_CHECKSUM)
    }
};

static ULONG OffloadTasksCount = sizeof(OffloadTasks) / sizeof(OffloadTasks[0]);
#endif


NDIS_OID VNIFSupportedOids[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_MEDIA_CONNECT_STATUS,
    OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_GEN_RCV_CRC_ERROR,
    OID_GEN_TRANSMIT_QUEUE_LENGTH,
#ifdef NDIS60_MINIPORT
    OID_GEN_STATISTICS,
    OID_GEN_INTERRUPT_MODERATION,
#else
    OID_802_3_MAC_OPTIONS,
#endif
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_DEFERRED,
    OID_802_3_XMIT_MAX_COLLISIONS,
    OID_802_3_RCV_OVERRUN,
    OID_802_3_XMIT_UNDERRUN,
    OID_802_3_XMIT_HEARTBEAT_FAILURE,
    OID_802_3_XMIT_TIMES_CRS_LOST,
    OID_802_3_XMIT_LATE_COLLISIONS,
    OID_PNP_QUERY_POWER,
    OID_PNP_SET_POWER,
	/*
    OID_PNP_ADD_WAKE_UP_PATTERN,
    OID_PNP_REMOVE_WAKE_UP_PATTERN,
    OID_PNP_ENABLE_WAKE_UP,
	*/

#ifdef NDIS60_MINIPORT
	OID_PNP_CAPABILITIES,
	OID_OFFLOAD_ENCAPSULATION,
	OID_TCP_OFFLOAD_PARAMETERS,
#else
    OID_TCP_TASK_OFFLOAD
#endif
};

#ifdef NDIS60_MINIPORT
uint32_t SupportedOidListLength = sizeof(VNIFSupportedOids);
#endif

static NDIS_STATUS
NICGetStatsCounters(PVNIF_ADAPTER adapter, NDIS_OID Oid, PULONG64 info64)
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
#ifdef NDIS60_MINIPORT
	PNDIS_STATISTICS_INFO stats;
#endif

	switch (Oid) {
		case OID_GEN_XMIT_ERROR:
			*info64 = adapter->ifOutErrors + adapter->ifOutDiscards;
			break;

		case OID_GEN_RCV_ERROR:
			*info64 = adapter->ifInErrors + adapter->in_no_buffers;
			break;

		case OID_GEN_RCV_NO_BUFFER:
			*info64 = adapter->in_no_buffers;
			break;

		case OID_GEN_RCV_CRC_ERROR:
			*info64 = 0;
			break;

		case OID_GEN_TRANSMIT_QUEUE_LENGTH:
			*info64 = adapter->RegNumTcb;
			break;

		case OID_802_3_RCV_ERROR_ALIGNMENT:
			*info64 = 0;
			break;

		case OID_802_3_XMIT_ONE_COLLISION:
			*info64 = 0;
			break;

		case OID_802_3_XMIT_MORE_COLLISIONS:
			*info64 = 0;
			break;

		case OID_802_3_XMIT_DEFERRED:
			*info64 = 0;
			break;

		case OID_802_3_XMIT_MAX_COLLISIONS:
			*info64 = 0;
			break;

		case OID_802_3_RCV_OVERRUN:
			*info64 = 0;
			break;

		case OID_802_3_XMIT_UNDERRUN:
			*info64 = 0;
			break;

		case OID_802_3_XMIT_HEARTBEAT_FAILURE:
			*info64 = 0;
			break;

		case OID_802_3_XMIT_TIMES_CRS_LOST:
			*info64 = 0;
			break;

		case OID_802_3_XMIT_LATE_COLLISIONS:
			*info64 = 0;
			break;
#ifdef NDIS60_MINIPORT
		case OID_GEN_XMIT_OK:
			*info64 = adapter->ifHCOutUcastPkts + 
				adapter->ifHCOutMulticastPkts +
				adapter->ifHCOutBroadcastPkts; 
			break;

		case OID_GEN_RCV_OK:
			*info64 = adapter->ifHCInUcastPkts +
				adapter->ifHCInMulticastPkts +
				adapter->ifHCInBroadcastPkts;  
			break;

		case OID_GEN_STATISTICS:
			stats = (PNDIS_STATISTICS_INFO)info64;
			NdisZeroMemory(stats,sizeof(NDIS_STATISTICS_INFO));
			stats->Header.Revision = NDIS_OBJECT_REVISION_1;
			stats->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
			stats->Header.Size =
				NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
			stats->SupportedStatistics =
				NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV |
				NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV |
				NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV |
				NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV |
				NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS |
				NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR |
				NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT |
				NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT |
				NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT |
				NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT |
				NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR |
				NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS |
				NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV |
				NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV |
				NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV |
				NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT |
				NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT |
				NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT;

			stats->ifInDiscards =
				adapter->ifInErrors +
				adapter->in_discards +
				adapter->in_no_buffers;
			stats->ifInErrors = adapter->ifInErrors;
            
			stats->ifHCInOctets =
				adapter->ifHCInBroadcastOctets +
				adapter->ifHCInMulticastOctets +
				adapter->ifHCInUcastOctets;         

			stats->ifHCInUcastPkts = adapter->ifHCInUcastPkts;
			stats->ifHCInMulticastPkts = adapter->ifHCInMulticastPkts;  
			stats->ifHCInBroadcastPkts = adapter->ifHCInBroadcastPkts;  

			stats->ifHCOutOctets =
				adapter->ifHCOutMulticastOctets +
				adapter->ifHCOutBroadcastOctets +
				adapter->ifHCOutUcastOctets;        

			stats->ifHCOutUcastPkts = adapter->ifHCOutUcastPkts;     
			stats->ifHCOutMulticastPkts = adapter->ifHCOutMulticastPkts; 
			stats->ifHCOutBroadcastPkts = adapter->ifHCOutBroadcastPkts; 

			stats->ifOutErrors = adapter->ifOutErrors + adapter->ifOutDiscards;
			stats->ifOutDiscards = adapter->ifOutErrors;

			stats->ifHCInUcastOctets = adapter->ifHCInUcastOctets;
			stats->ifHCInMulticastOctets = adapter->ifHCInMulticastOctets;
			stats->ifHCInBroadcastOctets = adapter->ifHCInBroadcastOctets;

			stats->ifHCOutUcastOctets = adapter->ifHCOutUcastOctets;
			stats->ifHCOutMulticastOctets = adapter->ifHCOutMulticastOctets;
			stats->ifHCOutBroadcastOctets = adapter->ifHCOutBroadcastOctets;
			break;
#else
		case OID_GEN_XMIT_OK:
			*info64 = adapter->GoodTransmits;
			break;

		case OID_GEN_RCV_OK:
			*info64 = adapter->GoodReceives;
			break;
#endif


		default:
			status = NDIS_STATUS_NOT_SUPPORTED;
	}
	return status;
}

NDIS_STATUS
MPQueryInformation(
  IN NDIS_HANDLE MiniportAdapterContext,
  IN NDIS_OID Oid,
  IN PVOID InformationBuffer,
  IN ULONG InformationBufferLength,
  OUT PULONG BytesWritten,
  OUT PULONG BytesNeeded
  )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    PVNIF_ADAPTER adapter;

    NDIS_HARDWARE_STATUS hardwareStatus = NdisHardwareStatusReady;
    NDIS_MEDIUM medium = NdisMedium802_3;
    UCHAR vendorDesc[] = VNIF_VENDOR_DESC;

    ULONG info = 0;
    USHORT info16;
    UINT64 info64;
    PVOID infoptr = (PVOID) &info;
    ULONG infolen = sizeof(info);
	ULONG ulBytesAvailable = infolen;

#ifdef NDIS60_MINIPORT
    NDIS_INTERRUPT_MODERATION_PARAMETERS int_mod;
#else
    PNDIS_TASK_OFFLOAD_HEADER pNdisTaskOffloadHdr;
    PNDIS_TASK_OFFLOAD pTaskOffload;
    PNDIS_TASK_TCP_IP_CHECKSUM pTcpIpChecksumTask;
#endif
    UINT i;
    BOOLEAN do_copy = TRUE;

	if (Oid < 0x20101 || Oid > 0x202ff) {
		DPR_OID(("Query oid %x.\n", Oid));
	}
    adapter = (PVNIF_ADAPTER) MiniportAdapterContext;

    *BytesWritten = 0;
    *BytesNeeded = 0;

	switch (Oid) {
		case OID_GEN_SUPPORTED_LIST:
			infoptr = (PVOID) VNIFSupportedOids;
			ulBytesAvailable = infolen = sizeof(VNIFSupportedOids);
			break;

		case OID_GEN_HARDWARE_STATUS:
			infoptr = (PVOID) &hardwareStatus;
			ulBytesAvailable = infolen = sizeof(NDIS_HARDWARE_STATUS);
			break;

		case OID_GEN_MEDIA_SUPPORTED:
		case OID_GEN_MEDIA_IN_USE:
			infoptr = (PVOID) &medium;
			ulBytesAvailable = infolen = sizeof(NDIS_MEDIUM);
			break;

		case OID_GEN_CURRENT_LOOKAHEAD:
		case OID_GEN_MAXIMUM_LOOKAHEAD:
			if (adapter->ulLookAhead == 0) {
				adapter->ulLookAhead = VNIF_MAX_LOOKAHEAD;
			}
			info = adapter->ulLookAhead;
			break;

		case OID_GEN_MAXIMUM_FRAME_SIZE:
			info = ETH_MAX_PACKET_SIZE - ETH_HEADER_SIZE;
			break;

		case OID_GEN_MAXIMUM_TOTAL_SIZE:
		case OID_GEN_TRANSMIT_BLOCK_SIZE:
		case OID_GEN_RECEIVE_BLOCK_SIZE:
			info = (ULONG) ETH_MAX_PACKET_SIZE;
			break;

		case OID_GEN_MAC_OPTIONS:
			info = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
				NDIS_MAC_OPTION_TRANSFERS_NOT_PEND     |
				NDIS_MAC_OPTION_NO_LOOPBACK;
			break;

		case OID_GEN_LINK_SPEED:
			info = adapter->ulLinkSpeed;
			break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:
			info = ETH_MAX_PACKET_SIZE * VNIF_MAX_BUSY_SENDS;
            break;

		case OID_GEN_RECEIVE_BUFFER_SPACE:
			info = ETH_MAX_PACKET_SIZE * NET_RX_RING_SIZE;
			break;

		case OID_GEN_VENDOR_ID:
			info = VNIF_VENDOR_ID;
			break;

		case OID_GEN_VENDOR_DESCRIPTION:
			infoptr = vendorDesc;
			ulBytesAvailable = infolen = sizeof(vendorDesc);
			break;

		/* driver version */
		case OID_GEN_VENDOR_DRIVER_VERSION:
			info = VNIF_VENDOR_DRIVER_VERSION;
			break;

		/* NDIS version  */
		case OID_GEN_DRIVER_VERSION:
			info16 = (USHORT) (VNIF_NDIS_MAJOR_VERSION<<8)
				+ VNIF_NDIS_MINOR_VERSION;
			infoptr = (PVOID) &info16;
			ulBytesAvailable = infolen = sizeof(USHORT);
			break;

		case OID_GEN_MAXIMUM_SEND_PACKETS:
			info = VNIF_MAX_SEND_PKTS;
			break;

		case OID_GEN_MEDIA_CONNECT_STATUS:
			info = VNIFGetMediaConnectStatus(adapter);
			break;

		/* allows upper driver to choose which type of packet it can see */
		case OID_GEN_CURRENT_PACKET_FILTER:
			info = adapter->PacketFilter;
			break;

		case OID_PNP_CAPABILITIES:
			DPR_INIT(("OID_PNP_CAPABILITIES: not supported.\n"));
			status = NDIS_STATUS_NOT_SUPPORTED;
			break;

		case OID_802_3_PERMANENT_ADDRESS:
			if (VNIF_TEST_FLAG(adapter, VNF_DISCONNECTED)) {
				if(ETH_LENGTH_OF_ADDRESS <= InformationBufferLength) {
					adapter->oid = Oid;
					adapter->oid_buffer = InformationBuffer;
					*BytesWritten = ETH_LENGTH_OF_ADDRESS;
					return NDIS_STATUS_PENDING;
				}
			}
			infoptr = adapter->PermanentAddress;
			ulBytesAvailable = infolen = ETH_LENGTH_OF_ADDRESS;
			break;

		case OID_802_3_CURRENT_ADDRESS:
			if (VNIF_TEST_FLAG(adapter, VNF_DISCONNECTED)) {
				if(ETH_LENGTH_OF_ADDRESS <= InformationBufferLength) {
					adapter->oid = Oid;
					adapter->oid_buffer = InformationBuffer;
					*BytesWritten = ETH_LENGTH_OF_ADDRESS;
					return NDIS_STATUS_PENDING;
				}
			}
			infoptr = adapter->CurrentAddress;
			ulBytesAvailable = infolen = ETH_LENGTH_OF_ADDRESS;
			break;

		case OID_802_3_MAXIMUM_LIST_SIZE:
			info = VNIF_MAX_MCAST_LIST;
			break;

#ifdef NDIS60_MINIPORT
		case OID_GEN_STATISTICS:
			// we are going to directly fill the information buffer
			do_copy = FALSE;
            
			ulBytesAvailable = infolen = sizeof(NDIS_STATISTICS_INFO);
			if (InformationBufferLength < infolen)
			{
				break;
			}
			status = NICGetStatsCounters(adapter, Oid,
				(PULONG64)InformationBuffer);
			break;

		case OID_GEN_INTERRUPT_MODERATION:
			// This driver does not support interrupt moderation at this time
			int_mod.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
			int_mod.Header.Revision =
				NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
			int_mod.Header.Size =
				NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
			int_mod.Flags = 0;
			int_mod.InterruptModeration = NdisInterruptModerationNotSupported; 	  
			ulBytesAvailable = infolen =
				sizeof(NDIS_INTERRUPT_MODERATION_PARAMETERS);
			infoptr = (PVOID) &int_mod;	
			break;

#else
		case OID_802_3_MAC_OPTIONS:
			info = 0;
			break;
#endif

		case OID_GEN_XMIT_OK:
		case OID_GEN_RCV_OK:
		case OID_GEN_XMIT_ERROR:
		case OID_GEN_RCV_ERROR:
		case OID_GEN_RCV_NO_BUFFER:
		case OID_GEN_RCV_CRC_ERROR:
		case OID_GEN_TRANSMIT_QUEUE_LENGTH:
		case OID_802_3_RCV_ERROR_ALIGNMENT:
		case OID_802_3_XMIT_ONE_COLLISION:
		case OID_802_3_XMIT_MORE_COLLISIONS:
		case OID_802_3_XMIT_DEFERRED:
		case OID_802_3_XMIT_MAX_COLLISIONS:
        case OID_802_3_RCV_OVERRUN:
		case OID_802_3_XMIT_UNDERRUN:
		case OID_802_3_XMIT_HEARTBEAT_FAILURE:
		case OID_802_3_XMIT_TIMES_CRS_LOST:
		case OID_802_3_XMIT_LATE_COLLISIONS:
			/*
			if (Oid == OID_GEN_XMIT_OK || Oid == OID_GEN_RCV_OK) {
				infolen = sizeof(info64);
			}
			if (InformationBufferLength < sizeof(ULONG)) {
				status = NDIS_STATUS_BUFFER_TOO_SHORT;
				*BytesNeeded = infolen;
				break;
			}
			if (InformationBufferLength < infolen) {
				*BytesNeeded = infolen;
			}
			if ((status = NICGetStatsCounters(adapter, Oid, &info64))
				== NDIS_STATUS_SUCCESS) {
				infolen = InformationBufferLength < infolen ?
					InformationBufferLength : infolen;
				infoptr = &info64;
			}
			*/

            status = NICGetStatsCounters(adapter, Oid, &info64);
            ulBytesAvailable = infolen = sizeof(info64);
            if (status == NDIS_STATUS_SUCCESS)
            {
                if (InformationBufferLength < sizeof(ULONG))
                {
                    status = NDIS_STATUS_BUFFER_TOO_SHORT;
                    *BytesNeeded = ulBytesAvailable;
                    break;
                }

                infolen = min(InformationBufferLength, ulBytesAvailable);
                infoptr = &info64;
            }
                    
			break;         

		case OID_TCP_TASK_OFFLOAD:
			DPR_INIT(("OID_TCP_TASK_OFFLOAD query\n"));
#ifdef NDIS60_MINIPORT
#else
			pNdisTaskOffloadHdr = (PNDIS_TASK_OFFLOAD_HEADER) InformationBuffer;

			infolen = sizeof(NDIS_TASK_OFFLOAD_HEADER) +
				FIELD_OFFSET(NDIS_TASK_OFFLOAD, TaskBuffer) +
				sizeof(NDIS_TASK_TCP_IP_CHECKSUM);

			if (infolen > InformationBufferLength) {
				*BytesNeeded = infolen;
				break;
			}

			/* the vnif miniport only support 802.3 encapsulation for now */
			if (pNdisTaskOffloadHdr->EncapsulationFormat.Encapsulation
					!= IEEE_802_3_Encapsulation
				&& pNdisTaskOffloadHdr->EncapsulationFormat.Encapsulation
					!= UNSPECIFIED_Encapsulation) {
				DBG_PRINT(("VNIF: Encapsulation type is not supported.\n"));
				pNdisTaskOffloadHdr->OffsetFirstTask = 0;
				status = NDIS_STATUS_NOT_SUPPORTED;
				break;
			}

			if (pNdisTaskOffloadHdr->Size != sizeof(NDIS_TASK_OFFLOAD_HEADER) ||
				pNdisTaskOffloadHdr->Version != NDIS_TASK_OFFLOAD_VERSION) {
				DBG_PRINT(("VNIF: Size or Version of offload header is incorrect.\n"));
				pNdisTaskOffloadHdr->OffsetFirstTask = 0;
				status = NDIS_STATUS_NOT_SUPPORTED;
				break;
			}

			pNdisTaskOffloadHdr->OffsetFirstTask = pNdisTaskOffloadHdr->Size;
			pTaskOffload = (PNDIS_TASK_OFFLOAD)(
				(PUCHAR)(InformationBuffer) + pNdisTaskOffloadHdr->Size);

			for (i = 0; i < OffloadTasksCount; i++) {
				pTaskOffload->Size = OffloadTasks[i].Size;
				pTaskOffload->Version = OffloadTasks[i].Version;
				pTaskOffload->Task = OffloadTasks[i].Task;
				pTaskOffload->TaskBufferLength =
					OffloadTasks[i].TaskBufferLength;

				/*
				 * There is a description mismatch between DDK and DDK sample code
				 * on OffsetNextTask member of NDIS_TASK_OFFLOAD. We are referring
				 * to the sample code.
				*/
				if (i != OffloadTasksCount - 1) {
					pTaskOffload->OffsetNextTask =
						FIELD_OFFSET(NDIS_TASK_OFFLOAD, TaskBuffer)
						+ pTaskOffload->TaskBufferLength;
				}
				else {
					pTaskOffload->OffsetNextTask = 0;
				}

				switch (OffloadTasks[i].Task) {
					case TcpIpChecksumNdisTask:
						pTcpIpChecksumTask = (PNDIS_TASK_TCP_IP_CHECKSUM)
							pTaskOffload->TaskBuffer;

						NdisMoveMemory(pTcpIpChecksumTask,
							&adapter->hw_chksum_task,
							sizeof(adapter->hw_chksum_task));
						break;
				}
				if (i != OffloadTasksCount) {
					pTaskOffload = (PNDIS_TASK_OFFLOAD)
					((PUCHAR)pTaskOffload + pTaskOffload->OffsetNextTask);
				}
			}

			/* set InformationBuffer directly, override the default handling */
			*BytesWritten = infolen;
			return NDIS_STATUS_SUCCESS;
#endif

		case OID_PNP_QUERY_POWER:
			DPR_INIT(("Query for POWER\n"));
            status = NDIS_STATUS_SUCCESS; 
            break;

		default:
			status = NDIS_STATUS_NOT_SUPPORTED;
			break;
	}

	if(status == NDIS_STATUS_SUCCESS) {
		*BytesNeeded = ulBytesAvailable;
		if(infolen <= InformationBufferLength) {
			// Copy result into InformationBuffer
			*BytesWritten = infolen;
			if(infolen && do_copy) {
				NdisMoveMemory(InformationBuffer, infoptr, infolen);
			}
		}
		else {
			// too short
			*BytesNeeded = infolen;
			status = NDIS_STATUS_BUFFER_TOO_SHORT;
		}
	}

	return status;
}

NDIS_STATUS
MPSetInformation(
  IN NDIS_HANDLE MiniportAdapterContext,
  IN NDIS_OID Oid,
  IN PVOID InformationBuffer,
  IN ULONG InformationBufferLength,
  OUT PULONG BytesRead,
  OUT PULONG BytesNeeded
  )
{
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;

#ifdef NDIS60_MINIPORT
	PNDIS_OFFLOAD_ENCAPSULATION encapsulation;
	PNDIS_OFFLOAD_PARAMETERS offload_parms;
	uint32_t new_chksum_tasks;
	uint32_t chksum_tasks_changed;
#else
	PNDIS_TASK_OFFLOAD_HEADER pNdisTaskOffloadHdr;
	PNDIS_TASK_OFFLOAD pTaskOffload;
	PNDIS_TASK_TCP_IP_CHECKSUM pTcpIpChecksumTask;
	uint32_t new_rx_chksum_tasks;
	uint32_t new_tx_chksum_tasks;
#endif
    NDIS_DEVICE_POWER_STATE NewPowerState;
	UINT i;

	*BytesRead = 0;
	*BytesNeeded = 0;

	DPR_OID(("Set oid %x.\n", Oid));
	switch (Oid) {
		case OID_802_3_MULTICAST_LIST:
			status = VNIFSetMulticastList(
				adapter,
				InformationBuffer,
				InformationBufferLength,
				BytesRead,
				BytesNeeded);
			break;

		case OID_GEN_CURRENT_PACKET_FILTER:
			if (InformationBufferLength != sizeof(ULONG)) {
				*BytesNeeded = sizeof(ULONG);
				status = NDIS_STATUS_INVALID_LENGTH;
				break;
			}

			*BytesRead = InformationBufferLength;

			status = VNIFSetPacketFilter(
				adapter,
				*((PULONG)InformationBuffer));
			break;

		case OID_GEN_CURRENT_LOOKAHEAD:
			if (InformationBufferLength < sizeof(ULONG)) {
				*BytesNeeded = sizeof(ULONG);
				status = NDIS_STATUS_INVALID_LENGTH;
				break;
			}

			if (*(UNALIGNED PULONG)InformationBuffer > ETH_MAX_DATA_SIZE) {
				status = NDIS_STATUS_INVALID_DATA;
				break;
			}
                    
            NdisMoveMemory(&adapter->ulLookAhead, InformationBuffer,
				sizeof(ULONG));
            
            *BytesRead = sizeof(ULONG);
            status = NDIS_STATUS_SUCCESS;
            break;

#ifdef NDIS60_MINIPORT
		case OID_GEN_INTERRUPT_MODERATION:
			// This driver does not support interrupt moderation at this time
			status = NDIS_STATUS_INVALID_DATA;
			break;

		case OID_OFFLOAD_ENCAPSULATION:
			DPR_INIT(("OID_OFFLOAD_ENCAPSULATION %s.\n", adapter->Nodename));
			if (InformationBufferLength < sizeof(NDIS_OFFLOAD_ENCAPSULATION)) {
				*BytesNeeded = sizeof(NDIS_OFFLOAD_ENCAPSULATION);
				status = NDIS_STATUS_INVALID_LENGTH;
				DPR_INIT(("OID_OFFLOAD_ENCAPSULATION: too small %x.\n",
						  InformationBufferLength ));
				break;
			}
			encapsulation = (PNDIS_OFFLOAD_ENCAPSULATION) InformationBuffer;
			if (encapsulation->Header.Type
					!= NDIS_OBJECT_TYPE_OFFLOAD_ENCAPSULATION
				&& encapsulation->Header.Revision
					!= NDIS_OFFLOAD_ENCAPSULATION_REVISION_1
				&& encapsulation->Header.Size
					!= NDIS_SIZEOF_OFFLOAD_ENCAPSULATION_REVISION_1 
				&& encapsulation->IPv4.EncapsulationType
					!= NDIS_ENCAPSULATION_IEEE_802_3 
				) {
				status = NDIS_STATUS_INVALID_DATA;
				DPR_INIT(("OID_OFFLOAD_ENCAPSULATION: bad data.\n"));
				break;
			}
			*BytesRead = sizeof(NDIS_OFFLOAD_ENCAPSULATION);
			/* What else are we to do? */
			VNIFIndicateOffload(adapter);
			DPR_INIT(("OID_OFFLOAD_ENCAPSULATION: BytesRead %x, 4e %x, 4t %x 6e %x 6t %x.\n",
					  *BytesRead,
					  encapsulation->IPv4.Enabled,
					  encapsulation->IPv4.EncapsulationType,
					  encapsulation->IPv6.Enabled,
					  encapsulation->IPv6.EncapsulationType
					  ));
			status = NDIS_STATUS_SUCCESS;
			break;

		case OID_TCP_OFFLOAD_PARAMETERS:
			DPR_INIT(("OID_TCP_OFFLOAD_PARAMETERS %s.\n", adapter->Nodename));
			if (InformationBufferLength < sizeof(NDIS_OFFLOAD_PARAMETERS)) {
				*BytesNeeded = sizeof(NDIS_OFFLOAD_PARAMETERS);
				status = NDIS_STATUS_INVALID_LENGTH;
				DPR_INIT(("OID_TCP_OFFLOAD_PARAMETERS: too small %x.\n",
						  InformationBufferLength ));
				break;
			}
			offload_parms = (PNDIS_OFFLOAD_PARAMETERS) InformationBuffer;
			if (offload_parms->Header.Type
					!= NDIS_OBJECT_TYPE_DEFAULT
				&& ((offload_parms->Header.Revision
							!= NDIS_OFFLOAD_PARAMETERS_REVISION_1
						&& offload_parms->Header.Size
							!= NDIS_SIZEOF_OFFLOAD_PARAMETERS_REVISION_1)
				)) {
				status = NDIS_STATUS_INVALID_DATA;
				DPR_INIT(("OID_TCP_OFFLOAD_PARAMETERS: bad data.\n"));
				break;
			}
			if (offload_parms->LsoV1
				|| offload_parms->IPsecV1
				|| offload_parms->LsoV2IPv4
				|| offload_parms->LsoV2IPv6
				|| offload_parms->TcpConnectionIPv4
				|| offload_parms->TcpConnectionIPv6
				|| offload_parms->TCPIPv6Checksum
				|| offload_parms->UDPIPv6Checksum
				) {
				status = NDIS_STATUS_NOT_SUPPORTED;
				DPR_INIT(("OID_TCP_OFFLOAD_PARAMETERS: not supported.\n"));
				break;
			}

			chksum_tasks_changed = 0;

			/* Check for RX checksum changes. */
			new_chksum_tasks = 0;
			if (checksum_capabilities.IPv4Receive.IpChecksum) {
				if (offload_parms->IPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED
					|| offload_parms->IPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED)
					new_chksum_tasks |= VNIF_CHKSUM_IPV4_IP;
			}

			if (checksum_capabilities.IPv4Receive.TcpChecksum) {
				if (offload_parms->TCPIPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED
					|| offload_parms->TCPIPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED)
					new_chksum_tasks |= VNIF_CHKSUM_IPV4_TCP;
			}

			if (checksum_capabilities.IPv4Receive.UdpChecksum) {
				if (offload_parms->UDPIPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED
					|| offload_parms->UDPIPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED)
					new_chksum_tasks |= VNIF_CHKSUM_IPV4_UDP;
			}

			if (adapter->cur_rx_tasks != new_chksum_tasks) {
				adapter->cur_rx_tasks = new_chksum_tasks;
				chksum_tasks_changed = 1;
			}

			/* Check for TX checksum changes. */
			new_chksum_tasks = 0;
			if (checksum_capabilities.IPv4Transmit.IpChecksum) {
				if (offload_parms->IPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED
					|| offload_parms->IPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED)
				new_chksum_tasks |= VNIF_CHKSUM_IPV4_IP;
			}

			if (checksum_capabilities.IPv4Transmit.TcpChecksum) {
				if (offload_parms->TCPIPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED
					|| offload_parms->TCPIPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED)
					new_chksum_tasks |= VNIF_CHKSUM_IPV4_TCP;
			}

			if (checksum_capabilities.IPv4Transmit.UdpChecksum) {
				if (offload_parms->UDPIPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED
					|| offload_parms->UDPIPv4Checksum == 
						NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED)
					new_chksum_tasks |= VNIF_CHKSUM_IPV4_UDP;
			}

			if (adapter->cur_tx_tasks != new_chksum_tasks) {
				adapter->cur_tx_tasks = new_chksum_tasks;
				chksum_tasks_changed = 1;
			}

			if (chksum_tasks_changed) {
				VNIFIndicateOffload(adapter);
			}
			*BytesRead = sizeof(NDIS_OFFLOAD_PARAMETERS);
			DPR_INIT(("OID_TCP_OFFLOAD_PARAMETERS: BytesRead %x.\n",*BytesRead));
			break;
#else
		case OID_TCP_TASK_OFFLOAD:

			if (InformationBufferLength < sizeof(NDIS_TASK_OFFLOAD_HEADER)) {
				status = NDIS_STATUS_INVALID_LENGTH;
				break;
			}

			*BytesRead = sizeof(NDIS_TASK_OFFLOAD_HEADER);

			pNdisTaskOffloadHdr = (PNDIS_TASK_OFFLOAD_HEADER) InformationBuffer;
			if (pNdisTaskOffloadHdr->EncapsulationFormat.Encapsulation
					!= IEEE_802_3_Encapsulation
				&& pNdisTaskOffloadHdr->EncapsulationFormat.Encapsulation
					!= UNSPECIFIED_Encapsulation) {
				pNdisTaskOffloadHdr->OffsetFirstTask = 0;
				status = NDIS_STATUS_INVALID_DATA;
				break;
			}

			if (pNdisTaskOffloadHdr->OffsetFirstTask == 0) {
				DBG_PRINT(("OID_TCP_TASK_OFFLOAD OffsetFistTask==0, clear\n"));
				adapter->cur_rx_tasks = 0;
				adapter->cur_tx_tasks = 0;
				status = NDIS_STATUS_SUCCESS;
				break;
			}

			/* sanity checks */
			if (pNdisTaskOffloadHdr->OffsetFirstTask < pNdisTaskOffloadHdr->Size) {
				pNdisTaskOffloadHdr->OffsetFirstTask = 0;
				status = NDIS_STATUS_FAILURE;
				break;
			}

			DPR_INIT(("OID_TCP_TASK_OFFLOAD: InfoBufferLength %x, %x, %x.\n",
				InformationBufferLength, pNdisTaskOffloadHdr->OffsetFirstTask,
				sizeof(NDIS_TASK_OFFLOAD)));
			if (InformationBufferLength < (pNdisTaskOffloadHdr->OffsetFirstTask
					+ sizeof(NDIS_TASK_OFFLOAD))) {
				DPR_INIT(("OID_TCP_TASK_OFFLOAD: InfoBufLength too small rx %x tx %x.\n",
						   adapter->cur_rx_tasks, adapter->cur_tx_tasks));
				status = NDIS_STATUS_INVALID_LENGTH;
				break;
			}

			NdisMoveMemory(&adapter->EncapsulationFormat,
				&pNdisTaskOffloadHdr->EncapsulationFormat,
				sizeof(NDIS_ENCAPSULATION_FORMAT));

			ASSERT(pNdisTaskOffloadHdr->EncapsulationFormat.Flags.FixedHeaderSize == 1);

			pTaskOffload = (PNDIS_TASK_OFFLOAD)((PUCHAR)pNdisTaskOffloadHdr
				+ pNdisTaskOffloadHdr->OffsetFirstTask);

			new_tx_chksum_tasks = 0;
			new_rx_chksum_tasks = 0;
			while (pTaskOffload) {
				*BytesRead += FIELD_OFFSET(NDIS_TASK_OFFLOAD, TaskBuffer);

				switch(pTaskOffload->Task) {
					case TcpIpChecksumNdisTask:
						if (InformationBufferLength < *BytesRead
								+ sizeof(NDIS_TASK_TCP_IP_CHECKSUM)) {
							*BytesNeeded = *BytesRead
								+ sizeof(NDIS_TASK_TCP_IP_CHECKSUM);
							status = NDIS_STATUS_INVALID_LENGTH;
							break;
						}

						for (i = 0; i < OffloadTasksCount; i++) {
							if (OffloadTasks[i].Task == pTaskOffload->Task &&
								OffloadTasks[i].Version == pTaskOffload->Version)
								break;
						}

						if (i == OffloadTasksCount) {
							DPR_INIT(("OID_TCP_TASK_OFFLOAD: i not supported.\n"));
							status = NDIS_STATUS_NOT_SUPPORTED;
							break;
						}

						/* TODO: set Tcp/Ip checksum related field in Adapter */

						pTcpIpChecksumTask = (PNDIS_TASK_TCP_IP_CHECKSUM)
							pTaskOffload->TaskBuffer;

						if (pTcpIpChecksumTask->V4Transmit.IpOptionsSupported) 
						{   
							if (adapter->hw_chksum_task.V4Transmit.IpOptionsSupported
									== 0) {
								DPR_INIT(("OID_TCP_TASK_OFFLOAD: tx op IP NS.\n"));
								return NDIS_STATUS_NOT_SUPPORTED;
							}
						}

						if (pTcpIpChecksumTask->V4Transmit.TcpOptionsSupported) 
						{   
							if (adapter->hw_chksum_task.V4Transmit.TcpOptionsSupported
									== 0) {
								DPR_INIT(("OID_TCP_TASK_OFFLOAD: tx op TCP NS.\n"));
								return NDIS_STATUS_NOT_SUPPORTED;
							}
						}

						/* IPV4 Transmit */
						if (pTcpIpChecksumTask->V4Transmit.TcpChecksum) 
						{   
							if (adapter->hw_chksum_task.V4Transmit.TcpChecksum == 0) {
								DPR_INIT(("OID_TCP_TASK_OFFLOAD: tx TCP NS %x.\n",
									adapter->cur_tx_tasks));
								return NDIS_STATUS_NOT_SUPPORTED;
							}
							new_tx_chksum_tasks |= VNIF_CHKSUM_IPV4_TCP;
						}

						if (pTcpIpChecksumTask->V4Transmit.IpChecksum) 
						{
							if (adapter->hw_chksum_task.V4Transmit.IpChecksum == 0) {
								DPR_INIT(("OID_TCP_TASK_OFFLOAD: tx ip NS.\n"));
								return NDIS_STATUS_NOT_SUPPORTED;
							}
							new_tx_chksum_tasks |= VNIF_CHKSUM_IPV4_IP;
						}

						if (pTcpIpChecksumTask->V4Transmit.UdpChecksum) 
						{
							if (adapter->hw_chksum_task.V4Transmit.UdpChecksum == 0) {
								DPR_INIT(("OID_TCP_TASK_OFFLOAD: tx UDP NS.\n"));
								return NDIS_STATUS_NOT_SUPPORTED;
							}
							new_tx_chksum_tasks |= VNIF_CHKSUM_IPV4_UDP;
						}

						/* IPV4 Receive */
						if (pTcpIpChecksumTask->V4Receive.TcpChecksum)
						{
							if (adapter->hw_chksum_task.V4Receive.TcpChecksum == 0) {
								DPR_INIT(("OID_TCP_TASK_OFFLOAD: rx TCP NS %x.\n",
									adapter->cur_rx_tasks));
								return NDIS_STATUS_NOT_SUPPORTED;
							}
							DPR_INIT(("OID_TCP_TASK_OFFLOAD: rx TCP %x.\n",
								adapter->hw_chksum_task.V4Receive.TcpChecksum));
							new_rx_chksum_tasks |= VNIF_CHKSUM_IPV4_TCP;
						}

						if (pTcpIpChecksumTask->V4Receive.IpChecksum)
						{
							if (adapter->hw_chksum_task.V4Receive.IpChecksum == 0) {
								DPR_INIT(("OID_TCP_TASK_OFFLOAD: rx IP NS.\n"));
								return NDIS_STATUS_NOT_SUPPORTED;
							}
							new_rx_chksum_tasks |= VNIF_CHKSUM_IPV4_IP;
						}

						if (pTcpIpChecksumTask->V4Receive.UdpChecksum)
						{
							if (adapter->hw_chksum_task.V4Receive.UdpChecksum == 0) {
								DPR_INIT(("OID_TCP_TASK_OFFLOAD: rx UDP NS.\n"));
								return NDIS_STATUS_NOT_SUPPORTED;
							}
							new_rx_chksum_tasks |= VNIF_CHKSUM_IPV4_UDP;
						}

						/* We don't do IPv6 so just return failure. */
						if (pTcpIpChecksumTask->V6Transmit.TcpChecksum
							|| pTcpIpChecksumTask->V6Transmit.UdpChecksum
							|| pTcpIpChecksumTask->V6Receive.TcpChecksum
							|| pTcpIpChecksumTask->V6Receive.UdpChecksum) {
							DPR_INIT(("OID_TCP_TASK_OFFLOAD: V6 NS.\n"));
							return NDIS_STATUS_NOT_SUPPORTED;
						}

						*BytesRead += sizeof(NDIS_TASK_TCP_IP_CHECKSUM);
						status = NDIS_STATUS_SUCCESS;
						break;

					default:
						DPR_INIT(("OID_TCP_TASK_OFFLOAD: default NS.\n"));
						status = NDIS_STATUS_NOT_SUPPORTED;
						break;
				} /* switch Task */

				if (status != NDIS_STATUS_SUCCESS)
					break;

				if (pTaskOffload->OffsetNextTask) {
					pTaskOffload = (PNDIS_TASK_OFFLOAD)((PUCHAR)
						pTaskOffload + pTaskOffload->OffsetNextTask);
				}
				else {
					pTaskOffload = NULL;
				}
			} /* while pTaskOffload */
			adapter->cur_rx_tasks = new_rx_chksum_tasks;
			adapter->cur_tx_tasks = new_tx_chksum_tasks;
			DPR_INIT(("OID: cur rx %d, cur tx %d.\n",
					  adapter->cur_rx_tasks,
					  adapter->cur_tx_tasks));
#endif

			break;

        case OID_PNP_SET_POWER:
			DPR_INIT(("Set for POWER\n"));
            if (InformationBufferLength != sizeof(NDIS_DEVICE_POWER_STATE ))
            {
				*BytesNeeded = sizeof(NDIS_STATUS_INVALID_LENGTH);
                return(NDIS_STATUS_INVALID_LENGTH);
            }

            NewPowerState = *(PNDIS_DEVICE_POWER_STATE UNALIGNED)InformationBuffer;
            *BytesRead = sizeof(NDIS_DEVICE_POWER_STATE);
            status = NDIS_STATUS_SUCCESS; 
            break;


		default:
			status = NDIS_STATUS_NOT_SUPPORTED;
			break;
	}

	return status;
}

ULONG
VNIFGetMediaConnectStatus(
  PVNIF_ADAPTER Adapter
  )
{
    if (VNIF_TEST_FLAG(Adapter, VNF_ADAPTER_NO_LINK)) {
        return NdisMediaStateDisconnected;
    }
    else {
        return NdisMediaStateConnected;
    }
}

NDIS_STATUS
VNIFSetPacketFilter(PVNIF_ADAPTER adapter, ULONG PacketFilter)
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    if (PacketFilter & ~VNIF_SUPPORTED_FILTERS) {
		DPR_INIT(("VNIFSetPacketFilter: trying to set an invalid filter %x.\n",
			PacketFilter));
        return NDIS_STATUS_NOT_SUPPORTED;
    }

    if (PacketFilter != adapter->PacketFilter) {
        /* TODO: change the filtering modes on `hardware' */
		DPR_INIT(("VNIFSetPacketFilter: new filter %x.\n", PacketFilter));
        adapter->PacketFilter = PacketFilter;
    }

    return status;
}

NDIS_STATUS
VNIFSetMulticastList(
  IN PVNIF_ADAPTER adapter,
  IN PVOID InformationBuffer,
  IN ULONG InformationBufferLength,
  OUT PULONG BytesRead,
  OUT PULONG BytesNeeded
  )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    ULONG i;

    *BytesNeeded = ETH_LENGTH_OF_ADDRESS;
    *BytesRead = InformationBufferLength;

    if (InformationBufferLength % ETH_LENGTH_OF_ADDRESS) {
        return NDIS_STATUS_INVALID_LENGTH;
    }

    if (InformationBufferLength > (VNIF_MAX_MCAST_LIST * ETH_LENGTH_OF_ADDRESS)) {
        *BytesNeeded = VNIF_MAX_MCAST_LIST * ETH_LENGTH_OF_ADDRESS;
        return NDIS_STATUS_MULTICAST_FULL;;
    }

    /* XXX: we should add a spin lock here for MP safe */
    NdisZeroMemory(adapter->MCList,
                   VNIF_MAX_MCAST_LIST * ETH_LENGTH_OF_ADDRESS);

    NdisMoveMemory(adapter->MCList,
                   InformationBuffer,
                   InformationBufferLength);

    adapter->ulMCListSize = InformationBufferLength / ETH_LENGTH_OF_ADDRESS;

	/*
    for (i = 0; i < adapter->ulMCListSize; i++)
    {
        DPR_INIT(("MC(%d) = %02x-%02x-%02x-%02x-%02x-%02x\n", 
            i,
            adapter->MCList[i][0],
            adapter->MCList[i][1],
            adapter->MCList[i][2],
            adapter->MCList[i][3],
            adapter->MCList[i][4],
            adapter->MCList[i][5]));
	}
	*/

    /* TODO: program the hardware to add multicast addresses */
    return NDIS_STATUS_SUCCESS;
}

#ifdef NDIS60_MINIPORT

#define     ADD_TWO_INTEGERS        1
#define     MINUS_TWO_INTEGERS      2

NDIS_STATUS
MPMethodRequest(PVNIF_ADAPTER adapter, PNDIS_OID_REQUEST Request)
{

	NDIS_OID Oid;
	ULONG  MethodId;
	PVOID InformationBuffer;
	ULONG InputBufferLength;
	ULONG OutputBufferLength;
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	ULONG FirstInt;
	ULONG SecondInt;
	ULONG BytesNeeded = 0;
	ULONG BytesWritten = 0;
	ULONG BytesRead = 0;

	Oid = Request->DATA.METHOD_INFORMATION.Oid;
	InformationBuffer = (PVOID)
		(Request->DATA.METHOD_INFORMATION.InformationBuffer);
	InputBufferLength = Request->DATA.METHOD_INFORMATION.InputBufferLength;
	OutputBufferLength = Request->DATA.METHOD_INFORMATION.OutputBufferLength;
	MethodId = Request->DATA.METHOD_INFORMATION.MethodId;//Only for method oids

	BytesNeeded = 0;
    
#if 0
	switch (Oid) {
		case OID_CUSTOM_METHOD:
			switch(MethodId) {
				case ADD_TWO_INTEGERS:
					if (InputBufferLength < 2 * sizeof(ULONG)) {
						Status = NDIS_STATUS_INVALID_DATA;
						break;
					}
					FirstInt = *((PULONG UNALIGNED)InformationBuffer);
					SecondInt = *((PULONG)((PULONG UNALIGNED)
						InformationBuffer + 1));

					BytesRead = 2 * sizeof(ULONG);
                    
					BytesNeeded = sizeof(ULONG);
					if (OutputBufferLength < BytesNeeded) {
						Status = NDIS_STATUS_BUFFER_TOO_SHORT;
						break;
					}
                    
					*((PULONG UNALIGNED)InformationBuffer) =
						FirstInt + SecondInt;
					BytesWritten = sizeof(ULONG);
					break;
                
				case MINUS_TWO_INTEGERS:
					if (InputBufferLength < 2 * sizeof(ULONG)) {
						Status = NDIS_STATUS_INVALID_DATA;
						break;
					}
					FirstInt = *((PULONG UNALIGNED)InformationBuffer);
					SecondInt = *((PULONG)((PULONG UNALIGNED)
						InformationBuffer + 1));

					BytesRead = 2 * sizeof(ULONG);
                    
					BytesNeeded = sizeof(ULONG);
					if (OutputBufferLength < BytesNeeded) {
						Status = NDIS_STATUS_BUFFER_TOO_SHORT;
						break;
					}
					if (FirstInt < SecondInt) {
						Status = NDIS_STATUS_INVALID_DATA;
						break;
					}
					
					*((PULONG UNALIGNED)InformationBuffer) =
						FirstInt - SecondInt;
					BytesWritten = sizeof(ULONG);
					break;
            
				default:
					Status = NDIS_STATUS_NOT_SUPPORTED;
					break;
			}
			break;
            
		default:
			Status = NDIS_STATUS_NOT_SUPPORTED;
			break;
	}

	if (Status != NDIS_STATUS_SUCCESS) {
		Request->DATA.METHOD_INFORMATION.BytesNeeded = BytesNeeded;
	}
	else {
		Request->DATA.METHOD_INFORMATION.BytesWritten = BytesWritten;
		Request->DATA.METHOD_INFORMATION.BytesRead = BytesRead;
	}
	return Status;
#endif
	Request->DATA.METHOD_INFORMATION.BytesNeeded = BytesNeeded;
	return NDIS_STATUS_NOT_SUPPORTED;
            
}

NDIS_STATUS 
MPOidRequest(IN NDIS_HANDLE  MiniportAdapterContext,
	IN  PNDIS_OID_REQUEST  NdisRequest)

{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;
	NDIS_STATUS status;

	DPR_OID(("--> MPOidRequest\n"));
	NdisAcquireSpinLock(&adapter->Lock);

	if (VNIF_TEST_FLAG(adapter, VNF_RESET_IN_PROGRESS)) {//| fMP_ADAPTER_REMOVE_IN_PROGRESS)))
		NdisReleaseSpinLock(&adapter->Lock);
		return NDIS_STATUS_REQUEST_ABORTED;
	}

	NdisReleaseSpinLock(&adapter->Lock);    
    
	switch (NdisRequest->RequestType) {
		case NdisRequestMethod:
			status = MPMethodRequest(adapter, NdisRequest);
			break;

		case NdisRequestSetInformation:            
			status = MPSetInformation(MiniportAdapterContext,
				NdisRequest->DATA.SET_INFORMATION.Oid,
				NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
				NdisRequest->DATA.SET_INFORMATION.InformationBufferLength,
				&NdisRequest->DATA.SET_INFORMATION.BytesRead,
				&NdisRequest->DATA.SET_INFORMATION.BytesNeeded);
			break;
                
		case NdisRequestQueryInformation:
		case NdisRequestQueryStatistics:
			status = MPQueryInformation(MiniportAdapterContext,
				NdisRequest->DATA.QUERY_INFORMATION.Oid,
				NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
				NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
				&NdisRequest->DATA.QUERY_INFORMATION.BytesWritten,
				&NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded);
			break;

		default:
			status = NDIS_STATUS_NOT_SUPPORTED;
			break;
	}

	DPR_OID(("<-- MPOidRequest: OID %x type %d, bytes written %x, bytes needed %x, status %x\n",
		NdisRequest->DATA.QUERY_INFORMATION.Oid,
		NdisRequest->RequestType,
		NdisRequest->DATA.QUERY_INFORMATION.BytesWritten,
		NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded,
		status));
	return status;
}

VOID
MPCancelOidRequest(IN NDIS_HANDLE MiniportAdapterContext, IN PVOID RequestId)
{
	PNDIS_OID_REQUEST PendingRequest;
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;

#if 0
	NdisAcquireSpinLock(&adapter->Lock);
    
	if (adapter->PendingRequest != NULL 
		&& adapter->PendingRequest->RequestId == RequestId)
	{
		PendingRequest = adapter->PendingRequest;
		adapter->PendingRequest = NULL;

		NdisReleaseSpinLock(&adapter->Lock);
        
		NdisMOidRequestComplete(adapter->AdapterHandle, 
			PendingRequest, 
			NDIS_STATUS_REQUEST_ABORTED);

	} 
	else
	{
		NdisReleaseSpinLock(&adapter->Lock);
	}
#endif
}

#if 0
void
VNIFIndicateOffload(PVNIF_ADAPTER adapter)
{
    
	NDIS_OFFLOAD            offload;
	NDIS_STATUS_INDICATION  status_indication;

	DPR_INIT(("VNIFIndicateOffload: tx %x, rx %x\n",
			  adapter->cur_tx_tasks, adapter->cur_rx_tasks));
	NdisZeroMemory(&offload, sizeof(NDIS_OFFLOAD));
	NdisZeroMemory(&status_indication, sizeof(NDIS_STATUS_INDICATION));

	offload.Header.Type = NDIS_OBJECT_TYPE_OFFLOAD; 
	offload.Header.Revision = NDIS_OFFLOAD_REVISION_1;
	offload.Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_1;

	offload.Checksum.IPv4Transmit.Encapsulation =
		checksum_capabilities.IPv4Transmit.Encapsulation;

	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_IP) {
		DPR_INIT(("VNIFIndicateOffload: TX IpOpt and sum ON\n"));
		offload.Checksum.IPv4Transmit.IpOptionsSupported = NDIS_OFFLOAD_SET_ON;
		offload.Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: TX IpOpt and sum OFF\n"));
		offload.Checksum.IPv4Transmit.IpOptionsSupported = NDIS_OFFLOAD_SET_OFF;
		offload.Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	if (adapter->cur_tx_tasks & (VNIF_CHKSUM_IPV4_TCP | VNIF_CHKSUM_IPV4_UDP)) {
		DPR_INIT(("VNIFIndicateOffload: TX TCPOpt ON\n"));
		offload.Checksum.IPv4Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: TX TCPOpt OFF\n"));
		offload.Checksum.IPv4Transmit.TcpOptionsSupported =NDIS_OFFLOAD_SET_OFF;
	}
	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_TCP) {
		DPR_INIT(("VNIFIndicateOffload: TX TCP checksum ON\n"));
		offload.Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: TX TCP checksum OFF\n"));
		offload.Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP) {
		DPR_INIT(("VNIFIndicateOffload: TX UDP checksum ON\n"));
		offload.Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: TX UDP checksum OFF\n"));
		offload.Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SET_OFF;
	}
	

	offload.Checksum.IPv4Receive.Encapsulation =
		checksum_capabilities.IPv4Receive.Encapsulation;

	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_IP) {
		DPR_INIT(("VNIFIndicateOffload: RX IpOpt and sum ON\n"));
		offload.Checksum.IPv4Receive.IpOptionsSupported = NDIS_OFFLOAD_SET_ON;
		offload.Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: RX IpOpt and sum OFF\n"));
		offload.Checksum.IPv4Receive.IpOptionsSupported = NDIS_OFFLOAD_SET_OFF;
		offload.Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	if (adapter->cur_rx_tasks & (VNIF_CHKSUM_IPV4_TCP | VNIF_CHKSUM_IPV4_UDP)) {
		DPR_INIT(("VNIFIndicateOffload: RX TCPOpt ON\n"));
		offload.Checksum.IPv4Receive.TcpOptionsSupported = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: RX TCPOpt OFF\n"));
		offload.Checksum.IPv4Receive.TcpOptionsSupported =NDIS_OFFLOAD_SET_OFF;
	}
	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_TCP) {
		DPR_INIT(("VNIFIndicateOffload: RX TCP checksum ON\n"));
		offload.Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: RX TCP checksum OFF\n"));
		offload.Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_UDP) {
		DPR_INIT(("VNIFIndicateOffload: RX UDP checksum ON\n"));
		offload.Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: RX UDP checksum OFF\n"));
		offload.Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	status_indication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
	status_indication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
	status_indication.Header.Size = sizeof(NDIS_STATUS_INDICATION);
	status_indication.SourceHandle = adapter->AdapterHandle;
	status_indication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
	status_indication.StatusBuffer = &offload;
	status_indication.StatusBufferSize = sizeof(offload);
	//status_indication.RequestId = OID_TCP_OFFLOAD_PARAMETERS;
	
	DPR_INIT(("VNIFIndicateOffload:NdisMIndicateStatusEx\n"));
	NdisMIndicateStatusEx(adapter->AdapterHandle, &status_indication);
	DPR_INIT(("VNIFIndicateOffload: out\n"));
}
#endif
#if 0
void
VNIFIndicateOffload(PVNIF_ADAPTER adapter)
{
    
	NDIS_OFFLOAD            offload;
	NDIS_STATUS_INDICATION  status_indication;

	DPR_INIT(("VNIFIndicateOffload: tx %x, rx %x\n",
			  adapter->cur_tx_tasks, adapter->cur_rx_tasks));
	NdisZeroMemory(&offload, sizeof(NDIS_OFFLOAD));
	NdisZeroMemory(&status_indication, sizeof(NDIS_STATUS_INDICATION));

	offload.Header.Type = NDIS_OBJECT_TYPE_OFFLOAD; 
	offload.Header.Revision = NDIS_OFFLOAD_REVISION_1;
	offload.Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_1;

	offload.Checksum.IPv4Transmit.Encapsulation =
		checksum_capabilities.IPv4Transmit.Encapsulation;
	offload.Checksum.IPv4Transmit.IpOptionsSupported =
		checksum_capabilities.IPv4Transmit.IpOptionsSupported;
	offload.Checksum.IPv4Transmit.TcpOptionsSupported =
		checksum_capabilities.IPv4Transmit.TcpOptionsSupported;

	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_IP) {
		DPR_INIT(("VNIFIndicateOffload: TX Ip sum ON\n"));
		offload.Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: TX Ip sum OFF\n"));
		offload.Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_TCP) {
		DPR_INIT(("VNIFIndicateOffload: TX TCP checksum ON\n"));
		offload.Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: TX TCP checksum OFF\n"));
		offload.Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP) {
		DPR_INIT(("VNIFIndicateOffload: TX UDP checksum ON\n"));
		offload.Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: TX UDP checksum OFF\n"));
		offload.Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SET_OFF;
	}
	

	offload.Checksum.IPv4Receive.Encapsulation =
		checksum_capabilities.IPv4Receive.Encapsulation;
	offload.Checksum.IPv4Receive.IpOptionsSupported =
		checksum_capabilities.IPv4Receive.IpOptionsSupported;
	offload.Checksum.IPv4Receive.TcpOptionsSupported =
		checksum_capabilities.IPv4Receive.TcpOptionsSupported;

	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_IP) {
		DPR_INIT(("VNIFIndicateOffload: RX Ip sum ON\n"));
		offload.Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: RX Ip sum OFF\n"));
		offload.Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_TCP) {
		DPR_INIT(("VNIFIndicateOffload: RX TCP checksum ON\n"));
		offload.Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: RX TCP checksum OFF\n"));
		offload.Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_UDP) {
		DPR_INIT(("VNIFIndicateOffload: RX UDP checksum ON\n"));
		offload.Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SET_ON;
	}
	else {
		DPR_INIT(("VNIFIndicateOffload: RX UDP checksum OFF\n"));
		offload.Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SET_OFF;
	}

	status_indication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
	status_indication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
	status_indication.Header.Size = sizeof(NDIS_STATUS_INDICATION);
	status_indication.SourceHandle = adapter->AdapterHandle;
	status_indication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
	status_indication.StatusBuffer = &offload;
	status_indication.StatusBufferSize = sizeof(offload);
	//status_indication.RequestId = OID_TCP_OFFLOAD_PARAMETERS;
	
	DPR_INIT(("VNIFIndicateOffload:NdisMIndicateStatusEx\n"));
	NdisMIndicateStatusEx(adapter->AdapterHandle, &status_indication);
	DPR_INIT(("VNIFIndicateOffload: out\n"));
}
#endif

void
VNIFIndicateOffload(PVNIF_ADAPTER adapter)
{
    
	NDIS_OFFLOAD            offload;
	NDIS_STATUS_INDICATION  status_indication;

	DPR_INIT(("VNIFIndicateOffload %s: tx %x, rx %x\n", adapter->Nodename,
			  adapter->cur_tx_tasks, adapter->cur_rx_tasks));
	NdisZeroMemory(&offload, sizeof(NDIS_OFFLOAD));
	NdisZeroMemory(&status_indication, sizeof(NDIS_STATUS_INDICATION));

	offload.Header.Type = NDIS_OBJECT_TYPE_OFFLOAD; 
	offload.Header.Revision = NDIS_OFFLOAD_REVISION_1;
	offload.Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_1;

	offload.Checksum.IPv4Transmit.Encapsulation =
		checksum_capabilities.IPv4Transmit.Encapsulation;
	offload.Checksum.IPv4Transmit.IpOptionsSupported =
		checksum_capabilities.IPv4Transmit.IpOptionsSupported;
	offload.Checksum.IPv4Transmit.TcpOptionsSupported =
		checksum_capabilities.IPv4Transmit.TcpOptionsSupported;
	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_IP)
		offload.Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_TCP)
		offload.Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP)
		offload.Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
	

	offload.Checksum.IPv4Receive.Encapsulation =
		checksum_capabilities.IPv4Receive.Encapsulation;
	offload.Checksum.IPv4Receive.IpOptionsSupported =
		checksum_capabilities.IPv4Receive.IpOptionsSupported;
	offload.Checksum.IPv4Receive.TcpOptionsSupported =
		checksum_capabilities.IPv4Receive.TcpOptionsSupported;
	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_IP)
		offload.Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_TCP)
		offload.Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_UDP)
		offload.Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;

	status_indication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
	status_indication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
	status_indication.Header.Size = sizeof(NDIS_STATUS_INDICATION);
	status_indication.SourceHandle = adapter->AdapterHandle;
	status_indication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
	status_indication.StatusBuffer = &offload;
	status_indication.StatusBufferSize = sizeof(offload);
	//status_indication.RequestId = OID_TCP_OFFLOAD_PARAMETERS;
	
	DPR_INIT(("VNIFIndicateOffload:NdisMIndicateStatusEx\n"));
	NdisMIndicateStatusEx(adapter->AdapterHandle, &status_indication);
	DPR_INIT(("VNIFIndicateOffload: out\n"));
}

void
VNIFOidRequestComplete(PVNIF_ADAPTER adapter)
{
	NdisMOidRequestComplete(adapter->AdapterHandle,
		adapter->NdisRequest, NDIS_STATUS_SUCCESS);
	adapter->NdisRequest = NULL;
}
#else
void
VNIFOidRequestComplete(PVNIF_ADAPTER adapter)
{
	KIRQL old_irql;

	KeRaiseIrql(DISPATCH_LEVEL, &old_irql);
	NdisMQueryInformationComplete(adapter->AdapterHandle,
		NDIS_STATUS_SUCCESS);
	KeLowerIrql(old_irql);
}
#endif

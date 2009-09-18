/*****************************************************************************
 *
 *   File Name:      init6.c
 *
 *   %version:       12 %
 *   %derived_by:    kallan %
 *   %date_modified: Tue Jul 28 11:49:19 2009 %
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

static NDIS_STATUS
VNIFSetRegistrationAttributes(PVNIF_ADAPTER adapter)
{
	NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES RegistrationAttributes;
	NDIS_STATUS status;

	NdisZeroMemory(&RegistrationAttributes,
		sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES));

	RegistrationAttributes.Header.Type =
		NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
	RegistrationAttributes.Header.Revision =
		NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
	RegistrationAttributes.Header.Size =
		sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES);

	RegistrationAttributes.MiniportAdapterContext = (NDIS_HANDLE)adapter;
	RegistrationAttributes.AttributeFlags =
		NDIS_MINIPORT_ATTRIBUTES_BUS_MASTER;
		//NDIS_MINIPORT_ATTRIBUTES_BUS_MASTER |
		//NDIS_MINIPORT_ATTRIBUTES_HARDWARE_DEVICE;

	RegistrationAttributes.CheckForHangTimeInSeconds = 0;
	RegistrationAttributes.InterfaceType = NdisInterfaceInternal;
	//RegistrationAttributes.InterfaceType = NdisInterfacePNPBus;
	//RegistrationAttributes.InterfaceType = NdisInterfacePci;

	status = NdisMSetMiniportAttributes(adapter->AdapterHandle,
		(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&RegistrationAttributes);
	DPR_INIT(("RegistrationAttributes status %x\n", status));
	return status;
}

static NDIS_STATUS
VNIFSetGeneralAttributes(PVNIF_ADAPTER adapter)
{
	NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES GeneralAttributes;
	//NDIS_PNP_CAPABILITIES pmc;
	NDIS_STATUS status;

	NdisZeroMemory(&GeneralAttributes,
		sizeof(NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES));

	GeneralAttributes.Header.Type =
		NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
	GeneralAttributes.Header.Revision =
		NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
	GeneralAttributes.Header.Size =
		sizeof(NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES);

	GeneralAttributes.MediaType = NdisMedium802_3;

	GeneralAttributes.MtuSize = ETH_MAX_DATA_SIZE;
	GeneralAttributes.MaxXmitLinkSpeed = VNIF_MEDIA_MAX_SPEED;
	GeneralAttributes.MaxRcvLinkSpeed = VNIF_MEDIA_MAX_SPEED;
	GeneralAttributes.XmitLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
	GeneralAttributes.RcvLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
	GeneralAttributes.MediaConnectState = MediaConnectStateUnknown;
	GeneralAttributes.MediaDuplexState = MediaDuplexStateUnknown;
	GeneralAttributes.LookaheadSize = ETH_MAX_DATA_SIZE;

	/*
	pmc.Flags = NDIS_DEVICE_WAKE_UP_ENABLE;
	pmc.WakeUpCapabilities.MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
	pmc.WakeUpCapabilities.MinPatternWakeUp = NdisDeviceStateUnspecified;
	pmc.WakeUpCapabilities.MinLinkChangeWakeUp  = NdisDeviceStateUnspecified;
	GeneralAttributes.PowerManagementCapabilities = &pmc;
	*/
	GeneralAttributes.PowerManagementCapabilities = NULL;

	GeneralAttributes.MacOptions = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | 
		NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
		NDIS_MAC_OPTION_8021P_PRIORITY |
		NDIS_MAC_OPTION_NO_LOOPBACK;

	GeneralAttributes.SupportedPacketFilters = NDIS_PACKET_TYPE_DIRECTED |
		NDIS_PACKET_TYPE_MULTICAST |
		NDIS_PACKET_TYPE_ALL_MULTICAST |
		NDIS_PACKET_TYPE_BROADCAST;

	GeneralAttributes.MaxMulticastListSize = VNIF_MAX_MCAST_LIST;
	GeneralAttributes.MacAddressLength = ETH_LENGTH_OF_ADDRESS;
	NdisMoveMemory(GeneralAttributes.PermanentMacAddress,
		adapter->PermanentAddress,
		ETH_LENGTH_OF_ADDRESS);

	NdisMoveMemory(GeneralAttributes.CurrentMacAddress,
		adapter->CurrentAddress,
		ETH_LENGTH_OF_ADDRESS);

	/* Must be NdisPhysicalMedium802_3 to pass WHQL test. */
	GeneralAttributes.PhysicalMediumType = NdisPhysicalMedium802_3;

	GeneralAttributes.RecvScaleCapabilities = NULL;
	GeneralAttributes.AccessType = NET_IF_ACCESS_BROADCAST;
	GeneralAttributes.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
	GeneralAttributes.ConnectionType = NET_IF_CONNECTION_DEDICATED;
	GeneralAttributes.IfType = IF_TYPE_ETHERNET_CSMACD;
	GeneralAttributes.IfConnectorPresent = TRUE; // RFC 2665 TRUE if physical adapter

	GeneralAttributes.SupportedStatistics =
		NDIS_STATISTICS_DIRECTED_FRAMES_RCV_SUPPORTED |
		NDIS_STATISTICS_MULTICAST_FRAMES_RCV_SUPPORTED |
		NDIS_STATISTICS_BROADCAST_FRAMES_RCV_SUPPORTED |
		NDIS_STATISTICS_BYTES_RCV_SUPPORTED |
		NDIS_STATISTICS_RCV_DISCARDS_SUPPORTED |
		NDIS_STATISTICS_RCV_ERROR_SUPPORTED |
		NDIS_STATISTICS_DIRECTED_FRAMES_XMIT_SUPPORTED |
		NDIS_STATISTICS_MULTICAST_FRAMES_XMIT_SUPPORTED |
		NDIS_STATISTICS_BROADCAST_FRAMES_XMIT_SUPPORTED |
		NDIS_STATISTICS_BYTES_XMIT_SUPPORTED |
		NDIS_STATISTICS_XMIT_ERROR_SUPPORTED |
		NDIS_STATISTICS_XMIT_DISCARDS_SUPPORTED |
		NDIS_STATISTICS_DIRECTED_BYTES_RCV_SUPPORTED |
		NDIS_STATISTICS_MULTICAST_BYTES_RCV_SUPPORTED |
		NDIS_STATISTICS_BROADCAST_BYTES_RCV_SUPPORTED |
		NDIS_STATISTICS_DIRECTED_BYTES_XMIT_SUPPORTED |
		NDIS_STATISTICS_MULTICAST_BYTES_XMIT_SUPPORTED |
		NDIS_STATISTICS_BROADCAST_BYTES_XMIT_SUPPORTED;

	GeneralAttributes.SupportedOidList = VNIFSupportedOids;
	GeneralAttributes.SupportedOidListLength = SupportedOidListLength;

	status = NdisMSetMiniportAttributes(adapter->AdapterHandle,
		(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&GeneralAttributes);
	DPR_INIT(("GeneralAttributes status %x\n",status));
	return status;
}

#ifdef XENNET_SG_DMA
static NDIS_STATUS
VNIFSetScatterGatherDma(PVNIF_ADAPTER adapter)
{
    NDIS_SG_DMA_DESCRIPTION DmaDescription;
	NDIS_STATUS status;

	NdisZeroMemory(&DmaDescription, sizeof(DmaDescription));

	DmaDescription.Header.Type = NDIS_OBJECT_TYPE_SG_DMA_DESCRIPTION;
	DmaDescription.Header.Revision = NDIS_SG_DMA_DESCRIPTION_REVISION_1;
	DmaDescription.Header.Size = NDIS_SIZEOF_SG_DMA_DESCRIPTION_REVISION_1;
	DmaDescription.Flags = NDIS_SG_DMA_64_BIT_ADDRESS;
	DmaDescription.MaximumPhysicalMapping = ETH_MAX_PACKET_SIZE;

	DmaDescription.ProcessSGListHandler = MpProcessSGList;
	DmaDescription.SharedMemAllocateCompleteHandler = NULL;

	adapter->NdisMiniportDmaHandle = NULL;
	status = NdisMRegisterScatterGatherDma(
				adapter->AdapterHandle,
				&DmaDescription,
				&adapter->NdisMiniportDmaHandle);
	DPR_INIT(("NdisMRegisterScatterGatherDma status %x\n", status));
	if (status != NDIS_STATUS_SUCCESS) {
#ifdef XENNET_COPY_TX
		/* We don't use DMA fragment when we copy the packets. */
		status = NDIS_STATUS_SUCCESS;
#else
		DBG_PRINT(("NdisMRegisterScatterGatherDma failed %x\n",status));
#endif
	}
	return status;
}
#endif

static void
VNIFSetOffloadDefaults(PVNIF_ADAPTER adapter)
{
	/* IPv4Transmit */
#ifdef VNIF_TX_CHECKSUM_ENABLED
	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_TCP) {
		DPR_INIT(("VNIFSetOffloadDefaults setting TX on.\n"));
		checksum_capabilities.IPv4Transmit.Encapsulation =
			NDIS_ENCAPSULATION_IEEE_802_3;
		checksum_capabilities.IPv4Transmit.TcpOptionsSupported =
			NDIS_OFFLOAD_SUPPORTED;
		checksum_capabilities.IPv4Transmit.TcpChecksum =
			NDIS_OFFLOAD_SUPPORTED;
		adapter->cur_tx_tasks = VNIF_CHKSUM_IPV4_TCP;
	}
	else {
		DPR_INIT(("VNIFSetOffloadDefaults setting TX off.\n"));
		checksum_capabilities.IPv4Transmit.Encapsulation =
			NDIS_ENCAPSULATION_NOT_SUPPORTED;
		checksum_capabilities.IPv4Transmit.TcpOptionsSupported =
			NDIS_OFFLOAD_NOT_SUPPORTED;
		checksum_capabilities.IPv4Transmit.TcpChecksum =
			NDIS_OFFLOAD_NOT_SUPPORTED;
	}
#else
	checksum_capabilities.IPv4Transmit.Encapsulation =
		NDIS_ENCAPSULATION_NOT_SUPPORTED;
	checksum_capabilities.IPv4Transmit.TcpOptionsSupported =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv4Transmit.TcpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	adapter->cur_tx_tasks = 0;
#endif
	checksum_capabilities.IPv4Transmit.IpOptionsSupported =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv4Transmit.UdpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv4Transmit.IpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;

    /* IPv4Receive */
	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_TCP) {
		DPR_INIT(("VNIFSetOffloadDefaults setting RX on.\n"));
		checksum_capabilities.IPv4Receive.Encapsulation =
			NDIS_ENCAPSULATION_IEEE_802_3;
		checksum_capabilities.IPv4Receive.TcpOptionsSupported =
			NDIS_OFFLOAD_SUPPORTED;
		checksum_capabilities.IPv4Receive.TcpChecksum =
			NDIS_OFFLOAD_SUPPORTED;
		adapter->cur_rx_tasks = VNIF_CHKSUM_IPV4_TCP;
	}
	else {
		DPR_INIT(("VNIFSetOffloadDefaults setting RX off.\n"));
		checksum_capabilities.IPv4Receive.Encapsulation =
			NDIS_ENCAPSULATION_NOT_SUPPORTED;
		checksum_capabilities.IPv4Receive.TcpOptionsSupported =
			NDIS_OFFLOAD_NOT_SUPPORTED;
		checksum_capabilities.IPv4Receive.TcpChecksum =
			NDIS_OFFLOAD_NOT_SUPPORTED;
		adapter->cur_rx_tasks = 0;
	}
	checksum_capabilities.IPv4Receive.IpOptionsSupported =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv4Receive.UdpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv4Receive.IpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;

	/* IPv6Transmit */
	checksum_capabilities.IPv6Transmit.Encapsulation =
		NDIS_ENCAPSULATION_NOT_SUPPORTED;
	checksum_capabilities.IPv6Transmit.IpExtensionHeadersSupported =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv6Transmit.TcpOptionsSupported =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv6Transmit.TcpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv6Transmit.UdpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;

    /* IPv6Receive */
	checksum_capabilities.IPv6Receive.Encapsulation =
		NDIS_ENCAPSULATION_NOT_SUPPORTED;
	checksum_capabilities.IPv6Receive.IpExtensionHeadersSupported =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv6Receive.TcpOptionsSupported =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv6Receive.TcpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;
	checksum_capabilities.IPv6Receive.UdpChecksum =
		NDIS_OFFLOAD_NOT_SUPPORTED;
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
				break;

			case VNIF_CHKSUM_TX:
				DPR_INIT(("Setting TCP Tx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_TCP;
				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_TCP;
				break;

			case VNIF_CHKSUM_RX:
				DPR_INIT(("Setting TCP Rx\n"));
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_TCP;
				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_TCP;
				break;

			case VNIF_CHKSUM_TXRX:
				DPR_INIT(("Setting TCP Tx Rx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_TCP;
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_TCP;
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
				break;

			case VNIF_CHKSUM_TX:
				DPR_INIT(("Setting UDP Tx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_UDP;
				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_UDP;
				break;

			case VNIF_CHKSUM_RX:
				DPR_INIT(("Setting UDP Rx\n"));
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_UDP;
				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_UDP;
				break;

			case VNIF_CHKSUM_TXRX:
				DPR_INIT(("Setting UDP Tx Rx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_UDP;
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_UDP;
				break;
			default:
				DPR_INIT(("Unknown UDP chksum value %x\n", chksum_value));
				break;
		}
	}
	else { // IP
		switch (chksum_value) {
			case VNIF_CHKSUM_DISABLED:
				DPR_INIT(("Setting IP off\n"));
				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_IP;
				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_IP;
				break;

			case VNIF_CHKSUM_TX:
				DPR_INIT(("Setting IP Tx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_IP;
				adapter->cur_rx_tasks &= ~VNIF_CHKSUM_IPV4_IP;
				break;

			case VNIF_CHKSUM_RX:
				DPR_INIT(("Setting IP Rx\n"));
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_IP;
				adapter->cur_tx_tasks &= ~VNIF_CHKSUM_IPV4_IP;
				break;

			case VNIF_CHKSUM_TXRX:
				DPR_INIT(("Setting IP Tx Rx\n"));
				adapter->cur_tx_tasks |= VNIF_CHKSUM_IPV4_IP;
				adapter->cur_rx_tasks |= VNIF_CHKSUM_IPV4_IP;
				break;
			default:
				DPR_INIT(("Unknown IP chksum value %x\n", chksum_value));
				break;
		}
	}
	DPR_INIT(("VNIFInitChksumOffload resulting tx %x rx %x\n",
		adapter->cur_tx_tasks, adapter->cur_rx_tasks));
}

static NDIS_STATUS
VNIFSetOffloadAttributes(PVNIF_ADAPTER adapter)
{
    NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES offload_attrs;
	NDIS_OFFLOAD hw_offload;
	NDIS_OFFLOAD def_offload;
    NDIS_TCP_CONNECTION_OFFLOAD connection;
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;

	NdisZeroMemory(&offload_attrs,
		sizeof(NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES));
	NdisZeroMemory(&hw_offload,
		sizeof(NDIS_OFFLOAD));
	NdisZeroMemory(&def_offload,
		sizeof(NDIS_OFFLOAD));
	NdisZeroMemory(&connection,
		sizeof(NDIS_TCP_CONNECTION_OFFLOAD));

	DPR_INIT(("OFFLOAD attributes size = %x, %x\n",
		NDIS_SIZEOF_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1,
		sizeof(NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES)));


	hw_offload.Header.Type = NDIS_OBJECT_TYPE_OFFLOAD; 
	hw_offload.Header.Revision = NDIS_OFFLOAD_REVISION_1;
	hw_offload.Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_1;

	hw_offload.Checksum = checksum_capabilities;

	def_offload.Header.Type = NDIS_OBJECT_TYPE_OFFLOAD; 
	def_offload.Header.Revision = NDIS_OFFLOAD_REVISION_1;
	def_offload.Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_1;

	if (adapter->cur_tx_tasks & (VNIF_CHKSUM_IPV4_TCP |VNIF_CHKSUM_IPV4_UDP)) {
		DPR_INIT(("VNIFSetOffloadAttributes TCP TX Encap and Options\n"));
		def_offload.Checksum.IPv4Transmit.Encapsulation =
			NDIS_ENCAPSULATION_IEEE_802_3;
		def_offload.Checksum.IPv4Transmit.TcpOptionsSupported =
			NDIS_OFFLOAD_SUPPORTED;
	}
	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_IP) {
		DPR_INIT(("VNIFSetOffloadAttributes IP TX IP and Options\n"));
		def_offload.Checksum.IPv4Transmit.IpOptionsSupported =
			NDIS_OFFLOAD_SUPPORTED;
		def_offload.Checksum.IPv4Transmit.IpChecksum =
			NDIS_OFFLOAD_SUPPORTED;
	}
	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_TCP) {
		DPR_INIT(("VNIFSetOffloadAttributes TCP TX\n"));
		def_offload.Checksum.IPv4Transmit.TcpChecksum =
			NDIS_OFFLOAD_SUPPORTED;
	}
	if (adapter->cur_tx_tasks & VNIF_CHKSUM_IPV4_UDP) {
		DPR_INIT(("VNIFSetOffloadAttributes UDP TX\n"));
		def_offload.Checksum.IPv4Transmit.UdpChecksum =
			NDIS_OFFLOAD_SUPPORTED;
	}

	if (adapter->cur_rx_tasks & (VNIF_CHKSUM_IPV4_TCP |VNIF_CHKSUM_IPV4_UDP)) {
		DPR_INIT(("VNIFSetOffloadAttributes TCP RX Encap and Options\n"));
		def_offload.Checksum.IPv4Receive.Encapsulation =
			NDIS_ENCAPSULATION_IEEE_802_3;
		def_offload.Checksum.IPv4Receive.TcpOptionsSupported =
			NDIS_OFFLOAD_SUPPORTED;
	}
	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_IP) {
		DPR_INIT(("VNIFSetOffloadAttributes IP RX IP and Options\n"));
		def_offload.Checksum.IPv4Receive.IpOptionsSupported =
			NDIS_OFFLOAD_SUPPORTED;
		def_offload.Checksum.IPv4Receive.IpChecksum =
			NDIS_OFFLOAD_SUPPORTED;
	}
	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_TCP) {
		DPR_INIT(("VNIFSetOffloadAttributes TCP RX\n"));
		def_offload.Checksum.IPv4Receive.TcpChecksum =
			NDIS_OFFLOAD_SUPPORTED;
	}
	if (adapter->cur_rx_tasks & VNIF_CHKSUM_IPV4_UDP) {
		DPR_INIT(("VNIFSetOffloadAttributes UDP RX\n"));
		def_offload.Checksum.IPv4Receive.UdpChecksum =
			NDIS_OFFLOAD_SUPPORTED;
	}

	/* Not supported values are 0, so just fill in the headers and */
	/* those values we support. */
	connection.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	connection.Header.Revision = NDIS_TCP_CONNECTION_OFFLOAD_REVISION_1;
	connection.Header.Size =
		NDIS_SIZEOF_TCP_CONNECTION_OFFLOAD_REVISION_1;

	offload_attrs.Header.Type =
		NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES;
	offload_attrs.Header.Revision =
		NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1;
	offload_attrs.Header.Size =
		NDIS_SIZEOF_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1;
	offload_attrs.DefaultOffloadConfiguration = &def_offload;
	offload_attrs.HardwareOffloadCapabilities = &hw_offload;
	offload_attrs.DefaultTcpConnectionOffloadConfiguration =
		&connection;
	offload_attrs.TcpConnectionOffloadHardwareCapabilities =
		&connection;

	status = NdisMSetMiniportAttributes(adapter->AdapterHandle,
		(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&offload_attrs);
	DPR_INIT(("VNIFSetOffloadAttributes NdisMSetMiniportAttributes %x\n",
		status));
	/*
	if (status == NDIS_STATUS_SUCCESS) {
		VNIFIndicateOffload(adapter);
	}
	*/
	return status;
}

NDIS_STATUS
VNIFInitialize(PVNIF_ADAPTER adapter)
{
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    
	do
	{
		if ((status = VNIFSetRegistrationAttributes(adapter))
			!= NDIS_STATUS_SUCCESS)
			break;

		/* Check for any overrides */
		VNIFReadRegParameters(adapter);
		VNIFSetOffloadDefaults(adapter);

		if ((status = VNIFSetGeneralAttributes(adapter))
			!= NDIS_STATUS_SUCCESS)
			break;

#ifdef XENNET_SG_DMA
		if ((status = VNIFSetScatterGatherDma(adapter))
			!= NDIS_STATUS_SUCCESS)
			break;
#endif

		if ((status = VNIFSetOffloadAttributes(adapter))
			!= NDIS_STATUS_SUCCESS)
			break;
	} while (FALSE);

	/* If errors, the calling function will clean up as necessary. */
	return status;
}


NDIS_STATUS
VNIFSetupNdisAdapterEx(PVNIF_ADAPTER adapter)
{
	NDIS_TIMER_CHARACTERISTICS Timer;               
	NDIS_STATUS status;

	DPR_INIT(("VNIF: VNIFSetupNdisAdapterEx - IN\n"));
	NdisAllocateSpinLock(&adapter->Lock);

	InitializeQueueHeader(&adapter->SendWaitQueue);
	InitializeQueueHeader(&adapter->SendCancelQueue);

	NdisZeroMemory(&Timer, sizeof(NDIS_TIMER_CHARACTERISTICS));
	Timer.Header.Type = NDIS_OBJECT_TYPE_TIMER_CHARACTERISTICS;
	Timer.Header.Revision = NDIS_TIMER_CHARACTERISTICS_REVISION_1;
	Timer.Header.Size = sizeof(NDIS_TIMER_CHARACTERISTICS);

	Timer.AllocationTag = VNIF_POOL_TAG;
	Timer.TimerFunction = VNIFResetCompleteTimerDpc;
	Timer.FunctionContext = adapter;

	status = NdisAllocateTimerObject(
		adapter->AdapterHandle,
		&Timer,
		&adapter->ResetTimer);

	if (status != NDIS_STATUS_SUCCESS) {
		NdisFreeSpinLock(&adapter->Lock);
	}

	Timer.TimerFunction = VNIFReceiveTimerDpc;
	Timer.FunctionContext = adapter;

	status = NdisAllocateTimerObject(
		adapter->AdapterHandle,
		&Timer,
		&adapter->rcv_timer);

	if (status != NDIS_STATUS_SUCCESS) {
		NdisFreeSpinLock(&adapter->Lock);
	}

	DPR_INIT(("VNIF: VNIFSetupNdisAdapterEx - out %x\n", status));
	return status;
}

NDIS_STATUS
VNIFSetupNdisAdapterRx(PVNIF_ADAPTER adapter)
{
    NET_BUFFER_LIST_POOL_PARAMETERS PoolParameters;
	RCB *rcb;
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	uint32_t i;

	DPR_INIT(("VNIF: VNIFNdisSetupNdisAdapter - IN\n"));
	do {

		/* Pre-allocate packet pool and buffer pool for recveive. */
		NdisZeroMemory(&PoolParameters, sizeof(NET_BUFFER_LIST_POOL_PARAMETERS));
		PoolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
		PoolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
		PoolParameters.Header.Size = sizeof(PoolParameters);
		PoolParameters.ProtocolId = 0;
		PoolParameters.ContextSize = 0;
		PoolParameters.fAllocateNetBuffer = TRUE;
		PoolParameters.PoolTag = VNIF_POOL_TAG;

		adapter->recv_pool = NdisAllocateNetBufferListPool(
			adapter->AdapterHandle,
			&PoolParameters); 
        if (adapter->recv_pool == NULL) {
			DBG_PRINT(("VNIF: NdisAllocateNetBufferListPool failed.\n"));
            status = NDIS_ERROR_CODE_OUT_OF_RESOURCES;
            break;
        }

		/*
		 * We have to initialize all of RCBs before receiving any data. The RCB
		 * is the control block for a single packet data structure. And we
		 * should pre-allocate the buffer and memory for receive. Because ring
		 * buffer is not initialized at the moment, putting RCB grant reference
		 * onto rx_ring.req is deferred.
		 */
		for(i = 0; i < adapter->num_rcb; i++) {
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

			rcb->mdl = NdisAllocateMdl(adapter->AdapterHandle, rcb->page,
				ETH_MAX_PACKET_SIZE);

			if (rcb->mdl == NULL) {
				DBG_PRINT(("VNIF: NdisAllocateMdl failed.\n"));
				status = NDIS_ERROR_CODE_OUT_OF_RESOURCES;
				break;
			}
			rcb->nb_list = NdisAllocateNetBufferAndNetBufferList(
				adapter->recv_pool, 0, 0, rcb->mdl, 0, 0);

			if (rcb->nb_list == NULL) {
				DBG_PRINT(("VNIF: NdisAllocateNetBufferAndNetBufferList failed.\n"));
				status = NDIS_ERROR_CODE_OUT_OF_RESOURCES;
				break;
			}
			rcb->nb_list->SourceHandle = adapter->AdapterHandle;
			VNIF_GET_NET_BUFFER_LIST_RFD(rcb->nb_list) = rcb;

			rcb->grant_rx_ref = GRANT_INVALID_REF;
		}

		if (status != NDIS_STATUS_SUCCESS)
			break;		/* Get out of the do while. */

	} while(FALSE);

	/*
	* In the failure case, the caller of this routine will end up
	* calling NICFreeAdapter to free all the successfully allocated
	* resources.
	*/

	DPR_INIT(("VNIF: VNIFNdisSetupNdisAdapter - OUT %x\n", status));
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

		if (rcb->nb_list) {
			NdisFreeNetBufferList(rcb->nb_list);
		}

		if (rcb->mdl) {
			NdisFreeMdl(rcb->mdl);
		}

		if (rcb->page) {
			NdisFreeMemory(rcb->page, PAGE_SIZE, 0);
		}
		NdisFreeMemory(adapter->RCBArray[i], sizeof(RCB), 0);
	}

	DPR_INIT(("VNIF: VNIFFreeAdapter NdisFreeNetBufferListPool\n"));
	if(adapter->recv_pool) {
		NdisFreeNetBufferListPool(adapter->recv_pool);
	}
}

VOID
VNIFFreeAdapterEx(PVNIF_ADAPTER adapter)
{
	RCB *rcb;
	uint32_t i;

	NdisFreeSpinLock(&adapter->Lock);
	NdisFreeTimerObject(adapter->ResetTimer);
	NdisFreeTimerObject(adapter->rcv_timer);
#ifdef XENNET_SG_DMA
	if (adapter->NdisMiniportDmaHandle) {
        NdisMDeregisterScatterGatherDma(adapter->NdisMiniportDmaHandle);
	}
#endif
}

NDIS_STATUS
VNIFNdisOpenConfiguration(PVNIF_ADAPTER adapter, NDIS_HANDLE *config_handle)
{
	NDIS_CONFIGURATION_OBJECT config_obj;

	DPR_INIT(("VNIFNdisOpenConfiguration: irql = %d IN\n",KeGetCurrentIrql()));
	config_obj.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
	config_obj.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
	config_obj.Header.Size = sizeof(NDIS_CONFIGURATION_OBJECT);
	config_obj.NdisHandle = adapter->AdapterHandle;
	config_obj.Flags = 0;

	return NdisOpenConfigurationEx(&config_obj, config_handle);
}

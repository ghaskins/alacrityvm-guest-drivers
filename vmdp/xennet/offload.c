/*****************************************************************************
 *
 *   File Name:      offload.c
 *
 *   %version:       3 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jan 24 15:19:30 2008 %
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
#include "offload.h"

VOID
CalculateIpChecksum(PUCHAR StartVa, ULONG IpHdrOffset)
{
	PIP_HEADER IpHdr;
	ULONG IpHdrLen;
	ULONG TempChecksum = 0;

	IpHdr = (PIP_HEADER) (StartVa + IpHdrOffset);
	IpHdrLen = IP_HEADER_LENGTH(IpHdr);

	XSUM(TempChecksum, StartVa, IpHdrLen, IpHdrOffset);
	IpHdr->iph_sum = ~(USHORT) TempChecksum;
}

VOID
CalculateTcpChecksum(PVOID Data, ULONG PacketLength, ULONG IpHdrOffset)
{
	PUCHAR Base = (PUCHAR) Data;
	ULONG Offset;
	PIP_HEADER IpHdr;
	ULONG IpHdrLength;
	PTCP_HEADER TcpHdr;
	ULONG TmpChecksum;
	ULONG PrevChecksum;

	Offset = IpHdrOffset;
	IpHdr = (PIP_HEADER) (Base + Offset);
	IpHdrLength = IP_HEADER_LENGTH(IpHdr);

	if (IP_HEADER_VERSION(IpHdr) != 4 || IpHdr->iph_protocol != PROTOCOL_TCP) {
		return;
	}

	Offset += IpHdrLength;
	TcpHdr = (PTCP_HEADER) (Base + Offset);

	PrevChecksum = TcpHdr->tcph_sum;
	TcpHdr->tcph_sum = 0;
	TmpChecksum = PrevChecksum;
	XSUM(TmpChecksum, Base, PacketLength - Offset, Offset);

	TcpHdr->tcph_sum = ~(USHORT) TmpChecksum;
}

uint16_t
VNIFCalculateChecksum(
	PVOID StartVa,
	ULONG PacketLength,
	PNDIS_PACKET Packet,
	ULONG IpHdrOffset)
{ 
	PNDIS_TCP_IP_CHECKSUM_PACKET_INFO pChecksumPktInfo;
	uint16_t flags = 0;
    
	if (NDIS_PROTOCOL_ID_TCP_IP != NDIS_GET_PACKET_PROTOCOL_TYPE(Packet))
	{
		DBG_PRINT(("Packet's protocol is wrong.\n"));
		return 0;
	}

	pChecksumPktInfo = (PNDIS_TCP_IP_CHECKSUM_PACKET_INFO) (
		&(NDIS_PER_PACKET_INFO_FROM_PACKET(Packet, TcpIpChecksumPacketInfo)));
    
	if (pChecksumPktInfo->Transmit.NdisPacketChecksumV4 == 0)
	{
		DBG_PRINT(("NdisPacketChecksumV4 is not set.\n"));
		return 0;
	}

	if (pChecksumPktInfo->Transmit.NdisPacketUdpChecksum)
	{
		return 0;
		//CalculateUdpChecksum(Packet, IpHdrOffset);
	}
    
	if (pChecksumPktInfo->Transmit.NdisPacketTcpChecksum)
	{
		CalculateTcpChecksum(StartVa, PacketLength, IpHdrOffset);
		flags = NETTXF_data_validated | NETTXF_csum_blank;
	}

	if (pChecksumPktInfo->Transmit.NdisPacketIpChecksum)
	{
		CalculateIpChecksum(StartVa, IpHdrOffset);
		flags = NETTXF_data_validated | NETTXF_csum_blank;
	}
	return flags;
}

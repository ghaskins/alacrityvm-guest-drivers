/*****************************************************************************
 *
 *   File Name:      mp_main5.c
 *
 *   %version:       1 %
 *   %derived_by:    kallan %
 *   %date_modified: Fri Jun 06 10:14:23 2008 %
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

#ifdef NDIS51_MINIPORT
#pragma NDIS_PAGEABLE_FUNCTION(MPPnPEventNotify)
#endif

NDIS_HANDLE NdisWrapperHandle;

NDIS_STATUS
DriverEntry5(PVOID DriverObject, PVOID RegistryPath)
{
	NDIS_MINIPORT_CHARACTERISTICS mp_char;
	NDIS_STATUS status;

	DPR_INIT(("VNIF: DriverEntry5 - IN.\n"));
	NdisZeroMemory(&mp_char, sizeof(mp_char));

	NdisMInitializeWrapper(&NdisWrapperHandle, DriverObject, RegistryPath,NULL);

	mp_char.MajorNdisVersion = VNIF_NDIS_MAJOR_VERSION;
	mp_char.MinorNdisVersion = VNIF_NDIS_MINOR_VERSION;

	mp_char.InitializeHandler = MPInitialize;
	mp_char.HaltHandler = MPHalt;

	mp_char.SetInformationHandler = MPSetInformation;
	mp_char.QueryInformationHandler = MPQueryInformation;

	mp_char.SendPacketsHandler = MPSendPackets;
	mp_char.ReturnPacketHandler = MPReturnPacket;

	mp_char.ResetHandler = MPReset;

#ifdef NDIS51_MINIPORT
	mp_char.CancelSendPacketsHandler = MPCancelSends;
	mp_char.PnPEventNotifyHandler    = MPPnPEventNotify;
	mp_char.AdapterShutdownHandler   = MPShutdown;
#endif


	status = NdisMRegisterMiniport(NdisWrapperHandle, &mp_char,
		sizeof(NDIS_MINIPORT_CHARACTERISTICS));

	if (status != NDIS_STATUS_SUCCESS) {
		DBG_PRINT(("VNIF: NdisMRegisterMiniport, status=0x%08x.\n", status));
		NdisTerminateWrapper(NdisWrapperHandle, NULL);
		return status;
	}
	NdisMRegisterUnloadHandler(NdisWrapperHandle, MPUnload);

	DPR_INIT(("VNIF: DriverEntry5 - OUT.\n"));
	return status;
}

#ifdef NDIS51_MINIPORT
VOID
MPCancelSends(IN NDIS_HANDLE MiniportAdapterContext, IN PVOID CancelId)
{
	PNDIS_PACKET packet;
	PVOID packetId;
	PLIST_ENTRY thisEntry, nextEntry, listHead;
	SINGLE_LIST_ENTRY sendCancelList;
	PSINGLE_LIST_ENTRY entry;

	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER)MiniportAdapterContext;

#define MP_GET_PACKET_MR(_p)  (PSINGLE_LIST_ENTRY)(&(_p)->MiniportReserved[0])

	DPR_INIT(("VNIF: MPCancelSendPackets CancelId = %p - IN\n", CancelId));
	sendCancelList.Next = NULL;

	NdisAcquireSpinLock(&adapter->SendLock);

	//
	// Walk through the send wait queue and complete the sends with matching Id
	//
	listHead = &adapter->SendWaitList;

	for(thisEntry = listHead->Flink,nextEntry = thisEntry->Flink;
		thisEntry != listHead;
		thisEntry = nextEntry,nextEntry = thisEntry->Flink) {
		packet = CONTAINING_RECORD(thisEntry, NDIS_PACKET, MiniportReserved);

		packetId = NdisGetPacketCancelId(packet);
		if (packetId == CancelId) {
			//DPR_INIT(("VNIF: MPCancelSendPackets, cancle packet %p\n", packet));
			//
			// This packet has the right CancelId
			//
			RemoveEntryList(thisEntry);
			//
			// Put this packet on sendCancelList
			//
			PushEntryList(&sendCancelList, MP_GET_PACKET_MR(packet));
		}
	}

	NdisReleaseSpinLock(&adapter->SendLock);

	//
	// Get the packets from SendCancelList and complete them if any
	//

	entry = PopEntryList(&sendCancelList);

	while (entry) {
		packet = CONTAINING_RECORD(entry, NDIS_PACKET, MiniportReserved);

		//DPR_INIT(("VNIF: MPCancelSendPackets, complete canceled packet %p\n", packet));
		NdisMSendComplete(
			adapter->AdapterHandle,
			packet,
			NDIS_STATUS_REQUEST_ABORTED);

		entry = PopEntryList(&sendCancelList);
	}
	DPR_INIT(("VNIF: MPCancelSendPackets - OUT\n"));
}
#endif

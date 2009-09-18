/*****************************************************************************
 *
 *   File Name:      mp_main6.c
 *
 *   %version:       5 %
 *   %derived_by:    kallan %
 *   %date_modified: Tue Sep 30 13:24:58 2008 %
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

NDIS_HANDLE NdisMiniportDriverHandle = NULL;
NDIS_HANDLE MiniportDriverContext = NULL;
NDIS_TCP_IP_CHECKSUM_OFFLOAD checksum_capabilities;

NDIS_STATUS
MPSetOptions(IN NDIS_HANDLE NdisMiniportDriverHandle,
    IN NDIS_HANDLE MiniportDriverContext);
NDIS_STATUS 
MPPause(IN NDIS_HANDLE MiniportAdapterContext,    
	IN PNDIS_MINIPORT_PAUSE_PARAMETERS MiniportPauseParameters);
NDIS_STATUS 
MPRestart(IN NDIS_HANDLE MiniportAdapterContext,    
	IN PNDIS_MINIPORT_RESTART_PARAMETERS  MiniportRestartParameters);

NDIS_STATUS
DriverEntry6(PVOID DriverObject, PVOID RegistryPath)
{
	NDIS_STATUS status;
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS mp_char;

	DPR_INIT(("VNIF: DriverEntry6 - IN.\n"));
	NdisZeroMemory(&mp_char, sizeof(mp_char));

	mp_char.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
	mp_char.Header.Size = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);
	mp_char.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;

	mp_char.MajorNdisVersion	= VNIF_NDIS_MAJOR_VERSION;
	mp_char.MinorNdisVersion	= VNIF_NDIS_MINOR_VERSION;
	mp_char.MajorDriverVersion	= VNIF_MAJOR_DRIVER_VERSION;
	mp_char.MinorDriverVersion	= VNIF_MINOR_DRIVER_VERSION;

	mp_char.SetOptionsHandler			= MPSetOptions;
    
	mp_char.InitializeHandlerEx			= MPInitialize;
	mp_char.HaltHandlerEx				= MPHalt;

	mp_char.UnloadHandler				= MPUnload;
    
	mp_char.PauseHandler				= MPPause;      
	mp_char.RestartHandler				= MPRestart;    
	mp_char.OidRequestHandler			= MPOidRequest;    
	mp_char.SendNetBufferListsHandler	= MPSendNetBufferLists;
	mp_char.ReturnNetBufferListsHandler	= MPReturnNetBufferLists;
	mp_char.CancelSendHandler			= MPCancelSends;
	mp_char.DevicePnPEventNotifyHandler	= MPPnPEventNotify;
	mp_char.ShutdownHandlerEx			= MPShutdown;
	mp_char.CheckForHangHandlerEx		= MPCheckForHang;
	mp_char.ResetHandlerEx				= MPReset;
	mp_char.CancelOidRequestHandler		= MPCancelOidRequest;
    
    
	DPRINT(("Calling NdisMRegisterMiniportDriver...\n"));

	status = NdisMRegisterMiniportDriver(DriverObject, RegistryPath,
		(PNDIS_HANDLE)MiniportDriverContext,
		&mp_char,
		&NdisMiniportDriverHandle);
	DPR_INIT(("VNIF: DriverEntry6 - OUT %x.\n", status));
	return status;
}

NDIS_STATUS
MPSetOptions(IN NDIS_HANDLE NdisMiniportDriverHandle,
    IN NDIS_HANDLE MiniportDriverContext)
{
    UNREFERENCED_PARAMETER(NdisMiniportDriverHandle);
    UNREFERENCED_PARAMETER(MiniportDriverContext);
    
	DPR_INIT(("VNIF: MPSetOptions.\n"));
    return (NDIS_STATUS_SUCCESS);
}

VOID 
MPCancelSends(NDIS_HANDLE MiniportAdapterContext, PVOID CancelId)
{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;
	PQUEUE_ENTRY entry, prev_entry, next_entry;
	PNET_BUFFER_LIST nb_list;
	PNET_BUFFER_LIST cancel_head_nb_list = NULL;
	PNET_BUFFER_LIST cancel_tail_nb_list = NULL;
	PVOID nb_list_id;
    
	DPRINT(("====> MPCancelSendNetBufferLists\n"));

	prev_entry = NULL;

	NdisAcquireSpinLock(&adapter->SendLock);

	//
	// Walk through the send wait queue and complete the sends with matching Id
	//
	do {
		if (IsQueueEmpty(&adapter->SendWaitQueue)) {
			break;
		}
        
		entry = GetHeadQueue(&adapter->SendWaitQueue); 

		while (entry != NULL) {
			nb_list = VNIF_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK(entry);
    
			nb_list_id = NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(nb_list);

			if ((nb_list_id == CancelId)
				&& (nb_list != adapter->SendingNetBufferList)) {
				NET_BUFFER_LIST_STATUS(nb_list) = NDIS_STATUS_REQUEST_ABORTED;
				adapter->nWaitSend--;
				//
				// This packet has the right CancelId
				//
				next_entry = entry->Next;

				if (prev_entry == NULL) {
					adapter->SendWaitQueue.Head = next_entry;
					if (next_entry == NULL) {
						adapter->SendWaitQueue.Tail = NULL;
					}
				}
				else {
					prev_entry->Next = next_entry;
					if (next_entry == NULL) {
						adapter->SendWaitQueue.Tail = prev_entry;
					}
				}

				entry = entry->Next;

				//
				// Queue this NetBufferList for cancellation
				//
				if (cancel_head_nb_list == NULL) {
					cancel_head_nb_list = nb_list;
					cancel_tail_nb_list = nb_list;
				}
				else {
					NET_BUFFER_LIST_NEXT_NBL(cancel_tail_nb_list) = nb_list;
					cancel_tail_nb_list = nb_list;
				}
			}
			else {
				// This packet doesn't have the right CancelId
				prev_entry = entry;
				entry = entry->Next;
			}
		}
	} while (FALSE);
    
	NdisReleaseSpinLock(&adapter->SendLock);
    
	//
	// Get the packets from SendCancelQueue and complete them if any
	//
	if (cancel_head_nb_list != NULL) {
		NET_BUFFER_LIST_NEXT_NBL(cancel_tail_nb_list) = NULL;

		NdisMSendNetBufferListsComplete(
			adapter->AdapterHandle,
			cancel_head_nb_list,
			NDIS_STATUS_SEND_ABORTED);   
	} 

	DPRINT(("<==== MPCancelSendNetBufferLists\n"));
}

NDIS_STATUS 
MPPause(IN NDIS_HANDLE MiniportAdapterContext,    
	IN PNDIS_MINIPORT_PAUSE_PARAMETERS MiniportPauseParameters)
{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;

	DPR_INIT(("VNIF: MPPause - IN %p\n", MiniportPauseParameters));

	NdisAcquireSpinLock(&adapter->RecvLock);
	NdisAcquireSpinLock(&adapter->SendLock);
	VNIF_SET_FLAG(adapter, VNF_ADAPTER_PAUSING);
	NdisReleaseSpinLock(&adapter->SendLock);    
	NdisReleaseSpinLock(&adapter->RecvLock);    

	VNIFQuiesce(adapter);

	/* Don't need to reacquire the locks since we are setting */
	/* another flag before clearing the first one. */
	VNIF_SET_FLAG(adapter, VNF_ADAPTER_PAUSED);
    VNIF_CLEAR_FLAG(adapter, VNF_ADAPTER_PAUSING);

	DPR_INIT(("VNIF: MPPause - OUT\n"));
	return NDIS_STATUS_SUCCESS;
}
    
NDIS_STATUS 
MPRestart(IN NDIS_HANDLE MiniportAdapterContext,    
	IN PNDIS_MINIPORT_RESTART_PARAMETERS  MiniportRestartParameters)
{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;

	DPR_INIT(("VNIF: MPRestart- IN %p\n", MiniportRestartParameters));
    
	NdisAcquireSpinLock(&adapter->RecvLock);
    VNIF_CLEAR_FLAG(adapter, VNF_ADAPTER_PAUSED);
	NdisReleaseSpinLock(&adapter->RecvLock);    

	DPR_INIT(("VNIF: MPRestart - OUT\n"));
	return NDIS_STATUS_SUCCESS;
}

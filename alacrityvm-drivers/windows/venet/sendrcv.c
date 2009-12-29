/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  sendrecv.c:  Handles netbuffers
 *
 * Author:
 *     Peter W. Morreale <pmorreale@novell.com> 
 * 
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "precomp.h"
#include "venet.h"

#define SPIN_LOCK(a,b) 				\
	if (b) {				\
		NdisDprAcquireSpinLock(a);	\
	}					\
	else {					\
		NdisAcquireSpinLock(a);		\
	}

#define SPIN_UNLOCK(a, b) 			\
	if (b) {				\
		NdisDprReleaseSpinLock(a);	\
	}       				\
	else {					\
		NdisReleaseSpinLock(a);		\
	}

static __inline  ULONG
SrCountBuffers(PNET_BUFFER_LIST list)
{
	PNET_BUFFER nb;
	ULONG cnt = 0;

	for (nb = NET_BUFFER_LIST_FIRST_NB(list); 
			nb != NULL; nb = NET_BUFFER_NEXT_NB(nb)) 
		cnt++;

	return cnt;
}


static __inline NDIS_STATUS             
SrGetAdapterStatus(PADAPTER a)
{                                               
	NDIS_STATUS rc = NDIS_STATUS_FAILURE;                     

	if (VENET_TEST_FLAG(a, VNET_ADAPTER_RESET))
		rc = NDIS_STATUS_RESET_IN_PROGRESS;
	else if (VENET_IS_PAUSED(a)) 
		rc = NDIS_STATUS_PAUSED;
	else if (VENET_NO_LINK(a))
		rc = NDIS_STATUS_DEVICE_FAILED;  
	else if (VENET_TEST_FLAG(a, VNET_ADAPTER_HALT_IN_PROGRESS))
		rc = NDIS_STATUS_CLOSING;
	else if (VENET_TEST_FLAG(a, VNET_ADAPTER_SURPRISE_REMOVED))
		rc = NDIS_STATUS_CLOSING;
	else if (VENET_TEST_FLAG(a, VNET_ADAPTER_SHUTDOWN))
		rc = NDIS_STATUS_CLOSING;

	return rc;                          
}                           

static VOID
SrSetListStatus(PNET_BUFFER_LIST sendList, NDIS_STATUS status)
{
	PNET_BUFFER_LIST curr;
	PNET_BUFFER_LIST next;

	for (curr = sendList; curr != NULL; curr = next) {
		next = NET_BUFFER_LIST_NEXT_NBL(curr);
		NET_BUFFER_LIST_STATUS(curr) = status;
	}
}

static VOID
SrSendNetBuffer(PADAPTER a, PNET_BUFFER_LIST list, BOOLEAN isQueue,
		BOOLEAN dlevel)
{
	UNREFERENCED_PARAMETER(a);
	UNREFERENCED_PARAMETER(list);
	UNREFERENCED_PARAMETER(isQueue);
	UNREFERENCED_PARAMETER(dlevel);
}

VOID
VenetSendNetBufs(NDIS_HANDLE handle, PNET_BUFFER_LIST sendList,
		NDIS_PORT_NUMBER port, ULONG flags)
{
	PADAPTER 		a = (PADAPTER) handle;
	BOOLEAN			dlevel =NDIS_TEST_SEND_AT_DISPATCH_LEVEL(flags);
	NDIS_STATUS		rc = NDIS_STATUS_PENDING;
	ULONG			complete_flags = 0;
	PNET_BUFFER_LIST	curr;
	PNET_BUFFER_LIST	next;

	UNREFERENCED_PARAMETER(port);

	/* Set the completion flags */
	if (dlevel) {
		 NDIS_SET_SEND_COMPLETE_FLAG(complete_flags, 
				 NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL);
	}


	SPIN_LOCK(&a->sendLock, dlevel);

	/*
	 * If we cannot send packets at this time, return the failure.
	 */
	if (VENET_IS_NOT_READY(a)) {
		rc = SrGetAdapterStatus(a);
		SPIN_UNLOCK(&a->sendLock, dlevel);
		SrSetListStatus(sendList, rc);
		NdisMSendNetBufferListsComplete(a->adapterHandle, sendList, 
				dlevel);
		return;
	}

	/*
	 * Queue or send...
	 */

	for(curr = sendList; curr != NULL; curr = next) {
		next = NET_BUFFER_LIST_NEXT_NBL(curr);
		VENET_SET_LIST_COUNT(curr) = (PULONG) SrCountBuffers(curr);
		
		if (!IsQueueEmpty(&a->sendWaitQueue)) {
			VNET_GET_NET_BUFFER_LIST_NEXT_SEND(curr) = 
				 NET_BUFFER_LIST_FIRST_NB(curr);
			NET_BUFFER_LIST_STATUS(curr) = NDIS_STATUS_SUCCESS;
			InsertTailQueue(&a->sendWaitQueue, 
				VNET_GET_NET_BUFFER_LIST_LINK(curr));
			a->nWaitSend++;
		}
		else {
			a->sendingNetBufferList = curr;
			SrSendNetBuffer(a, curr, FALSE, dlevel);
		}
	}

	SPIN_UNLOCK(&a->sendLock, dlevel);

}

VOID
VenetReturnNetBufs(NDIS_HANDLE adapter, PNET_BUFFER_LIST list, ULONG flags)
{

	UNREFERENCED_PARAMETER(adapter);
	UNREFERENCED_PARAMETER(list);
	UNREFERENCED_PARAMETER(flags);
	vlog("returnnetbufs");
}

VOID
VenetCancelSend(NDIS_HANDLE adapter, PVOID id)
{
	UNREFERENCED_PARAMETER(adapter);
	UNREFERENCED_PARAMETER(id);
	vlog("cancelsend");
}

VOID
VenetFreeQueuedSend(PADAPTER a, NDIS_STATUS status)
{
	UNREFERENCED_PARAMETER(a);
	UNREFERENCED_PARAMETER(status);
	vlog("freequeuedsend");

}

VOID 
VenetReceivePackets(PADAPTER a)
{
	UNREFERENCED_PARAMETER(a);
}

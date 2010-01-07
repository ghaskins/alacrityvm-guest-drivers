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

static ULONG
SrCountNBL(PNET_BUFFER_LIST nbl)
{
	PNET_BUFFER	nb;
	ULONG		cnt = 0;

	nb = NET_BUFFER_LIST_FIRST_NB(nbl);
	while(nb) {
		cnt++;
		nb = NET_BUFFER_NEXT_NB(nb);
	}
	return cnt;
}

static NDIS_STATUS
SrSubmitTCB(PTCB tcb)
{
	NDIS_STATUS	rc = NDIS_STATUS_SUCCESS;
	PADAPTER 	a = (PADAPTER) tcb->adapter;

	return rc;
}

static VOID
SrFlushTX(PADAPTER a)
{
	UNREFERENCED_PARAMETER(a);

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

static NDIS_STATUS
SrCopyFrame(PTCB tcb, PNET_BUFFER nb, UINT32 priority)
{
	ULONG	cur_len;
	PUCHAR	pSrc;
	PUCHAR	pDest;
	ULONG	bytes_copied = 0;
	ULONG	offset;
	PMDL	cur_mdl;
	ULONG	data_len;
	UINT32	priority_len;
	UINT32	mac_len;

	pDest = tcb->data;
	cur_mdl = NET_BUFFER_FIRST_MDL(nb);
	offset = NET_BUFFER_DATA_OFFSET(nb);
	data_len = NET_BUFFER_DATA_LENGTH(nb);

	priority_len = P8021_BYTE_LEN;
	mac_len = P8021_MAC_LEN;
	cur_len = 0;

	while (cur_mdl && data_len > 0) {
		NdisQueryMdl(cur_mdl, &pSrc, &cur_len, NormalPagePriority);
		if (pSrc == NULL) {
			bytes_copied = 0;
			break;
		}
		/*  buffer length is greater than the offset to the buffer. */
		if (cur_len > offset) {
			pSrc += offset;
			cur_len -= offset;

			if (cur_len > data_len)
				cur_len = data_len;

			data_len -= cur_len;

			/* 
			 * Make sure this is a priority packet and 
			 * not a Vlan packet. 
			 */
			if (priority && (priority & P8021_HOST_MASK) == 0 && 
								priority_len) {
				if (cur_len >= mac_len) {
					NdisMoveMemory(pDest, pSrc, mac_len);
					bytes_copied += mac_len;
					cur_len -= mac_len;
					pDest += mac_len;
					pSrc += mac_len;
					*(UINT16 *)pDest = P8021_TPID_TYPE;
					pDest += 2;
					mac_len = 0;
					priority_len = 0;
					bytes_copied += P8021_BYTE_LEN;
				}
				else
					mac_len -= cur_len;
			}
			NdisMoveMemory(pDest, pSrc, cur_len);
			bytes_copied += cur_len;
			pDest += cur_len;
			offset = 0;
		}
		else {
	    		offset -= cur_len;
		}
		NdisGetNextMdl(cur_mdl, &cur_mdl);

	}
	/* zero the tail */
	if ((bytes_copied != 0) && (bytes_copied < ETH_MIN_PACKET_SIZE)) {
		NdisZeroMemory(pDest, ETH_MIN_PACKET_SIZE - bytes_copied);
		bytes_copied = ETH_MIN_PACKET_SIZE;
    	}

	return bytes_copied;
}

static NDIS_STATUS 
SrProcessNBL(PADAPTER a)
{
	PNET_BUFFER				nb;
	PTCB					tcb;
	PNDIS_NET_BUFFER_LIST_8021Q_INFO	p8021;
	NDIS_STATUS				rc = NDIS_STATUS_SUCCESS;


	/* Don't bother if we don't have enough TCBS for this list */
	if (VENET_GET_NBL_COUNT(a->sendingNBL) > VENET_AVAILABLE_TCBS(a)) 
		return NDIS_STATUS_RESOURCES;

	 p8021 = (PNDIS_NET_BUFFER_LIST_8021Q_INFO) 
	   (&(NET_BUFFER_LIST_INFO(a->sendingNBL, Ieee8021QNetBufferListInfo)));


	/* Walk the list and process */
	nb = NET_BUFFER_LIST_FIRST_NB(a->sendingNBL);
	while(nb) {
		tcb = VenetAllocTCB(a, a->sendingNBL);

		rc = SrCopyFrame(tcb, nb, (UINT32) (*(UINT32 *) &p8021->Value));
		if (rc != NDIS_STATUS_SUCCESS) {
			VenetReleaseTCB(tcb);
			break;
		}

		rc = SrSubmitTCB(tcb);
		if (rc != NDIS_STATUS_SUCCESS) {
			VenetReleaseTCB(tcb);
			break;
		}

		nb = NET_BUFFER_NEXT_NB(nb);
	}

	/*
	 * If we're unable to put all NB's on the IOQ, then rewind the
	 * state and return the error. 
	 */
	if (rc != NDIS_STATUS_SUCCESS) {
		/* rewind IOQ */
	}

	return rc;

}

static VOID
SrProcessQueue(PADAPTER a, BOOLEAN dlevel)
{
	NDIS_STATUS	rc;
	BOOLEAN		do_flush = FALSE;

	/* 
	 * Attempt to process all NBL's.  We do this by placing as many
	 * frames on the IOQ (via the bus) as we can prior to injecting
	 * the signal.  Note that we allocate a TCB for each 'active'
	 * NBL.  The TCB remains in use to track completion of send
	 * through the bus.  
	 *
	 * Note also that only a single thread can execute this loop at
	 * a time.  We must ensure that frames are sent out in order 
	 * received.
	 */
	for(;;) {
		SPIN_LOCK(&a->sendLock, dlevel);

		/* 
		 * Are we already processing one?
		 * Any left?
		 * Adapter still ready?
		 * Got a tcb?
		 */
		if (a->sendingNBL || QueueEmpty(&a->sendQueue) || 
						VENET_IS_NOT_READY(a))
			break;

		a->sendingNBL = QueueDeque(&a->sendQueue);
		SPIN_UNLOCK(&a->sendLock, dlevel);

		rc = SrProcessNBL(a);

		SPIN_LOCK(&a->sendLock, dlevel);
		if (rc != NDIS_STATUS_SUCCESS) {
			QueueEnqueHead(&a->sendQueue, a->sendingNBL);
			a->sendingNBL = NULL;
			break;
		}

		a->sendingNBL = NULL;
		do_flush = TRUE;
	}

	if (do_flush) 
		SrFlushTX(a);

	SPIN_UNLOCK(&a->sendLock, dlevel);

}

/* Cancel matching queued NBLs for 'reason' */
static VOID
SrCancelQueued(PADAPTER a, PVOID cancelId, NDIS_STATUS reason)
{
	QUEUE_HEADER		cancelQueue;
	PNET_BUFFER_LIST	nbl;
	PQUEUE_ENTRY		curr;
	PQUEUE_ENTRY		prev;
	PVOID			cid;
	BOOLEAN			dlevel = FALSE;

	/* 
	 * Search send queue for matching cancel ids, special case is a
	 * NULL cancelId, which means all.  
	 *
	 * Put matches on a local queue.
	 */
	QueueInit(&cancelQueue);
	SPIN_LOCK(&a->sendLock, dlevel);

	curr = a->sendQueue.Head;
	prev = a->sendQueue.Head;

	while (curr) {
		nbl = QUEUE_NBL_FROM_ENTRY(curr);
		cid = NET_BUFFER_LIST_INFO(nbl, NetBufferListCancelId);

		if (!cancelId || cid == cancelId) {
			if (curr == a->sendQueue.Head) {
				a->sendQueue.Head = curr->Next;
				if (!curr->Next) 
					a->sendQueue.Tail = NULL;
			}
			else {
				prev->Next = curr->Next;
				if (!curr->Next)
					a->sendQueue.Tail = prev;
			}

			QueueEnqueHead(&cancelQueue, nbl);
		}

		prev = curr;
		curr = curr->Next;
	}

	SPIN_UNLOCK(&a->sendLock, dlevel);

	/*
	 * Dequeue the tmp list and complete the lists found 
	 */
	while(!QueueEmpty(&cancelQueue)) {
		nbl = QueueDeque(&cancelQueue);
		NET_BUFFER_LIST_STATUS(nbl) = reason;
		NdisMSendNetBufferListsComplete(a->adapterHandle, nbl, FALSE);
	}
}

/* TCB completion handler */
VOID 
VenetTxCompletion(PVOID handle)
{
	PTCB 		tcb = (PTCB) handle;
	PADAPTER	a = (PADAPTER) tcb->adapter;

	/* 
	 * This is called by the TX ioq notifier, once for each 
	 * completion.  When the count becomes zero, we are done with 
	 * this NBL.
	 *
	 * Note this can only imply success for the NBL.  Failures are
	 * handled elsewhere.
	 */
	if (!--tcb->nb_count) {
		SrSetListStatus(tcb->nbl, NDIS_STATUS_SUCCESS);
		NdisMSendNetBufferListsComplete(a->adapterHandle, 
				tcb->nbl, FALSE);
	}
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
	 * Everything not rejected is queued, then we start sending off
	 * the queue.
	 */
	for(curr = sendList; curr != NULL; curr = next) {
		next = NET_BUFFER_LIST_NEXT_NBL(curr);
		VENET_SET_NBL_COUNT(curr, SrCountNBL(curr));
		QueueEnqueTail(&a->sendQueue, curr);
	}

	SPIN_UNLOCK(&a->sendLock, dlevel);

	/* Now process everything we can. */
	SrProcessQueue(a, dlevel);

}

VOID 
VenetTxHandler(PVOID data)
{
	UNREFERENCED_PARAMETER(data);
}

VOID 
VenetRxHandler(PVOID data)
{
	UNREFERENCED_PARAMETER(data);
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
VenetCancelSend(NDIS_HANDLE handle, PVOID id)
{
	PADAPTER 		a = (PADAPTER) handle;

	SrCancelQueued(a, id, NDIS_STATUS_REQUEST_ABORTED);
}


VOID
VenetFreeQueuedSend(PADAPTER a, NDIS_STATUS status)
{
	SrCancelQueued(a, NULL, status);
}

VOID 
VenetReceivePackets(PADAPTER a)
{
	UNREFERENCED_PARAMETER(a);
}


/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  venet.h: Header for venet.
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
#ifndef _VENET_H
#define _VENET_H

#include <ndis.h>
#include <wdf.h>
#include <ntddk.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <ntintsafe.h>
#include <initguid.h>

#include <vbus_if.h>

#define VENET_NDIS_MAJOR_VERSION	6
#define VENET_NDIS_MINOR_VERSION	0
#define VENET_MAJOR_VERSION		1
#define VENET_MINOR_VERSION		0

#define ETH_HEADER_SIZE			14
#define ETH_MAX_DATA_SIZE		1500
#define ETH_MAX_PACKET_SIZE		ETH_HEADER_SIZE + ETH_MAX_DATA_SIZE
#define ETH_MIN_PACKET_SIZE		60

#define NIC_MAX_MCAST_LIST		32
#define NIC_MAX_BUSY_SENDS		20
#define NIC_MAX_SEND_PKTS		5
#define NIC_MAX_BUSY_RECVS		20
#define NIC_MAX_LOOKAHEAD		ETH_MAX_DATA_SIZE
#define NIC_BUFFER_SIZE			ETH_MAX_PACKET_SIZE
#define NIC_LINK_SPEED			1000000    // in 100 bps 
#define NIC_MEDIA_MAX_SPEED		10000000

#define NIC_SUPPORTED_FILTERS  (NDIS_PACKET_TYPE_DIRECTED    | 	\
				NDIS_PACKET_TYPE_MULTICAST   | 	\
				NDIS_PACKET_TYPE_BROADCAST   | 	\
				NDIS_PACKET_TYPE_PROMISCUOUS | 	\
				NDIS_PACKET_TYPE_ALL_MULTICAST)


/* Tag for allocations */
#define VNET	'vnet'

#define VENET_MEDIA_TYPE		NdisMedium802_3
#define VENET_VENDOR_DESC		"Novell"
#define VENET_VENDOR_DRIVER_VERSION     0x00010000
#define VENET_VENDOR_ID			0x00BEEEEF   

#define VENET_NAME_LEN		64
#define VENET_DEVICE_NAME	L"\\Device\\VenetDevice"
#define VENET_LINK_NAME		L"\\DosDevices\\VenetLink"

/* State flags */
#define VNET_ADAPTER_RESET		0x00000001
#define VNET_DISCONNECTED		0x00000002
#define VNET_ADAPTER_HALT_IN_PROGRESS	0x00000004
#define VNET_ADAPTER_SURPRISE_REMOVED	0x00000008
#define VNET_ADAPTER_RECV_LOOKASIDE	0x00000010
#define VNET_ADAPTER_SHUTDOWN		0x00000020
#define VNET_ADAPTER_NO_LINK		0x00000040
#define VNET_ADAPTER_SUSPENDING		0x00000080
#define VNET_ADAPTER_SEND_IN_PROGRESS	0x00000100
#define VNET_ADAPTER_PAUSED		0x00000200
#define VNET_ADAPTER_SEND_SLOWDOWN	0x00000400
#define VNET_IS_NOT_READY_MASK 		(VNET_DISCONNECTED 		\
	 				| VNET_ADAPTER_RESET		\
 					| VNET_ADAPTER_HALT_IN_PROGRESS \
					| VNET_ADAPTER_NO_LINK		\
					| VNET_ADAPTER_SUSPENDING 	\
					| VNET_ADAPTER_SHUTDOWN 	\
					| VNET_ADAPTER_PAUSED)

#define VENET_SET_FLAG(_M, _F)		((_M)->state |= (_F))
#define VENET_CLEAR_FLAG(_M, _F)	((_M)->state &= ~(_F))
#define VENET_CLEAR_FLAGS(_M, _F)	((_M)->state = 0)
#define VENET_TEST_FLAG(_M, _F)		(((_M)->state & (_F)) != 0)
#define VENET_TEST_FLAGS(_M, _F)	(((_M)->state & (_F)) == (_F))

#define VENET_SET_SYNC_FLAG(_M, _F)	VenetSetSyncFlag(_M, _F)

#define VENET_IS_NOT_READY(_M) 	((_M)->state & (VNET_IS_NOT_READY_MASK))
#define VENET_IS_READY(_M) 	(!((_M)->state & (VNET_IS_NOT_READY_MASK)))
#define VENET_IS_PAUSED(_M) 	((_M)->state & (VNET_ADAPTER_PAUSED))
#define VENET_NO_LINK(_M) 	((_M)->state & (VNET_ADAPTER_NO_LINK))
#define VENET_IS_BUSY(_M) 	((_M)->nBusySend || (_M)->nBusyRecv)

/*
 * Queue and NET_BUFFER support 
 */
typedef struct _QUEUE_ENTRY {
	struct _QUEUE_ENTRY *next;
} QUEUE_ENTRY, *PQUEUE_ENTRY;

typedef struct _QUEUE_HEADER {
	PQUEUE_ENTRY head;
	PQUEUE_ENTRY tail;
} QUEUE_HEADER, *PQUEUE_HEADER;

#define InitializeQueueHeader(_q) (_q)->Head = (_q)->Tail = NULL;

#define VENET_SET_LIST_COUNT(_nb) ((_nb)->MiniportReserved[1])
#define VENET_GET_LIST_COUNT(_nb) ((ULONG)(PULONG)((_nb)->MiniportReserved[1]))
#define IsQueueEmpty(_q) ((_q)->head == NULL)
#define GetHeadQueue(_q) ((_q)->head)

#define RemoveHeadQueue(_q)		\
{					\
	PQUEUE_ENTRY pNext;		\
	pNext = (_q)->head->next;	\
	(_q)->head = pNext;		\
	if (pNext == NULL)		\
                (_q)->tail = NULL;	\
}

#define InsertTailQueue(_h, _e)				\
{							\
	((PQUEUE_ENTRY)_e)->next = NULL;		\
	if ((_h)->tail)					\
		(_h)->tail->next = (PQUEUE_ENTRY)(_e);	\
	else						\
		(_h)->head = (PQUEUE_ENTRY)(_e);	\
	(_h)->tail = (PQUEUE_ENTRY)(_e);		\
}


#define VNET_GET_NET_BUFFER_LIST_LINK(_nb)  (&(NET_BUFFER_LIST_NEXT_NBL(_nb)))
#define VNET_GET_NET_BUFFER_LIST_NEXT_SEND(_nb) ((_nb)->MiniportReserved[0])
                                
#define VNET_GET_NET_BUFFER_LIST_REF_COUNT(_nb) \
				((ULONG)(ULONG_PTR)((_nb)->MiniportReserved[1]))
                                
#define VNET_GET_NET_BUFFER_PREV(_nb) ((_nb)->MiniportReserved[0])
#define VNET_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK(_e) \
			(CONTAINING_RECORD((_e), NET_BUFFER_LIST, next))
                                

/* Supported oid list */
extern NDIS_OID VenetSupportedOids[];

/* Adapter Context area */
typedef struct _ADAPTER {
	LIST_ENTRY	list;
	NDIS_HANDLE	adapterHandle;	
	ULONG		state;	/* state flags */
	LONG		refCount;

	PDEVICE_OBJECT	fdo;
	PDEVICE_OBJECT	pdo;
	PDEVICE_OBJECT	next;
	WDFDEVICE	wdf_device;
	VBUS_IF		vif;
	NDIS_SPIN_LOCK	lock;

	/* Control device */
	NDIS_HANDLE	control_handle;
	PDEVICE_OBJECT	control_object; /* Device for IOCTLs. */

	/* Timers */
	NDIS_HANDLE	resetTimer;
	NDIS_HANDLE	recvTimer;
	LONG		resetCount;
	LONG		recvCount;

	/* Events */
	NDIS_EVENT	removeEvent;

	/* Various oid data */
	UCHAR		permAddress[ETH_LENGTH_OF_ADDRESS];
	UCHAR		currAddress[ETH_LENGTH_OF_ADDRESS];

	/* multicast list */
	ULONG		mc_size;
	UCHAR 		mc_array[NIC_MAX_MCAST_LIST][ETH_LENGTH_OF_ADDRESS];

	/* Packet Filter and look ahead size. */
	ULONG		packetFilter;
	ULONG		lookAhead;
	ULONG		linkSpeed;
	ULONG		maxBusySends;
	ULONG		maxBusyRecvs;

	/* Packet counts */
	ULONG64		txCount;
	ULONG64		rxCount;

	/* Receive handling */
	NDIS_HANDLE	recv_pool;
	LIST_ENTRY	recvToProcess;
	LIST_ENTRY	recvFreeList;
	NDIS_SPIN_LOCK	recvLock;
	ULONG		nBusyRecv;

	/* Send Handling */
	QUEUE_HEADER		sendWaitQueue;
	LIST_ENTRY		sendWaitList;
	LIST_ENTRY		sendFreeList;
	NDIS_SPIN_LOCK		sendLock;
	ULONG			nBusySend;
	ULONG			nWaitSend;
	PNET_BUFFER_LIST	sendingNetBufferList;

} ADAPTER, *PADAPTER;



typedef struct _ADAPTER_CONTEXT {
	PADAPTER	adapter;
} ADAPTER_CONTEXT, *PADAPTER_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(ADAPTER_CONTEXT, GetAdapterContext)



#define VENET_ADAPTER_GET(_M)	NdisInterlockedIncrement(&(_M)->refCount)

static __inline VOID VENET_ADAPTER_PUT(PADAPTER a)	
{
	NdisInterlockedDecrement(&a->refCount);
	ASSERT(a->refCount >= 0);
	if (!a->refCount) 
		NdisSetEvent(&a->removeEvent);
}



/*
 * For logging to COM1.
 */
#define VENET_DEBUG	1

#ifdef VENET_DEBUG

void vlog(const char *fmt, ...);
#define VENET_DBG_PORT	 ((PUCHAR) 0x3F8)
#else
#define vlog(a, ...)  /**/
#endif

/* Protos */
MINIPORT_SET_OPTIONS 			VenetSetOptions;
MINIPORT_INITIALIZE			VenetInitialize;
MINIPORT_HALT				VenetHalt;
MINIPORT_UNLOAD				VenetUnload;
MINIPORT_PAUSE				VenetPause;
MINIPORT_RESTART			VenetRestart;
MINIPORT_OID_REQUEST			VenetOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS		VenetSendNetBufs;
MINIPORT_RETURN_NET_BUFFER_LISTS 	VenetReturnNetBufs;
MINIPORT_CANCEL_SEND			VenetCancelSend;
MINIPORT_CHECK_FOR_HANG			VenetCheckHang;
MINIPORT_RESET				VenetReset;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY	VenetDevicePnpEvent;
MINIPORT_SHUTDOWN			VenetShutdown;
MINIPORT_CANCEL_OID_REQUEST		VenetCancelOidRequest;

extern VOID VenetSetSyncFlag(PADAPTER a, int flag);
extern VOID VenetFreeQueuedSend(PADAPTER a, NDIS_STATUS status);
extern VOID VenetReceivePackets(PADAPTER a);
extern VOID VenetFreeQueuedPackets(PADAPTER a);


#endif /* _VENET_H_ */

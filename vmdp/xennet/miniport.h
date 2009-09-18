/*****************************************************************************
 *
 *   File Name:      miniport.h
 *
 *   %version:       32 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jul 27 16:52:05 2009 %
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

#ifndef _MINIPORT_H
#define _MINIPORT_H

/*
 * we are not including ndis.h here, because the WDM lower edge
 * would use of data structure as well.
 */
#define NTSTRSAFE_NO_DEPRECATE
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#define __XEN_INTERFACE_VERSION__ 0x00030202
#include <asm/win_compat.h>
#include <xen/public/win_xen.h>
#include <xen/public/grant_table.h>
#include <xen/public/io/netif.h>
#include <win_maddr.h>
#include <win_gnttab.h>
#include <win_xenbus.h>
#include <win_evtchn.h>
#include "offload.h"
#include "xennet_ver.h"

#define XENNET_COPY_TX 1
#define VNIF_TX_CHECKSUM_ENABLED 1
#ifdef DBG
#define XENNET_TRACK_TX 1
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifdef DBG
#ifdef NDIS60_MINIPORT
#define DPRINTK(_X_) XENBUS_PRINTK _X_
#define DPR_INIT(_X_) XENBUS_PRINTK _X_
#define DPR_CHKSUM(_X_)
#define DPRINT(_X_)
#define DPR_OID(_X_)
#else
#define DPRINTK(_X_) XENBUS_PRINTK _X_
#define DPR_INIT(_X_) XENBUS_PRINTK _X_
#define DPR_CHKSUM(_X_)
#define DPRINT(_X_)
#define DPR_OID(_X_)
#endif
#else
#define DPRINT(_X_)
#define DPR_INIT(_X_)
#define DPR_CHKSUM(_X_)
#define DPR_OID(_X_)
#endif

/* Always print errors. */
#define DBG_PRINT(_X_) XENBUS_PRINTK _X_

#if defined (NDIS60_MINIPORT)
# define VNIF_NDIS_MAJOR_VERSION 6
# define VNIF_NDIS_MINOR_VERSION 0
#elif defined (NDIS51_MINIPORT)
# define VNIF_NDIS_MAJOR_VERSION 5
# define VNIF_NDIS_MINOR_VERSION 1
#elif defined (NDIS50_MINIPORT)
# define VNIF_NDIS_MAJOR_VERSION 5
# define VNIF_NDIS_MINOR_VERSION 0
//# error Unsupported NDIS version
#endif

#define GRANT_INVALID_REF       0

/*
 * __WIN_RING_SIZE should be the same as RING_SIZE in ring.h.  However, Windows
 * will not compile correctly without the '&' on the ring.
 */
//#define __WIN_RING_SIZE(_s, _sz) \
//    (__RD32(((_sz) - (long)&(_s)->ring + (long)(_s)) / sizeof((_s)->ring[0])))

//#define NET_TX_RING_SIZE __WIN_RING_SIZE((struct netif_tx_sring *)0, PAGE_SIZE)
//#define NET_RX_RING_SIZE __WIN_RING_SIZE((struct netif_rx_sring *)0, PAGE_SIZE)
#define NET_TX_RING_SIZE __WIN_RING_SIZE((struct netif_tx_sring *)0, PAGE_SIZE)
#define NET_RX_RING_SIZE __WIN_RING_SIZE((struct netif_rx_sring *)0, PAGE_SIZE)

#define VNIF_POOL_TAG			((ULONG)'FINV')
#define VNIF_MEDIA_TYPE			NdisMedium802_3
#define VNIF_INTERFACE_TYPE		NdisInterfaceInternal
#define VNIF_VENDOR_DESC		"Xen"
#define VNIF_VENDOR_ID			0x0000163E

#define ETH_HEADER_SIZE				14
#define ETH_MAX_DATA_SIZE			1500
#define ETH_MAX_PACKET_SIZE			ETH_HEADER_SIZE + ETH_MAX_DATA_SIZE
#define ETH_MIN_PACKET_SIZE			60
#define XENNET_ZERO_FRAGMENT		0x5a

#define VNIF_MAX_MCAST_LIST			32
#define VNIF_MAX_BUSY_SENDS			NET_TX_RING_SIZE
#define VNIF_MAX_SEND_PKTS			5
#define VNIF_MAX_BUSY_RECVS			NET_RX_RING_SIZE
#define VNIF_MAX_LOOKAHEAD			ETH_MAX_DATA_SIZE
#define VNIF_BUFFER_SIZE			ETH_MAX_PACKET_SIZE
#define VNIF_LINK_SPEED				10000000    // in 100 bps, i.e. 1Gbps
#define VNIF_MEDIA_MAX_SPEED		100000000
#define VNIF_MAX_NUM_RCBS			4096
#define VNIF_MIN_RCV_LIMIT			20

#define VNIF_RECEIVE_DISCARD		0
#define VNIF_RECEIVE_COMPLETE		1
#define VNIF_RECEIVE_LAST_FRAG		2

#define VNIF_PACKET_OFFSET_IP		0xc
#define VNIF_PACKET_TYPE_IP			0x8
#define VNIF_PACKET_OFFSET_TCP_UDP	0x17
#define VNIF_PACKET_TYPE_TCP		0x6
#define VNIF_PACKET_TYPE_UDP		0x11


#define VNIF_SUPPORTED_FILTERS (					\
	NDIS_PACKET_TYPE_DIRECTED		|				\
	NDIS_PACKET_TYPE_MULTICAST		|				\
	NDIS_PACKET_TYPE_BROADCAST		|				\
	NDIS_PACKET_TYPE_PROMISCUOUS	|				\
	NDIS_PACKET_TYPE_ALL_MULTICAST)

#define VNIF_RESOURCE_BUF_SIZE						\
	(sizeof(NDIS_RESOURCE_LIST) +					\
	(10*sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)))

#define VNIF_CHKSUM_DISABLED		0
#define	VNIF_CHKSUM_TX				1
#define	VNIF_CHKSUM_RX				2
#define	VNIF_CHKSUM_TXRX			3

#define VNIF_CHKSUM_IPV4_TCP		1
#define VNIF_CHKSUM_IPV4_UDP		2
#define VNIF_CHKSUM_IPV4_IP			4

#define VNIF_IP_FLAGS_BYTE			0x14
#define VNIF_IP_FRAG_OFFSET_BYTE	0x15

#define SRC_ADDR_END_BYTE			11
#define P8021_MAC_LEN				12
#define P8021_TPID_BYTE				12
#define P8021_TCI_BYTE				14
#define P8021_VLAN_BYTE				15
#define P8021_BYTE_LEN				4
#define P8021_BIT_SHIFT				5
#define P8021_TPID_TYPE				0x0081	
#define P8021_HOST_MASK				0xfff8	
#define P8021_NETWORK_MASK			0x1f	

typedef struct _VNIF_GLOBAL_DATA
{
	LIST_ENTRY AdapterList;
	NDIS_SPIN_LOCK Lock;
} VNIF_GLOBAL_DATA, *PVNIF_GLOBAL_DATA;

extern VNIF_GLOBAL_DATA Global;

#define VNF_RESET_IN_PROGRESS				0x00000001
#define VNF_DISCONNECTED					0x00000002
#define VNF_ADAPTER_HALT_IN_PROGRESS		0x00000004
#define VNF_ADAPTER_SURPRISE_REMOVED		0x00000008
#define VNF_ADAPTER_RECV_LOOKASIDE			0x00000010
#define VNF_ADAPTER_SHUTDOWN				0x00000020
#define VNF_ADAPTER_NO_LINK					0x00000040
#define VNF_ADAPTER_SUSPENDING				0x00000080
#define VNF_ADAPTER_SEND_IN_PROGRESS		0x00000100
#define VNF_ADAPTER_PAUSING					0x00000200
#define VNF_ADAPTER_PAUSED					0x00000400
#define VNF_ADAPTER_SEND_SLOWDOWN			0x00000800
#define VNIF_IS_NOT_READY_MASK VNF_DISCONNECTED							\
		| VNF_RESET_IN_PROGRESS											\
		| VNF_ADAPTER_HALT_IN_PROGRESS									\
		| VNF_ADAPTER_NO_LINK											\
		| VNF_ADAPTER_SUSPENDING										\
		| VNF_ADAPTER_SHUTDOWN											\
		| VNF_ADAPTER_PAUSING											\
		| VNF_ADAPTER_PAUSED											

#define VNIF_SET_FLAG(_M, _F)				((_M)->Flags |= (_F))
#define VNIF_CLEAR_FLAG(_M, _F)				((_M)->Flags &= ~(_F))
#define VNIF_CLEAR_FLAGS(_M, _F)			((_M)->Flags = 0)
#define VNIF_TEST_FLAG(_M, _F)				(((_M)->Flags & (_F)) != 0)
#define VNIF_TEST_FLAGS(_M, _F)				(((_M)->Flags & (_F)) == (_F))

#define VVNIF_IS_READY(_M) (((_M)->Flags &								\
	(VNF_DISCONNECTED													\
		| VNF_RESET_IN_PROGRESS											\
		| VNF_ADAPTER_HALT_IN_PROGRESS									\
		| VNF_ADAPTER_NO_LINK											\
		| VNF_ADAPTER_SUSPENDING										\
		| VNF_ADAPTER_SHUTDOWN											\
		| VNF_ADAPTER_PAUSING											\
		| VNF_ADAPTER_PAUSED											\
		))																\
	== 0)

#define VNIF_IS_NOT_READY(_M) ((_M)->Flags & (VNIF_IS_NOT_READY_MASK))
#define VNIF_IS_READY(_M) !((_M)->Flags & (VNIF_IS_NOT_READY_MASK))

#define VNIF_INC_REF(_A)	NdisInterlockedIncrement(&(_A)->RefCount)

#define VNIF_DEC_REF(_A) {												\
	NdisInterlockedDecrement(&(_A)->RefCount);							\
	ASSERT(_A->RefCount >= 0);											\
	if((_A)->RefCount == 0){											\
		NdisSetEvent(&(_A)->RemoveEvent);								\
	}																	\
}

#define VNIF_GET_REF(_A)	((_A)->RefCount)
#define VNIFInterlockedIncrement(_inc) NdisInterlockedIncrement(&(_inc))

#define VNIFInterlockedDecrement(_dec) {								\
	NdisInterlockedDecrement(&(_dec));									\
		ASSERT((_dec) >= 0);											\
}

#ifdef NDIS60_MINIPORT
NDIS_STATUS DriverEntry6(PVOID DriverObject, PVOID RegistryPath);
#define DRIVER_ENTRY DriverEntry6

#define VNIF_INITIALIZE(_adapter, _mindex, _marray, _marraysize, _wrapctx)	\
	VNIFInitialize((_adapter))
#define MP_HALT(_adapter, _action) MPHalt((_adapter), (_action))
#define MP_SHUTDOWN(_adapter, _action) MPShutdown((_adapter), (_action))
#define VNIF_CANCEL_TIMER(_timer, _cancelled)							\
	NdisCancelTimerObject((_timer))
#define VNIF_SET_TIMER(_timer, _li)										\
	NdisSetTimerObject((_timer), (_li), 0, NULL)

#define VNIF_ACQUIRE_SPIN_LOCK(_lock, _dispatch_level)					\
{																		\
	if ((_dispatch_level)) {											\
		NdisDprAcquireSpinLock((_lock));								\
	}																	\
	else {																\
		NdisAcquireSpinLock((_lock));									\
	}																	\
}

#define VNIF_RELEASE_SPIN_LOCK(_lock, _dispatch_level)					\
{																		\
	if ((_dispatch_level)) {											\
		NdisDprReleaseSpinLock((_lock));								\
	}																	\
	else {																\
		NdisReleaseSpinLock((_lock));									\
	}																	\
}

#define VNIF_TCB_RESOURCES_AVAIABLE(_M) ((_M)->nBusySend < (_M)->NumTcb)

#define VNIF_ALLOCATE_MEMORY(_va, _len, _tag, _hndl, _pri)				\
	_va = NdisAllocateMemoryWithTagPriority((_hndl), (_len), (_tag), (_pri));

typedef struct _QUEUE_ENTRY
{
    struct _QUEUE_ENTRY *Next;
} QUEUE_ENTRY, *PQUEUE_ENTRY;

typedef struct _QUEUE_HEADER
{
    PQUEUE_ENTRY Head;
    PQUEUE_ENTRY Tail;
} QUEUE_HEADER, *PQUEUE_HEADER;

#define InitializeQueueHeader(QueueHeader)								\
{																		\
	(QueueHeader)->Head = (QueueHeader)->Tail = NULL;					\
}

#define IsQueueEmpty(QueueHeader) ((QueueHeader)->Head == NULL)
#define GetHeadQueue(QueueHeader) ((QueueHeader)->Head)

#define RemoveHeadQueue(QueueHeader)									\
	(QueueHeader)->Head;												\
{																		\
	PQUEUE_ENTRY pNext;													\
																		\
	ASSERT((QueueHeader)->Head);										\
	pNext = (QueueHeader)->Head->Next;									\
	(QueueHeader)->Head = pNext;										\
	if (pNext == NULL)													\
		(QueueHeader)->Tail = NULL;										\
}

#define InsertHeadQueue(QueueHeader, QueueEntry)						\
{																		\
	((PQUEUE_ENTRY)QueueEntry)->Next = (QueueHeader)->Head;				\
	(QueueHeader)->Head = (PQUEUE_ENTRY)(QueueEntry);					\
	if ((QueueHeader)->Tail == NULL)									\
		(QueueHeader)->Tail = (PQUEUE_ENTRY)(QueueEntry);				\
}

#define InsertTailQueue(QueueHeader, QueueEntry)						\
{																		\
	((PQUEUE_ENTRY)QueueEntry)->Next = NULL;							\
	if ((QueueHeader)->Tail)											\
		(QueueHeader)->Tail->Next = (PQUEUE_ENTRY)(QueueEntry);			\
	else																\
		(QueueHeader)->Head = (PQUEUE_ENTRY)(QueueEntry);				\
	(QueueHeader)->Tail = (PQUEUE_ENTRY)(QueueEntry);					\
}

#define VNIF_GET_NET_BUFFER_LIST_LINK(_NetBufferList)					\
	(&(NET_BUFFER_LIST_NEXT_NBL(_NetBufferList)))

#define VNIF_GET_NET_BUFFER_LIST_NEXT_SEND(_NetBufferList)				\
	((_NetBufferList)->MiniportReserved[0])

#define VNIF_GET_NET_BUFFER_LIST_REF_COUNT(_NetBufferList)				\
	((ULONG)(ULONG_PTR)((_NetBufferList)->MiniportReserved[1]))

#define VNIF_GET_NET_BUFFER_PREV(_NetBuffer)							\
	((_NetBuffer)->MiniportReserved[0])

#define VNIF_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK(_pEntry)				\
    (CONTAINING_RECORD((_pEntry), NET_BUFFER_LIST, Next))

#define VNIF_GET_NET_BUFFER_LIST_RFD(_NetBufferList)					\
	((PRCB)((_NetBufferList)->MiniportReserved[0])) 
            
#else
NDIS_STATUS DriverEntry5(PVOID DriverObject, PVOID RegistryPath);
#define DRIVER_ENTRY DriverEntry5

#define VNIF_INITIALIZE(_adapter, _marray, _marraysize, _mindex, _wrapctx)	\
	VNIFInitialize((_adapter), (_marray), (_marraysize), (_mindex), (_wrapctx))
#define MP_HALT(_adapter, _action) MPHalt((_adapter))
#define MP_SHUTDOWN(_adapter, _action) MPShutdown((_adapter))
#define VNIF_CANCEL_TIMER(_timer, _cancelled)							\
	NdisCancelTimer(&(_timer), (_cancelled))
#define VNIF_SET_TIMER(_timer, _li)										\
	NdisSetTimer(&(_timer), (uint32_t)((_li).QuadPart))
#define VNIF_ALLOCATE_MEMORY(_va, _len, _tag, _hndl,  _pri)				\
	NdisAllocateMemoryWithTag(&(_va), (_len), (_tag));
#endif

#ifdef XENNET_COPY_TX
// TCB (Transmit Control Block)
typedef struct _TCB
{
	LIST_ENTRY				list;
#ifdef NDIS60_MINIPORT
	PNET_BUFFER				nb;
	PNET_BUFFER_LIST		nb_list;
	void					*adapter;
	PVOID					sg_list;  // SG list passsed to MpProcessSGList
#else
	PNDIS_PACKET			orig_send_packet;
#endif
	UCHAR					data[VNIF_BUFFER_SIZE];

	/* give us a relation between grant_ref and TCB */
	UINT					index;
	grant_ref_t				grant_tx_ref;
#ifdef XENNET_TRACK_TX
	UINT					granted;
	UINT					ringidx;
	UINT					len;
#endif
} TCB, *PTCB;
#endif

// RCB (Receive Control Block)
typedef struct _RCB
{
	LIST_ENTRY				list;
	PVOID					adapter;
#ifdef NDIS60_MINIPORT
	PNET_BUFFER_LIST		nb_list;
	PMDL					mdl;
#else
	PNDIS_PACKET			packet;
	PNDIS_BUFFER			buffer;
#endif
	PUCHAR					page;
	UINT					index;
	grant_ref_t				grant_rx_ref;
	uint32_t				len;
	uint32_t				flags;
} RCB, *PRCB;

#define RX_MIN_TARGET 8
#define RX_DFL_MIN_TARGET 64
#define RX_MAX_TARGET min(NET_RX_RING_SIZE, 256)
#define TX_MAX_TARGET min(NET_TX_RING_SIZE, 256)

#ifdef XENNET_COPY_TX 
#define TX_MAX_GRANT_REFS	TX_MAX_TARGET
#else
#define TX_MAX_GRANT_REFS	TX_MAX_TARGET + 1	// 1 for the zero pad
#endif

#ifdef XENNET_TRACK_TX
typedef struct txlist_ent_s {
	grant_ref_t	ref;
	uint32_t id;
	uint32_t rid;
	uint32_t state;
	uint32_t sflags;
	uint32_t eflags;
} txlist_ent_t;
typedef struct txlist_s {
	txlist_ent_t list[VNIF_MAX_BUSY_SENDS];
	uint32_t cons;
	uint32_t prod;
} txlist_t;
#endif

typedef struct _VNIF_ADAPTER
{
	LIST_ENTRY			List;
	LONG				RefCount;
	NDIS_EVENT			RemoveEvent;

#if defined(NDIS_WDM)
	PDEVICE_OBJECT		Pdo;
	PDEVICE_OBJECT		Fdo;
	PDEVICE_OBJECT		NextDevice;
#endif

	NDIS_HANDLE			AdapterHandle;
	ULONG				Flags;
	UCHAR				PermanentAddress[ETH_LENGTH_OF_ADDRESS];
	UCHAR				CurrentAddress[ETH_LENGTH_OF_ADDRESS];

	uint32_t			cur_tx_tasks;
	uint32_t			cur_rx_tasks;
	uint32_t			num_rcb;
	uint32_t			rcv_limit;
#ifdef NDIS60_MINIPORT
	PNDIS_OID_REQUEST	NdisRequest;
#ifdef XENNET_SG_DMA
    NDIS_HANDLE			NdisMiniportDmaHandle;
#endif
	NDIS_HANDLE			ResetTimer;
	NDIS_HANDLE			rcv_timer;
	QUEUE_HEADER		SendWaitQueue;
	QUEUE_HEADER		SendCancelQueue;
    PNET_BUFFER_LIST	SendingNetBufferList;
	NDIS_TCP_IP_CHECKSUM_OFFLOAD hw_chksum_cap;
	LONG				nWaitSend;
#else
	NDIS_HANDLE			WrapperContext;
	NDIS_TIMER			ResetTimer;
	NDIS_TIMER			rcv_timer;
	NDIS_TASK_TCP_IP_CHECKSUM hw_chksum_task;
	uint32_t			tx_throttle_start;
	uint32_t			tx_throttle_stop;
#endif
	void				*oid_buffer;
	NDIS_OID			oid;
	//
	// Variables to track resources for the send operation
	//
	LIST_ENTRY			SendFreeList;
	LIST_ENTRY			SendWaitList;
#ifdef XENNET_COPY_TX
	PTCB				TCBArray[VNIF_MAX_BUSY_SENDS];
#else
	NDIS_PACKET			*tx_packets[VNIF_MAX_BUSY_SENDS];
	xen_ulong_t			tx_id_alloc_head;
	grant_ref_t			grant_tx_ref[VNIF_MAX_BUSY_SENDS]; 
	uint8_t				*zero_data;
	grant_ref_t			zero_ref; 
	UINT				zero_offset;
#endif
	NDIS_SPIN_LOCK		Lock;
	LONG				nBusySend;
	UINT				RegNumTcb;
	NDIS_SPIN_LOCK		SendLock;

	//
	// Variables to track resources for the Reset operation
	//
	LONG				nResetTimerCount;

	//
	// Variables to track resources for the Receive operation
	//
#ifndef NDIS60_MINIPORT
	NDIS_HANDLE			RecvBufferPoolHandle;
	NDIS_ENCAPSULATION_FORMAT EncapsulationFormat;
#endif
	LIST_ENTRY			RecvFreeList;
	LIST_ENTRY			RecvToProcess;
	RCB					**RCBArray;
	NDIS_SPIN_LOCK		RecvLock;
	LONG				nBusyRecv;
	NDIS_HANDLE			recv_pool;

	//
	// Packet Filter and look ahead size.
	//
	ULONG				PacketFilter;
	ULONG				ulLookAhead;
	ULONG				ulLinkSpeed;
	ULONG				ulMaxBusySends;
	ULONG				ulMaxBusyRecvs;

	// multicast list
	ULONG				ulMCListSize;
	UCHAR				MCList[VNIF_MAX_MCAST_LIST][ETH_LENGTH_OF_ADDRESS];

	struct xenbus_watch	watch;

	// Packet counts
	ULONG64				in_no_buffers;
	ULONG64				in_discards;
	ULONG64				ifInErrors;				// GEN_RCV_ERROR
	ULONG64				ifOutErrors;			// GEN_XMIT_ERROR
	ULONG64				ifOutDiscards;			// GEN_XMIT_DISCARDS
#ifdef NDIS60_MINIPORT
	ULONG64				ifHCInUcastPkts;		// GEN_DIRECTED_FRAMES_RCV
	ULONG64				ifHCInMulticastPkts;	// GEN_MULTICAST_FRAMES_RCV
	ULONG64				ifHCInBroadcastPkts;	// GEN_BROADCAST_FRAMES_RCV
	ULONG64				ifHCOutUcastPkts;		// GEN_DIRECTED_FRAMES_XMIT
	ULONG64				ifHCOutMulticastPkts;	// GEN_MULTICAST_FRAMES_XMIT
	ULONG64				ifHCOutBroadcastPkts;	// GEN_BROADCAST_FRAMES_XMIT
	ULONG64				ifHCInUcastOctets;		// GEN_DIRECTED_BYTES_RCV    
	ULONG64				ifHCInMulticastOctets;	// GEN_MULTICAST_BYTES_RCV
	ULONG64				ifHCInBroadcastOctets;	// GEN_BROADCAST_BYTES_RCV
	ULONG64				ifHCOutUcastOctets;		// GEN_DIRECTED_BYTES_XMIT    
	ULONG64				ifHCOutMulticastOctets;	// GEN_MULTICAST_BYTES_XMIT
	ULONG64				ifHCOutBroadcastOctets;	// GEN_BROADCAST_BYTES_XMIT
#else
	ULONG64				GoodTransmits;
	ULONG64				GoodReceives;
#endif

	// Xenstore device nodename
	PUCHAR				Nodename;
	PUCHAR				Otherend;
	domid_t				BackendID;
	UINT				copyall;
	UINT				evtchn;
	grant_ref_t			gref_tx_head;
	grant_ref_t			gref_rx_head;
	struct netif_tx_front_ring tx;
	struct netif_rx_front_ring rx;
	int					tx_ring_ref;
	int					rx_ring_ref;
#ifdef DBG
	uint32_t			dbg_print_cnt;
#endif
#ifdef XENNET_TRACK_TX
	txlist_t			txlist;
#endif
} VNIF_ADAPTER, *PVNIF_ADAPTER;


NDIS_STATUS
DriverEntry(
	IN PVOID DriverObject,
	IN PVOID RegistryPath
  );

#ifdef NDIS60_MINIPORT
extern NDIS_HANDLE NdisMiniportDriverHandle;
extern NDIS_OID VNIFSupportedOids[];
extern uint32_t SupportedOidListLength;
extern NDIS_TCP_IP_CHECKSUM_OFFLOAD checksum_capabilities;

NDIS_STATUS 
MPInitialize(
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN NDIS_HANDLE MiniportDriverContext,
	IN PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
    );

VOID
MPHalt(IN NDIS_HANDLE MiniportAdapterContext, IN NDIS_HALT_ACTION HaltAction);

NDIS_STATUS 
MPReset(IN NDIS_HANDLE MiniportAdapterContext, OUT PBOOLEAN AddressingReset);

VOID
MPShutdown(IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_SHUTDOWN_ACTION ShutdownAction);

VOID
MPPnPEventNotify(
	NDIS_HANDLE MiniportAdapterContext,
    IN PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent);

VOID MpProcessSGList(
    IN  PDEVICE_OBJECT          pDO,
    IN  PVOID                   pIrp,
    IN  PSCATTER_GATHER_LIST    pSGList,
    IN  PVOID                   Context
    );

void 
MPSendNetBufferLists(
	PVNIF_ADAPTER adapter,
	PNET_BUFFER_LIST nb_list,
	NDIS_PORT_NUMBER PortNumber,
	ULONG SendFlags);

VOID 
MPReturnNetBufferLists(
	NDIS_HANDLE MiniportAdapterContext,
	PNET_BUFFER_LIST    NetBufferLists,
	ULONG               ReturnFlags);

NDIS_STATUS 
MPOidRequest(
	IN NDIS_HANDLE  MiniportAdapterContext,
	IN  PNDIS_OID_REQUEST  NdisRequest);

VOID
MPCancelOidRequest(IN NDIS_HANDLE MiniportAdapterContext, IN PVOID RequestId);

NDIS_STATUS
VNIFInitialize(PVNIF_ADAPTER adapter);

void
VNIFIndicateOffload(PVNIF_ADAPTER adapter);

#else
extern NDIS_TASK_TCP_IP_CHECKSUM TcpIpChecksumTask;

NDIS_STATUS
MPInitialize(
	OUT PNDIS_STATUS OpenErrorStatus,
	OUT PUINT SelectedMediumIndex,
	IN PNDIS_MEDIUM MediumArray,
	IN UINT MediumArraySize,
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN NDIS_HANDLE WrapperConfigurationContext
  );

VOID
MPHalt(IN  NDIS_HANDLE MiniportAdapterContext);

NDIS_STATUS
MPReset(OUT PBOOLEAN AddressingReset, IN  NDIS_HANDLE MiniportAdapterContext);

VOID
MPShutdown(IN NDIS_HANDLE MiniportAdapterContext);

#ifdef NDIS51_MINIPORT
VOID
MPPnPEventNotify(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_DEVICE_PNP_EVENT PnPEvent,
	IN PVOID InformationBuffer,
	IN ULONG InformationBufferLength);
#endif

NDIS_STATUS
VNIFInitialize(PVNIF_ADAPTER adapter,
	PNDIS_MEDIUM MediumArray,
	UINT MediumArraySize,
	PUINT SelectedMediumIndex,
	NDIS_HANDLE WrapperConfigurationContext);

VOID
MPAllocateComplete(
	NDIS_HANDLE MiniportAdapterContext,
	IN PVOID VirtualAddress,
	IN PNDIS_PHYSICAL_ADDRESS PhysicalAddress,
	IN ULONG Length,
	IN PVOID Context
	);

NDIS_STATUS
MPQueryInformation(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_OID Oid,
	IN PVOID InformationBuffer,
	IN ULONG InformationBufferLength,
	OUT PULONG BytesWritten,
	OUT PULONG BytesNeeded);

VOID
MPReturnPacket(
	IN NDIS_HANDLE  MiniportAdapterContext,
	IN PNDIS_PACKET Packet);

NDIS_STATUS
MPSetInformation(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_OID Oid,
	IN PVOID InformationBuffer,
	IN ULONG InformationBufferLength,
	OUT PULONG BytesRead,
	OUT PULONG BytesNeeded);

void
MPSendPackets(PVNIF_ADAPTER adapter,PPNDIS_PACKET Packets, UINT Count);

VOID
VNIFReturnRecvPacket(IN PVNIF_ADAPTER adapter, IN PNDIS_PACKET Packet);
#endif

VOID
MPCancelSends(IN NDIS_HANDLE MiniportAdapterContext,IN PVOID CancelId);

BOOLEAN
MPCheckForHang(IN NDIS_HANDLE MiniportAdapterContext);

VOID
MPHandleInterrupt(IN NDIS_HANDLE MiniportAdapterContext);

VOID
MPUnload(IN  PDRIVER_OBJECT  DriverObject);

VOID
MPDisableInterrupt(IN PVOID adapter);

VOID
MPEnableInterrupt(IN PVOID adapter);

VOID
MPIsr(
	OUT PBOOLEAN InterruptRecognized,
	OUT PBOOLEAN QueueMiniportHandleInterrupt,
	IN NDIS_HANDLE MiniportAdapterContext);

NDIS_STATUS
VNIFSetupNdisAdapter(PVNIF_ADAPTER adapter);

NDIS_STATUS
VNIFSetupNdisAdapterEx(PVNIF_ADAPTER adapter);

NDIS_STATUS
VNIFSetupNdisAdapterRx(PVNIF_ADAPTER adapter);

void
VNIFFreeXenAdapter(PVNIF_ADAPTER adapter);

VOID
VNIFFreeAdapterRx(PVNIF_ADAPTER adapter);

NDIS_STATUS
VNIFFindXenAdapter(PVNIF_ADAPTER adapter);

NDIS_STATUS
VNIFSetupXenAdapter(PVNIF_ADAPTER adapter);

VOID
VNIFFreeAdapter(PVNIF_ADAPTER adapter);

VOID
VNIFFreeAdapterEx(PVNIF_ADAPTER adapter);

VOID
VNIFAttachAdapter(PVNIF_ADAPTER adapter);

VOID
VNIFDetachAdapter(PVNIF_ADAPTER adapter);


NDIS_STATUS
VNIFReadRegParameters(PVNIF_ADAPTER adapter);

NDIS_STATUS
VNIFNdisOpenConfiguration(PVNIF_ADAPTER adapter, NDIS_HANDLE *config_handle);

void
VNIFInitChksumOffload(PVNIF_ADAPTER adapter,
	uint32_t chksum_type,
	uint32_t chksum_value);

ULONG
VNIFGetMediaConnectStatus(PVNIF_ADAPTER adapter);

NDIS_STATUS
VNIFSetMulticastList(
	IN PVNIF_ADAPTER adapter,
	IN PVOID InformationBuffer,
	IN ULONG InformationBufferLength,
	OUT PULONG BytesRead,
	OUT PULONG BytesNeeded);

NDIS_STATUS
VNIFSetPacketFilter(IN PVNIF_ADAPTER adapter, IN ULONG PacketFilter);

uint32_t
VNIFQuiesce(PVNIF_ADAPTER adapter);

uint32_t
VNIFDisconnectBackend(PVNIF_ADAPTER adapter);

int
VNIFCheckSendCompletion(PVNIF_ADAPTER adapter);

KDEFERRED_ROUTINE VNIFInterruptDpc;

uint32_t
xennet_should_complete_packet(PVNIF_ADAPTER adapter, PUCHAR dest, UINT len);

void
xennet_add_rcb_to_ring(PVNIF_ADAPTER adapter);

void
xennet_return_rcb(PVNIF_ADAPTER Adapter, RCB *rcb);

void
xennet_drop_rcb(PVNIF_ADAPTER adapter, RCB *rcb, int status);

VOID
VNIFReceivePackets(IN PVNIF_ADAPTER adapter);

VOID
VNIFFreeQueuedSendPackets(PVNIF_ADAPTER adapter, NDIS_STATUS status);

VOID
VNIFFreeQueuedRecvPackets(PVNIF_ADAPTER adapter);

void
VNIFOidRequestComplete(PVNIF_ADAPTER adapter);

void vnif_timer(void *s1, void *context, void *s2, void *s3);
void VNIFResetCompleteTimerDpc(void *s1, void *context, void *s2, void *s3);
void VNIFReceiveTimerDpc(void *s1, void *context, void *s2, void *s3);
void vnif_dpc(PKDPC dpc, PVNIF_ADAPTER adapter, void *s1, void *s2);
void VNIFIndicateLinkStatus(PVNIF_ADAPTER adapter, uint32_t status);

/*
uint16_t
VNIFCalculateChecksum(
	PVOID StartVa,
	ULONG PacketLength,
	PNDIS_PACKET Packet,
	ULONG IpHdrOffset);
*/

void xennet_frontend_changed(struct xenbus_watch *watch,
	const char **vec, unsigned int len);

static __inline NDIS_STATUS
VNIF_GET_STATUS_FROM_FLAGS(PVNIF_ADAPTER adapter)
{
	NDIS_STATUS status;

	if (VNIF_TEST_FLAG(adapter, VNF_RESET_IN_PROGRESS))
		status = NDIS_STATUS_RESET_IN_PROGRESS;
	else if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_NO_LINK))
		status = NDIS_STATUS_NO_CABLE;
	else
		status = NDIS_STATUS_FAILURE;
	return status;
}

#ifdef XENNET_TRACK_TX
static __inline void
xennet_print_req(txlist_t *txlist)
{
	uint32_t i, j;
	uint32_t idx;

	DBG_PRINT(("txlist cons %x, prod %x\n", txlist->cons, txlist->prod));
	for (j = 0, i = txlist->prod; j < VNIF_MAX_BUSY_SENDS; j++, i++) {
		idx = i & (VNIF_MAX_BUSY_SENDS - 1);
		if (txlist->list[idx].state) {
			DBG_PRINT(("txlist %d: tcb %x, ref %x, rid %x, st %x, sf %x, ef %x cf %x.\n",
				idx,
				txlist->list[idx].id,
				txlist->list[idx].ref,
				txlist->list[idx].rid,
				txlist->list[idx].state,
				txlist->list[idx].sflags,
				txlist->list[idx].eflags,
				gnttab_query_foreign_access_flags(txlist->list[idx].ref)));
		}
	}
}

static __inline void
xennet_save_req(txlist_t *txlist, TCB *tcb, uint32_t ridx)
{
	uint32_t idx;

	if (tcb->granted) {
		DBG_PRINT(("Reusing TCB before cleared: %p ref %x, id %x, granted %d.\n",
			tcb, tcb->grant_tx_ref, tcb->index, tcb->granted));
	}
	tcb->granted++;
	tcb->ringidx = ridx;
	idx = txlist->prod & (VNIF_MAX_BUSY_SENDS - 1);
	if (txlist->list[idx].state == 1) {
		DBG_PRINT(("txlist wrapped %d: tcb %x, ref %x, rid %x, st %x, sf %x, ef %x cf %x.\n",
			idx,
			txlist->list[idx].id,
			txlist->list[idx].ref,
			txlist->list[idx].rid,
			txlist->list[idx].state,
			txlist->list[idx].sflags,
			txlist->list[idx].eflags,
			gnttab_query_foreign_access_flags(txlist->list[idx].ref)));
	}
	txlist->list[idx].ref = tcb->grant_tx_ref;
	txlist->list[idx].id = tcb->index;
	txlist->list[idx].rid = tcb->ringidx;
	txlist->list[idx].state = 1;
	txlist->list[idx].sflags =
		gnttab_query_foreign_access_flags(tcb->grant_tx_ref);
	txlist->list[idx].eflags = 0;
	//DBG_PRINT(("txlist adding %p: ref %x, idx %d, c %x, p %x\n",
	//	tcb, txlist->list[idx].ref, idx, txlist->cons, txlist->prod));
	txlist->prod++;
}

static __inline void
xennet_clear_req(txlist_t *txlist, TCB *tcb)
{
	uint32_t idx;
	uint32_t i;
	uint32_t j;
	//uint32_t printed = 0;

	//DBG_PRINT(("txlist clearing %p: ref %x, c %x, p %x\n",
	//	tcb, tcb->grant_tx_ref, txlist->cons, txlist->prod));
	tcb->granted-- ;
	tcb->ringidx = 0;
	for (i = txlist->cons; i < txlist->prod; i++) {
		idx = i & (VNIF_MAX_BUSY_SENDS - 1);
		if (txlist->list[idx].ref == tcb->grant_tx_ref) {
			if (i == txlist->cons) {
				txlist->list[idx].state = 0;
			}
			else {
				txlist->list[idx].state = 2;
				/*
				DBG_PRINT(("\n\t found it: ref %x, idx %d, i %x, c %x, p %x\n",
					txlist->list[idx].ref, i - txlist->cons, i,
					txlist->cons, txlist->prod));
				*/
			}
			txlist->list[idx].eflags =
				gnttab_query_foreign_access_flags(tcb->grant_tx_ref);
			txlist->cons = i + 1;
			return;
		}
		/*
		else {
			if (!printed) {
				DBG_PRINT(("txlist looking forward for ref %x: ",
						   tcb->grant_tx_ref));
				printed++;
			}
			DBG_PRINT(("%x:%x ", idx, txlist->list[idx].ref));
		}
		*/
	}

	j = VNIF_MAX_BUSY_SENDS - (txlist->prod - txlist->cons);
	//DBG_PRINT(("\ntxlist looking backwards for ref %x: ", tcb->grant_tx_ref));
	for (i = txlist->cons - 1; j; i--, j--) {
		idx = i & (VNIF_MAX_BUSY_SENDS - 1);
		if (txlist->list[idx].ref == tcb->grant_tx_ref) {
			txlist->list[idx].state = 3;
			/*
			DBG_PRINT(("\n\tfound it: ref %x, idx %d, i %x:%x, c %x, p %x\n",
				txlist->list[idx].ref, i - txlist->cons, i, idx,
				txlist->cons, txlist->prod));
			*/
			txlist->list[idx].eflags =
				gnttab_query_foreign_access_flags(tcb->grant_tx_ref);
			return;
		}
		/*
		else {
			DBG_PRINT(("%x:%x ", idx, txlist->list[idx].ref));
		}
		*/
	}
	DBG_PRINT(("\ntxlist req couldn't find it: ref %x, c %x, p %x\n",
			   tcb->grant_tx_ref, txlist->cons, txlist->prod));
}
#else
#define xennet_print_req(_txlist)
#define xennet_save_req(_txlist, _tcb, _ridx)
#define xennet_clear_req(_txlist, _tcb)
#endif

#endif


/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  venet.c:  Implement venet - virtual ethernet.
 *
 * Author:
 *	 Peter W. Morreale <pmorreale@novell.com>
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

NDIS_HANDLE	 mp_handle;
NDIS_HANDLE	 driver_context;
static LIST_ENTRY       adapterList;
static NDIS_SPIN_LOCK   adapterListLock;


VOID
VenetAttach(PADAPTER a)
{
	NdisInterlockedInsertTailList(&adapterList, &a->list, &adapterListLock);
}

VOID
VenetDetach(PADAPTER a)
{
	NdisAcquireSpinLock(&adapterListLock);
	RemoveEntryList(&a->list);
	NdisReleaseSpinLock(&adapterListLock);
}

VOID
VenetSetLinkState(PADAPTER a, BOOLEAN state)
{

        NDIS_LINK_STATE                link;
        NDIS_STATUS_INDICATION         status;

        NdisZeroMemory(&link, sizeof(NDIS_LINK_STATE));
        NdisZeroMemory(&status, sizeof(NDIS_STATUS_INDICATION));

        link.Header.Revision = NDIS_LINK_STATE_REVISION_1;
        link.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
        link.Header.Size = sizeof(NDIS_LINK_STATE);

        status.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
        status.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
        status.Header.Size = sizeof(NDIS_STATUS_INDICATION);
        status.SourceHandle = a->adapterHandle;
        status.StatusCode = NDIS_STATUS_LINK_STATE;
        status.StatusBuffer = &link;
        status.StatusBufferSize = sizeof(link);

        if (state) {
                link.MediaConnectState = MediaConnectStateConnected;
                link.MediaDuplexState = MediaDuplexStateFull;
                link.XmitLinkSpeed = 1000000000;
        }
        else {
                link.MediaConnectState = MediaConnectStateDisconnected;
                link.MediaDuplexState = MediaDuplexStateUnknown;
                link.XmitLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
        }


        NdisMIndicateStatusEx(a->adapterHandle, &status);
}

VOID
VenetSetSyncFlag(PADAPTER a, int flag)
{
	/* Need to sync for setting this state */
	NdisAcquireSpinLock(&a->recvLock);
	NdisAcquireSpinLock(&a->sendLock);
	VENET_SET_FLAG(a, flag);
	NdisReleaseSpinLock(&a->sendLock);
	NdisReleaseSpinLock(&a->recvLock);
}

VOID
VenetResetTimerDpc(PVOID s1, PVOID context, PVOID s2, PVOID s3)
{
	PADAPTER	a = (PADAPTER) context;
	LARGE_INTEGER	interval;
	BOOLEAN		done = TRUE;
	NDIS_STATUS	rc = NDIS_STATUS_SOFT_ERRORS;
	UNREFERENCED_PARAMETER(s1);
	UNREFERENCED_PARAMETER(s2);
	UNREFERENCED_PARAMETER(s3);

	VENET_ADAPTER_GET(a);

	if (VENET_IS_BUSY(a)) {
		done = FALSE;
		VenetFreeQueuedSend(a, NDIS_STATUS_RESET_IN_PROGRESS);
	}

	/* Not done, try again? */
	if (!done && ++a->resetCount <= 20) {
		interval.QuadPart = 500;
		NdisSetTimerObject(a->resetTimer, interval, 0, NULL);
		VENET_ADAPTER_PUT(a);
		return;
	}

	if (done) {
		VENET_CLEAR_FLAG(a, VNET_ADAPTER_RESET);
		rc = NDIS_STATUS_SUCCESS;
	}

	NdisMResetComplete(a->adapterHandle, rc, FALSE);
	VENET_ADAPTER_PUT(a);
}

VOID    
VenetReceiveTimerDpc(PVOID s1, PVOID context, PVOID s2, PVOID s3)
{               
	PADAPTER	a = (PADAPTER) context;
	UNREFERENCED_PARAMETER(s1);
	UNREFERENCED_PARAMETER(s2);
	UNREFERENCED_PARAMETER(s3);
		        
	if (!IsListEmpty(&a->recvToProcess)) 
		VenetReceivePackets(a);
}


NDIS_STATUS 
VenetSetOptions(NDIS_HANDLE driver_handle, NDIS_HANDLE driver_context)
{
	UNREFERENCED_PARAMETER(driver_handle);
	UNREFERENCED_PARAMETER(driver_context);
	vlog("setoptions called");
	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS 
VenetGetMacAddress(PADAPTER a) 
{
	/* XXX */

	a->permAddress[0] = 0xde;
	a->permAddress[1] = 0xad;
	a->permAddress[2] = 0xbe;
	a->permAddress[3] = 0xef;
	a->permAddress[4] = 0x02;
	a->permAddress[5] = 0x07;
	ETH_COPY_NETWORK_ADDRESS(a->currAddress, a->permAddress);

	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS 
VenetRegisterSG(PADAPTER a) 
{
	NDIS_SG_DMA_DESCRIPTION d;
	NDIS_STATUS rc;

	NdisZeroMemory(&d, sizeof(d));

	d.Header.Type = NDIS_OBJECT_TYPE_SG_DMA_DESCRIPTION;
	d.Header.Revision = NDIS_SG_DMA_DESCRIPTION_REVISION_1;
	d.Header.Size = NDIS_SIZEOF_SG_DMA_DESCRIPTION_REVISION_1;
	d.Flags = NDIS_SG_DMA_64_BIT_ADDRESS;
	d.MaximumPhysicalMapping = ETH_MAX_PACKET_SIZE;
	d.ProcessSGListHandler = VenetProcessSGList;
	d.SharedMemAllocateCompleteHandler = NULL;
	rc = NdisMRegisterScatterGatherDma(a->adapterHandle, &d, &a->dmaHandle);

	a->sgSize = d.ScatterGatherListSize;

	return rc;
}

NDIS_STATUS 
VenetSetRegistrationAttributes(NDIS_HANDLE handle, PADAPTER a) 
{
	NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES	attr;

	NdisZeroMemory(&attr, sizeof(attr));
	attr.Header.Type = 
		NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
	attr.Header.Revision = 
		NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
	attr.Header.Size = 
		sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES);

	attr.MiniportAdapterContext = a;
	attr.AttributeFlags = NDIS_MINIPORT_ATTRIBUTES_NO_HALT_ON_SUSPEND | 
			      NDIS_MINIPORT_ATTRIBUTES_NOT_CO_NDIS |
			      NDIS_MINIPORT_ATTRIBUTES_BUS_MASTER;
	attr.CheckForHangTimeInSeconds = 128;
	attr.InterfaceType = NdisInterfaceInternal;

	return (NdisMSetMiniportAttributes(handle,
				(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES) &attr));
}

static NDIS_STATUS
VenetSetGeneralAttributes(NDIS_HANDLE handle, PADAPTER a)
{
	NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES attr;

	NdisZeroMemory(&attr, sizeof(NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES));
	attr.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
	attr.Header.Revision = 
		NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
	attr.Header.Size = sizeof(NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES);

	attr.MediaType = NdisMedium802_3;
	attr.MtuSize = ETH_MAX_DATA_SIZE;
	attr.MaxXmitLinkSpeed = NIC_MEDIA_MAX_SPEED;
	attr.MaxRcvLinkSpeed = NIC_MEDIA_MAX_SPEED;
	attr.XmitLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
	attr.RcvLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
	attr.MediaConnectState = MediaConnectStateUnknown;
	attr.MediaDuplexState = MediaDuplexStateUnknown;
	attr.LookaheadSize = ETH_MAX_DATA_SIZE;
	attr.PowerManagementCapabilities = NULL;

	attr.MacOptions = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
		NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
		NDIS_MAC_OPTION_8021P_PRIORITY |
		NDIS_MAC_OPTION_NO_LOOPBACK;

	attr.SupportedPacketFilters = NDIS_PACKET_TYPE_DIRECTED |
		NDIS_PACKET_TYPE_MULTICAST |
		NDIS_PACKET_TYPE_ALL_MULTICAST |
		NDIS_PACKET_TYPE_BROADCAST;

	attr.MaxMulticastListSize = NIC_MAX_MCAST_LIST;
	attr.MacAddressLength = ETH_LENGTH_OF_ADDRESS;
	NdisMoveMemory(attr.PermanentMacAddress, a->permAddress, 
			ETH_LENGTH_OF_ADDRESS);
	NdisMoveMemory(attr.CurrentMacAddress, a->currAddress, 
			ETH_LENGTH_OF_ADDRESS);

	/* Must be NdisPhysicalMedium802_3 to pass WHQL test. */
	attr.PhysicalMediumType = NdisPhysicalMedium802_3;
	attr.RecvScaleCapabilities = NULL;
	attr.AccessType = NET_IF_ACCESS_BROADCAST;
	attr.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
	attr.ConnectionType = NET_IF_CONNECTION_DEDICATED;
	attr.IfType = IF_TYPE_ETHERNET_CSMACD;
	attr.IfConnectorPresent = TRUE; // RFC 2665 TRUE if physical adapter

	attr.SupportedStatistics =
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

	attr.SupportedOidList = VenetSupportedOids;
	attr.SupportedOidListLength = sizeof(*VenetSupportedOids);

	return (NdisMSetMiniportAttributes(handle, 
				(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES) &attr));
}

static NDIS_STATUS
VenetRegistryParams(PADAPTER a)
{
	UNREFERENCED_PARAMETER(a);
	return NDIS_STATUS_SUCCESS;
}

static VOID
VenetFreeTx(PADAPTER a)
{
	LIST_ENTRY 	*e;
	PTCB		t;

	/* Free list */
	while(!IsListEmpty(&a->tcbFree)) {
		e = RemoveHeadList(&a->tcbFree);
		t = CONTAINING_RECORD(e, TCB, list);
		VenetFree(t->sgListBuffer, a->sgSize);
		VenetFree(t, sizeof(ADAPTER));
	}

	/* Busy list.  Sanity check, should be empty */
	while(!IsListEmpty(&a->tcbBusy)) {
		e = RemoveHeadList(&a->tcbBusy);
		t = CONTAINING_RECORD(e, TCB, list);
		VenetFree(t->sgListBuffer, a->sgSize);
		VenetFree(t, sizeof(ADAPTER));
	}

	if (a->tx_handle) 
		a->vif.detach(a->tx_handle);

	a->tx_handle = NULL;
}

static NDIS_STATUS
VenetSetupTx(PADAPTER a)
{
	ULONG 		i;
	PTCB		t;

	/* 
	 * Allocate our TCB's.  This is the total number of 
	 * active send packets we can have outstanding.
	 */
	for(i = 0; i < a->numTcbs; i++) {
		t = (PTCB) VenetAlloc(sizeof(TCB));
		if (!t) 
			return STATUS_NO_MEMORY;
		t->sgListBuffer = VenetAlloc(a->sgSize);
		if (!t->sgListBuffer) {
			VenetFree(t, sizeof(TCB));
			return STATUS_NO_MEMORY;
		}
		t->adapter = (PVOID) a;
		InsertHeadList(&a->tcbFree, &t->list);
	}
	a->numTCBsFree = a->numTcbs;

	a->tx_handle = a->vif.attach(a->bus_handle, VBUS_ATTACH_SEND, VenetTxHandler);
	if (!a->tx_handle) {
		VenetFreeTx(a);
		return NDIS_STATUS_FAILURE;
	}

	return NDIS_STATUS_SUCCESS;
}


static NDIS_STATUS 
VenetSetupRx(PADAPTER a)
{
	NET_BUFFER_LIST_POOL_PARAMETERS p;
	NDIS_STATUS 			rc = NDIS_STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(a);
	
	NdisZeroMemory(&p, sizeof(NET_BUFFER_LIST_POOL_PARAMETERS));
	p.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	p.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
	p.Header.Size = sizeof(p);
	p.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
	p.ContextSize = 0;
	p.fAllocateNetBuffer = TRUE;
	p.PoolTag = VNET;
	//a->recv_pool = NdisAllocateNetBufferListPool(a->adapterHandle, &p); 

	return rc;
}

static VOID
VenetFreeRx(PADAPTER a)
{
	UNREFERENCED_PARAMETER(a);
}

static VOID 
VenetQuiesce(PADAPTER a)
{
	/*  we are shutting down, deal with remaining packets. */
	UNREFERENCED_PARAMETER(a);

	/* Wait for any outstanding TX to complete. */
	NdisWaitEvent(&a->sendEvent, 0);

}

static NDIS_STATUS
VenetSetupAdapter(PADAPTER a)
{

	NDIS_STATUS			rc;
	NDIS_TIMER_CHARACTERISTICS	timer;	       

	NdisAllocateSpinLock(&a->lock);
	NdisInitializeListHead(&a->list);

	QueueInit(&a->sendQueue);

	NdisInitializeListHead(&a->recvFreeList);
	NdisInitializeListHead(&a->recvToProcess);

	NdisAllocateSpinLock(&a->recvLock);
	NdisAllocateSpinLock(&a->sendLock);

	NdisInitializeEvent(&a->removeEvent);

	/* We use the opposite sense of the sendEvent, 
	 * SET == No Tx in use 
	 * UNSET == Tx in use 
	 */
	NdisInitializeEvent(&a->sendEvent);
	NdisSetEvent(&a->sendEvent);

	/* Create Rest and receive timers. */
	NdisZeroMemory(&timer, sizeof(NDIS_TIMER_CHARACTERISTICS));
	timer.Header.Type = NDIS_OBJECT_TYPE_TIMER_CHARACTERISTICS;
	timer.Header.Revision = NDIS_TIMER_CHARACTERISTICS_REVISION_1;
	timer.Header.Size = sizeof(NDIS_TIMER_CHARACTERISTICS);
	timer.AllocationTag = VNET;
	timer.TimerFunction = VenetResetTimerDpc;
	timer.FunctionContext = a;

	rc = NdisAllocateTimerObject(a->adapterHandle, &timer, &a->resetTimer);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto done;

	timer.TimerFunction = VenetReceiveTimerDpc;
	timer.FunctionContext = a;
	rc = NdisAllocateTimerObject(a->adapterHandle, &timer, &a->recvTimer);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto done;

	/* Register SG handling for Tx */
	rc = VenetRegisterSG(a);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto done;

	rc = VenetSetupRx(a);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto done;

	rc = VenetSetupTx(a);

done:
	return rc;
}

NDIS_STATUS 
VenetGetInterface(NDIS_HANDLE handle, PADAPTER a)
{
	WDF_OBJECT_ATTRIBUTES	attr;
	int			rc;
	NTSTATUS		nrc;

	/* Get my interface from Vbus... */
	NdisMGetDeviceProperty(handle, &a->pdo, &a->fdo, &a->next, NULL, NULL);
	WDF_OBJECT_ATTRIBUTES_INIT(&attr);
	nrc = WdfDeviceMiniportCreate(WdfGetDriver(), &attr, a->fdo, 
			a->next, a->pdo, &a->wdf_device);
	if (!NT_SUCCESS(nrc)) 
		return NDIS_STATUS_FAILURE;


	nrc = WdfIoTargetQueryForInterface( WdfDeviceGetIoTarget(a->wdf_device), 
				&VBUS_INTERFACE_GUID, (PINTERFACE) &a->vif, 
				VBUS_IF_SIZE, VBUS_IF_VERSION, NULL);
	if (!NT_SUCCESS(nrc))
		return NDIS_STATUS_FAILURE;

	rc = a->vif.open(a->pdo, &a->bus_handle);
	if (rc) 
		return NDIS_STATUS_FAILURE;


	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS 
VenetInitialize(NDIS_HANDLE handle, NDIS_HANDLE driver_context,
		PNDIS_MINIPORT_INIT_PARAMETERS params)
{
	NDIS_STATUS				rc = NDIS_STATUS_RESOURCES;
	PADAPTER				a = NULL;

	UNREFERENCED_PARAMETER(driver_context);
	UNREFERENCED_PARAMETER(params);

	vlog("VenetInitialize started");

	a =  VenetAlloc(sizeof(ADAPTER));
	if (!a) 
		goto err;

	/* Set default values */
	a->lookAhead = NIC_MAX_LOOKAHEAD;
	a->adapterHandle = handle;
	VENET_SET_FLAG(a, VNET_DISCONNECTED);
	InitializeListHead(&a->tcbFree);
	InitializeListHead(&a->tcbBusy);
	a->numTcbs = NIC_MAX_BUSY_SENDS;

	NdisInitializeEvent(&a->sendEvent);


	rc = VenetGetInterface(handle, a);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto err;

	rc = VenetGetMacAddress(a);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto err;

	rc = VenetRegistryParams(a);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto err;

	rc = VenetSetRegistrationAttributes(handle, a);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto err;

	rc = VenetSetGeneralAttributes(handle, a);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto err;

	rc = VenetSetupAdapter(a);
	if (rc != NDIS_STATUS_SUCCESS) 
		goto err;

	VenetAttach(a);
	VENET_CLEAR_FLAG(a, VNET_DISCONNECTED);

	vlog("VenetInitialize return SUCCESS");
	return NDIS_STATUS_SUCCESS;
err:

	vlog("VenetInitialize err return!!");

	VenetHalt(a, 0);

	return rc;
}

VOID 
VenetHalt(NDIS_HANDLE handle, NDIS_HALT_ACTION action)
{
	PADAPTER	a = (PADAPTER) handle;

	UNREFERENCED_PARAMETER(action);

	vlog("halt called");

	VENET_SET_SYNC_FLAG(a, VNET_ADAPTER_HALT_IN_PROGRESS);

	VenetShutdown(a, NdisShutdownPowerOff);

	VenetDetach(a);
	VenetFreeQueuedSend(a, NDIS_STATUS_FAILURE);

	/* Now dec and wait for the remove event */
	vlog("halt refcount = %d", a->refCount);
	VENET_ADAPTER_PUT(a);
	NdisWaitEvent(&a->removeEvent, 5000);

	/* Free resources */
	NdisFreeSpinLock(&a->Lock);
	NdisFreeTimerObject(a->resetTimer);
	NdisFreeTimerObject(a->recvTimer);

	NdisMDeregisterScatterGatherDma(a->dmaHandle);

	VenetFreeRx(a);
	VenetFreeTx(a);

	if (a->vif.close) 
		a->vif.close(a->bus_handle);

	NdisFreeSpinLock(&a->sendLock);
	NdisFreeSpinLock(&a->recvLock);
	VenetFree(a, sizeof(ADAPTER));
	vlog("halt done");
}

VOID 
VenetUnload(PDRIVER_OBJECT obj)
{
	UNREFERENCED_PARAMETER(obj);

	vlog("unload called\n");
	WdfDriverMiniportUnload(WdfGetDriver());
	NdisMDeregisterMiniportDriver(mp_handle);
}

NDIS_STATUS 
VenetPause(NDIS_HANDLE handle, PNDIS_MINIPORT_PAUSE_PARAMETERS parms)
{
	PADAPTER	a = (PADAPTER) handle;

	UNREFERENCED_PARAMETER(parms);

	vlog("pause called");
	/* 
	 * Set the pausing flag.
	 */
	VENET_SET_SYNC_FLAG(a, VNET_ADAPTER_PAUSED);

	VenetQuiesce(a);

	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS 
VenetRestart(NDIS_HANDLE handle, PNDIS_MINIPORT_RESTART_PARAMETERS parms)
{
	PADAPTER	a = (PADAPTER) handle;
	UNREFERENCED_PARAMETER(parms);

	/* 
	 * Restart is very simple, clear the paused flag.
	 */
	VENET_CLEAR_FLAG(a, VNET_ADAPTER_PAUSED);
	vlog("restart called");

	/* Reinit the send event. */
	NdisSetEvent(&a->sendEvent);

	return NDIS_STATUS_SUCCESS;
}

BOOLEAN
VenetCheckHang(NDIS_HANDLE adapter)
{
	UNREFERENCED_PARAMETER(adapter);
	vlog("checkhang called");
	return FALSE;
}


NDIS_STATUS 
VenetReset(NDIS_HANDLE handle, PBOOLEAN addr_reset)
{
	PADAPTER	a = (PADAPTER) handle;

	vlog("reset called");

	*addr_reset = FALSE;

	/* Ignore if we are halting */
	if (VENET_TEST_FLAG(a, VNET_ADAPTER_HALT_IN_PROGRESS))
		return NDIS_STATUS_FAILURE;

	/* Ignore if we are already resetting... */
	if (VENET_TEST_FLAG(a, VNET_ADAPTER_RESET))
		return NDIS_STATUS_RESET_IN_PROGRESS;

	/* Set state to reset */
	VENET_SET_SYNC_FLAG(a, VNET_ADAPTER_RESET);

	/* Free any pending sends... */
	VenetFreeQueuedSend(a, NDIS_STATUS_RESET_IN_PROGRESS);

	/* Reinit the send event. */
	NdisSetEvent(&a->sendEvent);

	/* Done with reset */
	VENET_CLEAR_FLAG(a, VNET_ADAPTER_RESET);

	return NDIS_STATUS_SUCCESS;
}

VOID 
VenetDevicePnPEvent(NDIS_HANDLE handle, PNET_DEVICE_PNP_EVENT event)
{
	PADAPTER	a = (PADAPTER) handle;

	vlog("pnpevent called");
	/* We are only concerned with surprise removal */
	if (event->DevicePnPEvent == NdisDevicePnPEventSurpriseRemoved) 
		VENET_SET_SYNC_FLAG(a, VNET_ADAPTER_SURPRISE_REMOVED);

}

VOID 
VenetShutdown(NDIS_HANDLE handle, NDIS_SHUTDOWN_ACTION action)
{
	PADAPTER	a = (PADAPTER) handle;
	UNREFERENCED_PARAMETER(action);
	vlog("shutdown called");

	/* Set state */
	VENET_SET_SYNC_FLAG(a, VNET_ADAPTER_SHUTDOWN);

	/* Quiesce */
	VenetQuiesce(a);

	/*XXX shutdown the VBUS handle */

}

VOID 
VenetCancelOidRequest(NDIS_HANDLE adapter, PVOID id)
{
	UNREFERENCED_PARAMETER(adapter);
	UNREFERENCED_PARAMETER(id);
	vlog("canceloidrequest called");
}

NDIS_STATUS 
DriverEntry(PVOID obj,PVOID path)
{
	NDIS_STATUS				rc;
	NDIS_MINIPORT_DRIVER_CHARACTERISTICS	c;
	WDF_DRIVER_CONFIG			dc;
	NTSTATUS				nrc;
	WDFDRIVER				fw_handle;


	vlog("DriverEntry started");

	if(NdisGetVersion() < 
		((VENET_NDIS_MAJOR_VERSION << 16) | VENET_NDIS_MINOR_VERSION)){
		vlog("Invalid version!");
		return NDIS_STATUS_FAILURE;
	}

	WDF_DRIVER_CONFIG_INIT(&dc, WDF_NO_EVENT_CALLBACK);
	dc.DriverInitFlags |= WdfDriverInitNoDispatchOverride;
	nrc = WdfDriverCreate(obj, path, WDF_NO_OBJECT_ATTRIBUTES, 
			&dc, &fw_handle);
	if(!NT_SUCCESS(nrc)){
		vlog("DriverEntry WdfDriverCreate: %d", nrc);
		return NDIS_STATUS_FAILURE;
	}

	/* For multiple instances */
	NdisInitializeListHead(&adapterList);
	NdisAllocateSpinLock(&adapterListLock);


	NdisZeroMemory(&c, sizeof(c));

	c.Header.Type 	  = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
	c.Header.Size 	  = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);
	c.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;

	c.Flags 	  		= NDIS_WDM_DRIVER;
	c.MajorNdisVersion		= VENET_NDIS_MAJOR_VERSION;
	c.MinorNdisVersion		= VENET_NDIS_MINOR_VERSION;
	c.MajorDriverVersion		= VENET_MAJOR_VERSION;
	c.MinorDriverVersion		= VENET_MINOR_VERSION;

	c.InitializeHandlerEx		= VenetInitialize;
	c.PauseHandler			= VenetPause; 
	c.RestartHandler		= VenetRestart; 

	c.ShutdownHandlerEx		= VenetShutdown;
	c.DevicePnPEventNotifyHandler	= VenetDevicePnPEvent;
	c.HaltHandlerEx			= VenetHalt;
	c.UnloadHandler			= VenetUnload;

	c.OidRequestHandler		= VenetOidRequest;
	c.CancelOidRequestHandler	= VenetCancelOidRequest;

	c.SetOptionsHandler		= VenetSetOptions;

	c.SendNetBufferListsHandler	= VenetSendNetBufs; 
	c.CancelSendHandler		= VenetCancelSend;
	c.ReturnNetBufferListsHandler	= VenetReturnNetBufs;

	c.CheckForHangHandlerEx		= VenetCheckHang;
	c.ResetHandlerEx		= VenetReset;

	rc = NdisMRegisterMiniportDriver(obj, path, (PNDIS_HANDLE)
			driver_context, &c, &mp_handle);
	
	vlog("DriverEntry return rc = %d", rc);
	return rc;
}

PVOID 
VenetAlloc(ULONG size)
{
	PVOID	p;

	p =  NdisAllocateMemoryWithTagPriority(mp_handle, size, VNET, 
			HighPoolPriority);
	if (p) 
		NdisZeroMemory(p, size);
	return p;
}

VOID VenetFree(PVOID p, ULONG size)
{
	if (p) 
		NdisFreeMemory(p, size, 0);
}

/* Can only be called with sendLock held */
PTCB 
VenetAllocTCB(PADAPTER a, PNET_BUFFER_LIST nbl)
{
	PLIST_ENTRY	e;
	PTCB		t = NULL;

	if (!IsListEmpty(&a->tcbFree)) {
		e = RemoveHeadList(&a->tcbFree);
		t = CONTAINING_RECORD(e, TCB, list);
		t->status = NDIS_STATUS_SUCCESS;
		t->nb_count = 0;
		t->nbl = nbl;
		InsertHeadList(&a->tcbBusy, &t->list);
		NdisResetEvent(&a->sendEvent);
		InterlockedDecrement((PLONG) &a->numTCBsFree);
	}
	return t;
}
/* Can only be called with sendLock held */
VOID 
VenetReleaseTCB(PTCB t)
{
	PADAPTER	a = (PADAPTER) t->adapter;

	RemoveEntryList(&t->list);
	InsertHeadList(&a->tcbFree, &t->list);
	InterlockedIncrement((PLONG) &a->numTCBsFree);

	/* 
	 * Signal the event that TX is quiesced
	 */
	if (IsListEmpty(&a->tcbBusy)) {
		NdisSetEvent(&a->sendEvent);
	}
}

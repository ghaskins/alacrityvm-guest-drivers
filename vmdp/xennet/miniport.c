/*****************************************************************************
 *
 *   File Name:      miniport.c
 *
 *   %version:       25 %
 *   %derived_by:    kallan %
 *   %date_modified: Fri Jun 26 16:00:16 2009 %
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

VNIF_GLOBAL_DATA Global;
KEVENT xennet_init_event;
#ifdef DBG
PVNIF_ADAPTER gmyadapter;
#endif

NDIS_STATUS
DriverEntry(PVOID DriverObject, PVOID RegistryPath)
{
	NDIS_STATUS status;

	status = DRIVER_ENTRY(DriverObject, RegistryPath);
	if(status != NDIS_STATUS_SUCCESS)
		return status;

	/* TODO: initialize global data here */
	NdisInitializeListHead(&Global.AdapterList);
	NdisAllocateSpinLock(&Global.Lock);

	KeInitializeEvent(&xennet_init_event, SynchronizationEvent, TRUE);

	DPR_INIT(("VNIF: DriverEntry - OUT %x.\n", status));
	return status;
}

#ifdef NDIS60_MINIPORT
NDIS_STATUS 
MPInitialize(
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN NDIS_HANDLE MiniportDriverContext,
	IN PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
    )
#else
NDIS_STATUS
MPInitialize(
	OUT PNDIS_STATUS OpenErrorStatus,
	OUT PUINT SelectedMediumIndex,
	IN PNDIS_MEDIUM MediumArray,
	IN UINT MediumArraySize,
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN NDIS_HANDLE WrapperConfigurationContext)
#endif
{
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	PVNIF_ADAPTER adapter;
	NDIS_HANDLE configHandle;
	UINT i, res;

	DPR_INIT(("VNIF: MPInitialize.\n"));

	KeWaitForSingleObject(
		&xennet_init_event,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	VNIF_ALLOCATE_MEMORY(
		adapter,
		sizeof(VNIF_ADAPTER),
		VNIF_POOL_TAG,
		NdisMiniportDriverHandle,
		NormalPoolPriority);
	if(adapter == NULL) {
		DBG_PRINT(("VNIF: fail to allocate memory for adapter context\n"));
		KeSetEvent(&xennet_init_event, 0, FALSE);
		return STATUS_NO_MEMORY;
	}

	DPR_INIT(("VNIF: MPInitialize - adapter = %p.\n", adapter));
#ifdef DBG
	gmyadapter = adapter;
#endif
	NdisZeroMemory(adapter, sizeof(VNIF_ADAPTER));
	VNIF_SET_FLAG(adapter, VNF_DISCONNECTED);
	adapter->AdapterHandle = MiniportAdapterHandle;

	NdisMGetDeviceProperty(adapter->AdapterHandle,
		&adapter->Pdo,
		&adapter->Fdo,
		&adapter->NextDevice,
		NULL,
		NULL);
	DPR_INIT(("VNIF: MPInitialize - adapter pdo = %p, fdo %p.\n",
			  adapter->Pdo, adapter->Fdo));

	status = VNIFFindXenAdapter(adapter);
	if (status != NDIS_STATUS_SUCCESS) {
		NdisFreeMemory(adapter, sizeof(VNIF_ADAPTER), 0);
		KeSetEvent(&xennet_init_event, 0, FALSE);
		DBG_PRINT(("MPInitialize VNIFFindXenAdapter failed = %x.\n", status));
		return NDIS_STATUS_RESOURCES;
	}

	status = VNIF_INITIALIZE(adapter,
		MediumArray,
		MediumArraySize,
		SelectedMediumIndex,
		WrapperConfigurationContext);
	if(status != NDIS_STATUS_SUCCESS) {
		VNIFFreeXenAdapter(adapter);
		NdisFreeMemory(adapter, sizeof(VNIF_ADAPTER), 0);
		KeSetEvent(&xennet_init_event, 0, FALSE);
		DBG_PRINT(("MPInitialize VNIF_INITIALIZE failed = %x.\n", status));
		return status;
	}

	/* Do all the Ndis and basic setup. */
	status = VNIFSetupNdisAdapter(adapter);
	if (status != NDIS_STATUS_SUCCESS) {
		VNIFFreeXenAdapter(adapter);
		NdisFreeMemory(adapter, sizeof(VNIF_ADAPTER), 0);
		KeSetEvent(&xennet_init_event, 0, FALSE);
		DBG_PRINT(("MPInitialize VNIFSetupNdisAdapter failed = %x.\n", status));
		return NDIS_STATUS_RESOURCES;
	}

	/* Do all the Xen specific setup. */
	status = VNIFSetupXenAdapter(adapter);
	if (status != NDIS_STATUS_SUCCESS) {
		VNIF_INC_REF(adapter);
		MP_HALT(adapter, 0);
		KeSetEvent(&xennet_init_event, 0, FALSE);
		DBG_PRINT(("MPInitialize VNIFSetupXenAdapter failed = %x.\n", status));
		return NDIS_STATUS_RESOURCES;
	}

	VNIFAttachAdapter(adapter);
	VNIF_INC_REF(adapter);
	VNIF_CLEAR_FLAG(adapter, VNF_DISCONNECTED);
	KeSetEvent(&xennet_init_event, 0, FALSE);
	DBG_PRINT(("Xennet %s: Initialization complete for %s using %d rcbs.\n",
		VER_FILEVERSION_STR, adapter->Nodename, adapter->num_rcb));
	return status;
}

#ifdef NDIS60_MINIPORT
VOID
MPHalt(IN NDIS_HANDLE MiniportAdapterContext, IN NDIS_HALT_ACTION HaltAction)
#else
VOID
MPHalt(IN NDIS_HANDLE MiniportAdapterContext)
#endif
{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;

	DPR_INIT(("VNIF: Miniport Halt for %s.\n", adapter->Nodename));

	NdisAcquireSpinLock(&adapter->RecvLock);
	NdisAcquireSpinLock(&adapter->SendLock);
	VNIF_SET_FLAG(adapter, VNF_ADAPTER_HALT_IN_PROGRESS);
	NdisReleaseSpinLock(&adapter->SendLock);    
	NdisReleaseSpinLock(&adapter->RecvLock);    

	MP_SHUTDOWN(MiniportAdapterContext, HaltAction);

	DPR_INIT(("VNIF: Miniport Halt VNIFDetachAdapter.\n"));
	VNIFDetachAdapter(adapter);

	/* Free the packets on SendWaitList */
	DPR_INIT(("VNIF: halt VNIFFreeQueuedSendPackets\n"));
	VNIFFreeQueuedSendPackets(adapter, NDIS_STATUS_FAILURE);

#ifdef NDIS50_MINIPORT
	/* Deregister shutdown handler as it's being halted */
	NdisMDeregisterAdapterShutdownHandler(adapter->AdapterHandle);
#endif

	DPR_INIT(("VNIF: halt VNIF_DEC_REF\n"));
	VNIF_DEC_REF(adapter);

	DPR_INIT(("VNIF: halt NdisWaitEvent\n"));
	NdisWaitEvent(&adapter->RemoveEvent, 0);

	/* We must wait for all our recources to be returned from the backend. */
	while (VNIFDisconnectBackend(adapter));

	/*
	 * We haven't release any receive refs in rx ring,
	 * but after disconnect, we don't care.
	 */

	VNIFFreeAdapter(adapter);
	DPR_INIT(("VNIF: MPHalt - OUT.\n"));
}

#ifdef NDIS60_MINIPORT
NDIS_STATUS 
MPReset(IN NDIS_HANDLE MiniportAdapterContext, OUT PBOOLEAN AddressingReset)
#else
NDIS_STATUS
MPReset(OUT PBOOLEAN AddressingReset, IN NDIS_HANDLE MiniportAdapterContext)
#endif
{
    LARGE_INTEGER li;
	NDIS_STATUS status;
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;
	BOOLEAN done = TRUE;

	DPR_INIT(("VNIF: Miniport Reset - IN.\n"));

	if (VNIF_TEST_FLAG(adapter, VNF_ADAPTER_HALT_IN_PROGRESS)) {
		DBG_PRINT(("VNIF: halt in progress, can't reset!\n"));
		return NDIS_STATUS_FAILURE;
	}

	if (VNIF_TEST_FLAG(adapter, VNF_RESET_IN_PROGRESS))
		return NDIS_STATUS_RESET_IN_PROGRESS;

	NdisAcquireSpinLock(&adapter->RecvLock);
	NdisAcquireSpinLock(&adapter->SendLock);
	VNIF_SET_FLAG(adapter, VNF_RESET_IN_PROGRESS);
	NdisReleaseSpinLock(&adapter->SendLock);    
	NdisReleaseSpinLock(&adapter->RecvLock);    

	VNIFFreeQueuedSendPackets(adapter, NDIS_STATUS_RESET_IN_PROGRESS);

	if (adapter->nBusyRecv)
		done = FALSE;
	if (adapter->nBusySend)
		done = FALSE;

	*AddressingReset = FALSE;

	if (!done) {
		adapter->nResetTimerCount = 0;
		li.QuadPart = 500;
		VNIF_SET_TIMER(adapter->ResetTimer, li);
		DPR_INIT(("VNIF: Miniport reset OUT %p: nrx = %x, ntx = %x.\n",
			adapter, adapter->nBusyRecv, adapter->nBusySend ));
		return NDIS_STATUS_PENDING;
	}

	VNIF_CLEAR_FLAG(adapter, VNF_RESET_IN_PROGRESS);
	DPR_INIT(("VNIF: Miniport Reset OUT.\n"));
	return NDIS_STATUS_SUCCESS;
}

VOID
VNIFResetCompleteTimerDpc(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
    LARGE_INTEGER li;
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) FunctionContext;
	BOOLEAN done = TRUE;

	DPR_INIT(("VNIF: VNIFResetCompleteTimerDpc - IN.\n"));
	VNIF_INC_REF(adapter);

	if (adapter->nBusyRecv) {
		done = FALSE;
	}

	if (adapter->nBusySend) {
		done = FALSE;
		VNIFInterruptDpc(NULL, adapter, NULL, NULL);
		VNIFFreeQueuedSendPackets(adapter, NDIS_STATUS_RESET_IN_PROGRESS);
	}

	if (!done && ++adapter->nResetTimerCount <= 20) {
		/* continue to try waiting */
		DPR_INIT(("VNIF: VNIFResetCompleteTimerDpc %p: nrx = %x, ntx = %x.\n",
			adapter, adapter->nBusyRecv, adapter->nBusySend ));
		li.QuadPart = 500;
		VNIF_SET_TIMER(adapter->ResetTimer, li);
	}
	else {
		if (!done) {
			/* try enough, fail the reset */
			DBG_PRINT(("VNIF: reset time out!\n"));
			NdisMResetComplete(adapter->AdapterHandle,
				NDIS_STATUS_SOFT_ERRORS, FALSE);
		}
		else {
			VNIF_CLEAR_FLAG(adapter, VNF_RESET_IN_PROGRESS);
			NdisMResetComplete(adapter->AdapterHandle,
				NDIS_STATUS_SUCCESS, FALSE);
		}
	}

	VNIF_DEC_REF(adapter);
	DPR_INIT(("VNIF: VNIFResetCompleteTimerDpc - OUT.\n"));
}

VOID
MPUnload(IN PDRIVER_OBJECT DriverObject)
{
	DPR_INIT(("VNIF: Miniport Unload - IN.\n"));

	ASSERT(IsListEmpty(&Global.AdapterList));
	NdisFreeSpinLock(&Global.Lock);
#ifdef NDIS60_MINIPORT
    NdisMDeregisterMiniportDriver(NdisMiniportDriverHandle);
#endif
	DPR_INIT(("VNIF: Miniport Unload - OUT.\n"));
}

/*
 * typical shutdown just disable the interrupt and stop DMA,
 * do nothing else. We are doing nothing in this function.
 */
#ifdef NDIS60_MINIPORT
VOID
MPShutdown(IN NDIS_HANDLE MiniportAdapterContext,
    IN  NDIS_SHUTDOWN_ACTION ShutdownAction)
#else
VOID
MPShutdown(IN NDIS_HANDLE MiniportAdapterContext)
#endif
{
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;

	DPR_INIT(("VNIF: MPShutdown ===>\n"));

	NdisAcquireSpinLock(&adapter->RecvLock);
	NdisAcquireSpinLock(&adapter->SendLock);
	VNIF_SET_FLAG(adapter, VNF_ADAPTER_SHUTDOWN);
	NdisReleaseSpinLock(&adapter->SendLock);    
	NdisReleaseSpinLock(&adapter->RecvLock);    

	if (adapter->nBusyRecv) {
		DBG_PRINT(("VNIF: ********************** MPShutdown for receives: %x\n",
			adapter->nBusyRecv));
	}
	if (adapter->nBusySend) {
		DBG_PRINT(("VNIF: *********************** MPShutdown for sends: %x\n",
			adapter->nBusySend));
	}

	VNIFQuiesce(adapter);
	DPR_INIT(("VNIF: MPShutdown <===\n"));
}

BOOLEAN
MPCheckForHang(IN NDIS_HANDLE MiniportAdapterContext)
{
	//DPR_INIT(("VNIF: MPCheckForHang.\n"));
	return FALSE;
}

VOID
MPHandleInterrupt(IN NDIS_HANDLE MiniportAdapterContext)
{
	DPR_INIT(("VNIF: MPHandleInterrupt.\n"));
}

VOID
MPIsr(
	OUT PBOOLEAN InterruptRecognized,
	OUT PBOOLEAN QueueMiniportHandleInterrupt,
	IN NDIS_HANDLE MiniportAdapterContext)
{
	DPR_INIT(("VNIF: MPIsr.\n"));
}

VOID
MPDisableInterrupt(IN PVOID MiniportAdapterContext)
{
	DPR_INIT(("VNIF: MPDisableInterrupt.\n"));
}

VOID
MPEnableInterrupt(IN PVOID MiniportAdapterContext)
{
	DPR_INIT(("VNIF: MPEnableInterrupt.\n"));
}

VOID
MPAllocateComplete(
	NDIS_HANDLE MiniportAdapterContext,
	IN PVOID VirtualAddress,
	IN PNDIS_PHYSICAL_ADDRESS PhysicalAddress,
	IN ULONG Length,
	IN PVOID Context)
{
	DPR_INIT(("VNIF: MPAllocateComplete.\n"));
}

#if defined (NDIS60_MINIPORT) || defined (NDIS51_MINIPORT)
#if defined (NDIS60_MINIPORT)
VOID
MPPnPEventNotify(NDIS_HANDLE MiniportAdapterContext,
    IN PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent)
{
    NDIS_DEVICE_PNP_EVENT   PnPEvent = NetDevicePnPEvent->DevicePnPEvent;
#elif defined (NDIS51_MINIPORT)
VOID
MPPnPEventNotify(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_DEVICE_PNP_EVENT PnPEvent,
	IN PVOID InformationBuffer,
	IN ULONG InformationBufferLength)
{
#endif
	PVNIF_ADAPTER adapter = (PVNIF_ADAPTER) MiniportAdapterContext;
	PNDIS_POWER_PROFILE NdisPowerProfile;

	DPR_INIT(("VNIF: MPPnPEventNotify - IN %x\n", PnPEvent));
	switch (PnPEvent) {
		case NdisDevicePnPEventQueryRemoved:
			//
			// Called when NDIS receives IRP_MN_QUERY_REMOVE_DEVICE.
			//
			break;

		case NdisDevicePnPEventRemoved:
			//
			// Called when NDIS receives IRP_MN_REMOVE_DEVICE.
			// NDIS calls MiniportHalt function after this call returns.
			//
			break;

		case NdisDevicePnPEventSurpriseRemoved:
			//
			// Called when NDIS receives IRP_MN_SUPRISE_REMOVAL.
			// NDIS calls MiniportHalt function after this call returns.
			//
			NdisAcquireSpinLock(&adapter->RecvLock);
			NdisAcquireSpinLock(&adapter->SendLock);
			VNIF_SET_FLAG(adapter, VNF_ADAPTER_SURPRISE_REMOVED);
			NdisReleaseSpinLock(&adapter->SendLock);    
			NdisReleaseSpinLock(&adapter->RecvLock);    
			break;

		case NdisDevicePnPEventQueryStopped:
			//
			// Called when NDIS receives IRP_MN_QUERY_STOP_DEVICE. ??
			//
			break;

		case NdisDevicePnPEventStopped:
			//
			// Called when NDIS receives IRP_MN_STOP_DEVICE.
			// NDIS calls MiniportHalt function after this call returns.
			//
			//
			break;

		case NdisDevicePnPEventPowerProfileChanged:
			//
			// After initializing a miniport driver and after miniport driver
			// receives an OID_PNP_SET_POWER notification that specifies
			// a device power state of NdisDeviceStateD0 (the powered-on state),
			// NDIS calls the miniport's MiniportPnPEventNotify function with
			// PnPEvent set to NdisDevicePnPEventPowerProfileChanged.
			//

			/*
			if(InformationBufferLength == sizeof(NDIS_POWER_PROFILE)) {
				NdisPowerProfile = (PNDIS_POWER_PROFILE)InformationBuffer;
				if(*NdisPowerProfile == NdisPowerProfileBattery) {
				}
				if(*NdisPowerProfile == NdisPowerProfileAcOnLine) {
				}
			}
			*/
			break;

		default:
			break;
	}
	DPR_INIT(("VNIF: MPPnPEventNotify - OUT\n"));
}
#endif

/*****************************************************************************
 *
 *   File Name:      interfacd.c
 *
 *   %version:       34 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jul 02 15:00:02 2009 %
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

/*
 * Our vnif driver have to know nodename, otherend and backend ID of the
 * device, they are part of the target device pdo's device extension. We
 * are here access them using the exported functions by bus driver.
 *
 * A normal driver should not rely on such techniques. However, in our driver,
 * this is a much easier way to implement. By this approach, some of the kernel
 * mode driver stack characteristics are circumvented. The internal device
 * control or driver interface may be the way to go.
 *
 * Trying to use defeinitions from ntddk.h in a network miniport driver is
 * simply a disaster. so we are not trying to include ntddk.h and use
 * ExFreePool to free he string from xenstore, this is done by exported
 * functions again.
 *
 * These behaviors are subjected to change in the future.
 */


static domid_t VNIFGetBackendIDFromPDO(PDEVICE_OBJECT pdo);
static int VNIFSetupPermanentAddress(PVNIF_ADAPTER adapter);
static int VNIFSetupXenFlags(PVNIF_ADAPTER Adapter);
static int VNIFTalkToBackend(PVNIF_ADAPTER Adapter);
static NDIS_STATUS VNIFInitRxGrants(PVNIF_ADAPTER adapter);
static NDIS_STATUS VNIFInitTxGrants(PVNIF_ADAPTER adapter);
static void	xennet_frontend_changed(struct xenbus_watch *watch,
	const char **vec, unsigned int len);
static void MPResume(PVNIF_ADAPTER adapter, uint32_t suspend_canceled);
static uint32_t MPSuspend(PVNIF_ADAPTER adapter, uint32_t reason);
static uint32_t MPIoctl(PVNIF_ADAPTER adapter, pv_ioctl_t data);

void
VNIFFreeXenAdapter(PVNIF_ADAPTER adapter)
{
	if (adapter->Nodename)
		NdisFreeMemory(adapter->Nodename, strlen(adapter->Nodename) + 1, 0);
	if (adapter->Otherend)
		NdisFreeMemory(adapter->Otherend, strlen(adapter->Otherend) + 1, 0);
	if (adapter->RCBArray) {
		NdisFreeMemory(adapter->RCBArray, sizeof(void *) * adapter->num_rcb, 0);
	}
}

NDIS_STATUS
VNIFFindXenAdapter(PVNIF_ADAPTER adapter)
{
	PUCHAR nodename, otherend;
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
	UINT i;

	do {
		/* Nodename */
		DPR_INIT(("VNIFFindXenAdapter: IN VNIFGetNodenameFromPDO\n"));
		nodename = xenbus_get_nodename_from_pdo(adapter->Pdo);
		if (nodename == NULL) {
			DBG_PRINT(("VNIF: failed to get nodename.\n"));
			status = NDIS_STATUS_FAILURE;
			break;
		}
		i = strlen(nodename) + 1;
		VNIF_ALLOCATE_MEMORY(
			adapter->Nodename,
			i,
			VNIF_POOL_TAG,
			NdisMiniportDriverHandle,
			NormalPoolPriority);
		if(adapter->Nodename == NULL) {
			DBG_PRINT(("VNIF: allocating memory for nodename fail.\n"));
			status = STATUS_NO_MEMORY;
			break;
		}
		NdisMoveMemory(adapter->Nodename, nodename, i);

		/* Otherend */
		otherend = xenbus_get_otherend_from_pdo(adapter->Pdo);
		if (otherend == NULL) {
			DBG_PRINT(("VNIF: failed to get otherend.\n"));
			status = NDIS_STATUS_FAILURE;
			break;
		}
		i = strlen(otherend) + 1;
		VNIF_ALLOCATE_MEMORY(
			adapter->Otherend,
			i,
			VNIF_POOL_TAG,
			NdisMiniportDriverHandle,
			NormalPoolPriority);
		if(adapter->Otherend == NULL) {
			DBG_PRINT(("VNIF: allocating memory for otherend fail.\n"));
			status = STATUS_NO_MEMORY;
			break;
		}
		NdisMoveMemory(adapter->Otherend, otherend, i);

		/* Backend */
		adapter->BackendID = VNIFGetBackendIDFromPDO(adapter->Pdo);
		if (adapter->BackendID == (domid_t)-1) {
			DBG_PRINT(("VNIF: failed to get backend id.\n"));
			status = NDIS_STATUS_FAILURE;
			break;
		}

		/* MAC */
		if (!VNIFSetupPermanentAddress(adapter)) {
			DBG_PRINT(("VNIF: set NIC MAC fail.\n"));
			status = NDIS_STATUS_FAILURE;
			break;
		}
		DPR_INIT(("VNIFFindXenAdapter: OUT %p, %s\n",
			adapter, adapter->Nodename));
	} while (FALSE);

	if (status != NDIS_STATUS_SUCCESS) {
		VNIFFreeXenAdapter(adapter);
	}
	return status;
}

NDIS_STATUS
VNIFSetupXenAdapter(PVNIF_ADAPTER adapter)
{
	NTSTATUS status;

	/* Set Flags */
	if (!VNIFSetupXenFlags(adapter)) {
		DBG_PRINT(("VNIF: set xen features fail.\n"));
		return NDIS_STATUS_FAILURE;
	}
	/* Let backend know our configuration */
	if (VNIFTalkToBackend(adapter)) {
		DBG_PRINT(("VNIF: NIC talk to backend fail.\n"));
		return NDIS_STATUS_FAILURE;
	}

	status = VNIFInitRxGrants(adapter);
	if (status != NDIS_STATUS_SUCCESS) {
		return NDIS_STATUS_FAILURE;
	}

	status = VNIFInitTxGrants(adapter);
	if (status != NDIS_STATUS_SUCCESS) {
		return NDIS_STATUS_FAILURE;
	}

	DPR_INIT(("VNIFSetupXenAdapter: claim %p, %s\n",
		adapter, adapter->Nodename));
	status = xenbus_claim_device(adapter, NULL, vnif, none, MPIoctl, MPIoctl);
	if (status != NDIS_STATUS_SUCCESS && status != STATUS_RESOURCE_IN_USE) {
		return status;
	}

	InitializeListHead(&adapter->watch.list);
	adapter->watch.callback = xennet_frontend_changed;
	adapter->watch.node = adapter->Nodename;
	adapter->watch.flags = XBWF_new_thread;
	adapter->watch.context = adapter;

	/* Get the initial status. */
	DPR_INIT(("VNIFSetupXenAdapter: xennet_frontend_changed %p, %s\n",
		adapter, adapter->Nodename));
	VNIFIndicateLinkStatus(adapter, 1);
	xennet_frontend_changed(&adapter->watch, NULL, 0);

	/* Enable the interrupt */
	register_dpc_to_evtchn(adapter->evtchn,	VNIFInterruptDpc, adapter, NULL);

	/* Now register for the watch. */
	DPR_INIT(("VNIFSetupXenAdapter: register_xenbus_watch %p, %s\n",
		adapter, adapter->Nodename));
	register_xenbus_watch(&adapter->watch);

	DPR_INIT(("VNIFSetupXenAdapter: OUT %p, %s\n", adapter, adapter->Nodename));
	return STATUS_SUCCESS;
}

static domid_t
VNIFGetBackendIDFromPDO(PDEVICE_OBJECT pdo)
{
    char *bidstr;
    domid_t bid;

    bidstr = xenbus_get_backendid_from_pdo(pdo);
	if (bidstr) {
		bid = (domid_t) cmp_strtoul(bidstr, NULL, 10);
		return bid;
	}
	return (domid_t)-1;

}

static int
VNIFSetupPermanentAddress(PVNIF_ADAPTER adapter)
{
	int res, i;
	unsigned long val;
	char *mac, *ptr, *str;

	res = xenbus_exists(XBT_NIL, adapter->Otherend, "mac");
	if (res == 0)
		return 0;

	str = mac = (char *)xenbus_read(XBT_NIL, adapter->Otherend, "mac", &res);
	if (mac == NULL)
		return 0;

	res = 1;
	for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++) {
		val = (unsigned long)cmp_strtoul(mac, &ptr, 16);
		if (ptr != mac)
			adapter->PermanentAddress[i] = (UCHAR) val;
		else
			res = 0;
		mac = ptr + 1;
	}
	xenbus_free_string(str);

	/* If the current address isn't already setup do it now. */
	if (adapter->CurrentAddress[0] == 0
		&& adapter->CurrentAddress[1] == 0
		&& adapter->CurrentAddress[2] == 0
		&& adapter->CurrentAddress[3] == 0
		&& adapter->CurrentAddress[4] == 0
		&& adapter->CurrentAddress[5] == 0) {

		ETH_COPY_NETWORK_ADDRESS(
			adapter->CurrentAddress,
            adapter->PermanentAddress);
	}

	DPR_INIT(("VNIFSetupPermAddr: Perm Addr = %02x-%02x-%02x-%02x-%02x-%02x\n",
		adapter->PermanentAddress[0],
		adapter->PermanentAddress[1],
		adapter->PermanentAddress[2],
		adapter->PermanentAddress[3],
		adapter->PermanentAddress[4],
		adapter->PermanentAddress[5]));

	DPR_INIT(("VNIFSetupPermAddr: Cur Addr = %02x-%02x-%02x-%02x-%02x-%02x\n",
		adapter->CurrentAddress[0],
		adapter->CurrentAddress[1],
		adapter->CurrentAddress[2],
		adapter->CurrentAddress[3],
		adapter->CurrentAddress[4],
		adapter->CurrentAddress[5]));

	return res;
}

static int
VNIFSetupXenFlags(PVNIF_ADAPTER Adapter)
{
    int res;
    unsigned int feature_rx_copy;
    char *val;

    val = xenbus_read(XBT_NIL, Adapter->Otherend, "feature-rx-copy", NULL);
    if (val == NULL) {
        DBG_PRINT(("VNIF: backend/feature-rx-copy missing.\n"));
        return 0;
    }

    feature_rx_copy = (unsigned int)cmp_strtoul(val, NULL, 10);
    Adapter->copyall = feature_rx_copy;
    xenbus_free_string(val);

    return 1;
}

static int
VNIFSetupDevice(PVNIF_ADAPTER Adapter)
{
    struct netif_tx_sring *txs;
    struct netif_rx_sring *rxs;
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    int err;

    DPR_INIT(("VNIF: VNIFSetupDevice - IN\n"));

    Adapter->tx_ring_ref = GRANT_INVALID_REF;
    Adapter->rx_ring_ref = GRANT_INVALID_REF;
    Adapter->rx.sring = NULL;
    Adapter->tx.sring = NULL;

	VNIF_ALLOCATE_MEMORY(
		txs,
		PAGE_SIZE,
		VNIF_POOL_TAG,
		NdisMiniportDriverHandle,
		NormalPoolPriority);
	if(txs == NULL) {
        DBG_PRINT(("VNIF: allocating tx ring page fail.\n"));
		status = STATUS_NO_MEMORY;
		goto fail;
	}

    SHARED_RING_INIT(txs);
    WIN_FRONT_RING_INIT(&Adapter->tx, txs, PAGE_SIZE);

    err = xenbus_grant_ring(Adapter->BackendID, virt_to_mfn(txs));
    if (err < 0) {
        NdisFreeMemory(txs, PAGE_SIZE, 0);
        goto fail;
    }
    Adapter->tx_ring_ref = err;

	VNIF_ALLOCATE_MEMORY(
		rxs,
		PAGE_SIZE,
		VNIF_POOL_TAG,
		NdisMiniportDriverHandle,
		NormalPoolPriority);
	if(rxs == NULL) {
        DBG_PRINT(("VNIF: allocating rx ring page fail.\n"));
		status = STATUS_NO_MEMORY;
		goto fail;
	}

    SHARED_RING_INIT(rxs);
    WIN_FRONT_RING_INIT(&Adapter->rx, rxs, PAGE_SIZE);

    err = xenbus_grant_ring(Adapter->BackendID, virt_to_mfn(rxs));
    if (err < 0) {
        NdisFreeMemory(txs, PAGE_SIZE, 0);
        NdisFreeMemory(rxs, PAGE_SIZE, 0);
        goto fail;
    }
    Adapter->rx_ring_ref = err;

    err = xenbus_alloc_evtchn(Adapter->BackendID, &Adapter->evtchn);
    if (err) {
        NdisFreeMemory(txs, PAGE_SIZE, 0);
        NdisFreeMemory(rxs, PAGE_SIZE, 0);
        goto fail;
    }

    DPR_INIT(("VNIF: VNIFSetupDevice - OUT success\n"));
    return 0;

 fail:
	 DPR_INIT(("VNIF: VNIFSetupDevice - OUT error %x\n", err));
    return err;
}

static int
VNIFTalkToBackend(PVNIF_ADAPTER Adapter)
{
    int err;
    struct xenbus_transaction xbt;

    DPR_INIT(("VNIF: VNIFTalkToBackend - IN\n"));

    err = VNIFSetupDevice(Adapter);
    if (err) {
        goto out;
    }

again:
	DPR_INIT(("VNIF: xenbus starting transaction.\n"));
	err = xenbus_transaction_start(&xbt);
	if (err) {
		DBG_PRINT(("VNIF: xenbus starting transaction fail.\n"));
		goto out;
	}

	DPR_INIT(("VNIF: xenbus writing tx ring-ref.\n"));
	err = xenbus_printf(xbt, Adapter->Nodename, "tx-ring-ref", "%u",
				  Adapter->tx_ring_ref);
	if (err) {
		DBG_PRINT(("VNIF: xenbus writing tx ring-ref fail.\n"));
		goto abort_transaction;
	}

	DPR_INIT(("VNIF: xenbus writing rx ring-ref.\n"));
	err = xenbus_printf(xbt, Adapter->Nodename, "rx-ring-ref", "%u",
				  Adapter->rx_ring_ref);
	if (err) {
		DBG_PRINT(("VNIF: xenbus writing rx ring-ref fail.\n"));
		goto abort_transaction;
	}

	DPR_INIT(("VNIF: xenbus writing event-channel.\n"));
	err = xenbus_printf(xbt, Adapter->Nodename, "event-channel", "%u",
				  Adapter->evtchn);
	if (err) {
		DBG_PRINT(("VNIF: xenbus writing event-channel fail.\n"));
		goto abort_transaction;
	}

	DPR_INIT(("VNIF: xenbus writing feature-rx-notify.\n"));
	err = xenbus_printf(xbt, Adapter->Nodename, "feature-rx-notify", "%d", 1);
	if (err) {
		DBG_PRINT(("VNIF: xenbus writing feature-rx-notify fail.\n"));
		goto abort_transaction;
	}

	/* 1=copyall, 0=page-flipping */
	DPR_INIT(("VNIF: xenbus writing request-rx-copy.\n"));
	err = xenbus_printf(xbt, Adapter->Nodename, "request-rx-copy", "%u",
				  Adapter->copyall);
	if (err) {
		DBG_PRINT(("VNIF: xenbus writing request-rx-copy fail.\n"));
		goto abort_transaction;
	}

	/*
	* currently we don't support backend scatter-gatter, tso. but turns on
	* backend csum offload as default.
	*/
	/* This causes Hot replace D3 power test to blue screen.
	err = xenbus_printf(xbt, Adapter->Nodename, "feature-gso-tcpv4", "%d", 1);
	if (err) {
		DBG_PRINT(("VNIF: xenbus writing feature-gso-tcpv4 fail.\n"));
		goto abort_transaction;
	}
	*/

	/*
	* this field is for backward compatibility
	*/
	DPR_INIT(("VNIF: xenbus writing copy-delivery-offset.\n"));
	err = xenbus_printf(xbt, Adapter->Nodename, "copy-delivery-offset", "%u",
				  0);
	if (err) {
		DBG_PRINT(("VNIF: xenbus writing copy-delivery-offset fail.\n"));
		goto abort_transaction;
	}

	/* If not supporting checksuming, need to tell backend. */
	if (!Adapter->cur_rx_tasks) {
		DPR_INIT(("VNIF: xenbus writing feature-no-csum-offload.\n"));
		err = xenbus_printf(xbt, Adapter->Nodename,
			"feature-no-csum-offload", "%d", 1);
		if (err) {
			DBG_PRINT(("VNIF: xenbus writing feature-no-csum-offload fail.\n"));
			goto abort_transaction;
		}
	}

	DPR_INIT(("VNIF: xenbus transcation end.\n"));
	err = xenbus_transaction_end(xbt, 0);
	if (err) {
		if (err == -EAGAIN)
			goto again;
		DBG_PRINT(("VNIF: xenbus transcation end fail.\n"));
		goto out;
	}

	xenbus_switch_state(Adapter->Nodename, XenbusStateConnected);

	DPR_INIT(("VNIF: VNIFTalkToBackend - OUT 1\n"));

	return 0;

abort_transaction:
	xenbus_transaction_end(xbt, 1);
out:
	DPR_INIT(("VNIF: VNIFTalkToBackend - OUT 0\n"));
	return err;
}

/*
 * we don't have any resource to handle, this function is for initializations
 * that must be done after connected to the backend.
 */
static NDIS_STATUS
VNIFInitRxGrants(PVNIF_ADAPTER adapter)
{
	PLIST_ENTRY entry;
	RCB *rcb;
	netif_rx_request_t *req;
	ULONG i;
	grant_ref_t ref;
	RING_IDX req_prod;

	DPR_INIT(("VNIF: VNIFInitRxGrants irql = %d - IN\n", KeGetCurrentIrql()));

	if (!adapter->copyall) {
		DBG_PRINT(("VNIF: page-flipping is not supported.\n"));
		return NDIS_STATUS_FAILURE;
	}

	/* Putting all receive buffers' grant table refs to net ring */
	NdisAcquireSpinLock(&adapter->RecvLock);

	if (gnttab_alloc_grant_references(
			(uint16_t)adapter->num_rcb, //VNIF_RCB_ARRAY_SIZE,
			&adapter->gref_rx_head) < 0) {
		DBG_PRINT(("VNIF: netfront can't alloc rx grant refs\n"));
		NdisReleaseSpinLock(&adapter->RecvLock);
		return NDIS_STATUS_FAILURE;
	}

	for (i = 0; i < adapter->num_rcb /*VNIF_RCB_ARRAY_SIZE*/; i++) {
		rcb = adapter->RCBArray[i];
		ref = gnttab_claim_grant_reference(&adapter->gref_rx_head);
		if ((signed short)ref < 0) {
			DBG_PRINT(("VNIF: gnttab_claim_grant_reference gref_rx_head.\n"));
			NdisReleaseSpinLock(&adapter->RecvLock);
			return NDIS_STATUS_FAILURE;
		}

		rcb->grant_rx_ref = ref;
		gnttab_grant_foreign_access_ref(
			ref,
			adapter->BackendID,
			virt_to_mfn(rcb->page),
			0);
		InsertTailList(&adapter->RecvFreeList, &rcb->list);
	}

	req_prod = adapter->rx.req_prod_pvt;
	for (i = 0; i < NET_RX_RING_SIZE; i++) {
		rcb = (RCB *) RemoveHeadList(&adapter->RecvFreeList);
		req = RING_GET_REQUEST(&adapter->rx, req_prod + i);
		req->gref = rcb->grant_rx_ref;
		req->id = (UINT16) rcb->index;
	}

	KeMemoryBarrier();
	adapter->rx.req_prod_pvt = req_prod + i;
	RING_PUSH_REQUESTS(&adapter->rx);

	NdisReleaseSpinLock(&adapter->RecvLock);

	DPR_INIT(("Xennet using %d receive buffers.\n", adapter->num_rcb));
	DPR_INIT(("VNIF: VNIFInitRxGrants `- OUT\n"));
	return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS
VNIFInitTxGrants(PVNIF_ADAPTER adapter)
{
#ifdef XENNET_COPY_TX
	TCB *tcb;
#endif
	xen_ulong_t xi;
	ULONG mfn;
	uint32_t i;
	grant_ref_t ref;

	/* Pre-allocate grant table references for send. */
	DPR_INIT(("VNIF: VNIFInitTxGrants - gnttab_alloc_grant_references\n"));
	if (gnttab_alloc_grant_references(TX_MAX_GRANT_REFS,
			&adapter->gref_tx_head) < 0) {
		DBG_PRINT(("VNIF: netfront can't alloc tx grant refs\n"));
		return NDIS_STATUS_FAILURE;
	}

#ifdef XENNET_COPY_TX
	/* Allocate for each TCB, because sizeof(TCB) is less than PAGE_SIZE, */
	/* it will not cross page boundary. */
	for (i = 0; i < VNIF_MAX_BUSY_SENDS; i++) {
		tcb = adapter->TCBArray[i];
		ref = gnttab_claim_grant_reference(&adapter->gref_tx_head);
		if ((signed short)ref < 0) {
			DBG_PRINT(("VNIF: fail to claim grant reference for tx.\n"));
			return NDIS_STATUS_FAILURE;
		}

		mfn = virt_to_mfn(tcb->data);
		gnttab_grant_foreign_access_ref(
			ref, adapter->BackendID, mfn, GNTMAP_readonly);
		tcb->grant_tx_ref = ref;

		NdisInterlockedInsertTailList(
			&adapter->SendFreeList,
			&tcb->list,
			&adapter->SendLock);
	}
#else
	ref = gnttab_claim_grant_reference(&adapter->gref_tx_head);
	if ((signed short)ref < 0) {
		DBG_PRINT(("VNIF: fail to claim grant reference for tx.\n"));
		return NDIS_STATUS_FAILURE;
	}
	mfn = virt_to_mfn(adapter->zero_data);
	gnttab_grant_foreign_access_ref(
		ref, adapter->BackendID, mfn, GNTMAP_readonly);
	adapter->zero_ref = ref;
	adapter->zero_offset = BYTE_OFFSET(adapter->zero_data);

	for (xi = 0; xi < VNIF_MAX_BUSY_SENDS; xi++) {
		adapter->tx_packets[xi] = (NDIS_PACKET *)(xi + 1);
		adapter->grant_tx_ref[xi] =
			gnttab_claim_grant_reference(&adapter->gref_tx_head);
	}
	adapter->tx_id_alloc_head = 0;
#endif
	return NDIS_STATUS_SUCCESS;
}

static uint32_t
VNIFOutstanding(PVNIF_ADAPTER adapter)
{
    struct netif_tx_request *tx;
	TCB *tcb;
    RING_IDX cons, prod;
	uint32_t cnt;
	uint32_t gnt_flags;
	uint32_t outstanding;

	outstanding = 0;
	cnt = 0;
    prod = adapter->tx.req_prod_pvt;

	for (cnt = 0; cnt < VNIF_MAX_BUSY_SENDS; cnt++) {
		tcb = adapter->TCBArray[cnt];
		if (!tcb) {
			continue;
		}
#ifdef XENNET_TRACK_TX
		if (tcb->granted) {
			DBG_PRINT(("\tid %x, refs %x, flags %x, granted %x, ringidx %x.\n",
				cnt,
				tcb->grant_tx_ref,
				gnttab_query_foreign_access_flags(tcb->grant_tx_ref),
				tcb->granted, tcb->ringidx));
		}
#else
		gnt_flags = gnttab_query_foreign_access_flags(tcb->grant_tx_ref);
		if (gnt_flags & (GTF_reading | GTF_writing)) {
			outstanding++;
			DBG_PRINT(("\tid %x, refs %x, flags %x.\n",	cnt, tcb->grant_tx_ref,
				gnt_flags));
		}
#endif
	}

	DBG_PRINT(("VNIF: Outstanding refs %x: out.\n", outstanding));
	return outstanding;
}

uint32_t
VNIFQuiesce(PVNIF_ADAPTER adapter)
{
	uint32_t waiting;
	uint32_t wait_count = 0;
	char *buf;

#ifdef DBG
	buf = xenbus_read(XBT_NIL, adapter->Otherend, "state", NULL);
	if (buf) {
		DBG_PRINT(("VNIFQuiesce: backend state %s, ", buf));
		xenbus_free_string(buf);
	}
	buf = xenbus_read(XBT_NIL, adapter->Nodename, "state", NULL);
	if (buf) {
		DBG_PRINT(("frontend state %s\n", buf));
		xenbus_free_string(buf);
	}
#endif
	do {
		if (adapter->nBusySend) {
			/* We've unregistered the evtchn, so the dpc wont be called. */
			/* The dpc will allow the transmits to complete. */
			/* Grab adapter->Lock so that we will be at DPC level. */
			
			if (adapter->evtchn) {
				if (notify_remote_via_evtchn(adapter->evtchn))
					DBG_PRINT(("VNIFQuiesce notify failed.\n"));
			}
			NdisAcquireSpinLock(&adapter->Lock);
			VNIFInterruptDpc(NULL, adapter, NULL, NULL);
			NdisReleaseSpinLock(&adapter->Lock);
		}

		/* Only need to wory about the receives that are in the process of */
		/* VNIFReceivePackets and xennet_return_packet. */
		waiting = adapter->nBusyRecv;
		waiting += adapter->nBusySend;

		if (!waiting) {
			break;
		}

		if ((++wait_count % 60) == 0) {
			if (adapter->nBusyRecv) {
				DBG_PRINT(("VNIF %s: VNIFQuiesce for receives %x, pvt %x, srsp %x, rsc %x\n",
					&adapter->Nodename[7],
					adapter->nBusyRecv,
					adapter->rx.req_prod_pvt,
					adapter->rx.sring->rsp_prod,
					adapter->rx.rsp_cons));
			}
			if (adapter->nBusySend) {
				DBG_PRINT(("VNIFQuiesce %s: still waiting %x for sends %x: flags %x, pvt %x, rcons %x\n",
					&adapter->Nodename[7], wait_count,
					adapter->nBusySend, adapter->Flags,
					adapter->tx.req_prod_pvt, adapter->tx.rsp_cons));
				DBG_PRINT(("\tsring: req_prod %x, rsp_prod %x, req_event %x, rsp_event %x\n",
					adapter->tx.sring->req_prod,
					adapter->tx.sring->rsp_prod,
					adapter->tx.sring->req_event,
					adapter->tx.sring->rsp_event));
				if (VNIFOutstanding(adapter) == 0) {
					waiting = 0;
					break;
				}
			}
		}
		NdisMSleep(1000000);  /* 1 seconds */
	} while (waiting && wait_count < 60);
    DPR_INIT(("VNIF: VNIFQuiesce exit - flags %x, waiting %x, wait_count %x. OUT\n",
		adapter->Flags, waiting, wait_count));
	return waiting;
}

static void
VNIFWaitStateChange(PVNIF_ADAPTER adapter,
	enum xenbus_state front_state, enum xenbus_state end_state)
{
	char *buf;
	uint32_t i;
    enum xenbus_state backend_state;

	DPR_INIT(("VNIFWaitStateChange: switching to %d\n", front_state));
	for (i = 0; i < 1000; i++) {
		DPR_INIT(("VNIFWaitStateChange: try switching to %d: %d\n", front_state, i));
		if (xenbus_switch_state(adapter->Nodename, front_state) == 0)
			break;
		DPR_INIT(("VNIFWaitStateChange: try switching to %d: sleeping\n", front_state ));
		NdisMSleep(1000);
	}
	for (i = 0; i < 1000; i++) {
		DPR_INIT(("VNIFWaitStateChange: try reading state: %d\n", i));
		buf = xenbus_read(XBT_NIL, adapter->Otherend, "state", NULL);
		if (buf) {
			backend_state = (enum xenbus_state)cmp_strtoul(buf, NULL, 10);
			xenbus_free_string(buf);
			if (backend_state == end_state)
				break;
		}
		DPR_INIT(("VNIFWaitStateChange: try reading state sleeping: %d\n", i));
		NdisMSleep(1000);
	}
	DPR_INIT(("VNIFWaitStateChange: done waiting %d, %d\n",
		end_state, backend_state));
}

static void
VNIFCleanupRings(PVNIF_ADAPTER adapter)
{
#ifdef XENNET_COPY_TX
	TCB *tcb;
#endif
	RCB *rcb;
	uint32_t i;

#ifdef XENNET_COPY_TX
	DPR_INIT(("VNIF: VNIFCleanupRings XENNET_COPY_TX\n"));
	for (i = 0; i < VNIF_MAX_BUSY_SENDS; i++) {
		tcb = adapter->TCBArray[i];
		if (!tcb) {
			continue;
		}
		if (tcb->grant_tx_ref != GRANT_INVALID_REF) {
			gnttab_end_foreign_access_ref(
				tcb->grant_tx_ref, GNTMAP_readonly);
			gnttab_release_grant_reference(
				&adapter->gref_tx_head, tcb->grant_tx_ref);
			tcb->grant_tx_ref = GRANT_INVALID_REF;
		}
	}
#else
	if (adapter->zero_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access_ref(
		  adapter->zero_ref, GNTMAP_readonly);
		gnttab_release_grant_reference(
		  &adapter->gref_tx_head, adapter->zero_ref);
		adapter->zero_ref = GRANT_INVALID_REF;
	}
	for (i = 0; i < VNIF_MAX_BUSY_SENDS; i++) {
		if (adapter->grant_tx_ref[i] != GRANT_INVALID_REF) {
			gnttab_end_foreign_access_ref(
				adapter->grant_tx_ref[i], GNTMAP_readonly);

			gnttab_release_grant_reference(
				&adapter->gref_tx_head, adapter->grant_tx_ref[i]);
			adapter->grant_tx_ref[i] = GRANT_INVALID_REF;
		}
	}
#endif

    if (adapter->tx_ring_ref != GRANT_INVALID_REF) {
		DPR_INIT(("VNIF: VNIFCleanupRings - end tx ring ref\n"));
        gnttab_end_foreign_access(adapter->tx_ring_ref, 0);
    }
	if (adapter->tx.sring)
		NdisFreeMemory(adapter->tx.sring, PAGE_SIZE, 0);

	DPR_INIT(("VNIF: VNIFCleanupRings gnttab_free_grant_references tx %x\n",
		adapter->gref_tx_head));
	if (adapter->gref_tx_head != GRANT_INVALID_REF)
		gnttab_free_grant_references(adapter->gref_tx_head);

	/* Now do the receive resources. */
	for(i = 0; i < NET_RX_RING_SIZE; i++) {
		rcb = adapter->RCBArray[i];
		if (!rcb)
			continue;

		if (rcb->grant_rx_ref != GRANT_INVALID_REF) {
			gnttab_end_foreign_access_ref(
				rcb->grant_rx_ref, 0);
			gnttab_release_grant_reference(
				&adapter->gref_rx_head, rcb->grant_rx_ref);
			rcb->grant_rx_ref = GRANT_INVALID_REF;
		}
	}
    if (adapter->rx_ring_ref != GRANT_INVALID_REF) {
		DPR_INIT(("VNIF: VNIFCleanupRings - end rx ring ref\n"));
        gnttab_end_foreign_access(adapter->rx_ring_ref, 0);
    }
	if (adapter->rx.sring)
		NdisFreeMemory(adapter->rx.sring, PAGE_SIZE, 0);

	DPR_INIT(("VNIF: VNIFCleanupRings gnttab_free_grant_references rx %x\n",
		adapter->gref_rx_head));
	if (adapter->gref_rx_head != GRANT_INVALID_REF)
		gnttab_free_grant_references(adapter->gref_rx_head);

    adapter->tx_ring_ref = GRANT_INVALID_REF;
    adapter->rx_ring_ref = GRANT_INVALID_REF;
    adapter->tx.sring = NULL;
    adapter->rx.sring = NULL;
}

uint32_t
VNIFDisconnectBackend(PVNIF_ADAPTER adapter)
{
	xenbus_release_device_t release_data;
	BOOLEAN cancelled = TRUE;

    DPR_INIT(("VNIF: VNIFDisconnectBackend - IN\n"));

	/* Wait to become idle for all sends and receives. */
	if (VNIFQuiesce(adapter))
		return 1;

	unregister_xenbus_watch(&adapter->watch);

	if (adapter->evtchn) {
		DPR_INIT(("VNIF: VNIFDisconnectBackend unregister_dpc_from_evtchn %d\n",
			adapter->evtchn));
		unregister_dpc_from_evtchn(adapter->evtchn);
	}

	/* Switch the state to closing, closed, initializign.  This will */
	/* allow the backend to release its references. */
	VNIFWaitStateChange(adapter, XenbusStateClosing, XenbusStateClosing);
	VNIFWaitStateChange(adapter, XenbusStateClosed, XenbusStateClosed);
	VNIFWaitStateChange(adapter, XenbusStateInitialising, XenbusStateInitWait);

	/* The evtchn has already been unregistered. */
	if (adapter->evtchn)
		xenbus_free_evtchn(adapter->evtchn);

    adapter->evtchn = 0;

	VNIFCleanupRings(adapter);

	release_data.action = RELEASE_ONLY;
	release_data.type = vnif;
	xenbus_release_device(adapter, NULL, release_data);

	/* cancel the reset timer and receive indication timer */
	DPR_INIT(("VNIF: NdisCancelTimer\n"));
	VNIF_CANCEL_TIMER(adapter->rcv_timer, &cancelled);
	VNIF_CANCEL_TIMER(adapter->ResetTimer, &cancelled);

	if (cancelled) {
		DPR_INIT(("VNIF: halt VNIFFreeQueuedRecvPackets\n"));
		VNIFFreeQueuedRecvPackets(adapter);
	}

    DPR_INIT(("VNIF: VNIFDisconnectBackend - flags %x. OUT\n", adapter->Flags));
	return 0;
}

static void
xennet_frontend_changed(struct xenbus_watch *watch,
	const char **vec, unsigned int len)
{
    PVNIF_ADAPTER adapter = (PVNIF_ADAPTER)watch->context;
	char *buf;
    uint32_t link_status;

	buf = xenbus_read(XBT_NIL, adapter->Nodename, "link-status", NULL);
	if (buf == NULL || IS_ERR(buf)){
		return;
	}

	link_status = (uint32_t)cmp_strtoul(buf, NULL, 10);
	xenbus_free_string(buf);
	DPR_INIT(("xennet_frontend_changed: %p, link status = %d.\n",
		adapter, link_status));

	if (link_status != !VNIF_TEST_FLAG(adapter, VNF_ADAPTER_NO_LINK)) {
		if (link_status) {
			VNIF_CLEAR_FLAG(adapter, VNF_ADAPTER_NO_LINK);
		}
		else {
			NdisAcquireSpinLock(&adapter->RecvLock);
			NdisAcquireSpinLock(&adapter->SendLock);
			VNIF_SET_FLAG(adapter, VNF_ADAPTER_NO_LINK);
			NdisReleaseSpinLock(&adapter->SendLock);    
			NdisReleaseSpinLock(&adapter->RecvLock);    
		}

		// Indicate the media event
		DPR_INIT(("xennet_frontend_changed: indicating status change.\n"));
		VNIFIndicateLinkStatus(adapter, link_status);
	}
}

static void
MPResume(PVNIF_ADAPTER adapter, uint32_t suspend_canceled)
{
	DPR_INIT(("MPResume: %p, %x\n", adapter, suspend_canceled));
	if (suspend_canceled) {
		VNIF_CLEAR_FLAG(adapter, VNF_ADAPTER_SUSPENDING);
	}
	else {
#ifdef DBG
		adapter->dbg_print_cnt = 0;
#endif
		if (VNIFFindXenAdapter(adapter) == STATUS_SUCCESS) {
			if (VNIFSetupXenAdapter(adapter) == STATUS_SUCCESS) {
				VNIF_CLEAR_FLAG(adapter, VNF_ADAPTER_SUSPENDING);
			}
		}
	}
}

static uint32_t
MPSuspend(PVNIF_ADAPTER adapter, uint32_t reason)
{
	uint32_t waiting;

	DPR_INIT(("MPSuspend: %p, %x\n", adapter, reason));
	if (reason == SHUTDOWN_suspend) {
		NdisAcquireSpinLock(&adapter->RecvLock);
		NdisAcquireSpinLock(&adapter->SendLock);
		VNIF_SET_FLAG(adapter, VNF_ADAPTER_SUSPENDING);
		NdisReleaseSpinLock(&adapter->SendLock);    
		NdisReleaseSpinLock(&adapter->RecvLock);    

		waiting = VNIFQuiesce(adapter);
	}
	else {
		NdisAcquireSpinLock(&adapter->RecvLock);
		NdisAcquireSpinLock(&adapter->SendLock);
		VNIF_SET_FLAG(adapter, VNF_DISCONNECTED);
		NdisReleaseSpinLock(&adapter->SendLock);    
		NdisReleaseSpinLock(&adapter->RecvLock);    

		if ((waiting = VNIFDisconnectBackend(adapter)) == 0)
			VNIFFreeXenAdapter(adapter);
	}
	DPR_INIT(("MPSuspend OUT\n"));
	return waiting;
}

static uint32_t
MPIoctl(PVNIF_ADAPTER adapter, pv_ioctl_t data)
{
	uint32_t cc = 0;

	switch (data.cmd) {
		case PV_SUSPEND:
			cc = MPSuspend(adapter, data.arg);
			break;
		case PV_RESUME:
			MPResume(adapter, data.arg);
			break;
		case PV_ATTACH:
			break;
		case PV_DETACH:
			break;
		default:
			break;
	}
	return cc;
}

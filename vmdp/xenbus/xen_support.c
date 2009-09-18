/*****************************************************************************
 *
 *   File Name:      xen_support.c
 *
 *   %version:       18 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jul 13 09:09:45 2009 %
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

#include "xenbus.h"

shared_info_t *shared_info_area = NULL;

u8 xen_features[XENFEAT_NR_SUBMAPS*32];

#define HYPERCALL_PAGE_MEM_SIZE	PAGE_SIZE * 2
#ifdef ARCH_x86
UCHAR hypercall_page_mem[HYPERCALL_PAGE_MEM_SIZE];
#endif

uint32_t gNR_GRANT_FRAMES;
uint32_t gNR_GRANT_ENTRIES;
uint32_t gGNTTAB_LIST_END;

static uint64_t xen_mmio = 0;
static unsigned long xen_mmio_alloc = 0;
static uint32_t xen_mmiolen = 0;
static uint8_t *xen_shared_mem = NULL;
static uint32_t xen_shared_len = 0;
static uint32_t xen_shared_alloc = 0;

static NTSTATUS
set_hvm_val(uint64_t val, uint32_t idx)
{
    struct xen_hvm_param a;

	DPR_INIT(("set_hvm_val: IN %d\n", idx));
    a.domid = DOMID_SELF;
    a.index = idx;
    a.value = val;
    if (HYPERVISOR_hvm_op(HVMOP_set_param, &a) != 0) {
        DBG_PRINT(("XENBUS: set hvm val failed.\n"));
        return STATUS_UNSUCCESSFUL;
    }

	DPR_INIT(("set_hvm_val: OUT\n"));

    return STATUS_SUCCESS;
}

static void
xenbus_mmio_init(void)
{

	if (xen_shared_mem)
		MmUnmapIoSpace(xen_shared_mem, xen_shared_len);

	xen_mmio = 0;
	xen_mmio_alloc = 0;
	xen_mmiolen = 0;
	xen_shared_mem = NULL;
	xen_shared_alloc = 0;
	xen_shared_len = 0;
	shared_info_area = NULL;
}

static NTSTATUS
xenbus_set_io_resources(uint64_t mmio, uint32_t mmio_len,
	uint32_t vector, uint32_t reason)
{
    PHYSICAL_ADDRESS phys_mmio;
	FDO_DEVICE_EXTENSION *fdx;
	NTSTATUS status;

	DPR_INIT(("xenbus_set_io_resources in: mmio %x, len %x, xen %x, sh %p.\n",
		(uint32_t)mmio, mmio_len, (uint32_t)xen_mmio, xen_shared_mem));

	if (gfdo && mmio) {
		/* Normal boot up init */
		DPR_INIT(("xenbus_set_io_resources: normal init, gfdo %p\n", gfdo));
		//xenbus_mmio_init();
		phys_mmio.QuadPart = mmio;
		if (xen_shared_mem) {
			DBG_PRINT(("xenbus_set_io_resources: unmapping xen_shared_mem %p\n",
				xen_shared_mem));
			MmUnmapIoSpace(xen_shared_mem, xen_shared_len);
		}
		xen_shared_mem = MmMapIoSpace(phys_mmio, mmio_len, MmNonCached);
		if (xen_shared_mem == NULL) {
			DBG_PRINT(("xenbus_set_io_resources: MmMapIoSpace failed for %x.\n",
				(uint32_t)mmio));
			return STATUS_NO_MEMORY;
		}
		xen_mmio = mmio;
		xen_mmiolen = mmio_len;
		xen_shared_len = mmio_len;

		fdx = (FDO_DEVICE_EXTENSION *) gfdo->DeviceExtension;
		fdx->mmio = mmio;
		fdx->mmiolen = mmio_len;
		fdx->mem = xen_shared_mem;
		fdx->device_irq = vector;
	}
	else {
		if (gfdo == NULL) {
			/* Init due to hibernate or crash dump. */
			DPR_INIT(("xenbus_set_io_resources: gfdo is null\n"));
			gfdo = (PDEVICE_OBJECT)hvm_get_parameter(HVM_PARAM_GUEST_SPECIFIC);
			if (gfdo == NULL) {
				DBG_PRINT(("xenbus_set_io_resources: failed to get gfdo.\n"));
				return STATUS_UNSUCCESSFUL;
			}
		}
		DPR_INIT(("xenbus_set_io_resources: retrieved gfdo %p\n", gfdo));
		fdx = (FDO_DEVICE_EXTENSION *) gfdo->DeviceExtension;
		xen_mmio = fdx->mmio;
		xen_mmiolen = fdx->mmiolen;
		xen_shared_len = fdx->mmiolen;


		if (reason == OP_MODE_NORMAL) {
			phys_mmio.QuadPart = xen_mmio;
			if (xen_shared_mem) {
				DBG_PRINT(("xenbus_set_io_resources: unmap xen_shared_mem %p\n",
					xen_shared_mem));
				MmUnmapIoSpace(xen_shared_mem, xen_shared_len);
			}
			xen_shared_mem = MmMapIoSpace(phys_mmio, xen_mmiolen, MmNonCached);
			if (xen_shared_mem == NULL) {
				DBG_PRINT(("xenbus_set_io_resources: MmMap failed for %x.\n",
					(uint32_t)xen_mmio));
				return STATUS_NO_MEMORY;
			}
			fdx->mem = xen_shared_mem;
		}
		else {
			xen_shared_mem = fdx->mem;
		}



	}
	set_hvm_val((uint64_t)gfdo , HVM_PARAM_GUEST_SPECIFIC);
	xen_mmio_alloc = 0;
	xen_shared_alloc = 0;
	shared_info_area = NULL;
	DPR_INIT(("xenbus_set_io_resources out: xen_mmio of %x is %p.\n",
		(uint32_t)xen_mmio, xen_shared_mem));
	return STATUS_SUCCESS;
}

NTSTATUS alloc_xen_mmio(unsigned long len, uint64_t *phys_addr)
{
    if (xen_mmio_alloc + len <= xen_mmiolen) {
        *phys_addr = xen_mmio + xen_mmio_alloc;
        xen_mmio_alloc += len;
		return STATUS_SUCCESS;
    }
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS alloc_xen_shared_mem(uint32_t len, void **addr)
{
    if (xen_shared_alloc + len <= xen_shared_len) {
        *addr = xen_shared_mem + xen_shared_alloc;
        xen_shared_alloc += len;
		return STATUS_SUCCESS;
    }
	return STATUS_UNSUCCESSFUL;
}

static void setup_xen_features(void)
{
    xen_feature_info_t fi;
    int i, j;

    for (i=0; i<XENFEAT_NR_SUBMAPS; i++) {
        fi.submap_idx = i;
		DPR_INIT(("XENBUS: call HYPERVISOR_xen_version.\n"));
        if (HYPERVISOR_xen_version(XENVER_get_features, &fi) < 0) {
            DBG_PRINT(("XENBUS: error setting up xen features.\n"));
            break;
        }
		DPR_INIT(("XENBUS: back from HYPERVISOR_xen_version.\n"));
        for (j=0; j<32; j++)
            xen_features[i*32+j] = !!(fi.submap & 1<<j);
    }
}

static NTSTATUS xen_info_init(void)
{
    unsigned long shared_info_frame;
    struct xen_add_to_physmap_compat xatp;
    PHYSICAL_ADDRESS addr;
    int status;

    DPR_INIT(("xen_info_init: IN, sizeof(shared_info_t) = %d.\n",
			 sizeof(shared_info_t) ));

    DPR_INIT(("xen_info_init: calling setup_xen_features.\n"));
    setup_xen_features();

    DPR_INIT(("xen_info_init: calling alloc_xen_mmio.\n"));
    if (alloc_xen_mmio(PAGE_SIZE, &addr.QuadPart) != STATUS_SUCCESS)
		return STATUS_UNSUCCESSFUL;

    shared_info_frame = (ULONG) (addr.QuadPart >> PAGE_SHIFT);

    if (shared_info_frame == 0)
        return STATUS_INSUFFICIENT_RESOURCES;

    xatp.domid = DOMID_SELF;
    xatp.idx = 0;
    xatp.space = XENMAPSPACE_shared_info;
    xatp.gpfn = shared_info_frame;

    DPR_INIT(("xen_info_init: calling HYPERVISOR_memory_op.\n"));
    status = HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp);
    if (status != 0) {
        DBG_PRINT(("XENBUS: shared_info hypercall failed.\n"));
        return STATUS_UNSUCCESSFUL;
    }

    DPR_INIT(("xen_info_init: OUT.\n"));
    return STATUS_SUCCESS;
}

VOID xen_info_cleanup(void)
{
	xenbus_mmio_init();
	shared_info_area = NULL;
}

NTSTATUS InitializeHypercallPage(VOID)
{
    PHYSICAL_ADDRESS addr;
    UINT32 eax, ebx, ecx, edx, pages, msr, i;
	uint32_t index_offset;
    char signature[13];

    GetCPUID(0x40000000, &eax, &ebx, &ecx, &edx);

    *(UINT32 *)(signature + 0) = ebx;
    *(UINT32 *)(signature + 4) = ecx;
    *(UINT32 *)(signature + 8) = edx;
    signature[12] = 0;

	if (eax < 0x40000002) {
		DBG_PRINT(("XENBUS: not on Xen VMM. (sig %s, eax %x)\n",
			signature, eax));
		return STATUS_UNSUCCESSFUL;
	}
	if (strcmp("XenVMMXenVMM", signature) == 0) {
		index_offset = 0;
	}
	else if (strcmp("NovellShimHv", signature) == 0) {
		index_offset = 0x1000;
	}
	else {
		DBG_PRINT(("XENBUS: not on Xen VMM. (sig %s, eax %x)\n",
			signature, eax));
		return STATUS_UNSUCCESSFUL;
	}

    GetCPUID(0x40000001 + index_offset, &eax, &ebx, &ecx, &edx);
    DPR_INIT(("Xen version: %d.%d.\n", eax >> 16, eax & 0xffff));
    DPR_INIT(("  eax = %x, ebx = %x, ecx= %x, edx = %x.\n", eax, ebx, ecx, edx));

	/* Only support flexible grant entries if Xen 3.2 or greater. */
	if (((eax >> 16) >= 3) && ((eax & 0xffff) >= 2)) {
		gNR_GRANT_FRAMES = MAX_NR_GRANT_FRAMES;
		gNR_GRANT_ENTRIES = MAX_NR_GRANT_ENTRIES;
		gGNTTAB_LIST_END = (gNR_GRANT_ENTRIES + 1);
	}
	else {
		gNR_GRANT_FRAMES = MIN_NR_GRANT_FRAMES;
		gNR_GRANT_ENTRIES = MIN_NR_GRANT_ENTRIES;
		gGNTTAB_LIST_END = (gNR_GRANT_ENTRIES + 1);
	}

    GetCPUID(0x40000002 + index_offset, &pages, &msr, &ecx, &edx);
    DPR_INIT(("XENBUS: hypercall msr 0x%x, pages %d.\n", msr, pages));

    if (pages == 0) {
        DBG_PRINT(("XENBUS: error: hypercall page count == 0?"));
        return STATUS_UNSUCCESSFUL;
    }

	hypercall_page = (PUCHAR)(((uintptr_t)hypercall_page_mem
		& ~(PAGE_SIZE - 1)) + PAGE_SIZE);

	if (hypercall_page + (pages * PAGE_SIZE) >
		hypercall_page_mem + HYPERCALL_PAGE_MEM_SIZE) {
		DBG_PRINT(("InitializeHypercallPage: not enough space for %d pages\n",
			pages));
        return STATUS_INSUFFICIENT_RESOURCES;
	}
    
    addr = MmGetPhysicalAddress(hypercall_page);

    DPR_INIT(("XENBUS: init hypercall_page %p at va 0x%p pa 0x%08x:%08x\n",
			 hypercall_page_mem,
             hypercall_page,
			 ((ULONG)(addr.QuadPart >> 32)),
			 (ULONG)(addr.QuadPart)));
	DPR_INIT(("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			  hypercall_page[0],
			  hypercall_page[1],
			  hypercall_page[2],
			  hypercall_page[3],
			  hypercall_page[4],
			  hypercall_page[5],
			  hypercall_page[6],
			  hypercall_page[7],
			  hypercall_page[8],
			  hypercall_page[9],
			  hypercall_page[10],
			  hypercall_page[11],
			  hypercall_page[12],
			  hypercall_page[13],
			  hypercall_page[14],
			  hypercall_page[15]));

    for (i = 0; i < pages; i++) {
        WriteMSR(msr, (UINT64) addr.QuadPart + (i * PAGE_SIZE));
    }

	DPR_INIT(("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			  hypercall_page[0],
			  hypercall_page[1],
			  hypercall_page[2],
			  hypercall_page[3],
			  hypercall_page[4],
			  hypercall_page[5],
			  hypercall_page[6],
			  hypercall_page[7],
			  hypercall_page[8],
			  hypercall_page[9],
			  hypercall_page[10],
			  hypercall_page[11],
			  hypercall_page[12],
			  hypercall_page[13],
			  hypercall_page[14],
			  hypercall_page[15]));

    DPR_INIT(("XENBUS: hypercall_page[0] %x\n", *(uint32_t *)hypercall_page));
	if (*(uint32_t *)hypercall_page == 0) {
		DBG_PRINT(("XENBUS: Xen failed to setup hypercall_page\n"));
		DBG_PRINT(("XENBUS: PV drivers will not be used.\n"));
		DBG_PRINT(("XENBUS: hypercall_page: va 0x%p, pa 0x%08x:%08x\n",
				 hypercall_page,
				 ((ULONG)(addr.QuadPart >> 32)),
				 (ULONG)(addr.QuadPart)));
        return STATUS_UNSUCCESSFUL;
	}

    return STATUS_SUCCESS;
}


NTSTATUS
xenbus_xen_shared_init(uint64_t mmio, uint32_t mmio_len,
	uint32_t vector, uint32_t reason)
{
	FDO_DEVICE_EXTENSION *fdx;
	NTSTATUS status;

	DPR_INIT(("xenbus_xen_shared_init: IN %p.\n", gfdo));
	if (xen_shared_mem && KeGetCurrentIrql() == PASSIVE_LEVEL) {
		DPR_INIT(("xenbus_xen_shared_init: already initialized %p.\n", gfdo));
		return STATUS_SUCCESS;
	}

	do {
		/* initialize hypercall_page */
		DPR_INIT(("xenbus_xen_shared_init: call InitializeHypercallPage.\n"));
		status = InitializeHypercallPage();
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("xenbus_xen_shared_init: init hypercall_page fail.\n"));
			break;
		}
		XENBUS_SET_FLAG(rtrace, INITIALIZE_HYPERCALL_PAGE_F);

		DPR_INIT(("xenbus_xen_shared_init: call xs_init.\n"));
		status = xs_init(reason);
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("xenbus_xen_shared_init: xs_init failed.\n"));
			return status;
		}
		XENBUS_SET_FLAG(rtrace, XS_INIT_F);

		DPR_INIT(("xenbus_xen_shared_init: call xenbus_set_io_resources.\n"));
		status = xenbus_set_io_resources(mmio, mmio_len, vector, reason);
		if (!NT_SUCCESS(status))
			break;

		DPR_INIT(("xenbus_xen_shared_init: call xen_info_init.\n"));
		status = xen_info_init();
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("xenbus_xen_shared_init: xen_info_init fail.\n"));
			break;
		}
		XENBUS_SET_FLAG(rtrace, XEN_INFO_INIT_F);

		DPR_INIT(("xenbus_xen_shared_init: call gnttab_init.\n"));
		status = gnttab_init();
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("xenbus_xen_shared_init: gnttab_init fail.\n"));
			break;
		}
		XENBUS_SET_FLAG(rtrace, GNTTAB_INIT_F);

		DPR_INIT(("xenbus_xen_shared_init: call evtchn_init.\n"));
		evtchn_init();
		XENBUS_SET_FLAG(rtrace, EVTCHN_INIT_F);

		DPR_INIT(("xenbus_xen_shared_init: calling alloc_xen_shared_mem.\n"));
		status = alloc_xen_shared_mem(PAGE_SIZE, &shared_info_area);
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("xenbus_xen_shared_init: alloc_xen_shared_mem fail.\n"));
			break;
		}
		DPR_INIT(("xenbus_xen_shared_init: shared_info_area = %p.\n",
			shared_info_area));
		shared_info_area->vcpu_info[0].evtchn_upcall_mask = 0;

		status = gnttab_finish_init(gfdo, reason);
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("xenbus_xen_shared_init: gnttab_init fail.\n"));
			break;
		}
		XENBUS_SET_FLAG(rtrace, GNTTAB_FINISH_INIT_F);

		status = xs_finish_init(gfdo, reason);
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("xenbus_xen_shared_init: xs_finish_init failed.\n"));
			break;
		}

		fdx = (FDO_DEVICE_EXTENSION *) gfdo->DeviceExtension;
		status = set_callback_irq(fdx->device_irq);
		if (!NT_SUCCESS(status)) {
			DPR_INIT(("xenbus_xen_shared_init: failed callback %x.\n", status));
			break;
		}

		status = xenbus_probe_init(gfdo, reason);
		if (!NT_SUCCESS(status)) {
			DBG_PRINT(("xenbus_xen_shared_init: xenbus_probe_init failed.\n"));
			break;
		}

		IoInvalidateDeviceRelations(fdx->Pdo, BusRelations);
	} while (FALSE);


	if (status != STATUS_SUCCESS)
		xenbus_mmio_init();

	return status;
}

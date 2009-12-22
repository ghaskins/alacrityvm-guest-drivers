/*
 * Copyright (C) 2009 Novell.  All Rights Reserved.
 *
 *  pci.c: Routines for managing pci resources.
 *
 * Author:
 *	Gregory Haskins <ghaskins@novell.com>
 *	Peter W. Morreale <pmorreale@novell.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "vbus.h"
#include "precomp.h"
#include "vbus_pci.h"
#include "ioq.h"
#include "shm-signal.h"
#include "vbus_driver.h"

struct vbus_pci {
	WDFSPINLOCK		lock;
	WDFINTERRUPT		interrupt;
	WDFDEVICE		bus;
	struct ioq		eventq;
	struct vbus_pci_event	*ring;
	struct vbus_pci_regs	*regs;    /* mmio */
	struct vbus_pci_signals	*signals; /* pio */
	ULONG			signals_length;
	ULONG			regs_length;
	int			enabled;
};

static struct vbus_pci vbus_pci;

struct vbus_pci_device {
	char				type[VBUS_MAX_DEVTYPE_LEN];
	UINT64				handle;
	LIST_ENTRY			shms;
	struct vbus_device_proxy 	vdev;
};


/*
 * -------------------
 * common routines
 * -------------------
 */

static __inline UINT64 
__pa(void *ptr)
{
	PHYSICAL_ADDRESS a = MmGetPhysicalAddress(ptr);

	return (UINT64) a.QuadPart;
}

static int
vbus_pci_bridgecall(unsigned long nr, void *data, unsigned long len)
{
	struct vbus_pci_call_desc params;
	int ret;

	params.vector = nr;
	params.len = len;
	params.datap  = __pa(data);

	/* Send over as array of 32bit words */

	len = (sizeof(params) >> 2)  + (sizeof(params) % 4);

	WdfSpinLockAcquire(vbus_pci.lock);
	WRITE_REGISTER_BUFFER_ULONG((PUCHAR)((PVOID)&vbus_pci.regs->bridgecall),
			((PULONG) ((PVOID) &params)), len);
	ret = READ_REGISTER_ULONG((PULONG) &vbus_pci.regs->bridgecall);
	WdfSpinLockRelease(vbus_pci.lock);

	return ret;
}

static int
vbus_pci_buscall(unsigned long nr, void *data, unsigned long len)
{
	struct vbus_pci_fastcall_desc params;
	int ret;

	params.call.vector = nr;
	params.call.len    = len;
	params.call.datap  = __pa(data);


	ret = vbus_pci_bridgecall(VBUS_PCI_BRIDGE_SLOWCALL, &params, 
			sizeof(struct vbus_pci_fastcall_desc));
	if (ret >= 0 ) 
		ret = params.result;

	return ret;
}

static struct vbus_pci_device *
to_dev(struct vbus_device_proxy *vdev)
{
	return CONTAINING_RECORD(vdev, struct vbus_pci_device, vdev);
}

static void
_signal_init(struct shm_signal *signal, struct shm_signal_desc *desc,
	     struct shm_signal_ops *ops)
{
	desc->magic = SHM_SIGNAL_MAGIC;
	desc->ver   = SHM_SIGNAL_VER;

	ShmSignalInit(signal, shm_locality_north, ops, desc);
}

/*
 * -------------------
 * _signal
 * -------------------
 */

struct _signal {
	char	       name[64];
	struct vbus_pci   *pcivbus;
	struct shm_signal  signal;
	UINT32	     handle;
	LIST_ENTRY	 list;
};

static struct _signal *
to_signal(struct shm_signal *signal)
{
       return CONTAINING_RECORD(signal, struct _signal, signal);
}

static int
_signal_inject(struct shm_signal *signal)
{
	struct _signal *_signal = to_signal(signal);

	WRITE_REGISTER_ULONG(&vbus_pci.signals->shmsignal, _signal->handle);

	return 0;
}

static void
_signal_release(struct shm_signal *signal)
{
	struct _signal *_signal = to_signal(signal);

	VbusFree(_signal);
}

static struct shm_signal_ops _signal_ops;

/*
 * -------------------
 * vbus_device_proxy routines
 * -------------------
 */

static NTSTATUS
vbus_pci_device_open(struct vbus_device_proxy *vdev, int version, int flags)
{
	struct vbus_pci_device *dev = to_dev(vdev);
	struct vbus_pci_deviceopen params;
	int ret;

	if (dev->handle)
		return STATUS_NO_SUCH_DEVICE;

	params.devid   = (UINT32) vdev->id;
	params.version = version;

	ret = vbus_pci_buscall(VBUS_PCI_HC_DEVOPEN,
				 &params, sizeof(params));
	if (ret < 0)
		return ret;

	dev->handle = params.handle;

	return STATUS_SUCCESS;
}

static int
vbus_pci_device_close(struct vbus_device_proxy *vdev, int flags)
{
	struct vbus_pci_device *dev = to_dev(vdev);
	int ret;

	if (!dev->handle)
		return STATUS_NO_SUCH_DEVICE;

	WdfSpinLockAcquire(vbus_pci.lock);

	while (!IsListEmpty(&dev->shms)) {
		struct _signal *_signal;

		_signal = CONTAINING_RECORD(&dev->shms, struct _signal, list);
		RemoveHeadList(&dev->shms);
		VbusFree(_signal);
	}

	WdfSpinLockRelease(vbus_pci.lock);

	/*
	 * The DEVICECLOSE will implicitly close all of the shm on the
	 * host-side, so there is no need to do an explicit per-shm
	 * hypercall
	 */
	ret = vbus_pci_buscall(VBUS_PCI_HC_DEVCLOSE,
				 &dev->handle, sizeof(dev->handle));
	dev->handle = 0;

	return 0;
}


static NTSTATUS
vbus_pci_device_shm(struct vbus_device_proxy *vdev, const char *name,
		    int id, int prio, void *ptr, size_t len,
		    struct shm_signal_desc *sdesc, struct shm_signal **signal,
		    int flags)
{
	struct vbus_pci_device *dev = to_dev(vdev);
	struct _signal *_signal = NULL;
	struct vbus_pci_deviceshm params;
	char			*label;
	PHYSICAL_ADDRESS pa;
	int ret;

	if (!dev->handle)
		return STATUS_NO_SUCH_DEVICE;

	params.devh   = dev->handle;
	params.id     = id;
	params.flags  = flags;
	params.datap  = __pa(ptr);
	params.len    = len;

	if (signal) {
		/*
		 * The signal descriptor must be embedded within the
		 * provided ptr
		 */
		if (!sdesc
		    || (len < sizeof(*sdesc))
		    || (UINT64)sdesc < (UINT64)ptr
		    || (UINT64)sdesc > ((UINT64)ptr + len - sizeof(*sdesc)))
			return STATUS_NO_SUCH_DEVICE;

		_signal = VbusAlloc(sizeof(*_signal));
		if (!_signal)
			return STATUS_NO_MEMORY;

		_signal_ops.inject  = _signal_inject,
		_signal_ops.release = _signal_release,
		_signal_init(&_signal->signal, sdesc, &_signal_ops);


		params.signal.offset = (UINT64)(unsigned long)sdesc -
					(UINT64)(unsigned long)ptr;
		params.signal.prio   = prio;
		params.signal.cookie = (UINT64)(unsigned long)_signal;

	} else
		params.signal.offset = -1; /* yes, this is a UINT32, but its ok */

	ret = vbus_pci_buscall(VBUS_PCI_HC_DEVSHM, &params, sizeof(params));
	if (ret < 0)
		goto fail;

	if (signal) {

		if (!name)
			RtlStringCchPrintfA(_signal->name, 
				sizeof(_signal->name), "dev%lld-id%d",
				vdev->id, id);
		else
			RtlStringCchPrintfA(_signal->name, 
				sizeof(_signal->name), "%s", name);

		WdfSpinLockAcquire(vbus_pci.lock);

		InsertTailList(&dev->shms, &_signal->list);

		WdfSpinLockRelease(vbus_pci.lock);

		*signal = &_signal->signal;
	}

	return 0;

fail:
	return ret;
}

static int
vbus_pci_device_call(struct vbus_device_proxy *vdev, UINT32 func, void *data,
		     size_t len, int flags)
{
	struct vbus_pci_device *dev = to_dev(vdev);
	struct vbus_pci_devicecall params;

	params.devh  = dev->handle;
	params.func  = func;
	params.datap = __pa(data);
	params.len   = len;
	params.flags = flags;

	if (!dev->handle)
		return STATUS_NO_SUCH_DEVICE;

	return vbus_pci_buscall(VBUS_PCI_HC_DEVCALL, &params, sizeof(params));
}

static void
vbus_pci_device_release(struct vbus_device_proxy *vdev)
{
	struct vbus_pci_device *_dev = to_dev(vdev);

	vbus_pci_device_close(vdev, 0);

	VbusFree(_dev);
}

/*
 * -------------------
 * vbus events
 * -------------------
 */

static void
event_devadd(struct vbus_pci_add_event *event)
{
	WDFCHILDLIST	list;
	PDO_ID_DESC	d;
	NTSTATUS	rc;
	char		*src, *dst;

	list = WdfFdoGetDefaultChildList(vbus_pci.bus);

	WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(&d.header, sizeof(d));
	memset(d.type, '\0', VBUS_MAX_DEVTYPE_LEN);
	src = event->type;
	dst = d.type;
	while(*src)
		*dst++ = *src++;
	d.id = event->id;

	rc = WdfChildListAddOrUpdateChildDescriptionAsPresent(list, 
			&d.header, NULL);
	if (!NT_SUCCESS(rc)) 
		vlog("event_devadd: AddOrUpdate: %d\n", rc);
}

static void
event_devdrop(struct vbus_pci_handle_event *event)
{
}

static void
event_shmsignal(struct vbus_pci_handle_event *event)
{
	struct _signal *_signal = (struct _signal *)(unsigned long)event->handle;
}

static void
event_shmclose(struct vbus_pci_handle_event *event)
{
	struct _signal *_signal = (struct _signal *)(unsigned long)event->handle;

}

/*
 * -------------------
 * eventq routines
 * -------------------
 */

static struct ioq_notifier eventq_notifier;


/* Invoked whenever the hypervisor ioq_signal()s our eventq */
static void
eventq_wakeup(struct ioq_notifier *notifier)
{
	struct ioq_iterator iter;
	int ret;

	/* We want to iterate on the head of the in-use index */
	ret = ioq_iter_init(&vbus_pci.eventq, &iter, ioq_idxtype_inuse, 0);
	ASSERT(ret >= 0);

	ret = ioq_iter_seek(&iter, ioq_seek_head, 0, 0);
	ASSERT(ret >= 0);

	/*
	 * The EOM is indicated by finding a packet that is still owned by
	 * the south side.
	 *
	 * FIXME: This in theory could run indefinitely if the host keeps
	 * feeding us events since there is nothing like a NAPI budget.  We
	 * might need to address that
	 */
	while (!iter.desc->sown) {
		struct ioq_ring_desc *desc  = iter.desc;
		struct vbus_pci_event *event;

		event = (struct vbus_pci_event *)(unsigned long)desc->cookie;

		switch (event->eventid) {
		case VBUS_PCI_EVENT_DEVADD:
			event_devadd(&event->data.add);
			break;
		case VBUS_PCI_EVENT_DEVDROP:
			event_devdrop(&event->data.handle);
			break;
		case VBUS_PCI_EVENT_SHMSIGNAL:
			event_shmsignal(&event->data.handle);
			break;
		case VBUS_PCI_EVENT_SHMCLOSE:
			event_shmclose(&event->data.handle);
			break;
		default:
			break;
		};

		memset(event, 0, sizeof(*event));

		/* Advance the in-use head */
		ret = ioq_iter_pop(&iter, 0);
		ASSERT(ret >= 0);

	}

	/* And let the south side know that we changed the queue */
	ioq_signal(&vbus_pci.eventq, 0);
}

static struct ioq_notifier eventq_notifier;


/*
 * -------------------
 */

static int
eventq_signal_inject(struct shm_signal *signal)
{
	/* The eventq uses the special-case handle=0 */
	WRITE_REGISTER_ULONG(&vbus_pci.signals->eventq, 0);

	return 0;
}

static void
eventq_signal_release(struct shm_signal *signal)
{
	if (signal)
		VbusFree(signal);
}

static struct shm_signal_ops eventq_signal_ops;

/*
 * -------------------
 */

static void
eventq_ioq_release(struct ioq *ioq)
{
	/* released as part of the vbus_pci object */
}

static struct ioq_ops eventq_ioq_ops;

static NTSTATUS 
eventq_init(int qlen)
{
	struct ioq_iterator iter;
	int ret;
	int i;

	vbus_pci.ring = VbusAlloc(sizeof(struct vbus_pci_event) * qlen);
	if (!vbus_pci.ring)
		return STATUS_NO_MEMORY;

	/*
	 * We want to iterate on the "valid" index.  By default the iterator
	 * will not "autoupdate" which means it will not hypercall the host
	 * with our changes.  This is good, because we are really just
	 * initializing stuff here anyway.  Note that you can always manually
	 * signal the host with ioq_signal() if the autoupdate feature is not
	 * used.
	 */
	ioq_iter_init(&vbus_pci.eventq, &iter, ioq_idxtype_valid, 0);

	/*
	 * Seek to the tail of the valid index (which should be our first
	 * item since the queue is brand-new)
	 */
	ioq_iter_seek(&iter, ioq_seek_tail, 0, 0);


	/*
	 * Now populate each descriptor with an empty vbus_event and mark it
	 * valid
	 */
	for (i = 0; i < qlen; i++) {
		struct vbus_pci_event	*event = &vbus_pci.ring[i];
		size_t			len   = sizeof(*event);
		struct ioq_ring_desc	*desc  = iter.desc;

		desc->cookie = (UINT64)(unsigned long)event;
		desc->ptr    = __pa(event);
		desc->len    = len; /* total length  */
		desc->valid  = 1;

		/*
		 * This push operation will simultaneously advance the
		 * valid-tail index and increment our position in the queue
		 * by one.
		 */
		ret = ioq_iter_push(&iter, 0);
		if (ret < 0) 
			return STATUS_NO_MORE_ENTRIES;
	}

	eventq_notifier.signal = &eventq_wakeup,
	vbus_pci.eventq.notifier = &eventq_notifier;

	/*
	 * And finally, ensure that we can receive notification
	 */
	ioq_notify_enable(&vbus_pci.eventq, 0);

	return STATUS_SUCCESS;
}

/*
 * -------------------
 */

static int
vbus_pci_open(void)
{
	struct vbus_pci_bridge_negotiate params;
	
	params.magic	= VBUS_PCI_ABI_MAGIC;
	params.version      = VBUS_PCI_HC_VERSION;
	params.capabilities = 0;

	return vbus_pci_bridgecall(VBUS_PCI_BRIDGE_NEGOTIATE,
				  &params, sizeof(params));
}

#define QLEN 1024

static int 
vbus_pci_eventq_register(void)
{
	struct vbus_pci_busreg params;

	params.count = 1;
	params.eventq[0].count = QLEN;
	params.eventq[0].ring  =  __pa(vbus_pci.eventq.head_desc);
	params.eventq[0].data  =  __pa(vbus_pci.ring);

	return vbus_pci_bridgecall(VBUS_PCI_BRIDGE_QREG,
				   &params, sizeof(params));
}

static NTSTATUS
eventq_ioq_init(size_t ringsize, struct ioq *ioq, struct ioq_ops *ops)
{
	struct shm_signal	*signal = NULL;
	struct ioq_ring_head	*head = NULL;
	size_t			len  = IOQ_HEAD_DESC_SIZE(ringsize);

	head = VbusAlloc(len);
	if (!head)
		return STATUS_NO_MEMORY;

	signal = VbusAlloc(sizeof(*signal));
	if (!signal) {
		VbusFree(head);
		return STATUS_NO_MEMORY;
	}

	head->magic     = IOQ_RING_MAGIC;
	head->ver	= IOQ_RING_VER;
	head->count     = ringsize;

	eventq_signal_ops.inject  = eventq_signal_inject,
	eventq_signal_ops.release = eventq_signal_release,
	_signal_init(signal, &head->signal, &eventq_signal_ops);

	ioq_init(ioq, ops, ioq_locality_north, head, signal, ringsize);

	return STATUS_SUCCESS;
}

static NTSTATUS
vbus_pci_parse_resource(WDFCMRESLIST rt, ULONG i)
{
	PCM_PARTIAL_RESOURCE_DESCRIPTOR	d;
	ULONGLONG			len;

	d = WdfCmResourceListGetDescriptor(rt, i);
	if (!d)
		return STATUS_DEVICE_CONFIGURATION_ERROR;

	switch (d->Type) {
		case CmResourceTypePort:
			vbus_pci.signals = ULongToPtr(d->u.Port.Start.LowPart);
			vbus_pci.signals_length = d->u.Port.Length;
			break;

		case CmResourceTypeMemory:
			vbus_pci.regs = MmMapIoSpace(d->u.Memory.Start,
					d->u.Memory.Length,
					MmNonCached);
			vbus_pci.regs_length = d->u.Memory.Length;
			break;

		case CmResourceTypeInterrupt:
			break;

	}

	return STATUS_SUCCESS;
}


static void 
vbus_pci_cleanup(void)
{
	/* Perform all cleanups for a shutdown or restart */

	if (vbus_pci.regs) 
	        MmUnmapIoSpace(vbus_pci.regs, vbus_pci.regs_length);
	VbusFree(vbus_pci.eventq.head_desc);
	VbusFree(vbus_pci.eventq.signal);
	VbusFree(vbus_pci.ring);
	memset(&vbus_pci, '\0', sizeof(vbus_pci));
}

static NTSTATUS
vbus_pci_map_resources(WDFCMRESLIST rt)
{
	NTSTATUS	rc = STATUS_DEVICE_CONFIGURATION_ERROR;
   	ULONG		i;
	ULONG		cnt;

	for (i = 0; i < WdfCmResourceListGetCount(rt); i++) {
		rc = vbus_pci_parse_resource(rt, i);
		if (!NT_SUCCESS(rc))
			break;
	}
	return rc;
}

static NTSTATUS
VbusIntrEnable(WDFINTERRUPT in, WDFDEVICE dev)
{
vlog("interrupt enable");
	ShmSignalEnable(vbus_pci.eventq.signal, 0);
	return STATUS_SUCCESS;
}

static NTSTATUS
VbusIntrDisable(WDFINTERRUPT in, WDFDEVICE dev)
{
vlog("interrupt disable");
	ShmSignalDisable(vbus_pci.eventq.signal, 0);
	return STATUS_SUCCESS;
}

static VOID 
VbusIntrDPC(WDFINTERRUPT in, WDFOBJECT dev)
{
	_ShmSignalWakeup(vbus_pci.eventq.signal);
	ShmSignalEnable(vbus_pci.eventq.signal, 0);
}

static BOOLEAN 
VbusIntrIsr(WDFINTERRUPT in, ULONG mid)
{
	/* If we are currently disabled, discard this interrupt. */
	if (!ShmSignalTestEnabled(vbus_pci.eventq.signal))
		return FALSE;
vlog("*** INTERUPPT ***");

	/* disable, re-enabled in the DPC... */
	ShmSignalDisable(vbus_pci.eventq.signal, 0);

	WdfInterruptQueueDpcForIsr(in);

	return TRUE;
}

/*
 * Init the Vbus.
 */
NTSTATUS
VbusPciPrepareHardware(WDFDEVICE dev, WDFCMRESLIST rt)
{
	NTSTATUS 		rc;

	if (vbus_pci.enabled)
		return STATUS_DEVICE_ALREADY_ATTACHED;

	/* Set the bus device */
	vbus_pci.bus = dev;

	/* Create the spin used for bridgecalls */
	rc = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &vbus_pci.lock);
	if (!NT_SUCCESS(rc))
		return rc;

	/* Get and map our pci hw resources */
	rc = vbus_pci_map_resources(rt);
	if (!NT_SUCCESS(rc))
		goto out_fail;

	/* Negotiate and verify */
	rc = vbus_pci_open();
	if (rc < 0) {
		vlog("vbus_pci_open: rc = %d", rc);
		rc = STATUS_DEVICE_PROTOCOL_ERROR;
		goto out_fail;
	}

	/*
	 * Allocate an IOQ to use for host-2-guest event notification
	 */
	eventq_ioq_ops.release = eventq_ioq_release;
	rc = eventq_ioq_init(QLEN, &vbus_pci.eventq, &eventq_ioq_ops);
	if (!NT_SUCCESS(rc))
		goto out_fail;

	rc = eventq_init(QLEN);
	if (!NT_SUCCESS(rc))
		goto out_fail;

	/*
	 * Finally register our queue on the host to start receiving events
	 */
	rc = vbus_pci_eventq_register();
	if (rc < 0) {
		rc = STATUS_DEVICE_PROTOCOL_ERROR;
		goto out_fail;
	}

	vbus_pci.enabled = 1;
vlog("success");

	return STATUS_SUCCESS;

out_fail:
	vbus_pci_cleanup();
	return rc;
}

/* 
 * Create our interrupt resources 
 */
NTSTATUS 
VbusPciCreateResources(WDFDEVICE dev) 
{
	NTSTATUS                rc;
	WDF_INTERRUPT_CONFIG    ic;

	WDF_INTERRUPT_CONFIG_INIT(&ic, VbusIntrIsr, VbusIntrDPC);

	ic.EvtInterruptEnable  = VbusIntrEnable;
	ic.EvtInterruptDisable = VbusIntrDisable;
	rc = WdfInterruptCreate(dev, &ic, WDF_NO_OBJECT_ATTRIBUTES, 
			&vbus_pci.interrupt);
	return rc;
}

NTSTATUS
VbusPciReleaseHardware(void)
{
	vbus_pci_cleanup();
	return STATUS_SUCCESS;
}

NTSTATUS 
VbusPciD0Entry(WDFDEVICE dev, WDF_POWER_DEVICE_STATE prev_state)
{
vlog("VbusPciD0Entry: prev_state = %d", prev_state);
	//ShmSignalEnable(vbus_pci.eventq.signal, 0);
	return STATUS_SUCCESS;
}

NTSTATUS 
VbusPciD0Exit(WDFDEVICE dev, WDF_POWER_DEVICE_STATE next_state)
{
vlog("VbusPciD0Exit: next_state = %d", next_state);
	//ShmSignalDisable(vbus_pci.eventq.signal, 0);
	return STATUS_SUCCESS;
}

int
VbusBridgeCall(unsigned long nr, void *data, unsigned long len)
{
	return (vbus_pci_bridgecall(nr, data, len));
}

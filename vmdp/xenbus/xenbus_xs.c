/*****************************************************************************
 *
 *   File Name:      xenbus_xc.c
 *
 *   %version:       35 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Jul 27 09:00:25 2009 %
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

struct xenstore_domain_interface *xen_store_interface = NULL;

int xen_store_evtchn = 0;

struct xs_handle
{
    LIST_ENTRY reply_list;
    KSPIN_LOCK reply_lock;

    /* we are using event to replace wait queue */
    KEVENT reply_list_notempty;

    KMUTEX request_mutex;

    /* windows ``Resource'' functions for readwritelock */
    ERESOURCE suspend_mutex;
};

static struct xs_handle xs_state = {{0},(KSPIN_LOCK)0xbad,{0},{0},{0}};

static LIST_ENTRY watches;
static KSPIN_LOCK watches_lock = 0xbad;
static KSPIN_LOCK xs_lock = 0xbad;

static KSPIN_LOCK watch_events_lock = 0xbad;
static KEVENT watch_events_notempty;
static LIST_ENTRY watch_events;

#if defined XENBUG_TRACE_FLAGS || defined DBG
static uint32_t xenbus_wait_events = 0;
uint32_t xenbus_locks = 0;
uint32_t rtrace;
#endif
#ifdef DBG
static uint32_t DBG_WAIT = 0;
static uint32_t DBG_XS = 0;
uint32_t evt_print = 0;
#endif

/* system thread related objects */

static KEVENT thread_xenwatch_kill;
static KEVENT thread_xenbus_kill;
#ifdef XENBUS_HAS_DEVICE
static PKTHREAD thread_xw, thread_xb;
#endif
static HANDLE xenwatch_tid;
static KSPIN_LOCK xenbus_dpc_lock = 0xbad;
static PIO_WORKITEM xenbus_watch_work_item;
static uint32_t xenbus_watch_work_scheduled;

static KEVENT xb_event;

/* Ignore multiple shutdown requests. */
static int shutting_down = SHUTDOWN_INVALID;
static struct xenbus_watch shutdown_watch = {0};

static void
XenbusDpcRoutine(
	IN PKDPC Dpc,
	IN PVOID DpcContext,
	IN PVOID RegisteredContext,
	IN PVOID DeviceExtension)
{
	XEN_LOCK_HANDLE lh;

	DPR_WAIT(("XenbusDpcRoutine: cpu %x IN\n", KeGetCurrentProcessorNumber()));
	XenAcquireSpinLock(&xenbus_dpc_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_DPC);
	xb_read_msg();
	XENBUS_CLEAR_FLAG(rtrace, EVTCHN_F);
	DPR_WAIT(("XenbusDpcRoutine: signaling xb_event\n"));
	KeSetEvent(&xb_event, 0, FALSE);
	if (!IsListEmpty(&watch_events)) {
		if (!xenbus_watch_work_scheduled) {
			xenbus_watch_work_scheduled = 1;
			if (DpcContext == NULL) {
				DBG_PRINT(("XenbusDpcRoutine: DpcContext is NULL\n"));
			}
			if ((xenbus_watch_work_item =
					IoAllocateWorkItem((PDEVICE_OBJECT)DpcContext)) != NULL) {
				DPR_WAIT(("XenbusDpcRoutine: IoQueueWorkItem\n"));
				IoQueueWorkItem(xenbus_watch_work_item,
					(void (*)(PDEVICE_OBJECT, void *))xenbus_watch_work,
					DelayedWorkQueue, xenbus_watch_work_item);
			}
		}
	}
	XENBUS_CLEAR_FLAG(xenbus_locks, X_DPC);
	XenReleaseSpinLock(&xenbus_dpc_lock, lh);
	DPR_WAIT(("XenbusDpcRoutine: cpu %x OUT\n", KeGetCurrentProcessorNumber()));
}


static int get_error(const char *errorstring)
{
	unsigned int i, len;

	len = (sizeof(xsd_errors)/sizeof(xsd_errors[0]));
	for (i=0; i<len; i++) {
		if (strcmp(errorstring, xsd_errors[i].errstring) == 0)
			break;
	}

	if (i == len) {
		DBG_PRINT(("XENBUS: xenstore gives unknown error %s", errorstring));
		return -1;
	}

	return xsd_errors[i].errnum;
}

static void *xenbus_get_output_chunck(XENSTORE_RING_IDX cons,
	XENSTORE_RING_IDX prod,
	char *buf, uint32_t *len)
{
	*len = XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(prod);
	if ((XENSTORE_RING_SIZE - (prod - cons)) < *len)
		*len = XENSTORE_RING_SIZE - (prod - cons);
	return buf + MASK_XENSTORE_IDX(prod);
}

static const void *xenbus_get_input_chunk(XENSTORE_RING_IDX cons,
	XENSTORE_RING_IDX prod,
	const char *buf, uint32_t *len)
{
	*len = XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(cons);
	if ((prod - cons) < *len)
		*len = prod - cons;
	return buf + MASK_XENSTORE_IDX(cons);
}

static int xenbus_check_indexes(XENSTORE_RING_IDX cons, XENSTORE_RING_IDX prod)
{
	return ((prod - cons) <= XENSTORE_RING_SIZE);
}

static int xb_write(const void *buf, unsigned int len)
{
	struct xenstore_domain_interface *intf;
	XENSTORE_RING_IDX cons, prod;
	int rc;
	PUCHAR data = (PUCHAR) buf;
	LARGE_INTEGER timeout;

	intf = xen_store_interface;
	timeout.QuadPart = 0;

	DPR_WAIT(("xb_write: In\n"));
	while (len != 0) {
		PUCHAR dst;
		unsigned int avail;
		NTSTATUS status;

		for (;;) {
			if ((intf->req_prod - intf->req_cons) != XENSTORE_RING_SIZE) {
				DPR_WAIT(("xb_write: break\n"));
				break;
			}

			DPR_WAIT(("xb_write: KeWaitForSingleObject\n"));
			XENBUS_SET_FLAG(xenbus_wait_events, XB_EVENT);
			status = KeWaitForSingleObject(
				&xb_event,
				Executive,
				KernelMode,
				FALSE,
				&timeout);
			if (status != STATUS_SUCCESS) {
				XENBUS_SET_FLAG(rtrace, XB_WRITE_F);
				if (gfdo) {
					XenbusDpcRoutine(NULL, gfdo, NULL, NULL);
				}
				else {
					DPR_INIT(("xb_write: gfdo is NULL\n"));
				}
				XENBUS_CLEAR_FLAG(rtrace, XB_WRITE_F);
			}
			XENBUS_CLEAR_FLAG(xenbus_wait_events, XB_EVENT);
			DPR_WAIT(("xb_write: KeWaitForSingleObject done\n"));

			status = KeWaitForSingleObject(
				&thread_xenbus_kill,
				Executive,
				KernelMode,
				FALSE,
				&timeout);
			if (status == STATUS_SUCCESS || status == STATUS_ALERTED) {
				DPR_WAIT(("xb_write: return -1\n"));
				return -1;
			}

			DPR_WAIT(("xb_write: KeClearEvent\n"));
			KeClearEvent(&xb_event);
		}

		/* Read indexes, then verify. */
		cons = intf->req_cons;
		prod = intf->req_prod;
		KeMemoryBarrier();

		DPR_WAIT(("xb_write: xenbus_check_indexes\n"));
		if (!xenbus_check_indexes(cons, prod)) {
			intf->req_cons = intf->req_prod = 0;
			DPR_WAIT(("XENBUS: xenstore ring overflow! reset.\n"));
			return -EIO;
		}

		DPR_WAIT(("xb_write: xenbus_get_output_chunck\n"));
		dst = xenbus_get_output_chunck(cons, prod, intf->req, &avail);
		if (avail == 0)
			continue;
		if (avail > len)
			avail = len;

		RtlCopyMemory(dst, data, avail);
		data += avail;
		len -= avail;

		KeMemoryBarrier();
		intf->req_prod += avail;

		DPR_WAIT(("xb_write: notify_remote_via_evtchn\n"));
		notify_remote_via_evtchn(xen_store_evtchn);
	}

	DPR_WAIT(("xb_write: Out\n"));
	return 0;
}


static void *read_reply(uint32_t *type, unsigned int *len)
{
	struct xs_stored_msg *msg;
	char *body;
	PLIST_ENTRY ple;
	XEN_LOCK_HANDLE lh;
	LARGE_INTEGER timeout;
	NTSTATUS status;

	timeout.QuadPart = 0;

	XenAcquireSpinLock(&xs_state.reply_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_RPL);

	XENBUS_SET_FLAG(xenbus_wait_events, XS_LIST);
	while (IsListEmpty(&xs_state.reply_list)) {
		XENBUS_CLEAR_FLAG(xenbus_locks, X_RPL);
		XenReleaseSpinLock(&xs_state.reply_lock, lh);

		status = KeWaitForSingleObject(
			&xs_state.reply_list_notempty,
			Executive,
			KernelMode,
			FALSE,
			&timeout);
		if (status != STATUS_SUCCESS) {
			XENBUS_SET_FLAG(rtrace, READ_REPLY_F);
			if (gfdo) {
				XenbusDpcRoutine(NULL, gfdo, NULL, NULL);
			}
			else {
				DPR_INIT(("read_reply: gfdo is NULL\n"));
			}
			XENBUS_CLEAR_FLAG(rtrace, READ_REPLY_F);
		}

		XenAcquireSpinLock(&xs_state.reply_lock, &lh);
		XENBUS_SET_FLAG(xenbus_locks, X_RPL);
	}
	XENBUS_CLEAR_FLAG(xenbus_wait_events, XS_LIST);

	ple = RemoveHeadList(&xs_state.reply_list);

	if (IsListEmpty(&xs_state.reply_list)) {
		DPR_WAIT(("read_reply: KeClearEvent xs_state.reply_list_notempty\n"));
		KeClearEvent(&xs_state.reply_list_notempty);
	}

	msg = CONTAINING_RECORD(ple, struct xs_stored_msg, list);

	XENBUS_CLEAR_FLAG(xenbus_locks, X_RPL);
	XenReleaseSpinLock(&xs_state.reply_lock, lh);

	*type = msg->hdr.type;
	if (len)
		*len = msg->hdr.len;
	body = msg->u.reply.body;

	ExFreePool(msg);

	DPR_XS(("read_reply: out\n"));
	return body;
}

static void *xs_talkv(struct xenbus_transaction t,
	enum xsd_sockmsg_type type,
	const struct kvec *iovec,
	unsigned int num_vecs,
	unsigned int *len)
{
	struct xsd_sockmsg msg;
	void *ret = NULL;
	unsigned int i;
	int err;
	LARGE_INTEGER timeout;
	XEN_LOCK_HANDLE lh;

	timeout.QuadPart = 0;

	DPR_XS(("xs_talkv: XenAcquireSpinLock, irql %x, cpu %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));

	msg.tx_id = t.id;
	msg.req_id = 0;
	msg.type = type;
	msg.len = 0;
	for (i = 0; i < num_vecs; i++)
		msg.len += iovec[i].iov_len;

	XenAcquireSpinLock(&xs_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_XSL);
	XENBUS_SET_FLAG(xenbus_wait_events, XS_REQUEST);

	DPR_XS(("xs_talkv: xb_write, irql %x, cpu %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	err = xb_write(&msg, sizeof(msg));
	if (err) {
		XENBUS_CLEAR_FLAG(xenbus_wait_events, XS_REQUEST);
		XENBUS_CLEAR_FLAG(xenbus_locks, X_XSL);
		XenReleaseSpinLock(&xs_lock, lh);
		DBG_PRINT(("xs_talkv: xb_write err %x\n", KeGetCurrentProcessorNumber()));
		return ERR_PTR(err);
	}

	for (i = 0; i < num_vecs; i++) {
		DPR_XS(("xs_talkv: xb_write iovec\n"));
		err = xb_write(iovec[i].iov_base, iovec[i].iov_len);;
		if (err) {
			XENBUS_CLEAR_FLAG(xenbus_wait_events, XS_REQUEST);
			XENBUS_CLEAR_FLAG(xenbus_locks, X_XSL);
			XenReleaseSpinLock(&xs_lock, lh);
			DBG_PRINT(("xs_talkv: xb_write err 2, %x\n",
				KeGetCurrentProcessorNumber()));
			return ERR_PTR(err);
		}
	}

	DPR_XS(("xs_talkv: read_reply, irql %x, cpu %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	ret = read_reply(&msg.type, len);

	XENBUS_CLEAR_FLAG(xenbus_wait_events, XS_REQUEST);
	XENBUS_CLEAR_FLAG(xenbus_locks, X_XSL);
	XenReleaseSpinLock(&xs_lock, lh);

	if (IS_ERR(ret)) {
		DBG_PRINT(("xs_talkv: read_reply err %x\n",KeGetCurrentProcessorNumber()));
		return ret;
	}

	if (msg.type == XS_ERROR) {
		DPR_XS(("xs_talkv: msg.type XS_ERROR: %s, cpu %x\n",
			ret, KeGetCurrentProcessorNumber()));
		err = get_error(ret);
		ExFreePool(ret);
		return ERR_PTR(-err);
	}

	if (msg.type != type) {
		DBG_PRINT(("XENBUS unexpected type %d, expected %d, %x\n",
			msg.type, type, KeGetCurrentProcessorNumber()));
		ExFreePool(ret);
		return ERR_PTR(-EINVAL);
	}
	DPR_XS(("xs_talkv: out, irql %x, cpu %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	return ret;
}

/* Simplified version of xs_talkv: single message. */
static void *xs_single(struct xenbus_transaction t,
	enum xsd_sockmsg_type type,
	const char *string,
	unsigned int *len)
{
	struct kvec iovec;

	iovec.iov_base = (void *)string;
	iovec.iov_len = strlen(string) + 1;
	DPR_XS(("xs_single: xs_talkv\n"));
	return xs_talkv(t, type, &iovec, 1, len);
}

/* Many commands only need an ack, don't care what it says. */
static int xs_error(char *reply)
{
	if (IS_ERR(reply))
		return (int)PTR_ERR(reply);

	ExFreePool(reply);
	return 0;
}

static unsigned int count_strings(const char *strings, unsigned int len)
{
	unsigned int num;
	const char *p;

	for (p = strings, num = 0; p < strings + len; p += strlen(p) + 1)
		num++;

	return num;
}

/* Simplified asprintf. */
char *kasprintf(size_t len, const char *fmt, ...)
{
	va_list ap;
	char *p;

	p = ExAllocatePoolWithTag(NonPagedPool, len + 1, XENBUS_POOL_TAG);
	if (!p)
		return NULL;

	va_start(ap, fmt);
	RtlStringCbVPrintfA(p, len + 1, fmt, ap);
	va_end(ap);
	return p;
}

/* Return the path to dir with /name appended. Buffer must be kfree()'ed. */
static char *join(const char *dir, const char *name)
{
	size_t i, j;
	char *buffer;

	i = strlen(dir);
	j = strlen(name);

	if (j == 0)
		buffer = kasprintf(i, "%s", dir);
	else
		buffer = kasprintf(i + j + 1, "%s/%s", dir, name);
	return (!buffer) ? ERR_PTR(-ENOMEM) : buffer;
}

static char **split(char *strings, unsigned int len, unsigned int *num)
{
	char *p, **ret;

	/* Count the strings. */
	*num = count_strings(strings, len);

	/* Transfer to one big alloc for easy freeing. */
	ret = ExAllocatePoolWithTag(
		NonPagedPool,
		*num * sizeof(char *) + len,
		XENBUS_POOL_TAG);

	if (!ret) {
		ExFreePool(strings);
		return ERR_PTR(-ENOMEM);
	}

	RtlCopyMemory(&ret[*num], strings, len);
	ExFreePool(strings);

	strings = (char *)&ret[*num];
	for (p = strings, *num = 0; p < strings + len; p += strlen(p) + 1)
		ret[(*num)++] = p;

	return ret;
}

char **xenbus_directory(struct xenbus_transaction t,
	const char *dir, const char *node, unsigned int *num)
{
	char *strings, *path;
	unsigned int len;

	path = join(dir, node);
	if (IS_ERR(path))
		return (char **)path;

	strings = xs_single(t, XS_DIRECTORY, path, &len);
	ExFreePool(path);
	if (IS_ERR(strings) || len == 0)
		return (char **)strings;

	return split(strings, len, num);
}

/* Check if a path exists. Return 1 if it does. */
int xenbus_exists(struct xenbus_transaction t,
	const char *dir, const char *node)
{
	char **d;
	int dir_n;

	d = xenbus_directory(t, dir, node, &dir_n);
	if (IS_ERR(d))
		return 0;
	ExFreePool(d);
	return 1;
}

/* Get the value of a single file.
 * Returns a kmalloced value: call free() on it after use.
 * len indicates length in bytes.
 */
void *xenbus_read(struct xenbus_transaction t,
	const char *dir, const char *node, unsigned int *len)
{
	char *path;
	void *ret;

	path = join(dir, node);
	if (IS_ERR(path))
		return (void *)path;

	DPR_XS(("xenbus_read: xs_single\n"));
	ret = xs_single(t, XS_READ, path, len);
	ExFreePool(path);
	if (IS_ERR(ret))
		return NULL;
	else
		return ret;
}


/* Write the value of a single file.
 * Returns -err on failure.
 */
int xenbus_write(struct xenbus_transaction t,
	const char *dir, const char *node, const char *string)
{
	char *path;
	struct kvec iovec[2];
	int ret;

	path = join(dir, node);
	if (IS_ERR(path))
		return (int)PTR_ERR(path);

	iovec[0].iov_base = (void *)path;
	iovec[0].iov_len = strlen(path) + 1;
	iovec[1].iov_base = (void *)string;
	iovec[1].iov_len = strlen(string);

	ret = xs_error(xs_talkv(t, XS_WRITE, iovec,
		sizeof(iovec)/sizeof(iovec[0]), NULL));
	ExFreePool(path);
	return ret;
}

/* Create a new directory. */
int xenbus_mkdir(struct xenbus_transaction t,
	const char *dir, const char *node)
{
	char *path;
	int ret;

	path = join(dir, node);
	if (IS_ERR(path))
		return (int)PTR_ERR(path);

	ret = xs_error(xs_single(t, XS_MKDIR, path, NULL));
	ExFreePool(path);
	return ret;
}

/* Destroy a file or directory (directories must be empty). */
int xenbus_rm(struct xenbus_transaction t, const char *dir, const char *node)
{
	char *path;
	int ret;

	path = join(dir, node);
	if (IS_ERR(path))
		return (int)PTR_ERR(path);

	ret = xs_error(xs_single(t, XS_RM, path, NULL));
	ExFreePool(path);
	return ret;
}


/* Start a transaction: changes by others will not be seen during this
 * transaction, and changes will not be visible to others until end.
 */
int xenbus_transaction_start(struct xenbus_transaction *t)
{
	char *id_str;
	ULONG id;

#ifdef DBG
	DBG_WAIT = 1;
	DBG_XS = 1;
#endif
	DPR_XS(("xenbus_transaction_start: irql %x, cpu %x IN\n", KeGetCurrentIrql(),
			  KeGetCurrentProcessorNumber()));
	KeEnterCriticalRegion();
	DPR_XS(("xenbus_transaction_start: ExAcquireResourceSharedLite\n"));
	ExAcquireResourceSharedLite(&xs_state.suspend_mutex, TRUE);


	DPR_XS(("xenbus_transaction_start: xs_single\n"));
#ifdef DBG
	//xenbus_debug_dump();
#endif
	id_str = xs_single(XBT_NIL, XS_TRANSACTION_START, "", NULL);
	if (IS_ERR(id_str)) {
		DPR_XS(("xenbus_transaction_start: ExReleaseResourceLite\n"));
		ExReleaseResourceLite(&xs_state.suspend_mutex);
		DPR_XS(("xenbus_transaction_start: KeLeaveCritical\n"));
		KeLeaveCriticalRegion();

		DPR_XS(("xenbus_transaction_start: OUT\n"));
		return (int)PTR_ERR(id_str);
	}

	id = (ULONG)cmp_strtoul(id_str, NULL, 10);
	t->id = id;
	ExFreePool(id_str);
	DPR_XS(("xenbus_transaction_start: irql %x, cpu %x OUT\n", KeGetCurrentIrql(),
			  KeGetCurrentProcessorNumber()));
	return 0;
}

/* End a transaction.
 * If abandon is true, transaction is discarded instead of committed.
 */
int xenbus_transaction_end(struct xenbus_transaction t, int abort)
{
	char abortstr[2];
	int err;

	if (abort)
		RtlStringCbCopyA(abortstr, 2, "F");
	else
		RtlStringCbCopyA(abortstr, 2, "T");

	DPR_XS(("xenbus_transaction_end: irql %x, cpu %x OUT\n", KeGetCurrentIrql(),
			  KeGetCurrentProcessorNumber()));
	err = xs_error(xs_single(t, XS_TRANSACTION_END, abortstr, NULL));

	DPR_XS(("xenbus_transaction_end: ExReleaseResourceLite irql %x, cpu %x\n",
			  KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	ExReleaseResourceLite(&xs_state.suspend_mutex);
	DPR_XS(("xenbus_transaction_end: KeLeaveCriticalRegion irql %x, cpu %x\n",
			  KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	KeLeaveCriticalRegion();

#ifdef DBG
	DBG_WAIT = 0;
	DBG_XS = 0;
#endif
	return err;
}

/* Single printf and write: returns -errno or 0. */
int xenbus_printf(struct xenbus_transaction t,
	const char *dir, const char *node, const char *fmt, ...)
{
	va_list ap;
	int ret;
	NTSTATUS status;
	char *printf_buffer;

	printf_buffer =ExAllocatePoolWithTag(
		NonPagedPool,
		PRINTF_BUFFER_SIZE,
		XENBUS_POOL_TAG);
	if (printf_buffer == NULL)
		return -ENOMEM;

	va_start(ap, fmt);
	status = RtlStringCbVPrintfA(printf_buffer, PRINTF_BUFFER_SIZE, fmt, ap);
	va_end(ap);

	if (status != STATUS_SUCCESS)
		return -ENOMEM;

	ret = xenbus_write(t, dir, node, printf_buffer);

	ExFreePool(printf_buffer);

	return ret;
}


/* we have to do this in order to mix xenbus and ndis code */
void
xenbus_free_string(char *str)
{
	ExFreePool(str);
}

static int xs_watch(const char *path, const char *token)
{
	struct kvec iov[2];

	iov[0].iov_base = (void *)path;
	iov[0].iov_len = strlen(path) + 1;
	iov[1].iov_base = (void *)token;
	iov[1].iov_len = strlen(token) + 1;

	return xs_error(xs_talkv(XBT_NIL, XS_WATCH, iov,
		sizeof(iov)/sizeof(iov[0]), NULL));
}

static int xs_unwatch(const char *path, const char *token)
{
	struct kvec iov[2];

	iov[0].iov_base = (char *)path;
	iov[0].iov_len = strlen(path) + 1;
	iov[1].iov_base = (char *)token;
	iov[1].iov_len = strlen(token) + 1;

	return xs_error(xs_talkv(XBT_NIL, XS_UNWATCH, iov,
		sizeof(iov)/sizeof(iov[0]), NULL));
}

static struct xenbus_watch *find_watch(const char *token)
{
	struct xenbus_watch *i, *cmp;
	PLIST_ENTRY li;

	cmp = (struct xenbus_watch *)cmp_strtoul(token, NULL, 16);

	li = watches.Flink;

	for (; li!=&watches; li=li->Flink) {
		i = CONTAINING_RECORD(li, struct xenbus_watch, list);
		if ((*(uintptr_t *)&i) == (*(uintptr_t *)&cmp))
			return i;
	}

	return NULL;
}

/* Register callback to watch this node. */
int register_xenbus_watch(struct xenbus_watch *watch)
{
	/* Pointer in ascii is the token. */
	char token[sizeof(watch) * 2 + 1];
	XEN_LOCK_HANDLE lh;
	int err = 0;

	RtlStringCbPrintfA(token, sizeof(token), "%p", watch);

	DPR_INIT(("register_xenbus_watch: registering %s, %p, irql %x, cpu %x IN\n",
		watch->node, watch->callback,
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));

	XenAcquireSpinLock(&watches_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_WAT);
	if (find_watch(token) == NULL) {
		InsertHeadList(&watches, &watch->list);

		XENBUS_CLEAR_FLAG(xenbus_locks, X_WAT);
		XenReleaseSpinLock(&watches_lock, lh);
		err = xs_watch(watch->node, token);

		/* Ignore errors due to multiple registration. */
		if ((err != 0) && (err != -EEXIST)) {
			DBG_PRINT(("register_xenbus_watch: err = 0x%x, %d.\n", err, err));
			XenAcquireSpinLock(&watches_lock, &lh);
			XENBUS_SET_FLAG(xenbus_locks, X_WAT);
			RemoveEntryList(&watch->list);
			XENBUS_CLEAR_FLAG(xenbus_locks, X_WAT);
			XenReleaseSpinLock(&watches_lock, lh);
		}
	}
	else {
		err = -EEXIST;
		DPR_INIT(("register_xenbus_watch: watch already registered.\n"));
		XENBUS_CLEAR_FLAG(xenbus_locks, X_WAT);
		XenReleaseSpinLock(&watches_lock, lh);
	}


	DPR_INIT(("register_xenbus_watch: irql %x, cpu %x OUT %x\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber(), err));
	return err;
}

void unregister_xenbus_watch(struct xenbus_watch *watch)
{
	struct xs_stored_msg *msg, *tmp;
	char token[sizeof(watch) * 2 + 1];
	int err;
	XEN_LOCK_HANDLE lh;
	PLIST_ENTRY li, nli;

	DPR_INIT(("XENBUS: unregister_xenbus_watch IN %p\n", watch));
	RtlStringCbPrintfA(token, sizeof(token), "%p", watch);

	XenAcquireSpinLock(&watches_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_WAT);
	if (!find_watch(token)) {
		DBG_PRINT(("XENBUS: error! trying to unregister noexist watch\n"));
		XENBUS_CLEAR_FLAG(xenbus_locks, X_WAT);
		XenReleaseSpinLock(&watches_lock, lh);
		return;
	}

	DPR_INIT(("XENBUS: unregister_xenbus_watch removing %p\n",watch->callback));
	RemoveEntryList(&watch->list);
	XENBUS_CLEAR_FLAG(xenbus_locks, X_WAT);
	XenReleaseSpinLock(&watches_lock, lh);

	err = xs_unwatch(watch->node, token);
	if (err)
		DBG_PRINT(("XENBUS: Failed to release watch %s: %i\n",
			watch->node, err));

	/* Cancel pending watch events. */
	XenAcquireSpinLock(&watch_events_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_WEL);

	li = watch_events.Flink;

	for (; li!=&watch_events; li=nli) {
		nli = li->Flink;
		msg = CONTAINING_RECORD(li, struct xs_stored_msg, list);
		if (msg->u.watch.handle != watch)
			continue;
		RemoveEntryList(&msg->list);
		if (IsListEmpty(&watch_events))
		{
			DPR_WAIT(("unregister_xenbus_watch: KeClearEvent watch_events_notempty\n"));
			KeClearEvent(&watch_events_notempty);
		}

		ExFreePool(msg->u.watch.vec);
		ExFreePool(msg);
	}

	watch->callback = NULL;
	XENBUS_CLEAR_FLAG(xenbus_locks, X_WAT);
	XenReleaseSpinLock(&watch_events_lock, lh);

	DPR_INIT(("XENBUS: unregister_xenbus_watch OUT\n"));

}

void xenbus_watch_work(IN PDEVICE_OBJECT DeviceObject,
	PIO_WORKITEM work_item)
{
	struct xenbus_watch *i;
	PLIST_ENTRY li;
	struct xs_stored_msg *msg;
	XEN_LOCK_HANDLE lh;
	PLIST_ENTRY ent;
	LARGE_INTEGER timeout;

	DPR_INIT(("xenbus_watch_work: irql %x, cpu %x IN\n", KeGetCurrentIrql(),
			  KeGetCurrentProcessorNumber()));
#ifdef DBG
	msg = NULL;
#endif

	timeout.QuadPart = 0;

	XenAcquireSpinLock(&watch_events_lock, &lh);
	XENBUS_SET_FLAG(xenbus_locks, X_WEL);
	ent = RemoveHeadList(&watch_events);
	while (ent != &watch_events) {
		if (IsListEmpty(&watch_events))
		{
			DPR_WAIT(("xenwatch_thread: KeClearEvent watch_events_notempty\n"));
			KeClearEvent(&watch_events_notempty);
		}
		msg = CONTAINING_RECORD(ent, struct xs_stored_msg, list);



		for (li = watches.Flink; li!=&watches; li=li->Flink) {
			i = CONTAINING_RECORD(li, struct xenbus_watch, list);
			if (i->callback == msg->u.watch.handle->callback) {
				DPR_WAIT(("xenbus_watch: calling callback\n"));
				XENBUS_CLEAR_FLAG(xenbus_locks, X_WEL);
				XenReleaseSpinLock(&watch_events_lock, lh);

				DPR_INIT(("xenbus_watch_work: calling callback %p, irql %x, cpu %x\n",
					  msg->u.watch.handle->callback,
					  KeGetCurrentIrql(),
					  KeGetCurrentProcessorNumber()));
				msg->u.watch.handle->callback(msg->u.watch.handle,
					(const char **)msg->u.watch.vec,
					msg->u.watch.vec_size);
				DPR_INIT(("xenbus_watch_work: back from callback %p, irql %x, cpu %x\n",
					  msg->u.watch.handle->callback,
					  KeGetCurrentIrql(),
					  KeGetCurrentProcessorNumber()));

				XenAcquireSpinLock(&watch_events_lock, &lh);
				XENBUS_SET_FLAG(xenbus_locks, X_WEL);
				break;
			}
		}



		ExFreePool(msg->u.watch.vec);
		ExFreePool(msg);
		ent = RemoveHeadList(&watch_events);
	}
	IoFreeWorkItem(work_item);
	xenbus_watch_work_scheduled = 0;
	XENBUS_CLEAR_FLAG(xenbus_locks, X_WEL);
	XenReleaseSpinLock(&watch_events_lock, lh);
#ifdef DBG
	if (msg == NULL) {
		DPR_INIT(("xenbus_watch: OUT - no watch events to process\n"));
	}
	DPR_INIT(("xenbus_watch_work: irql %x, cpu %x OUT\n", KeGetCurrentIrql(),
			  KeGetCurrentProcessorNumber()));
	//xenbus_debug_dump();
#endif
}

static int xb_read_fast(void *buf, uint32_t offset, unsigned int len)
{
    struct xenstore_domain_interface *intf = xen_store_interface;
    XENSTORE_RING_IDX cons, prod;
    int rc;
    PUCHAR data = buf;
	unsigned int avail;
	const char *src;

	/* Read indexes, then verify. */
	cons = intf->rsp_cons + offset;
	prod = intf->rsp_prod;
	KeMemoryBarrier();
	if (!xenbus_check_indexes(cons, prod)) {
		intf->rsp_cons = intf->rsp_prod = 0;
		DBG_PRINT(("XENBUS: xenstore ring overflow! reset.\n"));
		return -EIO;
	}

    while (len != 0) {
        src = xenbus_get_input_chunk(cons, prod, intf->rsp, &avail);
        if (avail == 0) {
			/* This can only happen if (prod - cons) < len and */
			/* this shouldn't happen if the backend only advances */
			/* after writing the whole chunk.  But if it does happen,*/
			/* just returning will eventually cause a re-read and */
			/* everything should then be ok. */
			DBG_PRINT(("XENBUS: xb_read_fast avail == 0.\n"));
            return ENOMEM;
		}
        if (avail > len)
            avail = len;

        RtlCopyMemory(data, src, avail);
        data += avail;
        len -= avail;
        cons += avail;
    }

    return 0;
}

void xb_read_msg(void)
{
	struct xenstore_domain_interface *intf = xen_store_interface;
	struct xs_stored_msg *msg;
	char *body;
    XENSTORE_RING_IDX cons, prod;
	int err;

	XEN_LOCK_HANDLE lh;

	DPR_WAIT(("xb_read_msg: %x IN\n", KeGetCurrentProcessorNumber()));

	if (xen_store_interface == NULL) {
		/* We have not finished initializing yet. */
		DPR_INIT(("xb_read_msg: xen_store_interface is NULL, %x.\n", rtrace)); 
		return;
	}
	while (TRUE) {
		DPR_WAIT(("xb_read_msg: top of while\n"));
		cons = intf->rsp_cons;
		prod = intf->rsp_prod;
		KeMemoryBarrier();
		if (prod - cons < sizeof(struct xsd_sockmsg)) {
			DPR_WAIT(("xb_read_msg: %x no more work - OUT\n",
				KeGetCurrentProcessorNumber()));
			return;
		}

		msg = ExAllocatePoolWithTag(NonPagedPool, sizeof(*msg),
			XENBUS_POOL_TAG);
		if (msg == NULL) {
			DPR_WAIT(("xb_read_msg: %x mem failure - OUT\n",
				KeGetCurrentProcessorNumber()));
			return;
		}

		DPR_WAIT(("xb_read_msg: xb_read_fast hdr\n"));
		err = xb_read_fast(&msg->hdr, 0, sizeof(msg->hdr));
		if (err) {
			ExFreePool(msg);
			DPR_WAIT(("xb_read_msg: %x mem failure 2 - OUT\n",
				KeGetCurrentProcessorNumber()));
			return;
		}

		KeMemoryBarrier();
		if (intf->rsp_prod - (intf->rsp_cons + sizeof(msg->hdr))
				< msg->hdr.len) {
			DPR_WAIT(("xb_read_msg: header len too big %x, %x, %x, %x\n",
				msg->hdr.type, msg->hdr.len, msg->hdr.req_id, msg->hdr.tx_id));
			DPR_WAIT(("xb_read_msg: rsp_prod %x, rsp_cons %x, hdr %x, rslt %x\n",
				intf->rsp_prod, intf->rsp_cons, sizeof(msg->hdr),
				intf->rsp_prod - (intf->rsp_cons + sizeof(msg->hdr))));
			ExFreePool(msg);
			return;
		}
		if ((body = ExAllocatePoolWithTag(NonPagedPool, msg->hdr.len + 1,
				XENBUS_POOL_TAG)) == NULL) {
			ExFreePool(msg);
			DPR_WAIT(("xb_read_msg: %x mem failure 3 - OUT\n",
				KeGetCurrentProcessorNumber()));
			return;
		}

		DPR_WAIT(("xb_read_msg: xb_read_fast body\n"));
		err = xb_read_fast(body, sizeof(msg->hdr), msg->hdr.len);
		if (err) {
			ExFreePool(body);
			ExFreePool(msg);
			DPR_WAIT(("xb_read_msg: %x xb_read_fast body failure - OUT\n",
				KeGetCurrentProcessorNumber()));
			return;
		}
		body[msg->hdr.len] = '\0';

		if (msg->hdr.type == XS_WATCH_EVENT) {
			msg->u.watch.vec = split(body, msg->hdr.len,
									 &msg->u.watch.vec_size);
			if (IS_ERR(msg->u.watch.vec)) {
				/* split already freed body */
				ExFreePool(msg);
				DPR_WAIT(("xb_read_msg: %x watch failure - OUT\n",
					KeGetCurrentProcessorNumber()));
				return;
			}

			XenAcquireSpinLock(&watches_lock, &lh);
			XENBUS_SET_FLAG(xenbus_locks, X_WAT);
			msg->u.watch.handle = find_watch(
				msg->u.watch.vec[XS_WATCH_TOKEN]);
			XENBUS_CLEAR_FLAG(xenbus_locks, X_WAT);
			XenReleaseSpinLock(&watches_lock, lh);
			if (msg->u.watch.handle != NULL) {
				XenAcquireSpinLock(&watch_events_lock, &lh);
				XENBUS_SET_FLAG(xenbus_locks, X_WEL);
				InsertTailList(&watch_events, &msg->list);
				XENBUS_CLEAR_FLAG(xenbus_locks, X_WEL);
				XenReleaseSpinLock(&watch_events_lock, lh);
				DPR_WAIT(("xb_read_msg: signaling watch_events_notempty\n"));
				KeSetEvent(&watch_events_notempty, 0, FALSE);
			} else {
				ExFreePool(msg->u.watch.vec);
				ExFreePool(msg);
			}
		} else {
			msg->u.reply.body = body;
			XenAcquireSpinLock(&xs_state.reply_lock, &lh);
			XENBUS_SET_FLAG(xenbus_locks, X_RPL);
			InsertTailList(&xs_state.reply_list, &msg->list);
			XENBUS_CLEAR_FLAG(xenbus_locks, X_RPL);
			XenReleaseSpinLock(&xs_state.reply_lock, lh);
			DPR_WAIT(("xb_read_msg: signaling xs_state\n"));
			KeSetEvent(&xs_state.reply_list_notempty, 0, FALSE);
		}

		/* Other side must not see free space until we've copied out */
		KeMemoryBarrier();
		intf->rsp_cons += sizeof(msg->hdr) + msg->hdr.len;

		/* Implies mb(): they will see new header. */
		notify_remote_via_evtchn(xen_store_evtchn);
	}
	DPR_WAIT(("xb_read_msg: OUT\n"));
}

static void
xenbus_suspend(PFDO_DEVICE_EXTENSION fdx, uint32_t reason)
{
    PPDO_DEVICE_EXTENSION pdx;
    PLIST_ENTRY entry;
	pv_ioctl_t ioctl_data;
	LARGE_INTEGER timeout;
	uint32_t waiting_cnt;
	int suspend_canceled;

	DPR_INIT(("xenbus_suspend: irql %d, reason %d.\n",
		KeGetCurrentIrql(), reason));

	ioctl_data.cmd = PV_SUSPEND;
	ioctl_data.arg = (uint16_t)reason;
	timeout.QuadPart = -10000000; /* 1 second */

	/* Suspend the vnifs first. */
	for (entry = fdx->ListOfPDOs.Flink;
		entry != &fdx->ListOfPDOs;
		entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (pdx->Type == vnif && pdx->frontend_dev) {
			DPR_INIT(("xenbus_suspend: suspending %p, %s.\n",
				pdx->frontend_dev, pdx->Nodename));
			waiting_cnt = 0;
			while (pdx->ioctl(pdx->frontend_dev, ioctl_data)) {
				DBG_PRINT(("xenbus_suspend: suspending %p, %s, cnt %d.\n",
					pdx->frontend_dev, pdx->Nodename, waiting_cnt));
				KeDelayExecutionThread(KernelMode, FALSE, &timeout);
				waiting_cnt++;
			}
		}
	}

	/* Suspend the disk controller.  There's jsut one. */
	for (entry = fdx->ListOfPDOs.Flink;
		entry != &fdx->ListOfPDOs;
		entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (pdx->Type == vbd && pdx->controller) {
			DPR_INIT(("xenbus_suspend: suspending %p, %s.\n",
				pdx->controller, pdx->Nodename));
			pdx->ioctl(pdx->controller, ioctl_data);
			break;
		}
	}

	/* Clear out any DPCs that may have been scheduled. */
	evtchn_remove_queue_dpc();

	/* Do the actual suspend. */
	DPR_INIT(("xenbus_suspend: HYPERVISOR_shutdown %p.\n", &suspend_canceled));
	suspend_canceled = HYPERVISOR_shutdown(reason);

	/* Start things back up again based on suspend_canceled. */
	ioctl_data.cmd = PV_RESUME;
	ioctl_data.arg = (uint16_t)suspend_canceled;

	/* Start up the vbd first. */
	for (entry = fdx->ListOfPDOs.Flink;
		entry != &fdx->ListOfPDOs;
		entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (pdx->Type == vbd && pdx->controller) {
			DPR_INIT(("xenbus_suspend: resume %p, %s %x.\n",
				pdx->controller, pdx->Nodename, suspend_canceled));
			pdx->ioctl(pdx->controller, ioctl_data);
			break;
		}
	}

	/* Start up all the vnifs. */
	for (entry = fdx->ListOfPDOs.Flink;
		entry != &fdx->ListOfPDOs;
		entry = entry->Flink) {
		pdx = CONTAINING_RECORD(entry, PDO_DEVICE_EXTENSION, Link);
		if (pdx->Type == vnif && pdx->frontend_dev) {
			DPR_INIT(("xenbus_suspend: resume %p, %s %x.\n",
				pdx->frontend_dev, pdx->Nodename, suspend_canceled));
			pdx->ioctl(pdx->frontend_dev, ioctl_data);
		}
	}
}

static void
xenbus_shutdown(PFDO_DEVICE_EXTENSION fdx, uint32_t reason)
{
	XEN_LOCK_HANDLE lh;
	xenbus_register_shutdown_event_t *ioctl;
	PIRP irp;
	PLIST_ENTRY ent;
	HANDLE registryKey;
	UNICODE_STRING valueName;
	NTSTATUS status;
	NTSTATUS open_key_status;
    uint32_t resultLength;
    uint32_t notify;
	uint32_t shutdown;
    UCHAR buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(uint32_t)];

	DPR_INIT(("==> xenbus_shutdown: irql %x\n", KeGetCurrentIrql()));

	/* Check the registry to see if anyone expects to be notified of a */
	/* shutdown via the registry. */
	notify = XENBUS_UNDEFINED_SHUTDOWN_NOTIFICATION;
	open_key_status = xenbus_open_device_key(&registryKey);
	if (NT_SUCCESS(open_key_status)) {
		RtlInitUnicodeString(&valueName, XENBUS_SHUTDOWN_NOTIFICATION_WSTR);

        status = ZwQueryValueKey(registryKey,
			&valueName,
			KeyValuePartialInformation,
			buffer,
			sizeof(buffer),
			&resultLength);

        if (NT_SUCCESS(status)) {
            notify = *((PULONG)
				&(((PKEY_VALUE_PARTIAL_INFORMATION) buffer)->Data));
        }
	}

	if (IsListEmpty(&fdx->shutdown_requests)
		&& (notify == XENBUS_UNDEFINED_SHUTDOWN_NOTIFICATION
			|| notify == XENBUS_NO_SHUTDOWN_NOTIFICATION)) {
		/* No one is listening for this shutdown.  Do the best we can.*/
		DPR_INIT(("    xenbus_shutdown calling xenbus_suspend\n"));

		if (NT_SUCCESS(open_key_status)) {
			ZwClose(registryKey);
		}
		xenbus_suspend(fdx, reason);
		return;
	}

#ifdef XENBUS_HAS_IOCTLS
	XenAcquireSpinLock(&fdx->qlock, &lh);
	while (!IsListEmpty(&fdx->shutdown_requests)) {
		ent = RemoveHeadList(&fdx->shutdown_requests);
		ioctl = CONTAINING_RECORD(ent, xenbus_register_shutdown_event_t, list);
		irp = ioctl->irp;
		if (irp != NULL) {
			if (IoSetCancelRoutine(irp, NULL) != NULL) {
				ioctl->shutdown_type = reason;
				DPR_INIT(("    xenbus_shutdown in shutdown_type = %x\n",
					ioctl->shutdown_type));
				irp->Tail.Overlay.DriverContext[3] = NULL;
				irp->IoStatus.Status = STATUS_SUCCESS;
				irp->IoStatus.Information =
					sizeof(xenbus_register_shutdown_event_t);
#ifdef TARGET_OS_Win2K
				if (reason == SHUTDOWN_poweroff) {
					/* Win2k wont power off after receiving a xm shutdown. */
					/* This lets the power function tell Xen to destroy */
					/* the VM for us. */
					fdx->power_state = PowerSystemMaximum;
				}
#endif
				XenReleaseSpinLock(&fdx->qlock, lh);
				IoCompleteRequest(irp, IO_NO_INCREMENT );
				XenAcquireSpinLock(&fdx->qlock, &lh);
			} else {
				//
				// Cancel routine will run as soon as we release the lock.
				// So let it complete the request and free the record.
				//
				DPR_INIT(("    xenbus_shutdown: IoSetCancelRoutine failed\n"));
				InitializeListHead(&ioctl->list);
			}
		}
	}
	XenReleaseSpinLock(&fdx->qlock, lh);
#endif

	if (notify == XENBUS_WANTS_SHUTDOWN_NOTIFICATION) {
#ifdef TARGET_OS_Win2K
		if (reason == SHUTDOWN_poweroff) {
			/* Win2k wont power off after receiving a xm shutdown. */
			/* This lets the power function tell Xen to destroy */
			/* the VM for us. */
			fdx->power_state = PowerSystemMaximum;
		}
#endif
		reason++;
		DPR_INIT(("    xenbus_shutdown: writing reg shutdown reason %x\n",
			reason));
		xenbus_shutdown_setup(&reason, NULL);
	}
	if (NT_SUCCESS(open_key_status)) {
		ZwClose(registryKey);
	}

	DPR_INIT(("<== xenbus_shutdown\n"));
	return;
}

static void
vbd_handler(struct xenbus_watch *watch,
	const char **vec, unsigned int len)
{
	PFDO_DEVICE_EXTENSION fdx = (PFDO_DEVICE_EXTENSION)watch->context;
	char *node;
	char *alloc_node;
    PLIST_ENTRY entry;
    PPDO_DEVICE_EXTENSION pdx;
	pv_ioctl_t ioctl_data;
	uint32_t i;

	node = (char *)vec[0];
	DPR_INIT(("vbd_handler called: %s.\n", node));

	if (gfdo == NULL) {
		DBG_PRINT(("vbd_handler: gfdo is NULL\n"));
		return;
	}

	len = strlen(node);
	for (i = 11; i < len; i++) {
		if (node[i] == '/') {
			node[i] = '\0';
			if (!xenbus_find_pdx_from_nodename(fdx, node)) {
				DPR_INIT(("    %s is new.\n", node));
				if (strcmp(&node[i + 1], "backend") == 0) {
					if (alloc_node = ExAllocatePoolWithTag(NonPagedPool,
							strlen(node) + 1, XENBUS_POOL_TAG)) {
						RtlStringCbCopyA(alloc_node, strlen(node) + 1, node);
						DPR_INIT(("    and %s is ready.\n", alloc_node));
						if (XenbusInitializePDO(gfdo, "vbd", alloc_node) ==
								STATUS_SUCCESS) {
							DPR_INIT(("    We want to call xenblk now\n"));
							if (fdx->disk_controller && fdx->disk_ioctl) {
								ioctl_data.cmd = PV_ATTACH;
								ioctl_data.arg = 0;
								fdx->disk_ioctl(fdx->disk_controller,
									ioctl_data);
								break;
							}
						}
					}
				}
			}
			break;
		}
	}
}

static void
shutdown_handler(struct xenbus_watch *watch,
	const char **vec, unsigned int len)
{
	PFDO_DEVICE_EXTENSION fdx = (PFDO_DEVICE_EXTENSION)watch->context;
	xenbus_pv_port_options_t options;
	char *str;
	NTSTATUS status;
	struct xenbus_transaction xbt;
	int err;

	if (shutting_down != SHUTDOWN_INVALID) {
		DPR_INIT(("shutdown_handler called while shutting_down = %x.\n",
			shutting_down));
		return;
	}

 again:
	err = xenbus_transaction_start(&xbt);
	if (err) {
		DPR_INIT(("shutdown_handler xenbus_transaction_start failed %x\n",err));
		return;
	}

	str = (char *)xenbus_read(xbt, "control", "shutdown", NULL);
	/* Ignore read errors and empty reads. */
	if (IS_ERR(str) || str == NULL){
		xenbus_transaction_end(xbt, 1);
		DPR_INIT(("shutdown_handler %p: empty or NULL irql = %d, cpu = %d.\n",
			shutdown_handler, KeGetCurrentIrql(),
			KeGetCurrentProcessorNumber()));
		return;
	}

	if (str[0] != '\0') {
		/* By writing to the control, the watch will fire again. */
		/* Don't write the null string if it is already NULL. */
		xenbus_write(xbt, "control", "shutdown", "");
	}

	err = xenbus_transaction_end(xbt, 0);
	if (err == -EAGAIN) {
		xenbus_free_string(str);
		goto again;
	}

	DPR_INIT(("shutdown_handler: shutdown request is: %s\n", str));
	if (strcmp(str, "poweroff") == 0) {
		shutting_down = SHUTDOWN_poweroff;
		xenbus_shutdown(fdx, shutting_down);
	}
	else if (strcmp(str, "halt") == 0) {
		shutting_down = SHUTDOWN_poweroff;
		xenbus_shutdown(fdx, shutting_down);
	}
	else if (strcmp(str, "reboot") == 0) {
		shutting_down = SHUTDOWN_reboot;
		xenbus_shutdown(fdx, shutting_down);
	}
	else if (strcmp(str, "suspend") == 0) {
		shutting_down = SHUTDOWN_suspend;
		xenbus_suspend(fdx, shutting_down);
	}
	else {
		DPR_INIT(("shutdown_handler: Ignoring shutdown request: %s\n", str));
		shutting_down = SHUTDOWN_INVALID;
	}

	xenbus_free_string(str);

	/* Reset so we can do another flavor of shutdiown. */
	shutting_down = SHUTDOWN_INVALID;
}

NTSTATUS xs_finish_init(PDEVICE_OBJECT fdo, uint32_t reason)
{
    PFDO_DEVICE_EXTENSION fdx;
	NTSTATUS status;

	if (reason == OP_MODE_CRASHDUMP) {
		return STATUS_SUCCESS;
	}

	if (reason == OP_MODE_NORMAL) {
		fdx = (PFDO_DEVICE_EXTENSION) fdo->DeviceExtension;
		shutdown_watch.callback = shutdown_handler;
		shutdown_watch.node = "control/shutdown";
		shutdown_watch.flags = XBWF_new_thread;
		shutdown_watch.context = fdx;
		register_xenbus_watch(&shutdown_watch);

		vbd_watch.callback = vbd_handler;
		vbd_watch.node = "device/vbd";
		vbd_watch.flags = XBWF_new_thread;
		vbd_watch.context = fdx;
		register_xenbus_watch(&vbd_watch);
	}

	status = register_dpc_to_evtchn(xen_store_evtchn,
		XenbusDpcRoutine, fdo, NULL);
	if (!NT_SUCCESS(status)) {
		DBG_PRINT(("XENBUS: request Dpc failed.\n"));
	}
	return status;
}

NTSTATUS xs_init(uint32_t reason)
{
	PHYSICAL_ADDRESS store_addr;
	unsigned long xen_store_mfn;

	if (reason == OP_MODE_CRASHDUMP) {
		xen_store_interface = NULL;
		return STATUS_SUCCESS;
	}

	DPR_INIT(("\txen_store_evtchn = %x\n", xen_store_evtchn));

	/* evtchn_init will unregister_dpc_from_evtchn for xen_store_evtchn*/
	DPR_INIT(("\txen_store_evtchn hvm_get_parameter evtchn\n"));
	xen_store_evtchn = (int)hvm_get_parameter(HVM_PARAM_STORE_EVTCHN);

	DPR_INIT(("\txen_store_evtchn hvm_get_parameter store pfn\n"));
	xen_store_mfn = (unsigned long)hvm_get_parameter(HVM_PARAM_STORE_PFN);
	store_addr.QuadPart = (UINT64) xen_store_mfn << PAGE_SHIFT;

	DPR_INIT(("\txen_store_evtchn = %x, xen_store_mfn = %lx\n",
		xen_store_evtchn, xen_store_mfn));

	if (xen_store_interface)
		MmUnmapIoSpace(xen_store_interface, PAGE_SIZE);
	xen_store_interface = MmMapIoSpace(store_addr, PAGE_SIZE, MmNonCached);
	if (xen_store_interface == NULL) {
		DBG_PRINT(("XENBUS: failed to map xenstore page, pfn: %x!\n",
			xen_store_mfn));
		return STATUS_NO_MEMORY;
	}

	DPR_INIT(("XENBUS: mapped xenstore page, pfn: %x, va: %x.\n",
		xen_store_mfn, xen_store_interface));

	InitializeListHead(&watches);
	InitializeListHead(&watch_events);
	InitializeListHead(&xs_state.reply_list);

	/* Reinitializing a lock may cause a deadlock. */
	if (xs_state.reply_lock == 0xbad) {
		ExReinitializeResourceLite(&xs_state.suspend_mutex);
		KeInitializeMutex(&xs_state.request_mutex, 0);
		KeInitializeSpinLock(&xs_state.reply_lock);
		KeInitializeSpinLock(&watches_lock);
		KeInitializeSpinLock(&watch_events_lock);
		KeInitializeSpinLock(&xs_lock);
		KeInitializeSpinLock(&xenbus_dpc_lock);

		KeInitializeEvent(&xs_state.reply_list_notempty, NotificationEvent,	FALSE);
		KeInitializeEvent(&thread_xenwatch_kill, NotificationEvent, FALSE);
		KeInitializeEvent(&thread_xenbus_kill, NotificationEvent, FALSE);
		KeInitializeEvent(&watch_events_notempty, NotificationEvent, FALSE);
		KeInitializeEvent(&xb_event, NotificationEvent, FALSE);
	}

	KeClearEvent(&xs_state.reply_list_notempty);
	KeClearEvent(&thread_xenwatch_kill);
	KeClearEvent(&thread_xenbus_kill);
	KeClearEvent(&watch_events_notempty);
	KeClearEvent(&xb_event);

	DPRINT(("XENBUS: xs_state.suspend_mutex at address: %x\n",
		&xs_state.suspend_mutex));

	xenbus_watch_work_scheduled = 0;

	DPR_INIT(("xs_init: OUT\n"));
	return STATUS_SUCCESS;
}


VOID xs_cleanup(void)
{
	unregister_xenbus_watch(&vbd_watch);
	vbd_watch.node = NULL;

	unregister_xenbus_watch(&shutdown_watch);
	shutdown_watch.node = NULL;

	if (xen_store_evtchn)
		unregister_dpc_from_evtchn(xen_store_evtchn);
#ifdef XENBUS_HAS_DEVICE
	KeSetEvent(&xb_event, 0, FALSE);
#endif
}

void
xenbus_debug_dump(void)
{
	shared_info_t *s;
	vcpu_info_t *v;
    struct xenstore_domain_interface *intf;

	s = shared_info_area;
	if (!s) {
		DBG_PRINT(("\n*** xenbus state dump: shard_info_area not setup.\n"));
		return;
	}
	v = &s->vcpu_info[0];
	if (!v) {
		DBG_PRINT(("\n*** xenbus state dump: vcpu_info not setup.\n"));
		return;
	}
	v->evtchn_upcall_pending = 0;
    intf = xen_store_interface;

	DBG_PRINT(("\n*** xenbus state dump: irql %d, cpu %d\n",
		KeGetCurrentIrql(), KeGetCurrentProcessorNumber()));
	DBG_PRINT(("\tshared_info: evtchn_mask %x, evtchn_pending %x\n",
		s->evtchn_mask[0], s->evtchn_pending[0]));
	DBG_PRINT(("\tvcpu: evtchn_pending_sel %x, evtchn_upcall_mask %x, evtchn_upcall_pending %x\n",
		v->evtchn_pending_sel,v->evtchn_upcall_mask,v->evtchn_upcall_pending));
	if (intf) {
		DBG_PRINT(("\tinterface: req_cons %x, req_prod %x, rsp_cons %x, rsp_prod %x\n",
			intf->req_cons, intf->req_prod,	intf->rsp_cons, intf->rsp_prod));
	}
	DBG_PRINT(("\tIsListEmpty: watch_events %x, xs_state %x\n",
		IsListEmpty(&watch_events), IsListEmpty(&xs_state.reply_list)));
#ifdef DBG
	DBG_PRINT(("\twe %x ws %x wl %x ***\n",
		xenbus_wait_events, xenbus_watch_work_scheduled, xenbus_locks));
	if (s->evtchn_pending[0] || v->evtchn_pending_sel) {
		DBG_PRINT(("\tSetting evt_print to 1\n"));
		evt_print = 1;
	}
#endif
}

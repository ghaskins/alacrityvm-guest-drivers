/*
 * Copyright 2009 Novell.  All Rights Reserved.
 *
 * IOQ is a generic shared-memory, lockless queue mechanism. It can be used
 * in a variety of ways, though its intended purpose is to become the
 * asynchronous communication path for virtual-bus drivers.
 *
 * The following are a list of key design points:
 *
 * #) All shared-memory is always allocated on explicitly one side of the
 *    link.  This typically would be the guest side in a VM/VMM scenario.
 * #) Each IOQ has the concept of "north" and "south" locales, where
 *    north denotes the memory-owner side (e.g. guest).
 * #) An IOQ is manipulated using an iterator idiom.
 * #) Provides a bi-directional signaling/notification infrastructure on
 *    a per-queue basis, which includes an event mitigation strategy
 *    to reduce boundary switching.
 * #) The signaling path is abstracted so that various technologies and
 *    topologies can define their own specific implementation while sharing
 *    the basic structures and code.
 *
 * Author:
 *      Gregory Haskins <ghaskins@novell.com>
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

#ifndef _LINUX_IOQ_H
#define _LINUX_IOQ_H

#include "shm-signal.h"

/*
 *---------
 * The following structures represent data that is shared across boundaries
 * which may be quite disparate from one another (e.g. Windows vs Linux,
 * 32 vs 64 bit, etc).  Therefore, care has been taken to make sure they
 * present data in a manner that is independent of the environment.
 *-----------
 */
struct ioq_ring_desc {
	UINT64                 cookie; /* for arbitrary use by north-side */
	UINT64                 ptr;
	UINT64                 len;
	UINT8                  valid;
	UINT8                  sown; /* South owned = 1, North owned = 0 */
};

#define IOQ_RING_MAGIC 0x47fa2fe4
#define IOQ_RING_VER   4

struct ioq_ring_idx {
	UINT32                 head;    /* 0 based index to head of ptr array */
	UINT32                 tail;    /* 0 based index to tail of ptr array */
	UINT8                  full;
};

enum ioq_locality {
	ioq_locality_north,
	ioq_locality_south,
};

struct ioq_ring_head {
	UINT32                  magic;
	UINT32                  ver;
	struct shm_signal_desc signal;
	struct ioq_ring_idx    idx[2];
	UINT32                  count;
	struct ioq_ring_desc   ring[1]; /* "count" elements will be allocated */
};

#define IOQ_HEAD_DESC_SIZE(count) \
    (sizeof(struct ioq_ring_head) + sizeof(struct ioq_ring_desc) * (count - 1))


enum ioq_idx_type {
	ioq_idxtype_valid,
	ioq_idxtype_inuse,
	ioq_idxtype_both,
	ioq_idxtype_invalid,
};

enum ioq_seek_type {
	ioq_seek_tail,
	ioq_seek_next,
	ioq_seek_head,
	ioq_seek_set
};

struct ioq_iterator {
	struct ioq            *ioq;
	struct ioq_ring_idx   *idx;
	UINT32                 pos;
	struct ioq_ring_desc  *desc;
	int                    update:1;
	int                    dualidx:1;
	int                    flipowner:1;
};

struct ioq_notifier {
	void (*signal)(struct ioq_notifier *);
};

struct ioq_ops {
	void     (*release)(struct ioq *ioq);
};

struct ioq {
	struct ioq_ops *ops;
	LONG			ref;
	enum ioq_locality      locale;
	struct ioq_ring_head  *head_desc;
	struct ioq_ring_desc  *ring;
	struct shm_signal     *signal;
	struct ioq_notifier   *notifier;
	size_t                 count;
	struct shm_signal_notifier shm_notifier;
};

#define IOQ_ITER_AUTOUPDATE  (1 << 0)
#define IOQ_ITER_NOFLIPOWNER (1 << 1)

/**
 * ioq_init() - initialize an IOQ
 * @ioq:        IOQ context
 *
 * Initializes IOQ context before first use
 *
 **/
void ioq_init(struct ioq *ioq,
	      struct ioq_ops *ops,
	      enum ioq_locality locale,
	      struct ioq_ring_head *head,
	      struct shm_signal *signal,
	      size_t count);

/**
 * ioq_get() - acquire an IOQ context reference
 * @ioq:        IOQ context
 *
 **/
static __inline struct ioq *ioq_get(struct ioq *ioq)
{
	InterlockedIncrement(&ioq->ref);
	return ioq;
}

/**
 * ioq_put() - release an IOQ context reference
 * @ioq:        IOQ context
 *
 **/
static __inline void ioq_put(struct ioq *ioq)
{
	if (!InterlockedDecrement(&ioq->ref)) 
		ioq->ops->release(ioq);
}

/**
 * ioq_notify_enable() - enables local notifications on an IOQ
 * @ioq:        IOQ context
 * @flags:      Reserved for future use, must be 0
 *
 * Enables/unmasks the registered ioq_notifier (if applicable) and waitq to
 * receive wakeups whenever the remote side performs an ioq_signal() operation.
 * A notification will be dispatched immediately if any pending signals have
 * already been issued prior to invoking this call.
 *
 * This is synonymous with unmasking an interrupt.
 *
 * Returns: success = 0, <0 = ERRNO
 *
 **/
static __inline int ioq_notify_enable(struct ioq *ioq, int flags)
{
	ShmSignalEnable(ioq->signal, 0);
	return 0;
}

/**
 * ioq_notify_disable() - disable local notifications on an IOQ
 * @ioq:        IOQ context
 * @flags:      Reserved for future use, must be 0
 *
 * Disables/masks the registered ioq_notifier (if applicable) and waitq
 * from receiving any further notifications.  Any subsequent calls to
 * ioq_signal() by the remote side will update the ring as dirty, but
 * will not traverse the locale boundary and will not invoke the notifier
 * callback or wakeup the waitq.  Signals delivered while masked will
 * be deferred until ioq_notify_enable() is invoked
 *
 * This is synonymous with masking an interrupt
 *
 * Returns: success = 0, <0 = ERRNO
 *
 **/
static __inline int ioq_notify_disable(struct ioq *ioq, int flags)
{
	ShmSignalDisable(ioq->signal, 0);
	return  0;
}

/**
 * ioq_signal() - notify the remote side about ring changes
 * @ioq:        IOQ context
 * @flags:      Reserved for future use, must be 0
 *
 * Marks the ring state as "dirty" and, if enabled, will traverse
 * a locale boundary to invoke a remote notification.  The remote
 * side controls whether the notification should be delivered via
 * the ioq_notify_enable/disable() interface.
 *
 * The specifics of how to traverse a locale boundary are abstracted
 * by the ioq_ops->signal() interface and provided by a particular
 * implementation.  However, typically going north to south would be
 * something like a syscall/hypercall, and going south to north would be
 * something like a posix-signal/guest-interrupt.
 *
 * Returns: success = 0, <0 = ERRNO
 *
 **/
static __inline int ioq_signal(struct ioq *ioq, int flags)
{
	return ShmSignalInject(ioq->signal, 0);
}

/**
 * ioq_count() - counts the number of outstanding descriptors in an index
 * @ioq:        IOQ context
 * @type:	Specifies the index type
 *                 (*) valid: the descriptor is valid.  This is usually
 *                     used to keep track of descriptors that may not
 *                     be carrying a useful payload, but still need to
 *                     be tracked carefully.
 *                 (*) inuse: Descriptors that carry useful payload
 *
 * Returns:
 *  (*) >=0: # of descriptors outstanding in the index
 *  (*) <0 = ERRNO
 *
 **/
int ioq_count(struct ioq *ioq, enum ioq_idx_type type);

/**
 * ioq_remain() - counts the number of remaining descriptors in an index
 * @ioq:        IOQ context
 * @type:	Specifies the index type
 *                 (*) valid: the descriptor is valid.  This is usually
 *                     used to keep track of descriptors that may not
 *                     be carrying a useful payload, but still need to
 *                     be tracked carefully.
 *                 (*) inuse: Descriptors that carry useful payload
 *
 * This is the converse of ioq_count().  This function returns the number
 * of "free" descriptors left in a particular index
 *
 * Returns:
 *  (*) >=0: # of descriptors remaining in the index
 *  (*) <0 = ERRNO
 *
 **/
int ioq_remain(struct ioq *ioq, enum ioq_idx_type type);

/**
 * ioq_size() - counts the maximum number of descriptors in an ring
 * @ioq:        IOQ context
 *
 * This function returns the maximum number of descriptors supported in
 * a ring, regardless of their current state (free or inuse).
 *
 * Returns:
 *  (*) >=0: total # of descriptors in the ring
 *  (*) <0 = ERRNO
 *
 **/
int ioq_size(struct ioq *ioq);

/**
 * ioq_full() - determines if a specific index is "full"
 * @ioq:        IOQ context
 * @type:	Specifies the index type
 *                 (*) valid: the descriptor is valid.  This is usually
 *                     used to keep track of descriptors that may not
 *                     be carrying a useful payload, but still need to
 *                     be tracked carefully.
 *                 (*) inuse: Descriptors that carry useful payload
 *
 * Returns:
 *  (*) 0: index is not full
 *  (*) 1: index is full
 *  (*) <0 = ERRNO
 *
 **/
int ioq_full(struct ioq *ioq, enum ioq_idx_type type);

/**
 * ioq_empty() - determines if a specific index is "empty"
 * @ioq:        IOQ context
 * @type:	Specifies the index type
 *                 (*) valid: the descriptor is valid.  This is usually
 *                     used to keep track of descriptors that may not
 *                     be carrying a useful payload, but still need to
 *                     be tracked carefully.
 *                 (*) inuse: Descriptors that carry useful payload
 *
 * Returns:
 *  (*) 0: index is not empty
 *  (*) 1: index is empty
 *  (*) <0 = ERRNO
 *
 **/
static __inline int ioq_empty(struct ioq *ioq, enum ioq_idx_type type)
{
    return !ioq_count(ioq, type);
}

/**
 * ioq_iter_init() - initialize an iterator for IOQ descriptor traversal
 * @ioq:        IOQ context to iterate on
 * @iter:	Iterator context to init (usually from stack)
 * @type:	Specifies the index type to iterate against
 *                 (*) valid: iterate against the "valid" index
 *                 (*) inuse: iterate against the "inuse" index
 *                 (*) both: iterate against both indexes simultaneously
 * @flags:      Bitfield with 0 or more bits set to alter behavior
 *                 (*) autoupdate: automatically signal the remote side
 *                     whenever the iterator pushes/pops to a new desc
 *                 (*) noflipowner: do not flip the ownership bit during
 *                     a push/pop operation
 *
 * Returns: success = 0, <0 = ERRNO
 *
 **/
int ioq_iter_init(struct ioq *ioq, struct ioq_iterator *iter,
		  enum ioq_idx_type type, int flags);

/**
 * ioq_iter_seek() - seek to a specific location in the IOQ ring
 * @iter:	Iterator context (must be initialized with ioq_iter_init)
 * @type:	Specifies the type of seek operation
 *                 (*) tail: seek to the absolute tail, offset is ignored
 *                 (*) next: seek to the relative next, offset is ignored
 *                 (*) head: seek to the absolute head, offset is ignored
 *                 (*) set: seek to the absolute offset
 * @offset:     Offset for ioq_seek_set operations
 * @flags:      Reserved for future use, must be 0
 *
 * Returns: success = 0, <0 = ERRNO
 *
 **/
int  ioq_iter_seek(struct ioq_iterator *iter, enum ioq_seek_type type,
		   long offset, int flags);

/**
 * ioq_iter_push() - push the tail pointer forward
 * @iter:	Iterator context (must be initialized with ioq_iter_init)
 * @flags:      Reserved for future use, must be 0
 *
 * This function will simultaneously advance the tail ptr in the current
 * index (valid/inuse, as specified in the ioq_iter_init) as well as
 * perform a seek(next) operation.  This effectively "pushes" a new pointer
 * onto the tail of the index.
 *
 * Returns: success = 0, <0 = ERRNO
 *
 **/
int  ioq_iter_push(struct ioq_iterator *iter, int flags);

/**
 * ioq_iter_pop() - pop the head pointer from the ring
 * @iter:	Iterator context (must be initialized with ioq_iter_init)
 * @flags:      Reserved for future use, must be 0
 *
 * This function will simultaneously advance the head ptr in the current
 * index (valid/inuse, as specified in the ioq_iter_init) as well as
 * perform a seek(next) operation.  This effectively "pops" a pointer
 * from the head of the index.
 *
 * Returns: success = 0, <0 = ERRNO
 *
 **/
int  ioq_iter_pop(struct ioq_iterator *iter,  int flags);

#endif /* _LINUX_IOQ_H */

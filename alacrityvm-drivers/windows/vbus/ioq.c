/*
 * Copyright 2009 Novell.  All Rights Reserved.
 *
 * See include/linux/ioq.h for documentation
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

#include "vbus.h"
#include "precomp.h"


static int ioq_iter_setpos(struct ioq_iterator *iter, UINT32 pos)
{
	struct ioq *ioq = iter->ioq;

	iter->pos  = pos;
	iter->desc = &ioq->ring[pos];

	return 0;
}

static UINT32 modulo_inc(UINT32 val, UINT32 mod)
{
	if (val == (mod - 1))
		return 0;

	return val + 1;
}

static int idx_full(struct ioq_ring_idx *idx)
{
	return idx->full && (idx->head == idx->tail);
}

int ioq_iter_seek(struct ioq_iterator *iter, enum ioq_seek_type type,
		  long offset, int flags)
{
	struct ioq_ring_idx *idx = iter->idx;
	UINT32 pos;

	switch (type) {
	case ioq_seek_next:
		pos = modulo_inc(iter->pos, iter->ioq->count);
		break;
	case ioq_seek_tail:
		pos = idx->tail;
		break;
	case ioq_seek_head:
		pos = idx->head;
		break;
	case ioq_seek_set:
		if (offset >= (long) iter->ioq->count)
			return -1;
		pos = offset;
		break;
	default:
		return -1;
	}

	return ioq_iter_setpos(iter, pos);
}

static int ioq_ring_count(struct ioq_ring_idx *idx, int count)
{
	if (idx->full && (idx->head == idx->tail))
		return count;
	else if (idx->tail >= idx->head)
		return idx->tail - idx->head;
	else
		return (idx->tail + count) - idx->head;
}

static void idx_tail_push(struct ioq_ring_idx *idx, int count)
{
	UINT32 tail = modulo_inc(idx->tail, count);

	if (idx->head == tail) {
		KeMemoryBarrier();
		/*
		 * Setting full here may look racy, but note that we havent
		 * flipped the owner bit yet.  So it is impossible for the
		 * remote locale to move head in such a way that this operation
		 * becomes invalid
		 */
		idx->full = 1;
		KeMemoryBarrier();
	}

	idx->tail = tail;
}

int ioq_iter_push(struct ioq_iterator *iter, int flags)
{
	struct ioq_ring_head *head_desc = iter->ioq->head_desc;
	struct ioq_ring_idx  *idx  = iter->idx;
	int ret;

	/*
	 * Its only valid to push if we are currently pointed at the tail
	 */
	if (iter->pos != idx->tail || iter->desc->sown != iter->ioq->locale)
		return -1;

	idx_tail_push(idx, iter->ioq->count);
	if (iter->dualidx) {
		idx_tail_push(&head_desc->idx[ioq_idxtype_inuse],
			      iter->ioq->count);
		if (head_desc->idx[ioq_idxtype_inuse].tail !=
		    head_desc->idx[ioq_idxtype_valid].tail) 
			return -1;
	}

	/* the index must be visible before the sown, or signal */
	KeMemoryBarrier();

	if (iter->flipowner) {
		iter->desc->sown = !iter->ioq->locale;
		KeMemoryBarrier(); /* sown must be visible before we signal */
	}

	ret = ioq_iter_seek(iter, ioq_seek_next, 0, flags);

	if (iter->update)
		ioq_signal(iter->ioq, 0);

	return ret;
}

int ioq_iter_pop(struct ioq_iterator *iter,  int flags)
{
	struct ioq_ring_idx *idx = iter->idx;
	int ret;

	/*
	 * Its only valid to pop if we are currently pointed at the head
	 */
	if (iter->pos != idx->head || iter->desc->sown != iter->ioq->locale)
		return -1;

	idx->head = modulo_inc(idx->head, iter->ioq->count);
	KeMemoryBarrier(); /* head must be visible before full */

	if (idx->full) {
		idx->full = 0;
		KeMemoryBarrier(); /* full must be visible before sown */
	}

	if (iter->flipowner) {
		iter->desc->sown = !iter->ioq->locale;
		KeMemoryBarrier(); /* sown must be visible before we signal */
	}

	ret = ioq_iter_seek(iter, ioq_seek_next, 0, flags);

	if (iter->update)
		ioq_signal(iter->ioq, 0);

	return ret;
}

static struct ioq_ring_idx *idxtype_to_idx(struct ioq *ioq,
					   enum ioq_idx_type type)
{
	struct ioq_ring_idx *idx;

	switch (type) {
	case ioq_idxtype_valid:
	case ioq_idxtype_inuse:
		idx = &ioq->head_desc->idx[type];
		break;
	default:
		break;
	}

	return idx;
}

int ioq_iter_init(struct ioq *ioq, struct ioq_iterator *iter,
		  enum ioq_idx_type type, int flags)
{
	iter->ioq        = ioq;
	iter->update     = (flags & IOQ_ITER_AUTOUPDATE);
	iter->flipowner  = !(flags & IOQ_ITER_NOFLIPOWNER);
	iter->pos        = -1;
	iter->desc       = NULL;
	iter->dualidx    = 0;

	if (type == ioq_idxtype_both) {
		/*
		 * "both" is a special case, so we set the dualidx flag.
		 *
		 * However, we also just want to use the valid-index
		 * for normal processing, so override that here
		 */
		type = ioq_idxtype_valid;
		iter->dualidx = 1;
	}

	iter->idx = idxtype_to_idx(ioq, type);

	return 0;
}

int ioq_count(struct ioq *ioq, enum ioq_idx_type type)
{
	return ioq_ring_count(idxtype_to_idx(ioq, type), ioq->count);
}

int ioq_remain(struct ioq *ioq, enum ioq_idx_type type)
{
	int count = ioq_ring_count(idxtype_to_idx(ioq, type), ioq->count);

	return ioq->count - count;
}

int ioq_size(struct ioq *ioq)
{
	return ioq->count;
}

int ioq_full(struct ioq *ioq, enum ioq_idx_type type)
{
	struct ioq_ring_idx *idx = idxtype_to_idx(ioq, type);

	return idx_full(idx);
}

static void ioq_shm_signal(struct shm_signal_notifier *notifier)
{
	struct ioq *ioq = CONTAINING_RECORD(notifier, struct ioq, shm_notifier);

	/* XXX Deal with any pending requests here */

	if (ioq->notifier)
		ioq->notifier->signal(ioq->notifier);
}

void ioq_init(struct ioq *ioq,
	      struct ioq_ops *ops,
	      enum ioq_locality locale,
	      struct ioq_ring_head *head,
	      struct shm_signal *signal,
	      size_t count)
{
	memset(ioq, 0, sizeof(*ioq));

	ioq->ops         = ops;
	ioq->locale      = locale;
	ioq->head_desc   = head;
	ioq->ring        = &head->ring[0];
	ioq->count       = count;
	ioq->signal      = signal;

	ioq->shm_notifier.signal = &ioq_shm_signal;
	signal->notifier         = &ioq->shm_notifier;
}

/*
 * Copyright 2009 Novell.  All Rights Reserved.
 *
 * Virtual-Ethernet adapter
 *
 * Author:
 *      Gregory Haskins <ghaskins@novell.com>
 *	Peter W. Morreale <pmorreale@novell.com> (port to Windows)
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

#ifndef _WINDOWS_VENET_H
#define _WINDOWS_VENET_H

#define VENET_VERSION 1

#define VENET_TYPE "virtual-ethernet"

#define VENET_QUEUE_RX 0
#define VENET_QUEUE_TX 1

struct venet_capabilities {
	UINT32 gid;
	UINT32 bits;
};

#define VENET_CAP_GROUP_SG     0
#define VENET_CAP_GROUP_EVENTQ 1
#define VENET_CAP_GROUP_L4RO    2 /* layer-4 reassem offloading */

/* CAPABILITIES-GROUP SG */
#define VENET_CAP_SG     (1 << 0)
#define VENET_CAP_TSO4   (1 << 1)
#define VENET_CAP_TSO6   (1 << 2)
#define VENET_CAP_ECN    (1 << 3)
#define VENET_CAP_UFO    (1 << 4)
#define VENET_CAP_PMTD   (1 << 5) /* pre-mapped tx desc */

/* CAPABILITIES-GROUP EVENTQ */
#define VENET_CAP_EVQ_LINKSTATE  (1 << 0)
#define VENET_CAP_EVQ_TXC        (1 << 1) /* tx-complete */

struct venet_iov {
	UINT32 len;
	UINT64 ptr;
};

#define VENET_SG_FLAG_NEEDS_CSUM (1 << 0)
#define VENET_SG_FLAG_GSO        (1 << 1)
#define VENET_SG_FLAG_ECN        (1 << 2)

struct venet_sg {
	UINT64            cookie;
	UINT32            flags;
	UINT32            len;     /* total length of all iovs */
	struct {
		UINT16    start;	  /* csum starting position */
		UINT16    offset;  /* offset to place csum */
	} csum;
	struct {
#define VENET_GSO_TYPE_TCPV4	0	/* IPv4 TCP (TSO) */
#define VENET_GSO_TYPE_UDP	1	/* IPv4 UDP (UFO) */
#define VENET_GSO_TYPE_TCPV6	2	/* IPv6 TCP */
		UINT8     type;
		UINT16    hdrlen;
		UINT16    size;
	} gso;
	UINT32            count;   /* nr of iovs */
	struct venet_iov iov[1];
};

struct venet_eventq_query {
	UINT32 flags;
	UINT32 evsize;  /* size of each event */
	UINT32 dpid;    /* descriptor pool-id */
	UINT32 qid;
	UINT8  pad[16];
};

#define VENET_EVENT_LINKSTATE 0
#define VENET_EVENT_TXC       1

struct venet_event_header {
	UINT32 flags;
	UINT32 size;
	UINT32 id;
};

struct venet_event_linkstate {
	struct venet_event_header header;
	UINT8                      state; /* 0 = down, 1 = up */
};

struct venet_event_txc {
	struct venet_event_header header;
	UINT32                     txqid;
	UINT64                     cookie;
};

struct venet_l4ro_query {
	UINT32 flags;
	UINT32 dpid;    /* descriptor pool-id */
	UINT32 pqid;    /* page queue-id */
	UINT8  pad[20];
};


#define VSG_DESC_SIZE(count) (sizeof(struct venet_sg) + \
			      sizeof(struct venet_iov) * ((count) - 1))

#define VENET_FUNC_LINKUP    0
#define VENET_FUNC_LINKDOWN  1
#define VENET_FUNC_MACQUERY  2
#define VENET_FUNC_NEGCAP    3 /* negotiate capabilities */
#define VENET_FUNC_FLUSHRX   4
#define VENET_FUNC_PMTDQUERY 5
#define VENET_FUNC_EVQQUERY  6
#define VENET_FUNC_L4ROQUERY  7

#endif /* _WINDOWS_VENET_H */

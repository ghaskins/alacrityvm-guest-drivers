
/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  oid.c:  Implement OID handling
 *
 * Author:
 *     Peter W. Morreale <pmorreale@novell.com>
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

NDIS_OID VenetSupportedOids[] =
{
	OID_GEN_CURRENT_PACKET_FILTER,
	OID_GEN_CURRENT_LOOKAHEAD,
	OID_GEN_INTERRUPT_MODERATION,
	OID_GEN_MAXIMUM_TOTAL_SIZE,
	OID_GEN_RCV_OK,
	OID_GEN_RECEIVE_BLOCK_SIZE,
	OID_GEN_RECEIVE_BUFFER_SPACE,
	OID_GEN_STATISTICS,
	OID_GEN_TRANSMIT_BLOCK_SIZE,
	OID_GEN_TRANSMIT_BUFFER_SPACE,
	OID_GEN_VENDOR_DESCRIPTION,
	OID_GEN_VENDOR_DRIVER_VERSION,
	OID_GEN_INTERRUPT_MODERATION,
	OID_GEN_VENDOR_ID,
	OID_GEN_SUPPORTED_GUIDS,
	OID_GEN_XMIT_OK,
	OID_GEN_STATISTICS,
	
	OID_802_3_PERMANENT_ADDRESS,
	OID_802_3_CURRENT_ADDRESS,
	OID_802_3_MULTICAST_LIST,
	OID_802_3_MAXIMUM_LIST_SIZE,
	OID_802_3_RCV_ERROR_ALIGNMENT,
	OID_802_3_XMIT_ONE_COLLISION,
	OID_802_3_XMIT_MORE_COLLISIONS,
	OID_802_3_XMIT_DEFERRED,
	OID_802_3_XMIT_MAX_COLLISIONS,
	OID_802_3_RCV_OVERRUN,
	OID_802_3_XMIT_UNDERRUN,
	OID_802_3_XMIT_HEARTBEAT_FAILURE,
	OID_802_3_XMIT_TIMES_CRS_LOST,
	OID_802_3_XMIT_LATE_COLLISIONS,
};
	
static NDIS_STATUS
venet_gen_stats(PVOID buf, ULONG len, PULONG written, PULONG needed)
{
	NDIS_STATUS 		rc = NDIS_STATUS_BUFFER_OVERFLOW;
	PNDIS_STATISTICS_INFO	s;

	 /* XXX : Add support to collect statistics */

	*written = 0;
	*needed = sizeof(NDIS_STATISTICS_INFO);

	if (len >= sizeof(NDIS_STATISTICS_INFO)) {
		s = (PNDIS_STATISTICS_INFO)buf;
		NdisZeroMemory(s, sizeof(NDIS_STATISTICS_INFO));
		s->Header.Revision = NDIS_OBJECT_REVISION_1;
		s->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
		s->Header.Size = sizeof(NDIS_STATISTICS_INFO);
		*needed = 0;
		*written = sizeof(NDIS_STATISTICS_INFO);
		rc = NDIS_STATUS_SUCCESS;
	}

	return rc;
}

static NDIS_STATUS 
set_mcast_list(PADAPTER a, __in_bcount(len) PVOID b, 
		ULONG len, PULONG nr_read, PULONG nr_needed)
{
	*nr_needed = ETH_LENGTH_OF_ADDRESS;
	*nr_read = len;

	if (len % ETH_LENGTH_OF_ADDRESS) 
		return NDIS_STATUS_INVALID_LENGTH;

	if (len > (NIC_MAX_MCAST_LIST * ETH_LENGTH_OF_ADDRESS)) {
		*nr_needed = NIC_MAX_MCAST_LIST * ETH_LENGTH_OF_ADDRESS;
		return NDIS_STATUS_MULTICAST_FULL;
	}

	NdisZeroMemory(a->mc_array, NIC_MAX_MCAST_LIST * ETH_LENGTH_OF_ADDRESS);
	NdisMoveMemory(a->mc_array, b, len);

	a->mc_size = len / ETH_LENGTH_OF_ADDRESS;
	
	return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS 
set_pkt_filter(PADAPTER a, ULONG pf)
{
	NDIS_STATUS	  rc = NDIS_STATUS_SUCCESS;
	
	if(pf & ~NIC_SUPPORTED_FILTERS)
		return NDIS_STATUS_NOT_SUPPORTED;
	
	if(pf != a->packetFilter) {   
		/* FIXME: Change the filtering modes on hardware */
		a->packetFilter = pf;
	}
	return(rc);
}
static NDIS_STATUS 
venet_intr_mod(PVOID b, ULONG len, PULONG nr_read, PULONG nr_needed)
{
	PNDIS_INTERRUPT_MODERATION_PARAMETERS	p =
		(PNDIS_INTERRUPT_MODERATION_PARAMETERS) b;

	if (len < sizeof(NDIS_INTERRUPT_MODERATION_PARAMETERS)) {
		*nr_needed = sizeof(*p);
		return NDIS_STATUS_INVALID_LENGTH;
	}

	NdisZeroMemory(p, sizeof(*p));
	p->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	p->Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
	p->Header.Size = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
	p->Flags = 0; 
	p->InterruptModeration = NdisInterruptModerationNotSupported;
	*nr_read = sizeof(*p);

	return NDIS_STATUS_SUCCESS;
}

/* handler for an OID set operation. */
static NDIS_STATUS 
venet_oid_set(NDIS_HANDLE handle, NDIS_OID oid, PVOID  b,
		ULONG len, PULONG count, PULONG needed)
{
	NDIS_STATUS	rc = NDIS_STATUS_INVALID_LENGTH;
	PADAPTER	a = (PADAPTER) handle;

	*count = 0;
	*needed = sizeof(ULONG);

	switch(oid) {
		case OID_802_3_MULTICAST_LIST:
			rc = set_mcast_list(a, b, len, count, needed);
			break;

		case OID_GEN_CURRENT_PACKET_FILTER:
			if(len == sizeof(ULONG)) {
				*count = len;
				rc = set_pkt_filter(a, *((PULONG)b));
			}
			break;

		case OID_GEN_CURRENT_LOOKAHEAD:
			if(len == sizeof(ULONG)) {
				*count = sizeof(ULONG);
				a->lookAhead = *(PULONG)b;				
			}				
			break;

		case OID_GEN_INTERRUPT_MODERATION:
			rc = NDIS_STATUS_INVALID_DATA;
			break;

		default:
			rc = NDIS_STATUS_INVALID_OID;
			break;
	}
	
	return(rc);
}

static NDIS_STATUS
venet_oid_query(NDIS_HANDLE handle, NDIS_OID oid, PVOID buf, ULONG len,
		PULONG written, PULONG needed)
{
	NDIS_STATUS	rc = NDIS_STATUS_SUCCESS;
	ULONG		size = sizeof(ULONG);
	PADAPTER	a = (PADAPTER) handle;
	UINT64		tmp64;
	ULONG		tmp;
	PVOID		p = &tmp;

	*written = 0;
	*needed = 0;

	switch(oid) {
		case OID_GEN_STATISTICS:
			rc = venet_gen_stats(buf, len, written, needed);
			goto done;
		
		case OID_GEN_CURRENT_LOOKAHEAD:
			p = &a->lookAhead;
			break;			

		case OID_GEN_MAXIMUM_TOTAL_SIZE:
		case OID_GEN_TRANSMIT_BLOCK_SIZE:
		case OID_GEN_RECEIVE_BLOCK_SIZE:
			tmp = (ULONG) ETH_MAX_PACKET_SIZE;
			break;
			
		case OID_GEN_INTERRUPT_MODERATION:
			rc = venet_intr_mod(buf, len, written, needed);
			goto done;

		case OID_GEN_TRANSMIT_BUFFER_SPACE:
			tmp = ETH_MAX_PACKET_SIZE * a->maxBusySends;
			break;

		case OID_GEN_RECEIVE_BUFFER_SPACE:
			tmp = ETH_MAX_PACKET_SIZE * a->maxBusyRecvs;
			break;

		case OID_GEN_VENDOR_ID:
			tmp = VENET_VENDOR_ID;
			break;

		case OID_GEN_VENDOR_DESCRIPTION:
			p = VENET_VENDOR_DESC;
			size = strlen(VENET_VENDOR_DESC);
			break;
			
		case OID_GEN_VENDOR_DRIVER_VERSION:
			tmp = VENET_VENDOR_DRIVER_VERSION;
			break;

		case OID_GEN_CURRENT_PACKET_FILTER:
			tmp = a->packetFilter;
			break;
					   
		case OID_802_3_PERMANENT_ADDRESS:
			p = a->permAddress;
			size = ETH_LENGTH_OF_ADDRESS;
			break;

		case OID_802_3_CURRENT_ADDRESS:
			p = a->currAddress;
			size = ETH_LENGTH_OF_ADDRESS;
			break;

		case OID_802_3_MAXIMUM_LIST_SIZE:
			tmp = NIC_MAX_MCAST_LIST;
			break;
			
		case OID_GEN_XMIT_OK:
			tmp64 = a->txCount;
			p = &tmp64;
			size = sizeof(ULONG64);
			break;
	
		case OID_GEN_RCV_OK:
			tmp64 = a->rxCount;
			p = &tmp64;
			size = sizeof(ULONG64);
			break;
	
		case OID_802_3_RCV_ERROR_ALIGNMENT:
			tmp = 0;
			break;
	
		case OID_802_3_XMIT_ONE_COLLISION:
			tmp = 0;
			break;
	
		case OID_802_3_XMIT_MORE_COLLISIONS:
			tmp = 0;
			break;
	
		case OID_802_3_XMIT_DEFERRED:
			tmp = 0;
			break;
	
		case OID_802_3_XMIT_MAX_COLLISIONS:
			tmp = 0;
			break;
	
		case OID_802_3_RCV_OVERRUN:
			tmp = 0;
			break;
	
		case OID_802_3_XMIT_UNDERRUN:
			tmp = 0;
			break;
	
		case OID_802_3_XMIT_HEARTBEAT_FAILURE:
			tmp = 0;
			break;
	
		case OID_802_3_XMIT_TIMES_CRS_LOST:
			tmp = 0;
			break;
	
		case OID_802_3_XMIT_LATE_COLLISIONS:
			tmp = 0;
			break;
		  
		default:
			rc = NDIS_STATUS_NOT_SUPPORTED;
			break;			
	}

	if(rc == NDIS_STATUS_SUCCESS) {
		if(size <= len) {
			*written = size;
			NdisMoveMemory(buf, p, size);
		}
		else {
			*needed = size;
			rc = NDIS_STATUS_INVALID_LENGTH;
		}
	}

done:

	return rc;
}

NDIS_STATUS 
VenetOidRequest(NDIS_HANDLE  adapter, PNDIS_OID_REQUEST	r)
{
	NDIS_STATUS	rc;

	switch(r->RequestType) {
		case NdisRequestQueryInformation:
		case NdisRequestQueryStatistics:
			rc = venet_oid_query(adapter,
			    r->DATA.QUERY_INFORMATION.Oid,
			    r->DATA.QUERY_INFORMATION.InformationBuffer,
			    r->DATA.QUERY_INFORMATION.InformationBufferLength,
			    (PULONG)&r->DATA.QUERY_INFORMATION.BytesWritten,
			    (PULONG)&r->DATA.QUERY_INFORMATION.BytesNeeded);
	    	break;

		case NdisRequestSetInformation:
			rc = venet_oid_set(adapter,
			    r->DATA.SET_INFORMATION.Oid,
			    r->DATA.SET_INFORMATION.InformationBuffer,
			    r->DATA.SET_INFORMATION.InformationBufferLength,
			    (PULONG)&r->DATA.SET_INFORMATION.BytesRead,
			    (PULONG)&r->DATA.SET_INFORMATION.BytesNeeded);
	    	break;

		default:
	    		rc = NDIS_STATUS_NOT_SUPPORTED;
	   	 break;
    }

	return rc;
}

/*****************************************************************************
 *
 *   File Name:      offload.h
 *
 *   %version:       3 %
 *   %derived_by:    %
 *   %date_modified: %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2006-2007 Unpublished Work of Novell, Inc. All Rights Reserved.
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

#ifndef _OFFLOAD_H
#define _OFFLOAD_H

#define TCP_FLAG_FIN  0x00000100
#define TCP_FLAG_SYN  0x00000200
#define TCP_FLAG_RST  0x00000400
#define TCP_FLAG_PUSH 0x00000800
#define TCP_FLAG_ACK  0x00001000
#define TCP_FLAG_URG  0x00002000

#define TCP_MAX_OPTION_SIZE 40
#define IP_MAX_OPTION_SIZE 40

#define PROTOCOL_TCP 6
#define PROTOCOL_UDP 17

typedef struct _TCP_HEADER
{
    UINT16 tcph_srcport;
    UINT16 tcph_destport;
    UINT32 tcph_seq;
    UINT32 tcph_ack;
    UINT16 tcph_flags;  /* flags and data offset (i.e. length) */
    UINT16 tcph_window;
    UINT16 tcph_sum;
    UINT16 tcph_urgent;
} TCP_HEADER, *PTCP_HEADER;

typedef struct _IP_HEADER
{
    UINT8  iph_verlen;
    UINT8  iph_tos;
    UINT16 iph_length;
    UINT16 iph_id;
    UINT16 iph_offset;  /* flags and fragment offset */
    UINT8  iph_ttl;
    UINT8  iph_protocol;
    UINT16 iph_sum;
    UINT32 iph_src;
    UINT32 iph_dest;
} IP_HEADER, *PIP_HEADER;

#define TCP_IP_MAX_HEADER_SIZE                 \
    TCP_MAX_OPTION_SIZE + IP_MAX_OPTION_SIZE + \
    sizeof(TCP_HEADER) + sizeof(IP_HEADER)


static __inline USHORT
LE_TO_NE16(
  ULONG NaturalData
  )
{
    USHORT ShortData = (USHORT) NaturalData;

    return (ShortData << 8) | (ShortData >> 8);
}

static __inline ULONG
LE_TO_NE32(
  ULONG NaturalData
  )
{
    ULONG ByteSwapped;

    ByteSwapped = ((NaturalData & 0x00ff00ff) << 8) |
        ((NaturalData & 0xff00ff00) >> 8);

    return (ByteSwapped << 16) | (ByteSwapped >> 16);
}


#define XSUM(_TmpXsum, _Base, _Length, _Offset)                         \
    {                                                                   \
        PUSHORT WordPtr = (PUSHORT) ((PUCHAR) (_Base) + (_Offset));     \
        ULONG   WordCount = (_Length) >> 1;                             \
        BOOLEAN OddLen = (BOOLEAN) ((_Length) & 1);                     \
        while (WordCount--) {                                           \
            _TmpXsum += *WordPtr;                                       \
            WordPtr++;                                                  \
        }                                                               \
        if (OddLen) {                                                   \
            _TmpXsum += (USHORT) *((PUCHAR) WordPtr);                   \
        }                                                               \
        _TmpXsum = (((_TmpXsum >> 16) | (_TmpXsum << 16)) + _TmpXsum) >> 16; \
    }

/*
 * TCP pseudo header checksum, be careful about endians,
 * src and dest are in NE, others in LE, and the result is
 * in NE.
 */
#define PHXSUM(src, dest, ptcl, len)           \
    (((UINT32) (src)) & 0xffff) +              \
    (((UINT32) (src)) >> 16) +                 \
    (((UINT32) (dest)) & 0xffff) +             \
    (((UINT32) (dest)) >> 16) +                \
    (((UINT32) (ptcl) << 8) & 0xffff) +        \
    (LE_TO_NE16 ((UINT16) (len)))


#define IP_HEADER_LENGTH(IpHdr)                 \
    ((ULONG) ((IpHdr->iph_verlen & 0x0f) << 2))

#define IP_HEADER_VERSION(IpHdr)                \
    ((ULONG) ((IpHdr->iph_verlen & 0xf0) >> 4))

#define TCP_HEADER_LENGTH(TcpHdr)               \
    ((USHORT) (((*((PUCHAR) (&(TcpHdr->tcph_flags))) & 0xf0) >> 4) << 2))


#endif
/*****************************************************************************
 *
 *   File Name:      win_compat.h
 *
 *   %version:       8 %
 *   %derived_by:    kallan %
 *   %date_modified: Fri Feb 08 11:47:49 2008 %
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

#ifndef _WINCOMPAT_H
#define _WINCOMPAT_H

#if defined(_X86_)
#define __i386__
#elif defined(_AMD64_)
#define __x86_64__
#elif defined(_IA64_)
#define __ia64__
#endif

#ifdef __BUSDRV__
#define DLLEXPORT
#else
#define DLLEXPORT __declspec(dllimport)
#endif

#define __attribute__(x)

typedef signed char         INT8;
typedef signed short        INT16;
typedef signed int          INT32;
typedef signed __int64      INT64;
typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned __int64    UINT64;

typedef uintptr_t 			xen_ulong_t;


typedef INT8    int8_t;
typedef UINT8   uint8_t;
typedef INT16   int16_t;
typedef UINT16  uint16_t;
typedef INT32   int32_t;
typedef UINT32  uint32_t;
typedef INT64   int64_t;
typedef UINT64  uint64_t;


typedef INT8    s8;
typedef UINT8   u8;
typedef INT16   s16;
typedef UINT16  u16;
typedef INT32   s32;
typedef UINT32  u32;
typedef INT64   s64;
typedef UINT64  u64;

struct kvec {
    void *iov_base; /* and that should *never* hold a userland pointer */
    size_t iov_len;
};

struct cpuid_args {
	uint32_t eax, ebx, ecx, edx;
};

#define IS_ERR_VALUE(x) ((x) > (ULONG_PTR)-1000L)

#ifdef TARGET_OS_Win2K		/* ------- Windows 2000 ---------*/
#define XEN_LOCK_HANDLE	KIRQL

#define XenAcquireSpinLock(_lock, _lh)	\
	KeAcquireSpinLock((_lock), (_lh))

#define XenReleaseSpinLock(_lock, _lh)	\
	KeReleaseSpinLock((_lock), (_lh))

static __inline void
xenbus_memory_barrier(void)
{
    LONG mem_barrier;
    __asm xchg mem_barrier, eax
}

#define mb  xenbus_memory_barrier
#define wmb xenbus_memory_barrier
#define rmb xenbus_memory_barrier

#define SRB_FUNCTION_RESET_LOGICAL_UNIT		0x20

#else					/* ------- Windows XP, 2003, LH, 7 ---------*/
#define XEN_LOCK_HANDLE	KLOCK_QUEUE_HANDLE

#define XenAcquireSpinLock(_lock, _lh)	\
    KeAcquireInStackQueuedSpinLock((_lock), (_lh))

#define XenReleaseSpinLock(_lock, _lh)	\
    KeReleaseInStackQueuedSpinLock(&(_lh))

#define mb()  KeMemoryBarrier()
#define wmb() KeMemoryBarrier()
#define rmb() KeMemoryBarrier()
#endif

/*
 * __WIN_RING_SIZE should be the same as __RING_SIZE in ring.h.  However,
 * Windows will not compile correctly without the '&' on the ring.
 */

#define __WIN_RING_SIZE(_s, _sz) 										\
    (__RD32(((_sz) - (ULONG_PTR)&(_s)->ring + (ULONG_PTR)(_s))			\
		/ sizeof((_s)->ring[0])))

#define WIN_FRONT_RING_INIT(_r, _s, __size) do {                        \
	ULONG_PTR ulp;														\
																		\
    (_r)->req_prod_pvt = 0;                                             \
    (_r)->rsp_cons = 0;                                                 \
    ulp = __WIN_RING_SIZE(_s, __size);									\
    (_r)->nr_ents = (unsigned int)ulp;									\
    (_r)->sring = (_s);                                                 \
} while (0)


static __inline void *ERR_PTR(long error)
{
    return ((void *)((ULONG_PTR)error));
}

static __inline LONG_PTR PTR_ERR(const void *ptr)
{
	return (LONG_PTR)ptr;
}

static __inline LONG_PTR IS_ERR(const void *ptr)
{
    return IS_ERR_VALUE((ULONG_PTR)ptr);
}

#ifdef ARCH_x86_64
void _cpuid64(struct cpuid_args *id);
void _XenbusKeepUsSane(void);
#define XenbusKeepUsSane _XenbusKeepUsSane
SHORT _InterlockedExchange16(IN OUT SHORT volatile *Target, IN SHORT Value);

struct xen_add_to_physmap_compat
{
    uint16_t domid;
    unsigned int space;
    uintptr_t idx;		/* Need to handle the 32/64 bit issue with longs. */
    uintptr_t gpfn;		/* Need to handle the 32/64 bit issue with longs. */
};

#define BitTestCompat BitTest64
#define InterlockedBitTestAndResetCompat InterlockedBitTestAndReset64
#define InterlockedBitTestAndSetCompat InterlockedBitTestAndSet64
#define InterlockedExchangeCompat InterlockedExchange64
#define XbBitScanForwardCompat XbBitScanForward64

#else

#define xen_add_to_physmap_compat xen_add_to_physmap
#define BitTestCompat BitTest
#define InterlockedBitTestAndResetCompat InterlockedBitTestAndReset
#define InterlockedBitTestAndSetCompat InterlockedBitTestAndSet
#define InterlockedExchangeCompat InterlockedExchange
#define XbBitScanForwardCompat XbBitScanForward
#define XenbusKeepUsSane()	\
{							\
	__asm xor eax, eax		\
    __asm cpuid				\
}
//    __asm rdtsc				\

#endif

/*
#ifndef TARGET_OS_WinLH
static __inline SHORT
InterlockedCompareExchange16(
  IN OUT SHORT volatile *Destination,
  IN SHORT Exchange,
  IN SHORT Comparand
  )
{
    __asm {
        mov ax, Comparand;
        mov ebx, Destination;
        mov cx, Exchange;
        lock cmpxchg [ebx], cx;
    }
}
#endif
*/

/*
static __inline void
XenbusKeepUsSane(void)
{
#ifdef ARCH_x86
    __asm {
        xor eax, eax;
        cpuid
        rdtsc
    }
#else
	_XenbusKeepUsSane();
#endif
}
*/

static __inline SHORT
InterlockedExchange16(
  IN OUT SHORT volatile *Target,
  IN SHORT Value
  )
{
#ifdef ARCH_x86
    __asm {
        mov ax, Value;
        mov ebx, Target;
        lock xchg [ebx], ax;
    }
#else
	return _InterlockedExchange16(Target, Value);
#endif
}
/*

static __inline BOOLEAN
InterlockedTestBit(
  IN OUT VOID volatile *Base,
  IN LONG Ofst
  )
{
    __asm {
        mov eax, Ofst;
        mov ebx, Base;
        bt [ebx], eax;
    }
}

static __inline VOID
InterlockedSetBit(
  IN OUT VOID volatile *Base,
  IN LONG Ofst
  )
{
    __asm {
        mov eax, Ofst;
        mov ebx, Base;
        lock bts [ebx], eax;
    }
}

static __inline VOID
InterlockedClearBit(
  IN OUT VOID volatile *Base,
  IN LONG Ofst
  )
{
    __asm {
        mov eax, Ofst;
        mov ebx, Base;
        lock btr [ebx], eax;
           mov eax, Bit
           mov ecx, Base
           lock btr [ecx], eax
           setc al
    }
}

static __inline VOID
InterlockedChangeBit(
  IN OUT VOID volatile *Base,
  IN LONG Ofst
  )
{
    __asm {
        mov eax, Ofst;
        mov ebx, Base;
        lock btc [ebx], eax;
    }
}

static __inline BOOLEAN
InterlockedTestAndSetBit(
  IN OUT VOID volatile *Base,
  IN LONG Ofst
  )
{
    __asm {
        mov eax, Ofst;
        mov ebx, Base;
        lock bts [ebx], eax;
        sbb eax, eax;
    }
}

static __inline BOOLEAN
InterlockedTestAndClearBit(
  IN OUT VOID volatile *Base,
  IN LONG Ofst
  )
{
    __asm {
        mov eax, Ofst;
        mov ebx, Base;
        lock btr [ebx], eax;
        sbb eax, eax;
    }
}
static __inline BOOLEAN
InterlockedTestAndChangeBit(
  IN OUT VOID volatile *Base,
  IN LONG Ofst
  )
{
    __asm {
        mov eax, Ofst;
        mov ebx, Base;
        lock btc [ebx], eax;
        sbb eax, eax;
    }
}
*/

static __inline LONG
XbBitScanForward(
  IN LONG volatile *Base
  )
{
	LONG index;

	_BitScanForward(&index, *Base);
	return index;
	/*
    __asm {
        mov ebx, Base;
        bsf eax, [ebx];
    }
	*/
}

#ifdef ARCH_x86_64
static __inline LONG
XbBitScanForward64(
  IN INT64 volatile *Base
  )
{
	LONG index;

	_BitScanForward64(&index, *Base);
	return index;
}
#endif

/* only support 10 and 16 based */
static __inline xen_ulong_t
cmp_strtoul(
  const char *ptr, char **endptr, int radix
  )
{
    unsigned char ch;
    unsigned int i;
    xen_ulong_t res = 0;
    const char *p;
    int valid = 1;

    if (ptr == NULL)
        return 0;

    for (p=ptr; *p!='\0' && valid; p++) {
        ch = *p;
        switch (ch) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            i = ch - '0';
            break;

        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            if (radix == 16)
                i = ch - 'A' + 10;
            else {
                valid = 0;
                p--;
            }
            break;

        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            if (radix == 16)
                i = ch - 'a' + 10;
            else {
                valid = 0;
                p--;
            }
            break;

        default:
            valid = 0;
            p--;
            break;
        }
        if (valid)
            res = res*radix + i;
    }
    if (endptr)
        *endptr = (char*) p;
    return res;
}

static __inline void
GetCPUID(UINT32 Leaf, UINT32 *resA, UINT32 *resB, UINT32 *resC, UINT32 *resD)
{
	struct cpuid_args cpu_args;

	cpu_args.eax = Leaf;
#ifdef ARCH_x86
    __asm {
        mov eax, cpu_args.eax;
        cpuid;
        mov cpu_args.eax, eax;
        mov cpu_args.ebx, ebx;
        mov cpu_args.ecx, ecx;
        mov cpu_args.edx, edx;
    }
#else
	_cpuid64(&cpu_args);
#endif
    *resA = cpu_args.eax;
    *resB = cpu_args.ebx;
    *resC = cpu_args.ecx;
    *resD = cpu_args.edx;
}

static __inline void
WriteMSR(
  UINT32 MSR,
  UINT64 Value
  )
{
	__writemsr(MSR, Value);
}

#endif

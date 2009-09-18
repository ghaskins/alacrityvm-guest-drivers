/*****************************************************************************
 *
 *   File Name:      win_hypercall.h
 *
 *   %version:       6 %
 *   %derived_by:    kallan %
 *   %date_modified: Mon Apr 14 09:36:35 2008 %
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

#ifndef _ASM_HYPERCALL_H
#define _ASM_HYPERCALL_H

DLLEXPORT extern PUCHAR hypercall_page;

#define HYPERCALL_EVENT_CHANNEL_OP	__HYPERVISOR_event_channel_op*32
#define HYPERCALL_SET_GDT			__HYPERVISOR_set_gdt*32
#define HYPERCALL_STACK_SWITCH		__HYPERVISOR_stack_switch*32
#define HYPERCALL_SCHED_OP			__HYPERVISOR_sched_op*32
#define HYPERCALL_SET_TIMER			__HYPERVISOR_set_timer_op*32
#define HYPERCALL_SET_DEBUGREG		__HYPERVISOR_set_debugreg*32
#define HYPERCALL_MEMORY_OP			__HYPERVISOR_memory_op*32
#define HYPERCALL_MULTICALL			__HYPERVISOR_multicall*32
#define HYPERCALL_ACM				__HYPERVISOR_acm_op*32
#define HYPERCALL_XEN_VERSION		__HYPERVISOR_xen_version*32
#define HYPERCALL_PHYSDEV			__HYPERVISOR_physdev_op*32
#define HYPERCALL_VM_ASSIST			__HYPERVISOR_vm_assist*32
#define HYPERCALL_NMI				__HYPERVISOR_nmi_op*32
#define HYPERCALL_HVM				__HYPERVISOR_hvm_op*32
#define HYPERCALL_CALLBACK			__HYPERVISOR_callback_op*32
#define HYPERCALL_XENOPROF			__HYPERVISOR_xenoprof_op*32
#define HYPERCALL_SET_TRAP_TABLE	__HYPERVISOR_set_trap_table*32
#define HYPERCALL_MMU_UPDATE		__HYPERVISOR_mmu_update*32
#define HYPERCALL_MMUEXT_OP			__HYPERVISOR_mmuext_op*32
#define HYPERCALL_SET_CALLBACKS		__HYPERVISOR_set_callbacks*32
#define HYPERCALL_FPU_TASKSWITCH	__HYPERVISOR_fpu_taskswitch*32
#define HYPERCALL_GET_DEBUGREG		__HYPERVISOR_get_debugreg*32
#define HYPERCALL_UPDATE_DESCRIPTOR	__HYPERVISOR_update_descriptor*32
#define HYPERCALL_CONSOLE_IO		__HYPERVISOR_console_io*32
#define HYPERCALL_PHYSDEV_OP_COMPAT	__HYPERVISOR_physdev_op_compat*32
#define HYPERVISOR_GRANT_TABLE_OP	__HYPERVISOR_grant_table_op*32
#define HYPERCALL_VCPU_OP			__HYPERVISOR_vcpu_op*32
#define HYPERCALL_SCHED_OP_COMPAT	__HYPERVISOR_sched_op_compat*32
#define HYPERCALL_EVENT_CHANNEL_OP_COMPAT	__HYPERVISOR_event_channel_op_compat*32
/*
 * We add a local variable to circumvent problems brought by inline assembly.
 * when hypercall_page is imported, the code cl generated for ``__asm add eax,
 * [hypercall_page]'' is indeed ``add eax, [_imp__hypercall_page]'', which is
 * very wrong.
 */

#ifdef ARCH_x86
#define _hypercall0(hpg, op) {                                  \
        __asm mov eax, [hpg]                                    \
        __asm add eax, op                                       \
        __asm call eax                                          \
    }

#define _hypercall1(hpg, op, a1) {                              \
        __asm mov ebx, a1                                       \
        __asm mov eax, [hpg]                                    \
        __asm add eax, op                                       \
        __asm call eax                                          \
    }

static __inline int
_hypercall2(PUCHAR hpg, ULONG_PTR op, void *a1, void *a2)
{
	int cc;

	__asm mov ebx, a1
	__asm mov ecx, a2
	__asm mov eax, [hpg]
	__asm add eax, op
	__asm call eax
	__asm mov cc, eax
	return cc;
}

static __inline int
_hypercall3(PUCHAR hpg, ULONG_PTR op, void *a1, void *a2, void *a3)
{
	int cc;

	__asm mov ebx, a1
	__asm mov ecx, a2
	__asm mov edx, a3
	__asm mov eax, [hpg]
    __asm add eax, op
	__asm call eax
	__asm mov cc, eax
	return cc;
}

#define _hypercall4(hpg, op, a1, a2, a3, a4) {                  \
        __asm mov ebx, a1                                       \
        __asm mov ecx, a2                                       \
        __asm mov edx, a3                                       \
        __asm mov esi, a4                                       \
        __asm mov eax, [hpg]                                    \
        __asm add eax, op                                       \
        __asm call eax                                          \
    }

#define _hypercall5(hpg, op, a1, a2, a3, a4, a5) {              \
        __asm mov ebx, a1                                       \
        __asm mov ecx, a2                                       \
        __asm mov edx, a3                                       \
        __asm mov esi, a4                                       \
        __asm mov edi, a5                                       \
        __asm mov eax, [hpg]                                    \
        __asm add eax, op                                       \
        __asm call eax                                          \
    }
#endif

#ifdef ARCH_x86_64
/* Hypercalls declaration */
void _hypercall0(PUCHAR hpg, ULONG_PTR);
void _hypercall1(PUCHAR hpg, ULONG_PTR op, void *a1);
int _hypercall2(PUCHAR hpg, ULONG_PTR op, void *a1, void *a2);
int _hypercall3(PUCHAR hpg, ULONG_PTR op, void *a1, void *a2,
	void *a3);
void _hypercall4(PUCHAR hpg, ULONG_PTR op, void *a1, void *a2,
	void *a3, void *a4);
void _hypercall5(PUCHAR hpg, ULONG_PTR op, void *a1, void *a2,
	void *a3, void *a4, void *a5);
extern uint8_t hypercall_page_mem[];
#endif

static __inline int
HYPERVISOR_set_trap_table(
  trap_info_t *table)
{
	PUCHAR hpg = hypercall_page;

    _hypercall1(hpg, HYPERCALL_SET_TRAP_TABLE, table);
}

static __inline int
HYPERVISOR_mmu_update(
  mmu_update_t *req, int count, int *success_count, domid_t domid)
{
	PUCHAR hpg = hypercall_page;
    void *_count = (void *)((ULONG_PTR)count);
    void *_domid = (void *)((ULONG_PTR)domid);

    _hypercall4(hpg, HYPERCALL_MMU_UPDATE, req, _count, success_count, _domid);
}

static __inline int
HYPERVISOR_mmuext_op(
  struct mmuext_op *op, int count, int *success_count, domid_t domid)
{
	PUCHAR hpg = hypercall_page;
    void *_count = (void *)((ULONG_PTR)count);
    void *_domid = (void *)((ULONG_PTR)domid);

    _hypercall4(hpg, HYPERCALL_MMUEXT_OP, op, _count, success_count, _domid);
}

static __inline int
HYPERVISOR_set_gdt(
  unsigned long *frame_list, int entries)
{
	PUCHAR hpg = hypercall_page;
    void *_entries = (void *)((ULONG_PTR)entries);

    return _hypercall2(hpg, HYPERCALL_SET_GDT, frame_list, _entries);
}

static __inline int
HYPERVISOR_stack_switch(
  unsigned long ss, unsigned long esp)
{
	PUCHAR hpg = hypercall_page;
    void * _ss = (void *)((ULONG_PTR)ss);
    void * _esp = (void *)((ULONG_PTR)esp);

    return _hypercall2(hpg, HYPERCALL_STACK_SWITCH, _ss, _esp);
}

static __inline int
HYPERVISOR_set_callbacks(
  unsigned long event_selector, unsigned long event_address,
  unsigned long failsafe_selector, unsigned long failsafe_address)
{
	PUCHAR hpg = hypercall_page;
	void *a1 = (void *)((ULONG_PTR)event_selector);
	void *a2 = (void *)((ULONG_PTR)event_address);
	void *a3 = (void *)((ULONG_PTR)failsafe_selector);
	void *a4 = (void *)((ULONG_PTR)failsafe_address);

    _hypercall4(hpg, HYPERCALL_SET_CALLBACKS, a1, a2, a3, a4);
}

static __inline int
HYPERVISOR_fpu_taskswitch(
  int set)
{
	PUCHAR hpg = hypercall_page;
	void *_set = (void *)((ULONG_PTR)set);

    _hypercall1(hpg, HYPERCALL_FPU_TASKSWITCH, _set);
}

static __inline int
HYPERVISOR_sched_op_compat(
  int cmd, unsigned long arg)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);
	void *_arg = (void *)((ULONG_PTR)arg);

    return _hypercall2(hpg, HYPERCALL_SCHED_OP_COMPAT, _cmd, _arg);
}

static __inline int
HYPERVISOR_sched_op(
  int cmd, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);

    return _hypercall2(hpg, HYPERCALL_SCHED_OP, _cmd, arg);
}

/*
static __inline long
HYPERVISOR_set_timer_op(
  u64 timeout)
{
	PUCHAR hpg = hypercall_page;
    unsigned long timeout_hi = (unsigned long)(timeout>>32);
    unsigned long timeout_lo = (unsigned long)timeout;

    _hypercall2(hpg, HYPERCALL_SET_TIMER, timeout_lo, timeout_hi);
}

static __inline int
HYPERVISOR_set_debugreg(
  int reg, unsigned long value)
{
	PUCHAR hpg = hypercall_page;

    _hypercall2(hpg, HYPERCALL_SET_DEBUGREG, reg, value);
}

static __inline unsigned long
HYPERVISOR_get_debugreg(
  int reg)
{
	PUCHAR hpg = hypercall_page;

    _hypercall1(hpg, HYPERCALL_GET_DEBUGREG, reg);
}

static __inline int
HYPERVISOR_update_descriptor(
  u64 ma, u64 desc)
{
	PUCHAR hpg = hypercall_page;
    u32 _mal = (u32) ma & 0xFFFFFFFF;
    u32 _mah = (u32) (ma>>32);
    u32 _descl = (u32) desc & 0xFFFFFFFF;
    u32 _desch = (u32) (desc>>32);

    _hypercall4(hpg, HYPERCALL_UPDATE_DESCRIPTOR, _mal, _mah, _descl, _desch);
}
*/

static __inline int
HYPERVISOR_memory_op(
  unsigned int cmd, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);

    return _hypercall2(hpg, HYPERCALL_MEMORY_OP, _cmd, arg);
}

static __inline int
HYPERVISOR_multicall(
  void *call_list, int nr_calls)
{
	PUCHAR hpg = hypercall_page;
	void *_nr_calls = (void *)((ULONG_PTR)nr_calls);

    return _hypercall2(hpg, HYPERCALL_MULTICALL, call_list, _nr_calls);
}

static __inline int
HYPERVISOR_update_va_mapping(
  unsigned long va, unsigned long new_val, unsigned long flags)
{
	PUCHAR hpg = hypercall_page;
    unsigned long pte_hi = 0;
#if 0 /*CONFIG_X86_PAE */
    pte_hi = new_val.pte_high;
#endif
//    _hypercall4(hpg, update_va_mapping, va,
//                new_val.pte_low, pte_hi, flags);
}

static __inline int
HYPERVISOR_event_channel_op(
  int cmd, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);
	int j;

    return _hypercall2(hpg, HYPERCALL_EVENT_CHANNEL_OP, _cmd, arg);
#if 0
    if (unlikely(rc == -ENOSYS)) {
        struct evtchn_op op;
        op.cmd = cmd;
        memcpy(&op.u, arg, sizeof(op.u));
        rc = _hypercall1(hpg, HYPERCALL_EVENT_CHANNEL_OP_COMPAT, &op);
        memcpy(arg, &op.u, sizeof(op.u));
    }
#endif
}

static __inline int
HYPERVISOR_acm_op(
  int cmd, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);

    return _hypercall2(hpg, HYPERCALL_ACM, _cmd, arg);
}

static __inline int
HYPERVISOR_xen_version(
  int cmd, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);

    return _hypercall2(hpg, HYPERCALL_XEN_VERSION, _cmd, arg);
}

static __inline int
HYPERVISOR_console_io(
  int cmd, int count, char *string)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);
	void *_count = (void *)((ULONG_PTR)count);

    return _hypercall3(hpg, HYPERCALL_CONSOLE_IO, _cmd, _count, string);
}

static __inline int
HYPERVISOR_physdev_op(
  int cmd, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);

    return _hypercall2(hpg, HYPERCALL_PHYSDEV, _cmd, arg);
#if 0
    if (unlikely(rc == -ENOSYS)) {
        struct physdev_op op;
        op.cmd = cmd;
        memcpy(&op.u, arg, sizeof(op.u));
        rc = _hypercall1(hpg, HYPERCALL_PHYSDEV_OP_COMPAT, &op);
        memcpy(arg, &op.u, sizeof(op.u));
    }
#endif
}

static __inline int
HYPERVISOR_grant_table_op(
  unsigned int cmd, void *uop, unsigned int count)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);
	void *_count = (void *)((ULONG_PTR)count);

    return _hypercall3(hpg, HYPERVISOR_GRANT_TABLE_OP, _cmd, uop, _count);
}

static __inline int
HYPERVISOR_update_va_mapping_otherdomain(
  unsigned long va, unsigned long new_val, unsigned long flags, domid_t domid)
{
    unsigned long pte_hi = 0;
#if 0 /*CONFIG_X86_PAE*/
    pte_hi = new_val.pte_high;
#endif
//    _hypercall5(hpg, update_va_mapping_otherdomain, va,
//                new_val.pte_low, pte_hi, flags, domid);
}

static __inline int
HYPERVISOR_vm_assist(
  unsigned int cmd, unsigned int optype)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);
	void *_optype = (void *)((ULONG_PTR)optype);

    return _hypercall2(hpg, HYPERCALL_VM_ASSIST, _cmd, _optype);
}

static __inline int
HYPERVISOR_vcpu_op(
  int cmd, int vcpuid, void *extra_args)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);
	void *_vcpuid = (void *)((ULONG_PTR)vcpuid);

    return _hypercall3(hpg, HYPERCALL_VCPU_OP, _cmd, _vcpuid, extra_args);
}

static __inline int
HYPERVISOR_shutdown(
  unsigned long reason)
{
    struct sched_shutdown sched_shutdown;
    struct sched_shutdown *arg = &sched_shutdown;
	PUCHAR hpg = hypercall_page;
	void *_shutdown = (void *)((ULONG_PTR)SCHEDOP_shutdown);

    sched_shutdown.reason = reason;

    return _hypercall2(hpg, HYPERCALL_SCHED_OP, _shutdown, arg);
#if 0
    if (rc == -ENOSYS)
        rc = _hypercall3(hpg, HYPERCALL_SCHED_OP_COMPAT, SCHEDOP_shutdown,
                         SHUTDOWN_suspend, srec);

#endif
}

static __inline int
HYPERVISOR_nmi_op(
  unsigned long op, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_op = (void *)((ULONG_PTR)op);

    return _hypercall2(hpg, HYPERCALL_NMI, _op, arg);
}

static __inline unsigned long
HYPERVISOR_hvm_op(
  int op, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_op = (void *)((ULONG_PTR)op);

    return _hypercall2(hpg, HYPERCALL_HVM, _op, arg);
}

static __inline int
HYPERVISOR_callback_op(
  int cmd, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_cmd = (void *)((ULONG_PTR)cmd);

    return _hypercall2(hpg, HYPERCALL_CALLBACK, _cmd, arg);
}

static __inline int
HYPERVISOR_xenoprof_op(
  int op, void *arg)
{
	PUCHAR hpg = hypercall_page;
	void *_op = (void *)((ULONG_PTR)op);

    return _hypercall2(hpg, HYPERCALL_XENOPROF, _op, arg);
}

#endif

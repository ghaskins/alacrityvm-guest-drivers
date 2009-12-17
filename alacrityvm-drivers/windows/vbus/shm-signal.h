/*
 * Copyright 2009 Novell.  All Rights Reserved.
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

#ifndef _SHM_SIGNAL_H
#define _SHM_SIGNAL_H

/*
 *---------
 * The following structures represent data that is shared across boundaries
 * which may be quite disparate from one another (e.g. Windows vs Linux,
 * 32 vs 64 bit, etc).  Therefore, care has been taken to make sure they
 * present data in a manner that is independent of the environment.
 *-----------
 */

#include <basetsd.h>

#define SHM_SIGNAL_MAGIC 0x58fa39df
#define SHM_SIGNAL_VER   1

struct shm_signal_irq {
  UINT8                  enabled;
  UINT8                  pending;
  UINT8                  dirty;
};

enum shm_signal_locality {
  shm_locality_north,
  shm_locality_south,
};

struct shm_signal_desc {
  UINT32                magic;
  UINT32                ver;
  struct shm_signal_irq irq[2];
};

/* --- END SHARED STRUCTURES --- */

struct shm_signal_notifier {
	void (*signal)(struct shm_signal_notifier *);
};


struct shm_signal;

struct shm_signal_ops {
  int      (*inject)(struct shm_signal *s);
  void     (*release)(struct shm_signal *s);
};

struct shm_signal {
  enum shm_signal_locality    locale;
  struct shm_signal_ops      *ops;
  struct shm_signal_desc     *desc;
  struct shm_signal_notifier *notifier;
};

 /*
  * This function should only be used internally
  */
void _ShmSignalWakeup(struct shm_signal *s);

/**
 * ShmSignalInit() - initialize an SHM_SIGNAL
 * @s:        SHM_SIGNAL context
 *
 * Initializes SHM_SIGNAL context before first use
 *
 **/
void
ShmSignalInit(struct shm_signal *s, enum shm_signal_locality locale,
	      struct shm_signal_ops *ops, struct shm_signal_desc *desc);

/**
 * ShmSignalEnable() - enables local notifications on an SHM_SIGNAL
 * @s:        SHM_SIGNAL context
 * @flags:      Reserved for future use, must be 0
 *
 * Enables/unmasks the registered notifier (if applicable) to receive wakeups
 * whenever the remote side performs an shm_signal() operation. A notification
 * will be dispatched immediately if any pending signals have already been
 * issued prior to invoking this call.
 *
 * This is synonymous with unmasking an interrupt.
 *
 **/
void ShmSignalEnable(struct shm_signal *s, int flags);

/**
 * ShmSignalDisable() - disable local notifications on an SHM_SIGNAL
 * @s:        SHM_SIGNAL context
 * @flags:      Reserved for future use, must be 0
 *
 * Disables/masks the registered shm_signal_notifier (if applicable) from
 * receiving any further notifications.  Any subsequent calls to shm_signal()
 * by the remote side will update the shm as dirty, but will not traverse the
 * locale boundary and will not invoke the notifier callback.  Signals
 * delivered while masked will be deferred until shm_signal_enable() is
 * invoked.
 *
 * This is synonymous with masking an interrupt
 *
 **/
void ShmSignalDisable(struct shm_signal *s, int flags);

/**
 * ShmSignalTestEnabled() - Test whether signal is enabled.
 * @s:        SHM_SIGNAL context
 *
 * Tests the current enabled state of the signal.
 *
 **/
int ShmSignalTestEnabled(struct shm_signal *s);

/**
 * ShmSignalEoi() - denotes the "end of interrupt" condition for a SHM_SIGNAL
 * @s:        SHM_SIGNAL context
 * @flags:      Reserved for future use, must be 0
 *
 * Denotes that processing on the SHM_SIGNAL notification has completed, possibly
 * reinjecting more signals if present.
 *
 * This is synonymous with EOI an interrupt.
 *
 **/
void ShmSignalEoi(struct shm_signal *s, int flags);

/**
 * ShmSignalInject() - notify the remote side about shm changes
 * @s:        SHM_SIGNAL context
 * @flags:      Reserved for future use, must be 0
 *
 * Marks the shm state as "dirty" and, if enabled, will traverse
 * a locale boundary to inject a remote notification.  The remote
 * side controls whether the notification should be delivered via
 * the shm_signal_enable/disable() interface.
 *
 * The specifics of how to traverse a locale boundary are abstracted
 * by the shm_signal_ops->signal() interface and provided by a particular
 * implementation.  However, typically going north to south would be
 * something like a syscall/hypercall, and going south to north would be
 * something like a posix-signal/guest-interrupt.
 *
 **/
NTSTATUS ShmSignalInject(struct shm_signal *s, int flags);

#endif /* _SHM_SIGNAL_H */

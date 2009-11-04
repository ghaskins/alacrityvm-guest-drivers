/*
 * Copyright 2009 Novell.  All Rights Reserved.
 *
 * See include/linux/shm_signal.h for documentation
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

#include <wdm.h>
#include "shm-signal.h"

__declspec(dllexport) void
ShmSignalEnable(struct shm_signal *s, int flags)
{
  struct shm_signal_irq *irq = &s->desc->irq[s->locale];

  irq->enabled = 1;
  KeMemoryBarrier();
}

__declspec(dllexport) void
ShmSignalDisable(struct shm_signal *s, int flags)
{
  struct shm_signal_irq *irq = &s->desc->irq[s->locale];

  irq->enabled = 0;
  KeMemoryBarrier();
}

/*
 * signaling protocol:
 *
 * each side of the shm_signal has an "irq" structure with the following
 * fields:
 *
 *    - enabled: controlled by shm_signal_enable/disable() to mask/unmask
 *               the notification locally
 *    - dirty:   indicates if the shared-memory is dirty or clean.  This
 *               is updated regardless of the enabled/pending state so that
 *               the state is always accurately tracked.
 *    - pending: indicates if a signal is pending to the remote locale.
 *               This allows us to determine if a remote-notification is
 *               already in flight to optimize spurious notifications away.
 */
__declspec(dllexport) NTSTATUS
ShmSignalInject(struct shm_signal *s, int flags)
{
  /* Load the irq structure from the other locale */
  struct shm_signal_irq *irq = &s->desc->irq[!s->locale];

  /*
   * We always mark the remote side as dirty regardless of whether
   * they need to be notified.
   */
  irq->dirty = 1;
  KeMemoryBarrier();   /* dirty must be visible before we test the pending state */
  
  if (irq->enabled && !irq->pending) {
    /*
     * If the remote side has enabled notifications, and we do
     * not see a notification pending, we must inject a new one.
     */
    irq->pending = 1;
    KeMemoryBarrier(); /* make it visible before we do the injection */
    
    s->ops->inject(s);
  }
  
  return STATUS_SUCCESS;
}

__declspec(dllexport) void
_ShmSignalWakeup(struct shm_signal *s)
{
  struct shm_signal_irq *irq = &s->desc->irq[s->locale];
  int dirty;
  
  s->in_wakeup = TRUE;
  
  /*
   * The outer loop protects against race conditions between
   * irq->dirty and irq->pending updates
   */
  while (irq->enabled && (irq->dirty || irq->pending)) {
    
    /*
     * Run until we completely exhaust irq->dirty (it may
     * be re-dirtied by the remote side while we are in the
     * callback).  We let "pending" remain untouched until we have
     * processed them all so that the remote side knows we do not
     * need a new notification (yet).
     */
    do {
      irq->dirty = 0;
      KeMemoryBarrier();
      
      if (s->notifier)
	s->notifier->signal(s->notifier);
      
      dirty = irq->dirty;
      KeMemoryBarrier();
      
    } while (irq->enabled && dirty);
    
    KeMemoryBarrierWithoutFence();
    
    /*
     * We can finally acknowledge the notification by clearing
     * "pending" after all of the dirty memory has been processed
     * Races against this clearing are handled by the outer loop.
     * Subsequent iterations of this loop will execute with
     * pending=0 potentially leading to future spurious
     * notifications, but this is an acceptable tradeoff as this
     * will be rare and harmless.
     */
    irq->pending = 0;
    KeMemoryBarrier();
    
  }
  
  s->in_wakeup = FALSE;
}

__declspec(dllexport) void
ShmSignalInit(struct shm_signal *s, enum shm_signal_locality locale,
	      struct shm_signal_ops *ops, struct shm_signal_desc *desc)
{
  memset(s, 0, sizeof(*s));

  s->locale   = locale;
  s->ops      = ops;
  s->desc     = desc;
}

/*
 * No-op to satisfy the build-system
 */
NTSTATUS 
DriverEntry( 
    __in struct _DRIVER_OBJECT  *DriverObject,
    __in PUNICODE_STRING  RegistryPath 
    )
{
  return STATUS_SUCCESS;
}



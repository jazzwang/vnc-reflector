/* VNC Reflector
 * Copyright (C) 2001 HorizonLive.com, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file LICENSE,
 * included.  HorizonLive provides e-Learning and collaborative synchronous
 * presentation solutions in a totally Web-based environment.  For more
 * information about HorizonLive, please see our website at
 * http://www.horizonlive.com.
 *
 * This software was authored by Constantin Kaplinsky <const@ce.cctpu.edu.ru>
 * and sponsored by HorizonLive.com, Inc.
 *
 * $Id: host_io.h,v 1.8 2001/10/02 09:03:45 const Exp $
 * Asynchronous interaction with VNC host.
 */

#ifndef _REFLIB_HOST_IO_H
#define _REFLIB_HOST_IO_H

#define TYPE_HOST_LISTENING_SLOT   2
#define TYPE_HOST_CONNECTING_SLOT  3
#define TYPE_HOST_ACTIVE_SLOT      4

/* Extension to AIO_SLOT structure to hold state for host connection */
typedef struct _HOST_SLOT {
  AIO_SLOT s;

  CARD32 temp_len;
} HOST_SLOT;

void host_set_fbs_prefix(char *fbs_prefix);
void host_activate(void);
void host_close_hook(void);

void pass_msg_to_host(CARD8 *msg, size_t len);
void pass_cuttext_to_host(CARD8 *text, size_t len);

#endif /* _REFLIB_HOST_IO_H */

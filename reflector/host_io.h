/* VNC Reflector
 * Copyright (C) 2001-2004 HorizonLive.com, Inc.  All rights reserved.
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
 * $Id: host_io.h,v 1.18 2004/08/08 15:23:35 const_k Exp $
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
  CARD16 fb_width;
  CARD16 fb_height;

  unsigned int convert_copyrect  :1;
} HOST_SLOT;

extern void host_activate(void);
extern void host_close_hook(void);

extern void pass_msg_to_host(CARD8 *msg, size_t len);
extern void pass_cuttext_to_host(CARD8 *text, size_t len);

extern void fill_fb_rect(FB_RECT *r, CARD32 color);
extern void fbupdate_rect_done(void);

/* decode_hextile.c */

extern void setread_decode_hextile(FB_RECT *r);

/* decode_tight.c */

extern void setread_decode_tight(FB_RECT *r);
extern void reset_tight_streams(void);

/* decode_cursor.c */

extern void setread_decode_xcursor(FB_RECT *r);
extern void setread_decode_richcursor(FB_RECT *r);
extern void setread_decode_pointerpos(FB_RECT *r);

#endif /* _REFLIB_HOST_IO_H */

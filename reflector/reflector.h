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
 * $Id: reflector.h,v 1.16 2001/10/02 09:44:53 const Exp $
 * Global include file
 */

#ifndef _REF_REFLECTOR_H
#define _REF_REFLECTOR_H

#define VERSION  "1.1.2"

/* FIXME: Too many header files with too many dependencies */

typedef struct _TILE_HINTS {
  CARD8 subenc8;
  CARD8 bg8;
  CARD8 fg8;
  CARD8 datasize8;
} TILE_HINTS;

/* Framebuffer and related metadata */

extern RFB_SCREEN_INFO g_screen_info;
extern CARD32 *g_framebuffer;
extern CARD16 g_fb_width, g_fb_height;
extern TILE_HINTS *g_hints;
extern CARD8 *g_cache8;

#endif /* _REF_REFLECTOR_H */

/* VNC Reflector
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: reflector.h,v 1.13 2001/08/28 18:16:26 const Exp $
 * Global include file
 */

#ifndef _REF_REFLECTOR_H
#define _REF_REFLECTOR_H

#define VERSION  "1.1"

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

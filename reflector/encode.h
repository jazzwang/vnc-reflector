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
 * $Id: encode.h,v 1.9 2001/10/10 06:33:46 const Exp $
 * Encoding screen rectangles.
 */

#ifndef _REFLIB_ENCODE_H
#define _REFLIB_ENCODE_H

typedef struct _PALETTE2 {
  int num_colors;
  CARD32 bg;
  CARD32 fg;
} PALETTE2;

typedef struct _TILE_HINTS {
  CARD8 subenc8;
  CARD8 bg8;
  CARD8 fg8;
  CARD8 datasize8;
} TILE_HINTS;

#define BUF_PUT_PIXEL8(buf, pixel)  *(buf) = (pixel)

#define BUF_PUT_PIXEL16(buf, pixel)             \
{                                               \
  (buf)[0] = ((CARD8 *)&(pixel))[0];            \
  (buf)[1] = ((CARD8 *)&(pixel))[1];            \
}

#define BUF_PUT_PIXEL32(buf, pixel)             \
{                                               \
  (buf)[0] = ((CARD8 *)&(pixel))[0];            \
  (buf)[1] = ((CARD8 *)&(pixel))[1];            \
  (buf)[2] = ((CARD8 *)&(pixel))[2];            \
  (buf)[3] = ((CARD8 *)&(pixel))[3];            \
}

/* encode.c */

extern TILE_HINTS *g_hints;
extern CARD8 *g_cache8;

int allocate_encoders_cache(void);
int sizeof_encoders_cache(void);
void invalidate_encoders_cache(FB_RECT *r);
void free_encoders_cache(void);

int put_rect_header(CARD8 *buf, FB_RECT *r, CARD32 enc);

AIO_BLOCK *rfb_encode_raw_block(CL_SLOT *cl, FB_RECT *r);
AIO_BLOCK *rfb_encode_copyrect_block(CL_SLOT *cl, FB_RECT *r);
AIO_BLOCK *rfb_encode_hextile_block(CL_SLOT *cl, FB_RECT *r);

void get_hextile_caching_stats(long *hits, long *misses);

void analyze_rect8(CARD8 *buf, FB_RECT *r, PALETTE2 *pal);
void analyze_rect16(CARD16 *buf, FB_RECT *r, PALETTE2 *pal);
void analyze_rect32(CARD32 *buf, FB_RECT *r, PALETTE2 *pal);

/* encode-tight8.c */

int rfb_encode_tight8(CL_SLOT *cl, FB_RECT *r);

#endif /* _REFLIB_ENCODE_H */

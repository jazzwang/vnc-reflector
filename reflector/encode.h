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
 * $Id: encode.h,v 1.12 2001/12/02 08:30:07 const Exp $
 * Encoding screen rectangles.
 */

#ifndef _REFLIB_ENCODE_H
#define _REFLIB_ENCODE_H

typedef struct _PALETTE2 {
  int num_colors;
  CARD32 bg;
  CARD32 fg;
} PALETTE2;

/* This structure describes cached data for a properly-aligned 16x16 tile. */
/* NOTE: If hextile_datasize is not 0 then valid_f should be non-zero too, */
/* but if valid_f is not 0, do not expect hextile_datasize to be non-zero. */
typedef struct _TILE_HINTS {
  CARD8 valid_f;                /* At least meta-data available if not 0   */
  CARD8 num_colors;             /* Meta-data: number of colors (1, 2 or 0) */
  CARD8 bg;                     /* Meta-data: background color             */
  CARD8 fg;                     /* Meta-data: foreground color             */
  CARD16 hextile_datasize;      /* Hextile-encoded data available if not 0 */
} TILE_HINTS;

/* Max size of hextile-encoded data per one 16x16 tile */
/* FIXME: Bad name? */
#define HEXTILE_MAX_TILE_DATASIZE  260

/* Macros to place pixel values to a byte-aligned memory location */

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

int allocate_enc_cache(void);
int sizeof_enc_cache(void);
void invalidate_enc_cache(FB_RECT *r);
void free_enc_cache(void);

int put_rect_header(CARD8 *buf, FB_RECT *r);
void get_hextile_caching_stats(long *hits, long *misses);

AIO_BLOCK *rfb_encode_raw_block(CL_SLOT *cl, FB_RECT *r);
AIO_BLOCK *rfb_encode_copyrect_block(CL_SLOT *cl, FB_RECT *r);
AIO_BLOCK *rfb_encode_hextile_block(CL_SLOT *cl, FB_RECT *r);

/* encode-tight8.c */

int rfb_encode_tight8(CL_SLOT *cl, FB_RECT *r);

#endif /* _REFLIB_ENCODE_H */

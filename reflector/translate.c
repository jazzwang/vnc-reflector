/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: translate.c,v 1.1 2001/08/18 10:44:27 const Exp $
 * Pixel format translation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "reflector.h"
#include "rect.h"

static void *gen_trans_table8(RFB_PIXEL_FORMAT *fmt);
static void *gen_trans_table16(RFB_PIXEL_FORMAT *fmt);
static void *gen_trans_table32(RFB_PIXEL_FORMAT *fmt);

void *gen_trans_table(RFB_PIXEL_FORMAT *fmt)
{
  switch(fmt->bits_pixel) {
  case 8:
    return gen_trans_table8(fmt);
  case 16:
    return gen_trans_table16(fmt);
  case 32:
    return gen_trans_table32(fmt);
  }
  return NULL;
}

#define DEFINE_GEN_TRANS_TABLE(bpp)                                           \
                                                                              \
static void *gen_trans_table##bpp(RFB_PIXEL_FORMAT *fmt)                      \
{                                                                             \
  CARD##bpp *table;                                                           \
  int c;                                                                      \
                                                                              \
  /* Allocate space for 3 tables for 8-bit R, G, B components */              \
  table = malloc(256 * 3 * sizeof(CARD##bpp));                                \
  if (table == NULL)                                                          \
    return NULL;                                                              \
                                                                              \
  /* Fill in translation tables */                                            \
  for (c = 0; c < 255; c++) {                                                 \
    table[c] = (CARD##bpp)(c * fmt->r_max + 127) / 255 << fmt->r_shift;       \
    table[256 + c] = (CARD##bpp)(c * fmt->g_max + 127) / 255 << fmt->g_shift; \
    table[512 + c] = (CARD##bpp)(c * fmt->b_max + 127) / 255 << fmt->b_shift; \
  }                                                                           \
                                                                              \
  return table;                                                               \
}

DEFINE_GEN_TRANS_TABLE(8)
DEFINE_GEN_TRANS_TABLE(16)
DEFINE_GEN_TRANS_TABLE(32)

void transfunc_null(CARD32 *dst_buf, FB_RECT *r)
{
  CARD32 *fb_ptr;
  int y;

  fb_ptr = &g_framebuffer[r->y * g_screen_info->width + r->x];

  for (y = 0; y < r->h; y++) {
    memcpy(dst_buf, fb_ptr, r->w * sizeof(CARD32));
    fb_ptr += g_screen_info->width;
    dst_buf += r->w;
  }
}


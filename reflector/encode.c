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
 * $Id: encode.c,v 1.8 2001/10/02 09:03:45 const Exp $
 * Encoding screen rectangles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "rect.h"
#include "translate.h"
#include "client_io.h"
#include "encode.h"

/*
 * Raw encoder
 */

AIO_BLOCK *rfb_encode_raw_block(CL_SLOT *cl, FB_RECT *r)
{
  AIO_BLOCK *block;

  block = malloc(sizeof(AIO_BLOCK) +
                 r->w * r->h * (cl->format.bits_pixel / 8));
  if (block) {
    (*cl->trans_func)(&block->data, r, cl->trans_table);
    block->data_size = r->w * r->h * (cl->format.bits_pixel / 8);
  }

  return block;
}

/*
 * Hextile encoder
 */

static int encode_tile_bgr233(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile8(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile16(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile32(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile_raw8(CARD8 *dst_buf,CL_SLOT *cl,FB_RECT *r);
static int encode_tile_raw16(CARD8 *dst_buf,CL_SLOT *cl,FB_RECT *r);
static int encode_tile_raw32(CARD8 *dst_buf,CL_SLOT *cl,FB_RECT *r);
static void prepare_tile8(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static void prepare_tile16(CARD16 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static void prepare_tile32(CARD32 *dst_buf, CL_SLOT *cl, FB_RECT *r);

static CARD32 bg, fg;
static CARD32 prev_bg;
static int prev_bg_set;
static int num_colors;

AIO_BLOCK *rfb_encode_hextile_block(CL_SLOT *cl, FB_RECT *r)
{
  AIO_BLOCK *block;
  int num_tiles;
  int aligned_f;
  int rx1, ry1;
  FB_RECT tile_r;
  CARD8 *data_ptr;

  /* Calculate number of tiles per this rectangle */
  num_tiles = ((r->w + 15) / 16) * ((r->h + 15) / 16);

  /* Check if tiles are aligned on 16-pixel boundary */
  aligned_f = (r->x & 0x0F) == 0 && (r->y & 0x0F) == 0;

  /* Allocate a memory block of maximum possible size */
  block = malloc(sizeof(AIO_BLOCK) +
                 r->w * r->h * (cl->format.bits_pixel / 8) +
                 num_tiles);
  if (block == NULL)
    return NULL;

  prev_bg_set = 0;
  data_ptr = (CARD8 *)block->data;
  rx1 = r->x + r->w;
  ry1 = r->y + r->h;
  tile_r.h = 16;

  for (tile_r.y = r->y; tile_r.y < ry1; tile_r.y += 16) {
    if (ry1 - tile_r.y < 16)
      tile_r.h = ry1 - tile_r.y;
    tile_r.w = 16;
    for (tile_r.x = r->x; tile_r.x < rx1; tile_r.x += 16) {
      if (rx1 - tile_r.x < 16)
        tile_r.w = rx1 - tile_r.x;
      switch (cl->format.bits_pixel) {
      case 8:
        /* Can we use caching for this tile? */
        if (aligned_f && cl->bgr233_f && tile_r.w == 16 && tile_r.h == 16)
          data_ptr += encode_tile_bgr233(data_ptr, cl, &tile_r);
        else
          data_ptr += encode_tile8(data_ptr, cl, &tile_r);
        break;
      case 16:
        data_ptr += encode_tile16(data_ptr, cl, &tile_r);
        break;
      case 32:
        data_ptr += encode_tile32(data_ptr, cl, &tile_r);
        break;
      }
    }
  }

  block->data_size = data_ptr - (CARD8 *)block->data;
  return realloc(block, sizeof(AIO_BLOCK) + block->data_size);
}

/*
 * Some statistics
 */

static long s_cache_hits = 0, s_cache_misses = 0;

void get_hextile_caching_stats(long *hits, long *misses)
{
  *hits = s_cache_hits;
  *misses = s_cache_misses;
}

/*
 * Encode properly-aligned 16x16 BGR233 tile, using data from cache if
 * available, or saving encoded data in cache otherwise.
 */

static int encode_tile_bgr233(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r)
{
  int tiles_in_row, tile_ord;
  TILE_HINTS *hints;
  CARD8 *dst = dst_buf;
  size_t data_size;
  int dst_bytes;

  tiles_in_row = ((int)g_fb_width + 15) / 16;
  tile_ord = (r->y / 16) * tiles_in_row + (r->x / 16);
  hints = &g_hints[tile_ord];

  if (hints->subenc8 != 0) {    /* Cache hit */
    s_cache_hits++;

    *dst++ = hints->subenc8;
    if (hints->subenc8 & HEXTILE_RAW) {
        prev_bg_set = 0;
        memcpy(dst, &g_cache8[tile_ord * 256], 256);
        dst += 256;
    } else {
      bg = (CARD32)hints->bg8;
      if (prev_bg != bg || !prev_bg_set) {
        *dst++ = hints->bg8;
      } else {
        *dst_buf &= ~HEXTILE_BG_SPECIFIED;
      }
      prev_bg = bg;                                                         \
      prev_bg_set = 1;                                                      \
      if (hints->subenc8 & HEXTILE_FG_SPECIFIED)
        *dst++ = hints->fg8;
      if (hints->subenc8 & HEXTILE_ANY_SUBRECTS) {
        data_size = (size_t)hints->datasize8 + 1;
        memcpy(dst, &g_cache8[tile_ord * 256], data_size);
        dst += data_size;
      }
    }
    dst_bytes = dst - dst_buf;

  } else {                      /* Cache miss */
    s_cache_misses++;

    dst_bytes = encode_tile8(dst_buf, cl, r);

    hints->subenc8 = *dst++;
    if (hints->subenc8 & HEXTILE_RAW) {
      memcpy(&g_cache8[tile_ord * 256], dst, 256);
    } else {
      if (hints->subenc8 & HEXTILE_BG_SPECIFIED) {
        hints->bg8 = *dst++;
      } else {
        hints->bg8 = (CARD8)bg;
        hints->subenc8 |= HEXTILE_BG_SPECIFIED;
      }
      if (hints->subenc8 & HEXTILE_FG_SPECIFIED)
        hints->fg8 = *dst++;
      if (hints->subenc8 & HEXTILE_ANY_SUBRECTS) {
        data_size = (dst_buf + dst_bytes) - dst;
        hints->datasize8 = (CARD8)(data_size - 1);
        memcpy(&g_cache8[tile_ord * 256], dst, data_size);
      }
    }

  }

  return dst_bytes;
}

/*
 * A function to encode a tile (of size 16x16 or less) using Hextile
 * encoding. The dst_buf argument should point to an array of size at
 * least (256 * (cl->format.bits_pixel/8) + 1) bytes. Returns number
 * of bytes put into the dst_buf.
 */

#define DEFINE_ENCODE_TILE(bpp)                                         \
                                                                        \
static int encode_tile##bpp(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r)    \
{                                                                       \
  CARD##bpp tile_buf[256];                                              \
  CARD8 *dst = dst_buf;                                                 \
  CARD8 *dst_num_subrects;                                              \
  CARD8 *dst_limit;                                                     \
  int x, y, sx, sy;                                                     \
  int best_w, best_h, max_x;                                            \
  CARD##bpp color, bg_color;                                            \
  CARD8 subenc = 0;                                                     \
                                                                        \
  /* Get tile data, count colors, consider bg, fg */                    \
  prepare_tile##bpp(tile_buf, cl, r);                                   \
  bg_color = (CARD##bpp)bg;                                             \
                                                                        \
  /* Set appropriate sub-encoding flags */                              \
  if (prev_bg != bg || !prev_bg_set) {                                  \
    subenc |= HEXTILE_BG_SPECIFIED;                                     \
  }                                                                     \
  if (num_colors != 1) {                                                \
    subenc |= HEXTILE_ANY_SUBRECTS;                                     \
    if (num_colors == 0)                                                \
      subenc |= HEXTILE_SUBRECTS_COLOURED;                              \
    else                                                                \
      subenc |= HEXTILE_FG_SPECIFIED;                                   \
  }                                                                     \
  *dst++ = subenc;                                                      \
  prev_bg = bg;                                                         \
  prev_bg_set = 1;                                                      \
                                                                        \
  /* Write subencoding-dependent heading data */                        \
  if (subenc & HEXTILE_BG_SPECIFIED) {                                  \
    BUF_PUT_PIXEL##bpp(dst, bg_color);                                  \
    dst += sizeof(CARD##bpp);                                           \
  }                                                                     \
  if (subenc & HEXTILE_FG_SPECIFIED) {                                  \
    color = (CARD##bpp)fg;                                              \
    BUF_PUT_PIXEL##bpp(dst, color);                                     \
    dst += sizeof(CARD##bpp);                                           \
  }                                                                     \
  if (subenc & HEXTILE_ANY_SUBRECTS) {                                  \
      dst_num_subrects = dst;                                           \
      *dst++ = 0;                                                       \
  }                                                                     \
                                                                        \
  /* Sort out the simplest case, solid-color tile */                    \
  if (num_colors == 1)                                                  \
    return (dst - dst_buf);                                             \
                                                                        \
  /* Limit data size in dst_buf */                                      \
  dst_limit = dst_buf + r->w * r->h * sizeof(CARD##bpp) + 1;            \
                                                                        \
  /* Find and encode sub-rectangles */                                  \
                                                                        \
  for (y = 0; y < r->h; y++) {                                          \
    for (x = 0; x < r->w; x++) {                                        \
      /* Skip background-colored pixels */                              \
      if (tile_buf[y * r->w + x] == bg_color) {                         \
        continue;                                                       \
      }                                                                 \
      /* Determine dimensions of the best subrect */                    \
      color = tile_buf[y * r->w + x];                                   \
      best_w = 1;                                                       \
      best_h = 1;                                                       \
      max_x = r->w;                                                     \
      for (sy = y; sy < r->h; sy++) {                                   \
        for (sx = x; sx < max_x; sx++) {                                \
          if (tile_buf[sy * r->w + sx] != color)                        \
            break;                                                      \
        }                                                               \
        max_x = sx;                                                     \
        if (max_x == x)                                                 \
          break;                                                        \
        if ((sx - x) * (sy - y + 1) > best_w * best_h) {                \
          best_w = (sx - x);                                            \
          best_h = (sy - y + 1);                                        \
        }                                                               \
      }                                                                 \
      /* Encode subrect of size (best_w * best_h) */                    \
      if (subenc & HEXTILE_SUBRECTS_COLOURED) {                         \
        if (dst + sizeof(CARD##bpp) + 2 >= dst_limit)                   \
          return encode_tile_raw##bpp(dst_buf, cl, r);                  \
                                                                        \
        BUF_PUT_PIXEL##bpp(dst, color);                                 \
        dst += sizeof(CARD##bpp);                                       \
      } else {                                                          \
        if (dst + 2 >= dst_limit)                                       \
          return encode_tile_raw##bpp(dst_buf, cl, r);                  \
      }                                                                 \
      *dst++ = (CARD8)((x << 4) | (y & 0x0F));                          \
      *dst++ = (CARD8)(((best_w - 1) << 4) | ((best_h - 1) & 0x0F));    \
      (*dst_num_subrects)++;                                            \
                                                                        \
      /* Fill in processed subrect with background color */             \
      for (sy = y + 1; sy < y + best_h; sy++) {                         \
        for (sx = x; sx < x + best_w; sx++)                             \
          tile_buf[sy * r->w + sx] = bg_color;                          \
      }                                                                 \
      /* Skip to the next pixel of different color */                   \
      x += (best_w - 1);                                                \
    }                                                                   \
  }                                                                     \
                                                                        \
  return (dst - dst_buf);                                               \
}

DEFINE_ENCODE_TILE(8)
DEFINE_ENCODE_TILE(16)
DEFINE_ENCODE_TILE(32)

/*
 * Encoding a tile using raw sub-encoding in hextile.
 */

#define DEFINE_ENCODE_TILE_RAW(bpp)                                      \
                                                                         \
static int encode_tile_raw##bpp(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r) \
{                                                                        \
  CARD##bpp raw_data[256];                                               \
                                                                         \
  (*cl->trans_func)(raw_data, r, cl->trans_table);                       \
                                                                         \
  *dst_buf++ = HEXTILE_RAW;                                              \
  memcpy(dst_buf, raw_data, r->w * r->h * sizeof(CARD##bpp));            \
  prev_bg_set = 0;                                                       \
                                                                         \
  return (1 + r->w * r->h * sizeof(CARD##bpp));                          \
}

DEFINE_ENCODE_TILE_RAW(8)
DEFINE_ENCODE_TILE_RAW(16)
DEFINE_ENCODE_TILE_RAW(32)

/*
 * Prepare tile for encoding. This function copies small (upto 16x16)
 * rectangle from screenbuffer to memory array, performs color
 * translation and counts number of colors and determines background
 * and foreground colors for the tile.
 */

#define DEFINE_PREPARE_TILE(bpp)                                           \
                                                                           \
static void prepare_tile##bpp(CARD##bpp *dst_buf, CL_SLOT *cl, FB_RECT *r) \
{                                                                          \
  int i;                                                                   \
  int bg_count = 0, fg_count = 0;                                          \
  CARD32 pixel;                                                            \
                                                                           \
  /* Perform pixel format translation */                                   \
  (*cl->trans_func)(dst_buf, r, cl->trans_table);                          \
                                                                           \
  /* Determine number of colors in a tile, choose background and           \
     foreground colors. */                                                 \
  bg = fg = (CARD32)*dst_buf++;                                            \
  num_colors = 1;                                                          \
  for (i = 1; i < r->h * r->w; i++) {                                      \
    pixel = (CARD32)*dst_buf++;                                            \
    if (pixel == bg) {                                                     \
      bg_count++;                                                          \
    } else if (pixel == fg) {                                              \
      fg_count++;                                                          \
    } else if (num_colors == 1) {                                          \
      num_colors++;             /* Two different colors */                 \
      fg = pixel;                                                          \
      fg_count++;                                                          \
    } else {                                                               \
      num_colors = 0;           /* More than two different colors */       \
      break;                                                               \
    }                                                                      \
  }                                                                        \
                                                                           \
  /* Background color is one that occupies more pixels */                  \
  if (fg_count > bg_count) {                                               \
    pixel = bg;                                                            \
    bg = fg;                                                               \
    fg = pixel;                                                            \
  }                                                                        \
}

DEFINE_PREPARE_TILE(8)
DEFINE_PREPARE_TILE(16)
DEFINE_PREPARE_TILE(32)


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
 * $Id: encode.c,v 1.13 2001/10/09 14:57:23 const Exp $
 * Encoding screen rectangles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <zlib.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "rect.h"
#include "translate.h"
#include "client_io.h"
#include "encode.h"

/********************************************************************/
/*                        Simple "encoders"                         */
/********************************************************************/

/*
 * Raw encoder
 */

AIO_BLOCK *rfb_encode_raw_block(CL_SLOT *cl, FB_RECT *r)
{
  AIO_BLOCK *block;

  block = malloc(sizeof(AIO_BLOCK) + 12 +
                 r->w * r->h * (cl->format.bits_pixel / 8));
  if (block) {
    put_rect_header(block->data, r, RFB_ENCODING_RAW);
    (*cl->trans_func)(&block->data[12], r, cl->trans_table);
    block->data_size = 12 + r->w * r->h * (cl->format.bits_pixel / 8);
  }

  return block;
}

/*
 * CopyRect "encoder" :-)
 */

AIO_BLOCK *rfb_encode_copyrect_block(CL_SLOT *cl, FB_RECT *r)
{
  AIO_BLOCK *block;

  block = malloc(sizeof(AIO_BLOCK) + 12 + 4);
  if (block) {
    put_rect_header(block->data, r, RFB_ENCODING_COPYRECT);
    buf_put_CARD16(&block->data[12], r->src_x);
    buf_put_CARD16(&block->data[14], r->src_y);
    block->data_size = 12 + 4;
  }

  return block;
}

/*
 * Tiny function to fill in rectangle header in an RFB update
 */

int put_rect_header(CARD8 *buf, FB_RECT *r, CARD32 enc)
{

  buf_put_CARD16(buf, r->x);
  buf_put_CARD16(&buf[2], r->y);
  buf_put_CARD16(&buf[4], r->w);
  buf_put_CARD16(&buf[6], r->h);
  buf_put_CARD32(&buf[8], enc);

  return 12;                    /* 12 bytes written */
}

/********************************************************************/
/*                         Hextile encoder                          */
/********************************************************************/

/* Medium-level functions */
static int encode_tile_using_cache(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile8(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile16(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile32(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);

/* Low-level functions */
static int encode_tile_ht8(CARD8 *dst_buf, CARD8 *tile_buf,
                           PALETTE2 *pal, FB_RECT *r);
static int encode_tile_ht16(CARD8 *dst_buf, CARD16 *tile_buf,
                            PALETTE2 *pal, FB_RECT *r);
static int encode_tile_ht32(CARD8 *dst_buf, CARD32 *tile_buf,
                            PALETTE2 *pal, FB_RECT *r);
static int encode_tile_raw8(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile_raw16(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);
static int encode_tile_raw32(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r);

/* FIXME: Get rid of static vars that can be made function-level locals. */
static PALETTE2 pal;
static CARD32 prev_bg;
static int prev_bg_set;

/********************************************************************/
/*                Hextile encoder: High-level stuff                 */
/********************************************************************/

/*
 * Highest-level function implementing Hextile encoder. It iterates
 * over tiles 16x16 or less pixels each, and calls appropriate
 * lower-level functions for them.
 */

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
  block = malloc(sizeof(AIO_BLOCK) + 12 +
                 r->w * r->h * (cl->format.bits_pixel / 8) +
                 num_tiles);
  if (block == NULL)
    return NULL;

  put_rect_header(block->data, r, RFB_ENCODING_HEXTILE);

  prev_bg_set = 0;
  data_ptr = (CARD8 *)&block->data[12];
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
        /* 8-bit color: to cache or not to cache? */
        if (aligned_f && cl->bgr233_f && tile_r.w == 16 && tile_r.h == 16)
          data_ptr += encode_tile_using_cache(data_ptr, cl, &tile_r);
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
 * Some statistics for cache utilization.
 */

static long s_cache_hits = 0, s_cache_misses = 0;

void get_hextile_caching_stats(long *hits, long *misses)
{
  *hits = s_cache_hits;
  *misses = s_cache_misses;
}

/********************************************************************/
/*                        Medium-level stuff                        */
/********************************************************************/

/*
 * Encode properly-aligned 16x16 tile in BGR233 pixel format, using
 * data from cache if available, or saving encoded data in cache
 * otherwise.
 */

static int encode_tile_using_cache(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r)
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
    if (hints->subenc8 & RFB_HEXTILE_RAW) {
        prev_bg_set = 0;
        memcpy(dst, &g_cache8[tile_ord * 256], 256);
        dst += 256;
    } else {
      pal.bg = (CARD32)hints->bg8;
      if (prev_bg != pal.bg || !prev_bg_set) {
        *dst++ = hints->bg8;
      } else {
        *dst_buf &= ~RFB_HEXTILE_BG_SPECIFIED;
      }
      prev_bg = pal.bg;
      prev_bg_set = 1;
      if (hints->subenc8 & RFB_HEXTILE_FG_SPECIFIED)
        *dst++ = hints->fg8;
      if (hints->subenc8 & RFB_HEXTILE_ANY_SUBRECTS) {
        data_size = (size_t)hints->datasize8 + 1;
        memcpy(dst, &g_cache8[tile_ord * 256], data_size);
        dst += data_size;
      }
    }
    dst_bytes = dst - dst_buf;

  } else {                      /* Cache miss */
    s_cache_misses++;

    /* Encode tile... */
    dst_bytes = encode_tile8(dst_buf, cl, r);

    /* ... and save encoded data in the cache. */
    hints->subenc8 = *dst++;
    if (hints->subenc8 & RFB_HEXTILE_RAW) {
      memcpy(&g_cache8[tile_ord * 256], dst, 256);
    } else {
      if (hints->subenc8 & RFB_HEXTILE_BG_SPECIFIED) {
        hints->bg8 = *dst++;
      } else {
        hints->bg8 = (CARD8)pal.bg;
        hints->subenc8 |= RFB_HEXTILE_BG_SPECIFIED;
      }
      if (hints->subenc8 & RFB_HEXTILE_FG_SPECIFIED)
        hints->fg8 = *dst++;
      if (hints->subenc8 & RFB_HEXTILE_ANY_SUBRECTS) {
        data_size = (dst_buf + dst_bytes) - dst;
        hints->datasize8 = (CARD8)(data_size - 1);
        memcpy(&g_cache8[tile_ord * 256], dst, data_size);
      }
    }

  }

  return dst_bytes;
}

/*
 * Analyze and encode a tile.
 */

#define DEFINE_ENCODE_TILE(bpp)                                         \
                                                                        \
static int encode_tile##bpp(CARD8 *dst_buf, CL_SLOT *cl, FB_RECT *r)    \
{                                                                       \
  CARD##bpp tile_buf[256];                                              \
  int bytes;                                                            \
                                                                        \
  /* Translate pixel data into client's format. */                      \
  (*cl->trans_func)(tile_buf, r, cl->trans_table);                      \
                                                                        \
  /* Count number of colors, consider background & foreground. */       \
  analyze_rect##bpp(tile_buf, r, &pal);                                 \
                                                                        \
  /* Try to encode tile representing it as a set of subrects. */        \
  bytes = encode_tile_ht##bpp(dst_buf, tile_buf, &pal, r);              \
                                                                        \
  /* If such encoding was inefficient, use raw sub-encoding. */         \
  if (bytes < 0)                                                        \
    bytes = encode_tile_raw##bpp(dst_buf, cl, r);                       \
                                                                        \
  return bytes;                                                         \
}

DEFINE_ENCODE_TILE(8)
DEFINE_ENCODE_TILE(16)
DEFINE_ENCODE_TILE(32)

/********************************************************************/
/*                         Low-level stuff                          */
/********************************************************************/

/*
 * A function to encode a tile (of size 16x16 or less) using Hextile
 * encoding. The tile_buf should point to raw pixel data for the tile
 * in client pixel format. The dst_buf argument should point to an
 * array of size at least (256 * (cl->format.bits_pixel/8) + 1) bytes.
 * The pal argument should point to a valid PALETTE2 structire filled
 * in by the analyze_rectNN funtion.
 *
 * Return value: the number of bytes put into the dst_buf or -1 if
 * this tile should be raw-encoded.
 *
 * NOTE: tile_buf[] contents would be destroyed by this function.
 */

#define DEFINE_ENCODE_TILE_HT(bpp)                                           \
                                                                             \
static int encode_tile_ht##bpp(CARD8 *dst_buf, CARD##bpp *tile_buf,          \
                               PALETTE2 *pal, FB_RECT *r)                    \
{                                                                            \
  CARD8 *dst = dst_buf;                                                      \
  CARD8 *dst_num_subrects;                                                   \
  CARD8 *dst_limit;                                                          \
  int x, y, sx, sy;                                                          \
  int best_w, best_h, max_x;                                                 \
  CARD##bpp color, bg_color;                                                 \
  CARD8 subenc = 0;                                                          \
                                                                             \
  bg_color = (CARD##bpp)pal->bg;                                             \
                                                                             \
  /* Set appropriate sub-encoding flags */                                   \
  if (prev_bg != pal->bg || !prev_bg_set) {                                  \
    subenc |= RFB_HEXTILE_BG_SPECIFIED;                                      \
  }                                                                          \
  if (pal->num_colors != 1) {                                                \
    subenc |= RFB_HEXTILE_ANY_SUBRECTS;                                      \
    if (pal->num_colors == 0)                                                \
      subenc |= RFB_HEXTILE_SUBRECTS_COLOURED;                               \
    else                                                                     \
      subenc |= RFB_HEXTILE_FG_SPECIFIED;                                    \
  }                                                                          \
  *dst++ = subenc;                                                           \
  prev_bg = pal->bg;                                                         \
  prev_bg_set = 1;                                                           \
                                                                             \
  /* Write subencoding-dependent heading data */                             \
  if (subenc & RFB_HEXTILE_BG_SPECIFIED) {                                   \
    BUF_PUT_PIXEL##bpp(dst, bg_color);                                       \
    dst += sizeof(CARD##bpp);                                                \
  }                                                                          \
  if (subenc & RFB_HEXTILE_FG_SPECIFIED) {                                   \
    color = (CARD##bpp)pal->fg;                                              \
    BUF_PUT_PIXEL##bpp(dst, color);                                          \
    dst += sizeof(CARD##bpp);                                                \
  }                                                                          \
  if (subenc & RFB_HEXTILE_ANY_SUBRECTS) {                                   \
      dst_num_subrects = dst;                                                \
      *dst++ = 0;                                                            \
  }                                                                          \
                                                                             \
  /* Sort out the simplest case, solid-color tile */                         \
  if (pal->num_colors == 1)                                                  \
    return (dst - dst_buf);                                                  \
                                                                             \
  /* Limit data size in dst_buf */                                           \
  dst_limit = dst_buf + r->w * r->h * sizeof(CARD##bpp) + 1;                 \
                                                                             \
  /* Find and encode sub-rectangles */                                       \
                                                                             \
  for (y = 0; y < r->h; y++) {                                               \
    for (x = 0; x < r->w; x++) {                                             \
      /* Skip background-colored pixels */                                   \
      if (tile_buf[y * r->w + x] == bg_color) {                              \
        continue;                                                            \
      }                                                                      \
      /* Determine dimensions of the best subrect */                         \
      color = tile_buf[y * r->w + x];                                        \
      best_w = 1;                                                            \
      best_h = 1;                                                            \
      max_x = r->w;                                                          \
      for (sy = y; sy < r->h; sy++) {                                        \
        for (sx = x; sx < max_x; sx++) {                                     \
          if (tile_buf[sy * r->w + sx] != color)                             \
            break;                                                           \
        }                                                                    \
        max_x = sx;                                                          \
        if (max_x == x)                                                      \
          break;                                                             \
        if ((sx - x) * (sy - y + 1) > best_w * best_h) {                     \
          best_w = (sx - x);                                                 \
          best_h = (sy - y + 1);                                             \
        }                                                                    \
      }                                                                      \
      /* Encode subrect of size (best_w * best_h) */                         \
      if (subenc & RFB_HEXTILE_SUBRECTS_COLOURED) {                          \
        if (dst + sizeof(CARD##bpp) + 2 >= dst_limit)                        \
          return -1;                                                         \
                                                                             \
        BUF_PUT_PIXEL##bpp(dst, color);                                      \
        dst += sizeof(CARD##bpp);                                            \
      } else {                                                               \
        if (dst + 2 >= dst_limit)                                            \
          return -1;                                                         \
      }                                                                      \
      *dst++ = (CARD8)((x << 4) | (y & 0x0F));                               \
      *dst++ = (CARD8)(((best_w - 1) << 4) | ((best_h - 1) & 0x0F));         \
      (*dst_num_subrects)++;                                                 \
                                                                             \
      /* Fill in processed subrect with background color */                  \
      for (sy = y + 1; sy < y + best_h; sy++) {                              \
        for (sx = x; sx < x + best_w; sx++)                                  \
          tile_buf[sy * r->w + sx] = bg_color;                               \
      }                                                                      \
      /* Skip to the next pixel of different color */                        \
      x += (best_w - 1);                                                     \
    }                                                                        \
  }                                                                          \
                                                                             \
  return (dst - dst_buf);                                                    \
}

DEFINE_ENCODE_TILE_HT(8)
DEFINE_ENCODE_TILE_HT(16)
DEFINE_ENCODE_TILE_HT(32)

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
  *dst_buf++ = RFB_HEXTILE_RAW;                                          \
  memcpy(dst_buf, raw_data, r->w * r->h * sizeof(CARD##bpp));            \
  prev_bg_set = 0;                                                       \
                                                                         \
  return (1 + r->w * r->h * sizeof(CARD##bpp));                          \
}

DEFINE_ENCODE_TILE_RAW(8)
DEFINE_ENCODE_TILE_RAW(16)
DEFINE_ENCODE_TILE_RAW(32)

/*
 * Determine number of colors in a tile, choose background and
 * foreground colors. Note that this function is used not only by
 * Hextile encoder, therefore it's not declared as static.
 */

#define DEFINE_ANALYZE_RECT(bpp)                                             \
                                                                             \
void analyze_rect##bpp(CARD##bpp *buf, FB_RECT *r, PALETTE2 *pal)            \
{                                                                            \
  int i;                                                                     \
  int bg_count = 0, fg_count = 0;                                            \
  CARD32 pixel;                                                              \
                                                                             \
  pal->bg = pal->fg = (CARD32)*buf++;                                        \
  pal->num_colors = 1;                                                       \
  for (i = 1; i < r->h * r->w; i++) {                                        \
    pixel = (CARD32)*buf++;                                                  \
    if (pixel == pal->bg) {                                                  \
      bg_count++;                                                            \
    } else if (pixel == pal->fg) {                                           \
      fg_count++;                                                            \
    } else if (pal->num_colors == 1) {                                       \
      pal->num_colors++;        /* Two different colors */                   \
      pal->fg = pixel;                                                       \
      fg_count++;                                                            \
    } else {                                                                 \
      pal->num_colors = 0;      /* More than two different colors */         \
      break;                                                                 \
    }                                                                        \
  }                                                                          \
                                                                             \
  /* Background color is one that occupies more pixels */                    \
  if (fg_count > bg_count) {                                                 \
    pixel = pal->bg;                                                         \
    pal->bg = pal->fg;                                                       \
    pal->fg = pixel;                                                         \
  }                                                                          \
}

DEFINE_ANALYZE_RECT(8)
DEFINE_ANALYZE_RECT(16)
DEFINE_ANALYZE_RECT(32)


/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: encode.c,v 1.3 2001/08/11 02:49:04 const Exp $
 * Encoding screen rectangles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "rect.h"
#include "client_io.h"
#include "encode.h"

/*
 * Raw encoder
 */

static void rfb_encode_raw8(void *outbuf, RFB_PIXEL_FORMAT *fmt, FB_RECT *r);
static void rfb_encode_raw16(void *outbuf, RFB_PIXEL_FORMAT *fmt, FB_RECT *r);
static void rfb_encode_raw32(void *outbuf, RFB_PIXEL_FORMAT *fmt, FB_RECT *r);

AIO_BLOCK *rfb_encode_raw_block(RFB_PIXEL_FORMAT *fmt, FB_RECT *r)
{
  AIO_BLOCK *block;

  block = malloc(sizeof(AIO_BLOCK) + r->w * r->h * (fmt->bits_pixel / 8));
  if (block) {
    switch (fmt->bits_pixel) {
    case 8:
      rfb_encode_raw8(&block->data, fmt, r);
      break;
    case 16:
      rfb_encode_raw16(&block->data, fmt, r);
      break;
    case 32:
      rfb_encode_raw32(&block->data, fmt, r);
      break;
    }
    block->data_size = r->w * r->h * (fmt->bits_pixel / 8);
  }

  return block;
}

#define DEFINE_ENCODE_RAW(bpp)                                  \
                                                                \
static void rfb_encode_raw##bpp(void *outbuf,                   \
                                RFB_PIXEL_FORMAT *fmt,          \
                                FB_RECT *r)                     \
{                                                               \
  CARD32 *fb_ptr;                                               \
  CARD##bpp *dst = (CARD##bpp *)outbuf;                         \
  int skip, x, y;                                               \
                                                                \
  fb_ptr = &g_framebuffer[r->y * g_screen_info->width + r->x];  \
  skip = g_screen_info->width - r->w;                           \
                                                                \
  for (y = 0; y < r->h; y++) {                                  \
    for (x = 0; x < r->w; x++) {                                \
      *dst++ = (CARD##bpp)TRANSLATE_PIXEL(*fb_ptr, fmt);        \
      fb_ptr++;                                                 \
    }                                                           \
    fb_ptr += skip;                                             \
  }                                                             \
}

DEFINE_ENCODE_RAW(8)
DEFINE_ENCODE_RAW(16)
DEFINE_ENCODE_RAW(32)

/*
 * Hextile encoder
 */

static int encode_tile8(CARD8 *dst_buf, RFB_PIXEL_FORMAT *fmt, FB_RECT *r);
static int encode_tile16(CARD8 *dst_buf, RFB_PIXEL_FORMAT *fmt, FB_RECT *r);
static int encode_tile32(CARD8 *dst_buf, RFB_PIXEL_FORMAT *fmt, FB_RECT *r);
static int encode_tile_raw8(CARD8 *dst_buf,RFB_PIXEL_FORMAT *fmt,FB_RECT *r);
static int encode_tile_raw16(CARD8 *dst_buf,RFB_PIXEL_FORMAT *fmt,FB_RECT *r);
static int encode_tile_raw32(CARD8 *dst_buf,RFB_PIXEL_FORMAT *fmt,FB_RECT *r);
static void prepare_tile(CARD32 *dst_buf, FB_RECT *r);

static CARD32 bg, fg;
static CARD32 prev_bg;
static int prev_bg_set;
static int num_colors;

AIO_BLOCK *rfb_encode_hextile_block(RFB_PIXEL_FORMAT *fmt, FB_RECT *r)
{
  AIO_BLOCK *block;
  int num_tiles;
  int rx1, ry1;
  FB_RECT tile;
  CARD8 *data_ptr;

  /* Calculate the number of tiles per this rectangle */
  num_tiles = ((r->w + 15) / 16) * ((r->h + 15) / 16);

  /* Allocate a memory block of maximum possible size */
  block = malloc(sizeof(AIO_BLOCK) +
                 r->w * r->h * (fmt->bits_pixel / 8) +
                 num_tiles);
  if (block == NULL)
    return NULL;

  prev_bg_set = 0;
  data_ptr = (CARD8 *)block->data;
  rx1 = r->x + r->w;
  ry1 = r->y + r->h;
  tile.h = 16;

  for (tile.y = r->y; tile.y < ry1; tile.y += 16) {
    if (ry1 - tile.y < 16)
      tile.h = ry1 - tile.y;
    tile.w = 16;
    for (tile.x = r->x; tile.x < rx1; tile.x += 16) {
      if (rx1 - tile.x < 16)
        tile.w = rx1 - tile.x;
      switch (fmt->bits_pixel) {
      case 8:
        data_ptr += encode_tile8(data_ptr, fmt, &tile);
        break;
      case 16:
        data_ptr += encode_tile16(data_ptr, fmt, &tile);
        break;
      case 32:
        data_ptr += encode_tile32(data_ptr, fmt, &tile);
        break;
      }
    }
  }

  block->data_size = data_ptr - (CARD8 *)block->data;
  return realloc(block, sizeof(AIO_BLOCK) + block->data_size);
}

/*
 * A function to encode a tile (of size 16x16 or less) using Hextile
 * encoding. The dst_buf argument should point to an array of size at
 * least (256 * (fmt->bits_pixel/8) + 1) bytes. Returns number of
 * bytes put into the dst_buf.
 */

#define DEFINE_ENCODE_TILE(bpp)                                         \
                                                                        \
static int encode_tile##bpp(CARD8 *dst_buf,                             \
                            RFB_PIXEL_FORMAT *fmt,                      \
                            FB_RECT *r)                                 \
{                                                                       \
  CARD32 tile_buf[256 * sizeof(CARD32)];                                \
  CARD8 *dst = dst_buf;                                                 \
  CARD8 *dst_num_subrects;                                              \
  CARD8 *dst_limit;                                                     \
  int x, y, sx, sy;                                                     \
  int best_w, best_h, max_x;                                            \
  CARD32 color;                                                         \
  CARD##bpp cl_color;                                                   \
  CARD8 subenc = 0;                                                     \
                                                                        \
  /* Get tile data, count colors, consider bg, fg */                    \
  prepare_tile(tile_buf, r);                                            \
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
    cl_color = (CARD##bpp)TRANSLATE_PIXEL(bg, fmt);                     \
    BUF_PUT_PIXEL##bpp(dst, cl_color);                                  \
    dst += (bpp/8);                                                     \
  }                                                                     \
  if (subenc & HEXTILE_FG_SPECIFIED) {                                  \
    cl_color = (CARD##bpp)TRANSLATE_PIXEL(fg, fmt);                     \
    BUF_PUT_PIXEL##bpp(dst, cl_color);                                  \
    dst += (bpp/8);                                                     \
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
  dst_limit = dst_buf + r->w * r->h * (bpp/8) + 1;                      \
                                                                        \
  /* Find and encode sub-rectangles */                                  \
                                                                        \
  for (y = 0; y < r->h; y++) {                                          \
    for (x = 0; x < r->w; x++) {                                        \
      /* Skip background-colored pixels */                              \
      if (tile_buf[y * r->w + x] == bg) {                               \
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
        if (dst + (bpp/8) + 2 >= dst_limit)                             \
          return encode_tile_raw##bpp(dst_buf, fmt, r);                 \
                                                                        \
        cl_color = (CARD##bpp)TRANSLATE_PIXEL(color, fmt);              \
        BUF_PUT_PIXEL##bpp(dst, cl_color);                              \
        dst += (bpp/8);                                                 \
      } else {                                                          \
        if (dst + 2 >= dst_limit)                                       \
          return encode_tile_raw##bpp(dst_buf, fmt, r);                 \
      }                                                                 \
      *dst++ = (CARD8)((x << 4) | (y & 0x0F));                          \
      *dst++ = (CARD8)(((best_w - 1) << 4) | ((best_h - 1) & 0x0F));    \
      (*dst_num_subrects)++;                                            \
                                                                        \
      /* Fill in processed subrect with background color */             \
      for (sy = y + 1; sy < y + best_h; sy++) {                         \
        for (sx = x; sx < x + best_w; sx++)                             \
          tile_buf[sy * r->w + sx] = bg;                                \
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

#define DEFINE_ENCODE_TILE_RAW(bpp)                             \
                                                                \
static int encode_tile_raw##bpp(CARD8 *dst_buf,                 \
                                RFB_PIXEL_FORMAT *fmt,          \
                                FB_RECT *r)                     \
{                                                               \
  CARD##bpp raw_data[256];                                      \
  CARD##bpp *ptr = raw_data;                                    \
  CARD32 *fb_ptr;                                               \
  int skip, x, y;                                               \
                                                                \
  fb_ptr = &g_framebuffer[r->y * g_screen_info->width + r->x];  \
  skip = g_screen_info->width - r->w;                           \
                                                                \
  for (y = 0; y < r->h; y++) {                                  \
    for (x = 0; x < r->w; x++) {                                \
      *ptr++ = (CARD##bpp)TRANSLATE_PIXEL(*fb_ptr, fmt);        \
      fb_ptr++;                                                 \
    }                                                           \
    fb_ptr += skip;                                             \
  }                                                             \
                                                                \
  *dst_buf = HEXTILE_RAW;                                       \
  memcpy(&dst_buf[1], raw_data, r->w * r->h * (bpp/8));         \
                                                                \
  return (1 + r->w * r->h * (bpp/8));                           \
}                                                               \

DEFINE_ENCODE_TILE_RAW(8)
DEFINE_ENCODE_TILE_RAW(16)
DEFINE_ENCODE_TILE_RAW(32)

/*
 * Prepare tile for encoding. This function copies small (upto 16x16)
 * rectangle from screenbuffer to memory array, counts number of
 * colors and determines background and foreground colors for the
 * tile.
 */

static void prepare_tile(CARD32 *dst_buf, FB_RECT *r)
{
  int x, y;
  int bg_count = 0, fg_count = 0;
  CARD32 *fb_ptr;
  CARD32 pixel;

  fb_ptr = &g_framebuffer[r->y * g_screen_info->width + r->x];

  bg = fg = fb_ptr[0];
  num_colors = 1;

  /* Determine number of colors in a tile, choose background and
     foreground colors. */
  for (y = 0; y < r->h; y++) {
    for (x = 0; x < r->w; x++) {
      *dst_buf++ = pixel = fb_ptr[x];
      if (num_colors) {
        if (pixel == bg) {
          bg_count++;
        } else if (pixel == fg) {
          fg_count++;
        } else if (num_colors == 1) {
          num_colors++;         /* Two different colors */
          fg = pixel;
          fg_count++;
        } else {
          num_colors = 0;       /* More than two different colors */
        }
      }
    }
    fb_ptr += g_screen_info->width;
  }
  if (fg_count > bg_count) {
    pixel = bg;
    bg = fg;
    fg = pixel;
  }
}


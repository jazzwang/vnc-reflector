/* VNC Reflector
 * Copyright (C) 2001 HorizonLive.com, Inc.  All rights reserved.
 * Copyright (C) 2000, 2001 Constantin Kaplinsky.  All rights reserved.
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
 * $Id: encode_tight8.c,v 1.2 2001/10/11 08:58:12 const Exp $
 * Tight encoder.
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

/* Note: The following constant should not be changed. */
#define TIGHT_MIN_TO_COMPRESS 12

/* Compression level stuff. The following array contains various
   encoder parameters for each of 10 compression levels (0..9).
   NOTE: Greatest max_rect_size determines sizes of data buffers,
   see s_buf_pixels[] declaration below. */

typedef struct TIGHT_CONF_s {
    int max_rect_size, max_rect_width, min_mono_rect_size;
    int mono_zlib_level, raw_zlib_level;
} TIGHT_CONF;

static TIGHT_CONF s_tight_conf[10] = {
    {   512,   32,   6, 0, 0 },
    {  2048,  128,   6, 1, 1 },
    {  6144,  256,   8, 3, 2 },
    { 10240, 1024,  12, 5, 3 },
    { 16384, 2048,  12, 6, 4 },
    { 32768, 2048,  12, 7, 5 },
    { 65536, 2048,  16, 7, 6 },
    { 65536, 2048,  16, 8, 7 },
    { 65536, 2048,  32, 9, 8 },
    { 65536, 2048,  32, 9, 9 }
};

/* Data buffers for translated pixel data and for the data to compress. */
static CARD8 s_buf_pixels[65536];

/* Background and foreground colors in two-color rectangle. */
static PALETTE2 s_pal;


/*
 * Prototypes for static functions.
 */

static AIO_BLOCK *encode_tight8_block(CL_SLOT *cl, FB_RECT *r);
static int encode_solid_color(CARD8 *buf, CL_SLOT *cl);
static int encode_two_colors(CARD8 *buf, CL_SLOT *cl, int w, int h);
static int encode_full_color(CARD8 *buf, CL_SLOT *cl, int w, int h);
static int compress_data(CARD8 *buf, CL_SLOT *cl,
                         int stream_id, int data_len, int zlib_level);
static void detect_colors(CARD8 *data, PALETTE2 *pal, FB_RECT *r);
static void encode_mono_rect(CARD8 *buf, CARD8 bg_color, int w, int h);


/*
 * Tight encoding implementation. This particular function splits
 * large rectangles into smaller ones depending on compression setting
 * and calls lower-level encoding function for each of them.
 */

int rfb_encode_tight8(CL_SLOT *cl, FB_RECT *r)
{
  /* FIXME: Variable names. */
  int max_rect_size, max_rect_width;
  int max_subrect_width, max_subrect_height;
  FB_RECT sr;

  max_rect_size = s_tight_conf[cl->compress_level].max_rect_size;
  max_rect_width = s_tight_conf[cl->compress_level].max_rect_width;

  if (r->w > max_rect_width || r->w * r->h > max_rect_size) {
    max_subrect_width = (r->w > max_rect_width) ? max_rect_width : r->w;
    max_subrect_height = max_rect_size / max_subrect_width;

    sr.h = max_subrect_height;
    for (sr.y = r->y; sr.y < r->y + r->h; sr.y += max_subrect_height) {
      if (sr.y + max_subrect_height > r->y + r->h) {
        sr.h = r->y + r->h - sr.y;
      }
      sr.w = max_rect_width;
      for (sr.x = r->x; sr.x < r->x + r->w; sr.x += max_rect_width) {
        if (sr.x + max_rect_width > r->x + r->w) {
          sr.w = r->x + r->w - sr.x;
        }
        if (!encode_tight8_block(cl, &sr))
          return 0;
      }
    }
  } else {
    if (!encode_tight8_block(cl, r))
      return 0;
  }

  return 1;
}

/*
 * Encode one rectangle. This funtion determines number of colors and
 * calls appropriate encoding function depending on the result.
 */

static AIO_BLOCK *encode_tight8_block(CL_SLOT *cl, FB_RECT *r)
{
  AIO_BLOCK *block;
  int size;

  /* Allocate a memory block of maximum possible size: 12 bytes
     rectangle header, 8 bytes Tight header, uncompressed data size,
     worst-case zlib overhead. */
  block = malloc(sizeof(AIO_BLOCK) + 12 + 8 + r->w * r->h +
                 (r->w * r->h + 99) / 100 + 12);
  if (block == NULL)
    return NULL;

  /* Get pixel data in client pixel format. */
  (*cl->trans_func)(&s_buf_pixels, r, cl->trans_table);

  /* Prepare RFB rectangle header. */
  block->data_size = put_rect_header(block->data, r, RFB_ENCODING_TIGHT);

  /* Get number of colors in this rectangle. */
  detect_colors(s_buf_pixels, &s_pal, r);

  /* Encode data. */
  switch (s_pal.num_colors) {
  case 1:
    /* Solid-color rectangle */
    size = encode_solid_color(&block->data[block->data_size], cl);
    break;
  case 2:
    /* Two-color rectangle */
    if (r->w * r->h >= s_tight_conf[cl->compress_level].min_mono_rect_size) {
      size = encode_two_colors(&block->data[block->data_size], cl, r->w, r->h);
      break;
    }
    /* PASS THROUGH */
  case 0:
    /* Truecolor image */
    size = encode_full_color(&block->data[block->data_size], cl, r->w, r->h);
    break;
  }

  if (size > 0) {
    block->data_size += size;
    block = realloc(block, sizeof(AIO_BLOCK) + block->data_size);
  } else {
    free(block);
    block = NULL;
  }

  /* Send the block.
     FIXME: This should be moved elsewhere (client_io.c). */
  if (block != NULL) {
    aio_write_nocopy(NULL, block);
  }

  return block;
}

/*
 * Sub-encoding implementations.
 */

static int encode_solid_color(CARD8 *buf, CL_SLOT *cl)
{
  buf[0] = RFB_TIGHT_FILL;
  buf[1] = s_buf_pixels[0];

  return 2;                     /* 2 bytes written */
}

static int encode_two_colors(CARD8 *buf, CL_SLOT *cl, int w, int h)
{
  int data_len, size;
  int stream_id = 1;

  /* Prepare tight encoding header. */
  data_len = (w + 7) / 8;
  data_len *= h;

  buf[0] = stream_id << 4 | RFB_TIGHT_EXPLICIT_FILTER;
  buf[1] = RFB_TIGHT_FILTER_PALETTE;
  buf[2] = 1;                   /* Number of colors minus one */
  buf[3] = s_pal.bg;
  buf[4] = s_pal.fg;

  encode_mono_rect(s_buf_pixels, s_pal.bg, w, h);

  size = compress_data(&buf[5], cl, stream_id, data_len,
                       s_tight_conf[cl->compress_level].mono_zlib_level);
  return 5 + size;
}

static int encode_full_color(CARD8 *buf, CL_SLOT *cl, int w, int h)
{
  int size;
  int stream_id = 0;

  *buf = (CARD8)0x00;           /* Stream id = 0, no flushing, no filter */

  size = compress_data(&buf[1], cl, stream_id, w * h,
                       s_tight_conf[cl->compress_level].raw_zlib_level);
  return 1 + size;
}

/*
 * Compress data, return data size written to the buffer or -1 on error.
 */

static int compress_data(CARD8 *buf, CL_SLOT *cl,
                         int stream_id, int data_len, int zlib_level)
{
  z_streamp pz;
  int old_avail_out, compressed_len, size_field_len;

  /* Don't try to compress small data chunks */
  if (data_len < TIGHT_MIN_TO_COMPRESS) {
    memcpy(buf, s_buf_pixels, data_len);
    return data_len;
  }

  pz = &cl->zs_struct[stream_id];

  /* Initialize compression stream if necessary. */
  if (!cl->zs_active[stream_id]) {
    pz->zalloc = Z_NULL;
    pz->zfree = Z_NULL;
    pz->opaque = Z_NULL;

    if (deflateInit2(pz, zlib_level, Z_DEFLATED, MAX_WBITS,
                     MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
      return -1;

    cl->zs_active[stream_id] = 1;
    cl->zs_level[stream_id] = zlib_level;
  }

  /* Prepare pointer to the source buffer. */
  pz->next_in = (Bytef *)s_buf_pixels;
  pz->avail_in = data_len;

  /* Prepare pointer to the destination buffer, reserving two bytes
     for data size. Ensure that buffer size is enough for one-step
     compression. */
  pz->next_out = (Bytef *)&buf[2];
  pz->avail_out = old_avail_out = data_len + (data_len + 99) / 100 + 12;

  /* Change compression parameters if needed. */
  if (zlib_level != cl->zs_level[stream_id]) {
    if (deflateParams(pz, zlib_level, Z_DEFAULT_STRATEGY) != Z_OK) {
      return -1;
    }
    cl->zs_level[stream_id] = zlib_level;
  }

  /* Finally, compress the data. */
  if ( deflate(pz, Z_SYNC_FLUSH) != Z_OK ||
       pz->avail_in != 0 || pz->avail_out == 0 ) {
    return -1;
  }

  /* Insert variable-length data size field before the compressed data.
     NOTE: if this field is not two bytes long (that is, 1 or 3), we'll
     have to move compressed data in the buffer using memmove(3). */
  size_field_len = 0;
  compressed_len = old_avail_out - pz->avail_out;
  buf[size_field_len++] = compressed_len & 0x7F;
  if (compressed_len <= 0x7F) {
    memmove(&buf[1], &buf[2], compressed_len);
  } else {
    buf[size_field_len-1] |= 0x80;
    buf[size_field_len++] = compressed_len >> 7 & 0x7F;
    if (compressed_len > 0x3FFF) {
      memmove(&buf[3], &buf[2], compressed_len);
      buf[size_field_len-1] |= 0x80;
      buf[size_field_len++] = compressed_len >> 14 & 0xFF;
    }
  }

  return size_field_len + compressed_len;
}

/*
 * Code to determine how many different colors are used in a rectangle.
 */

static void detect_colors(CARD8 *data, PALETTE2 *pal, FB_RECT *r)
{
  CARD8 c0, c1;
  int i, n0, n1;
  int data_size = r->w * r->h;

  c0 = data[0];
  for (i = 1; i < data_size && data[i] == c0; i++);
  if (i == data_size) {
    pal->num_colors = 1;        /* Solid-color rectangle */
    return;
  }

  n0 = i;
  c1 = data[i];
  n1 = 0;
  for (i++; i < data_size; i++) {
    if (data[i] == c0) {
      n0++;
    } else if (data[i] == c1) {
      n1++;
    } else
      break;
  }
  if (i == data_size) {
    if (n0 > n1) {
      pal->bg = c0; pal->fg = c1;
    } else {
      pal->bg = c1; pal->fg = c0;
    }
    pal->num_colors = 2;        /* Two colors */
  } else {
    pal->num_colors = 0;        /* More than two colors */
  }
}

static void encode_mono_rect(CARD8 *buf, CARD8 bg_color, int w, int h)
{
  CARD8 *ptr = buf;
  unsigned int value, mask;
  int aligned_width;
  int x, y, bg_bits;

  aligned_width = w - w % 8;

  for (y = 0; y < h; y++) {
    for (x = 0; x < aligned_width; x += 8) {
      for (bg_bits = 0; bg_bits < 8; bg_bits++) {
        if (*ptr++ != bg_color)
          break;
      }
      if (bg_bits == 8) {
        *buf++ = 0;
        continue;
      }
      mask = 0x80 >> bg_bits;
      value = mask;
      for (bg_bits++; bg_bits < 8; bg_bits++) {
        mask >>= 1;
        if (*ptr++ != bg_color) {
          value |= mask;
        }
      }
      *buf++ = (CARD8)value;
    }

    mask = 0x80;
    value = 0;
    if (x >= w)
      continue;

    for (; x < w; x++) {
      if (*ptr++ != bg_color) {
        value |= mask;
      }
      mask >>= 1;
    }
    *buf++ = (CARD8)value;
  }
}


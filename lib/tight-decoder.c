/*
 * Tight Encoding (efficient encoding for true-color pixel data)
 *
 * Copyright (C) 2000-2008 Constantin Kaplinsky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tight-decoder.c - Decoding Tight-encoded rectangles.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "tight-decoder.h"

#define TIGHT_EXPLICIT_FILTER  0x04
#define TIGHT_FILL             0x08
#define TIGHT_JPEG             0x09
#define TIGHT_MAX_SUBENCODING  0x09

#define TIGHT_FILTER_COPY      0x00
#define TIGHT_FILTER_PALETTE   0x01
#define TIGHT_FILTER_GRADIENT  0x02

#define TIGHT_MIN_TO_COMPRESS  12

static int td_func_compctl(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_fill(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_filter(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_numcolors(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_palette(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_rawdata(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_len1(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_len2(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_len3(TIGHT_DECODER *td, unsigned char *buf);
static int td_func_zlibdata(TIGHT_DECODER *td, unsigned char *buf);

/************************* Public Functions *************************/

int
tight_decode_init(TIGHT_DECODER *td)
{
  char msg[] = "No error";

  memset(td, 0, sizeof(TIGHT_DECODER));
  memcpy(td->error_msg, msg, sizeof(msg));

  return 1;
}

int
tight_decode_set_framebuffer(TIGHT_DECODER *td, u_int32_t *fb,
                             int width, int height, int stride)
{
  if (fb != NULL) {
    if (width <= 0 || height <= 0 || stride < width) {
      snprintf(td->error_msg, sizeof(td->error_msg),
               "Incorrect framebuffer attributes");
      return 0;
    }
  }

  td->fb = fb;
  td->fb_width = width;
  td->fb_height = height;
  td->fb_stride = stride;

  return 1;
}

void
tight_decode_cleanup(TIGHT_DECODER *td)
{
}

int
tight_decode_start(TIGHT_DECODER *td, int x, int y, int w, int h)
{
  td->rect_x = x;
  td->rect_y = y;
  td->rect_w = w;
  td->rect_h = h;

  /* Make sure the rectangle is within framebuffer bounds */
  if (x < 0 || x + w > td->fb_width ||
      y < 0 || y + h > td->fb_height) {
    snprintf(td->error_msg, sizeof(td->error_msg),
             "Rectangle out of the framebuffer bounds (%d,%d %dx%d)",
             x, y, w, h);
    td->func = NULL;
    return -1;
  }

  /* If the rectangle is empty, we don't expect any data */
  if (w * h == 0) {
    td->func = NULL;
    return 0;
  }

  /* Expect compression control byte */
  td->func = &td_func_compctl;
  td->num_bytes = 1;
  return 1;
}

int
tight_decode_continue(TIGHT_DECODER *td, char *buf)
{
  int num_bytes_expected;

  if (td->func == NULL) {
    snprintf(td->error_msg, sizeof(td->error_msg),
             "Invalid decoder state");
    return -1;
  }

  num_bytes_expected = (*td->func)(td, (unsigned char *)buf);
  td->num_bytes += num_bytes_expected;
  return num_bytes_expected;
}

char *
tight_decode_get_error(TIGHT_DECODER *td)
{
  return td->error_msg;
}

int
tdstat_get_zlib_reset_mask(TIGHT_DECODER *td)
{
  return td->zlib_reset_mask;
}

int
tdstat_get_zlib_stream_id(TIGHT_DECODER *td)
{
  return td->zlib_stream_id;
}

int
tdstat_get_num_colors(TIGHT_DECODER *td)
{
  return td->num_colors;
}

int
tdstat_get_num_raw_bytes(TIGHT_DECODER *td)
{
  return td->rect_w * td->rect_h * 4;
}

int
tdstat_get_num_encoded_bytes(TIGHT_DECODER *td)
{
  return td->num_bytes;
}

int
tdstat_get_num_compressed_bytes(TIGHT_DECODER *td)
{
  return td->compressed_size;
}

/************************* Helper Functions *************************/

static int td_dispatch_decoding(TIGHT_DECODER *td)
{
  if (td->filter_id == TIGHT_FILTER_COPY ||
      td->filter_id == TIGHT_FILTER_GRADIENT) {
    td->uncompressed_size = td->rect_w * td->rect_h * 3;
  } else if (td->filter_id == TIGHT_FILTER_PALETTE) {
    if (td->num_colors <= 2) {
      td->uncompressed_size = (td->rect_w + 7) / 8;
      td->uncompressed_size *= td->rect_h;
    } else {
      td->uncompressed_size = td->rect_w * td->rect_h;
    }
  }

  if (td->uncompressed_size < TIGHT_MIN_TO_COMPRESS) {
    td->func = &td_func_rawdata;
    return td->uncompressed_size;
  } else {
    td->func = &td_func_len1;
    return 1;
  }
}

static void td_fill_rect(TIGHT_DECODER *td, u_int32_t color)
{
  u_int32_t *fb_ptr;
  int x, y;

  if (td->fb != NULL) {
    /* Fill the first row */
    fb_ptr = &td->fb[td->rect_y * td->fb_stride + td->rect_x];
    for (x = 0; x < td->rect_w; x++) {
      fb_ptr[x] = color;
    }
    /* Copy the first row into all other rows */
    for (y = 1; y < td->rect_h; y++) {
      memcpy(&fb_ptr[y * td->fb_stride], fb_ptr, td->rect_w * 4);
    }
  }
}

static void td_draw_pixels_mono(TIGHT_DECODER *td, unsigned char *buf)
{
  u_int32_t *fb_ptr;
  unsigned char *read_ptr;
  int x, y, b, w;

  fb_ptr = &td->fb[td->rect_y * td->fb_stride + td->rect_x];
  read_ptr = buf;

  w = (td->rect_w + 7) / 8;
  for (y = 0; y < td->rect_h; y++) {
    for (x = 0; x < td->rect_w / 8; x++) {
      for (b = 7; b >= 0; b--) {
        *fb_ptr++ = td->palette[*read_ptr >> b & 1];
      }
      read_ptr++;
    }
    for (b = 7; b >= 8 - td->rect_w % 8; b--) {
      *fb_ptr++ = td->palette[*read_ptr >> b & 1];
    }
    if (td->rect_w & 0x07)
      read_ptr++;
    fb_ptr += td->fb_stride - td->rect_w;
  }
}

static void td_draw_pixels_indexed(TIGHT_DECODER *td, unsigned char *buf)
{
  u_int32_t *fb_ptr;
  unsigned char *read_ptr;
  int x, y;

  fb_ptr = &td->fb[td->rect_y * td->fb_stride + td->rect_x];
  read_ptr = buf;

  for (y = 0; y < td->rect_h; y++) {
    for (x = 0; x < td->rect_w; x++) {
      *fb_ptr++ = td->palette[*read_ptr++];
    }
    fb_ptr += td->fb_stride - td->rect_w;
  }
}

static void td_draw_pixels_rgb(TIGHT_DECODER *td, unsigned char *buf)
{
  u_int32_t *fb_ptr;
  unsigned char *read_ptr;
  int x, y;

  fb_ptr = &td->fb[td->rect_y * td->fb_stride + td->rect_x];
  read_ptr = buf;

  for (y = 0; y < td->rect_h; y++) {
    for (x = 0; x < td->rect_w; x++) {
      *fb_ptr++ = read_ptr[0] << 16 | read_ptr[1] << 8 | read_ptr[2];
      read_ptr += 3;
    }
    fb_ptr += td->fb_stride - td->rect_w;
  }
}

static void td_draw_pixels_gradient(TIGHT_DECODER *td, unsigned char *buf)
{
  u_int32_t *fb_ptr;
  unsigned char prev_row[2048*3];
  unsigned char this_row[2048*3];
  unsigned char pix[3];
  int est;
  int x, y, c;

  fb_ptr = &td->fb[td->rect_y * td->fb_stride + td->rect_x];

  memset(prev_row, 0, td->rect_w * 3);

  for (y = 0; y < td->rect_h; y++) {

    /* First pixel in a row */
    for (c = 0; c < 3; c++) {
      pix[c] = prev_row[c] + buf[y*td->rect_w*3+c];
      this_row[c] = pix[c];
    }
    *fb_ptr++ = pix[0] << 16 | pix[1] << 8 | pix[2];

    /* Remaining pixels of a row */
    for (x = 1; x < td->rect_w; x++) {
      for (c = 0; c < 3; c++) {
        est = (int)prev_row[x*3+c] + (int)pix[c] - (int)prev_row[(x-1)*3+c];
        if (est > 0xFF) {
          est = 0xFF;
        } else if (est < 0x00) {
          est = 0x00;
        }
        pix[c] = (unsigned char)est + buf[(y*td->rect_w+x)*3+c];
        this_row[x*3+c] = pix[c];
      }
      *fb_ptr++ = pix[0] << 16 | pix[1] << 8 | pix[2];
    }

    fb_ptr += td->fb_stride - td->rect_w;
    memcpy(prev_row, this_row, td->rect_w * 3);
  }
}

static void td_draw_pixels(TIGHT_DECODER *td, unsigned char *buf)
{
  if (td->fb != NULL) {
    if (td->filter_id == TIGHT_FILTER_PALETTE && td->num_colors <= 2) {
      /* Two-color palette, 1 bits per pixel */
      td_draw_pixels_mono(td, buf);
    } else if (td->filter_id == TIGHT_FILTER_PALETTE) {
      /* Up to 256 colors in the palette, 8 bits per pixel */
      td_draw_pixels_indexed(td, buf);
    } else if (td->filter_id == TIGHT_FILTER_GRADIENT) {
      /* RGB colors, "gradient" filter, 24 bits per pixel */
      td_draw_pixels_gradient(td, buf);
    } else {
      /* RGB colors, 24 bits per pixel */
      td_draw_pixels_rgb(td, buf);
    }
  }
}

/********************* Data Handling Functions **********************/

static int td_func_compctl(TIGHT_DECODER *td, unsigned char *buf)
{
  unsigned int comp_ctl;
  int i;

  /* Read compression control byte, split into two 4-bit parts  */
  comp_ctl = buf[0];
  td->zlib_reset_mask = comp_ctl & 0x0F;
  comp_ctl >>= 4;

  /* Reset zlib streams if requested */
  if (td->zlib_reset_mask != 0) {
    for (i = 0; i < 4; i++) {
      if ((td->zlib_reset_mask & (1 << i)) && td->zstream_active[i]) {
        if (inflateEnd(&td->zstream[i]) != Z_OK) {
          if (td->zstream[i].msg != NULL) {
            snprintf(td->error_msg, sizeof(td->error_msg),
                     "inflateEnd() failed: %s", td->zstream[i].msg);
          } else {
            snprintf(td->error_msg, sizeof(td->error_msg),
                     "inflateEnd() failed");
          }
          td->func = NULL;
          return -1;
        }
        td->zstream_active[i] = 0;
      }
    }
  }

  td->compressed_size = 0;
  td->zlib_stream_id = -1;

  if (comp_ctl == TIGHT_FILL) {
    td->func = &td_func_fill;
    return 3;
  }

  if (comp_ctl == TIGHT_JPEG) {
    snprintf(td->error_msg, sizeof(td->error_msg),
             "JPEG is not supported in this version");
    td->func = NULL;
    return -1;
  }

  if (comp_ctl > TIGHT_MAX_SUBENCODING) {
    snprintf(td->error_msg, sizeof(td->error_msg),
             "Invalid sub-encoding in Tight-encoded data");
    td->func = NULL;
    return -1;
  }

  /* "Basic" compression */
  td->zlib_stream_id = comp_ctl & 0x03;
  if (comp_ctl & TIGHT_EXPLICIT_FILTER) {
    td->func = &td_func_filter;
    return 1;
  } else {
    td->filter_id = TIGHT_FILTER_COPY;
    td->num_colors = 0;
    return td_dispatch_decoding(td);
  }
}

static int td_func_fill(TIGHT_DECODER *td, unsigned char *buf)
{
  u_int32_t color;

  td->num_colors = 1;
  color = buf[0] << 16 | buf[1] << 8 | buf[2];
  td_fill_rect(td, color);

  td->func = NULL;
  return 0;
}

static int td_func_filter(TIGHT_DECODER *td, unsigned char *buf)
{
  td->filter_id = buf[0];
  if (td->filter_id == TIGHT_FILTER_PALETTE) {
    td->func = &td_func_numcolors;
    return 1;
  } else if (td->filter_id == TIGHT_FILTER_COPY) {
    td->num_colors = 0;
    return td_dispatch_decoding(td);
  } else if (td->filter_id == TIGHT_FILTER_GRADIENT) {
    td->num_colors = -1;
    return td_dispatch_decoding(td);
  } else {
    snprintf(td->error_msg, sizeof(td->error_msg),
             "Unrecognized filter ID");
    td->func = NULL;
    return -1;
  }
}

static int td_func_numcolors(TIGHT_DECODER *td, unsigned char *buf)
{
  td->num_colors = buf[0] + 1;
  td->func = td_func_palette;
  return td->num_colors * 3;
}

static int td_func_palette(TIGHT_DECODER *td, unsigned char *buf)
{
  int i;

  for (i = 0; i < td->num_colors; i++) {
    td->palette[i] = (buf[i*3] << 16 | buf[i*3+1] << 8 | buf[i*3+2]);
  }
  return td_dispatch_decoding(td);
}

static int td_func_rawdata(TIGHT_DECODER *td, unsigned char *buf)
{
  td_draw_pixels(td, buf);
  td->func = NULL;
  return 0;
}

static int td_func_len1(TIGHT_DECODER *td, unsigned char *buf)
{
  td->compressed_size = buf[0] & 0x7F;
  if (buf[0] & 0x80) {
    td->func = &td_func_len2;
    return 1;
  } else {
    td->func = &td_func_zlibdata;
    return td->compressed_size;
  }
}

static int td_func_len2(TIGHT_DECODER *td, unsigned char *buf)
{
  td->compressed_size |= (buf[0] & 0x7F) << 7;
  if (buf[0] & 0x80) {
    td->func = &td_func_len3;
    return 1;
  } else {
    td->func = &td_func_zlibdata;
    return td->compressed_size;
  }
}

static int td_func_len3(TIGHT_DECODER *td, unsigned char *buf)
{
  td->compressed_size |= (buf[0] & 0xFF) << 14;
  td->func = &td_func_zlibdata;
  return td->compressed_size;
}

static int td_func_zlibdata(TIGHT_DECODER *td, unsigned char *buf)
{
  z_streamp zs;
  static unsigned char static_buf[1024];
  unsigned char *data = static_buf;
  int data_allocated = 0;
  int err;

  /* Initialize compression stream if needed */

  zs = &td->zstream[td->zlib_stream_id];
  if (!td->zstream_active[td->zlib_stream_id]) {
    zs->zalloc = Z_NULL;
    zs->zfree = Z_NULL;
    zs->opaque = Z_NULL;
    err = inflateInit(zs);
    if (err != Z_OK) {
      if (zs->msg != NULL) {
        snprintf(td->error_msg, sizeof(td->error_msg),
                 "inflateInit() failed: %s", zs->msg);
      } else {
        snprintf(td->error_msg, sizeof(td->error_msg),
                 "inflateInit() failed");
      }
      td->func = NULL;
      return -1;
    }
    td->zstream_active[td->zlib_stream_id] = 1;
  }

  /* Prepare a buffer to put decompressed data into */

  if (td->uncompressed_size > sizeof(static_buf)) {
    data = malloc(td->uncompressed_size);
    if (data == NULL) {
      snprintf(td->error_msg, sizeof(td->error_msg),
               "Error allocating memory");
      td->func = NULL;
      return -1;
    }
    data_allocated = 1;
  }

  /* Decompress the data */

  zs->next_in = buf;
  zs->avail_in = td->compressed_size;
  zs->next_out = data;
  zs->avail_out = td->uncompressed_size;

  err = inflate(zs, Z_SYNC_FLUSH);
  if (err != Z_OK && err != Z_STREAM_END) {
    if (zs->msg != NULL) {
      snprintf(td->error_msg, sizeof(td->error_msg),
               "inflate() failed: %s", zs->msg);
    } else {
      snprintf(td->error_msg, sizeof(td->error_msg),
               "inflate() failed: %d", err);
    }
    if (data_allocated) {
      free(data);
    }
    td->func = NULL;
    return -1;
  }

  if (zs->avail_out > 0) {
    snprintf(td->error_msg, sizeof(td->error_msg),
             "Decompressed data size is less than expected");
  }

  /* Handle decompressed pixels */

  td_draw_pixels(td, data);

  if (data_allocated) {
    free(data);
  }

  td->func = NULL;
  return 0;
}

/* VNC Reflector
 * Copyright (C) 2001-2002 HorizonLive.com, Inc.  All rights reserved.
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
 * $Id: decode_tight.c,v 1.1 2002/09/03 19:57:28 const Exp $
 * Decoding Tight-encoded rectangles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <zlib.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "logging.h"
#include "rect.h"
#include "host_io.h"

/*
 * File-local data.
 */

static FB_RECT s_rect;
static z_stream s_zstream[4];
static int s_zstream_active[4] = { 0, 0, 0, 0 };

static int s_stream_id;
static int s_filter_id;
static int s_num_colors;
static CARD32 s_palette[256];
static int s_compressed_size;

static void rf_host_tight_compctl(void);
static void rf_host_tight_fill(void);
static void rf_host_tight_filter(void);
static void rf_host_tight_numcolors(void);
static void rf_host_tight_palette(void);
static void rf_host_tight_raw(void);
static void rf_host_tight_indexed(void);
static void rf_host_tight_len1(void);
static void rf_host_tight_len2(void);
static void rf_host_tight_len3(void);
static void rf_host_tight_compressed(void);

static void tight_draw_truecolor_data(CARD8 *src);
static void tight_draw_indexed_data(CARD8 *src);

void setread_decode_tight(FB_RECT *r)
{
  s_rect = *r;
  aio_setread(rf_host_tight_compctl, NULL, sizeof(CARD8));
}

static void rf_host_tight_compctl(void)
{
  CARD8 comp_ctl;
  int stream_id;

  fbs_spool_data(cur_slot->readbuf, 1);

  /* Compression control byte */
  comp_ctl = cur_slot->readbuf[0];

  /* Flush zlib streams if we are told by the server to do so */
  for (stream_id = 0; stream_id < 4; stream_id++) {
    if ((comp_ctl & (1 < stream_id)) && s_zstream_active[stream_id]) {
      if (inflateEnd (&s_zstream[stream_id]) != Z_OK) {
        if (s_zstream[stream_id].msg != NULL) {
          log_write(LL_WARN, "inflateEnd() failed: %s",
                    s_zstream[stream_id].msg);
        } else {
          log_write(LL_WARN, "inflateEnd() failed");
        }
      }
      s_zstream_active[stream_id] = 0;
    }
  }
  comp_ctl &= 0xF0;             /* unset bits 3..0 */

  if (comp_ctl == RFB_TIGHT_FILL) {
    aio_setread(rf_host_tight_fill, NULL, 3);
  }
  else if (comp_ctl == RFB_TIGHT_JPEG) {
    /* FIXME: Handle the error correctly. */
    log_write(LL_WARN, "JPEG is not supported on host connections");
    aio_close(0);
    return;
  }
  else if (comp_ctl > RFB_TIGHT_MAX_SUBENCODING) {
    /* FIXME: Handle the error correctly. */
    log_write(LL_ERROR, "Invalid sub-encoding in Tight-encoded data");
    aio_close(0);
    return;
  }
  else {                        /* "basic" compression */
    s_stream_id = (comp_ctl >> 4) & 0x03;
    if (comp_ctl & RFB_TIGHT_EXPLICIT_FILTER) {
      aio_setread(rf_host_tight_filter, NULL, 1);
    } else {
      s_filter_id = RFB_TIGHT_FILTER_COPY;
      if (s_rect.w * s_rect.h * 3 < RFB_TIGHT_MIN_TO_COMPRESS) {
        aio_setread(rf_host_tight_raw, NULL, s_rect.w * s_rect.h * 3);
      } else {
        aio_setread(rf_host_tight_len1, NULL, 1);
      }
    }
  }
}

static void rf_host_tight_fill(void)
{
  CARD32 color;

  log_write(LL_WARN, "solid-color data"); /* DEBUG! */

  /* Note: cur_slot->readbuf is unsigned char[]. */
  color = (cur_slot->readbuf[0] << 16 |
           cur_slot->readbuf[1] << 8 |
           cur_slot->readbuf[2]);

  fill_fb_rect(&s_rect, color);
  fbupdate_rect_done();
}

/* FIXME: Implement "gradient" filter. */

static void rf_host_tight_filter(void)
{
  log_write(LL_WARN, "explicit filter"); /* DEBUG! */

  s_filter_id = cur_slot->readbuf[0];
  if (s_filter_id == RFB_TIGHT_FILTER_PALETTE) {
    aio_setread(rf_host_tight_numcolors, NULL, 1);
  } else {
    if (s_rect.w * s_rect.h * 3 < RFB_TIGHT_MIN_TO_COMPRESS) {
      aio_setread(rf_host_tight_raw, NULL, s_rect.w * s_rect.h * 3);
    } else {
      aio_setread(rf_host_tight_len1, NULL, 1);
    }
  }
}

static void rf_host_tight_numcolors(void)
{
  s_num_colors = cur_slot->readbuf[0] + 1;
  aio_setread(rf_host_tight_palette, NULL, s_num_colors * 3);

  log_write(LL_WARN, "num_colors = %d", s_num_colors); /* DEBUG! */
}

static void rf_host_tight_palette(void)
{
  int i, row_size, data_size;

  for (i = 0; i < s_num_colors; i++) {
    s_palette[i] = (cur_slot->readbuf[i*3] << 16 |
                    cur_slot->readbuf[i*3+1] << 8 |
                    cur_slot->readbuf[i*3+2]);
  }
  row_size = (s_num_colors <= 2) ? (s_rect.w + 7) / 8 : s_rect.w;
  data_size = s_rect.h * row_size;
  if (data_size < RFB_TIGHT_MIN_TO_COMPRESS) {
    aio_setread(rf_host_tight_indexed, NULL, data_size);
  } else {
    aio_setread(rf_host_tight_len1, NULL, 1);
  }
}

static void rf_host_tight_raw(void)
{
  log_write(LL_WARN, "raw data"); /* DEBUG! */

  tight_draw_truecolor_data(cur_slot->readbuf);
  fbupdate_rect_done();
}

static void rf_host_tight_indexed(void)
{
  tight_draw_indexed_data(cur_slot->readbuf);
  fbupdate_rect_done();
}

static void rf_host_tight_len1(void)
{
  s_compressed_size = cur_slot->readbuf[0] & 0x7F;
  if (cur_slot->readbuf[0] & 0x80) {
    aio_setread(rf_host_tight_len2, NULL, 1);
  } else {
    aio_setread(rf_host_tight_compressed, NULL, s_compressed_size);
  }
}

static void rf_host_tight_len2(void)
{
  s_compressed_size |= (cur_slot->readbuf[0] & 0x7F) << 7;
  if (cur_slot->readbuf[0] & 0x80) {
    aio_setread(rf_host_tight_len3, NULL, 1);
  } else {
    aio_setread(rf_host_tight_compressed, NULL, s_compressed_size);
  }
}

static void rf_host_tight_len3(void)
{
  s_compressed_size |= (cur_slot->readbuf[0] & 0x7F) << 14;
  aio_setread(rf_host_tight_compressed, NULL, s_compressed_size);
}

static void rf_host_tight_compressed(void)
{
  z_streamp zs;
  int row_size, uncompressed_size;
  CARD8 *buf;
  int err;

  log_write(LL_WARN, "compressed data, size = %d",
            s_compressed_size); /* DEBUG! */

  /* Initialize compression stream if needed */

  zs = &s_zstream[s_stream_id];
  if (!s_zstream_active[s_stream_id]) {
    zs->zalloc = Z_NULL;
    zs->zfree = Z_NULL;
    zs->opaque = Z_NULL;
    err = inflateInit(zs);
    if (err != Z_OK) {
      if (zs->msg != NULL) {
        log_write(LL_ERROR, "inflateInit() failed: %s", zs->msg);
      } else {
        log_write(LL_ERROR, "inflateInit() failed");
      }
      return;                   /* FIXME. */
    }
    s_zstream_active[s_stream_id] = 1;
  }

  /* Compute uncompressed data size and allocate a buffer */

  if (s_filter_id == RFB_TIGHT_FILTER_PALETTE) {
    /* FIXME: Don't compute second time, we already did it. */
    row_size = (s_num_colors <= 2) ? (s_rect.w + 7) / 8 : s_rect.w;
    uncompressed_size = s_rect.h * row_size;
  } else {
    uncompressed_size = s_rect.h * s_rect.w * 3;
  }
  buf = malloc(uncompressed_size);
  if (buf == NULL) {
    return;                     /* FIXME. */
  }

  /* Decompress the data */

  zs->next_in = cur_slot->readbuf;
  zs->avail_in = s_compressed_size;
  zs->next_out = buf;
  zs->avail_out = uncompressed_size;

  err = inflate(zs, Z_SYNC_FLUSH);
  if (err != Z_OK && err != Z_STREAM_END) {
    if (zs->msg != NULL) {
      log_write(LL_ERROR, "Inflate error: %s.\n", zs->msg);
    } else {
      log_write(LL_ERROR, "Inflate error: %d.\n", err);
    }
    free(buf);
    return;                     /* FIXME. */
  }

  /* FIXME: Check the amount of data decompressed. */

  if (s_filter_id == RFB_TIGHT_FILTER_PALETTE) {
    tight_draw_indexed_data(buf);
  } else {
    tight_draw_truecolor_data(buf);
  }

  free(buf);

  fbupdate_rect_done();
}

static void tight_draw_truecolor_data(CARD8 *src)
{
  int x, y;
  CARD32 *fb_ptr;
  CARD8 *read_ptr;

  fb_ptr = &g_framebuffer[s_rect.y * (int)g_fb_width + s_rect.x];
  read_ptr = src;

  for (y = 0; y < s_rect.h; y++) {
    for (x = 0; x < s_rect.w; x++) {
      *fb_ptr++ = read_ptr[0] << 16 | read_ptr[1] << 8 | read_ptr[2];
      read_ptr += 3;
    }
    fb_ptr += g_fb_width - s_rect.w;
  }
}

static void tight_draw_indexed_data(CARD8 *src)
{
  int x, y, b, w;
  CARD32 *fb_ptr;
  CARD8 *read_ptr;

  fb_ptr = &g_framebuffer[s_rect.y * (int)g_fb_width + s_rect.x];
  read_ptr = src;

  if (s_num_colors <= 2) {
    w = (s_rect.w + 7) / 8;
    for (y = 0; y < s_rect.h; y++) {
      for (x = 0; x < s_rect.w / 8; x++) {
        for (b = 7; b >= 0; b--) {
          *fb_ptr++ = s_palette[*read_ptr >> b & 1];
        }
        read_ptr++;
      }
      for (b = 7; b >= 8 - s_rect.w % 8; b--) {
        *fb_ptr++ = s_palette[*read_ptr >> b & 1];
      }
      if (s_rect.w & 0x07)
        read_ptr++;
      fb_ptr += g_fb_width - s_rect.w;
    }
  } else {
    for (y = 0; y < s_rect.h; y++) {
      for (x = 0; x < s_rect.w; x++) {
        *fb_ptr++ = s_palette[*read_ptr++];
      }
      fb_ptr += g_fb_width - s_rect.w;
    }
  }
}


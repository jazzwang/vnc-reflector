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
 * $Id: decode_hextile.c,v 1.1 2002/09/03 13:16:58 const Exp $
 * Decoding hextile-encoded rectangles.
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

static CARD8 hextile_subenc;
static CARD8 hextile_num_subrects;
static CARD32 hextile_bg;
static CARD32 hextile_fg;
static FB_RECT hextile_rect, hextile_tile;

static void rf_host_hextile_subenc(void);
static void rf_host_hextile_raw(void);
static void rf_host_hextile_hex(void);
static void rf_host_hextile_subrects(void);

static void hextile_fill_tile(void);
static void hextile_fill_subrect(CARD8 pos, CARD8 dim);
static void hextile_next_tile(void);

/* FIXME: Evil/buggy servers can overflow this buffer */
static CARD32 hextile_buf[256 + 2];

void setread_decode_hextile(FB_RECT *r)
{
  hextile_rect = *r;

  hextile_tile.x = hextile_rect.x;
  hextile_tile.y = hextile_rect.y;
  hextile_tile.w = (hextile_rect.w < 16) ? hextile_rect.w : 16;
  hextile_tile.h = (hextile_rect.h < 16) ? hextile_rect.h : 16;
  aio_setread(rf_host_hextile_subenc, NULL, sizeof(CARD8));
}

static void rf_host_hextile_subenc(void)
{
  int data_size;

  /* Copy data for saving in a file if necessary */
  fbs_spool_data(cur_slot->readbuf, 1);

  hextile_subenc = cur_slot->readbuf[0];
  if (hextile_subenc & RFB_HEXTILE_RAW) {
    data_size = hextile_tile.w * hextile_tile.h * sizeof(CARD32);
    aio_setread(rf_host_hextile_raw, hextile_buf, data_size);
    return;
  }
  data_size = 0;
  if (hextile_subenc & RFB_HEXTILE_BG_SPECIFIED) {
    data_size += sizeof(CARD32);
  } else {
    hextile_fill_tile();
  }
  if (hextile_subenc & RFB_HEXTILE_FG_SPECIFIED)
    data_size += sizeof(CARD32);
  if (hextile_subenc & RFB_HEXTILE_ANY_SUBRECTS)
    data_size += sizeof(CARD8);
  if (data_size) {
    aio_setread(rf_host_hextile_hex, hextile_buf, data_size);
  } else {
    hextile_next_tile();
  }
}

static void rf_host_hextile_raw(void)
{
  int row;
  CARD32 *from_ptr;
  CARD32 *fb_ptr;

  fbs_spool_data(hextile_buf, hextile_tile.w * hextile_tile.h * sizeof(CARD32));

  from_ptr = hextile_buf;
  fb_ptr = &g_framebuffer[hextile_tile.y * (int)g_fb_width + hextile_tile.x];

  /* Just copy raw data into the framebuffer */
  for (row = 0; row < hextile_tile.h; row++) {
    memcpy(fb_ptr, from_ptr, hextile_tile.w * sizeof(CARD32));
    from_ptr += hextile_tile.w;
    fb_ptr += g_fb_width;
  }

  hextile_next_tile();
}

static void rf_host_hextile_hex(void)
{
  CARD32 *from_ptr = hextile_buf;
  int data_size;

  /* Get background and foreground colors */
  if (hextile_subenc & RFB_HEXTILE_BG_SPECIFIED) {
    hextile_bg = *from_ptr++;
    hextile_fill_tile();
  }
  if (hextile_subenc & RFB_HEXTILE_FG_SPECIFIED) {
    hextile_fg = *from_ptr++;
  }

  if (hextile_subenc & RFB_HEXTILE_ANY_SUBRECTS) {
    fbs_spool_data(hextile_buf, (from_ptr - hextile_buf) * sizeof(CARD32) + 1);
    hextile_num_subrects = *((CARD8 *)from_ptr);
    if (hextile_subenc & RFB_HEXTILE_SUBRECTS_COLOURED) {
      data_size = 6 * (unsigned int)hextile_num_subrects;
    } else {
      data_size = 2 * (unsigned int)hextile_num_subrects;
    }
    if (data_size > 0) {
      aio_setread(rf_host_hextile_subrects, NULL, data_size);
      return;
    }
  } else {
    fbs_spool_data(hextile_buf, (from_ptr - hextile_buf) * sizeof(CARD32));
  }

  hextile_next_tile();
}

/* FIXME: Not as efficient as it could be. */
static void rf_host_hextile_subrects(void)
{
  CARD8 *ptr;
  CARD8 pos, dim;
  int i;

  ptr = cur_slot->readbuf;

  if (hextile_subenc & RFB_HEXTILE_SUBRECTS_COLOURED) {
    fbs_spool_data(ptr, hextile_num_subrects * 6);
    for (i = 0; i < (int)hextile_num_subrects; i++) {
      memcpy(&hextile_fg, ptr, sizeof(hextile_fg));
      ptr += sizeof(hextile_fg);
      pos = *ptr++;
      dim = *ptr++;
      hextile_fill_subrect(pos, dim);
    }
  } else {
    fbs_spool_data(ptr, hextile_num_subrects * 2);
    for (i = 0; i < (int)hextile_num_subrects; i++) {
      pos = *ptr++;
      dim = *ptr++;
      hextile_fill_subrect(pos, dim);
    }
  }

  hextile_next_tile();
}

/********************/
/* Helper functions */
/********************/

static void hextile_fill_tile(void)
{
  int x, y;
  CARD32 *fb_ptr;

  fb_ptr = &g_framebuffer[hextile_tile.y * (int)g_fb_width + hextile_tile.x];

  /* Fill first row */
  for (x = 0; x < hextile_tile.w; x++)
    fb_ptr[x] = hextile_bg;

  /* Copy first row into other rows */
  for (y = 1; y < hextile_tile.h; y++)
    memcpy(&fb_ptr[y * g_fb_width], fb_ptr, hextile_tile.w * sizeof(CARD32));
}

static void hextile_fill_subrect(CARD8 pos, CARD8 dim)
{
  int pos_x, pos_y, dim_w, dim_h;
  int x, y, skip;
  CARD32 *fb_ptr;

  pos_x = pos >> 4 & 0x0F;
  pos_y = pos & 0x0F;
  fb_ptr = &g_framebuffer[(hextile_tile.y + pos_y) * (int)g_fb_width +
                          (hextile_tile.x + pos_x)];

  /* Optimization for 1x1 subrects */
  if (dim == 0) {
    *fb_ptr = hextile_fg;
    return;
  }

  /* Actually, we should add 1 to both dim_h and dim_w. */
  dim_w = dim >> 4 & 0x0F;
  dim_h = dim & 0x0F;
  skip = g_fb_width - (dim_w + 1);

  for (y = 0; y <= dim_h; y++) {
    for (x = 0; x <= dim_w; x++) {
      *fb_ptr++ = hextile_fg;
    }
    fb_ptr += skip;
  }
}

static void hextile_next_tile(void)
{
  if (hextile_tile.x + 16 < hextile_rect.x + hextile_rect.w) {
    /* Next tile in the same row */
    hextile_tile.x += 16;
    if (hextile_tile.x + 16 < hextile_rect.x + hextile_rect.w)
      hextile_tile.w = 16;
    else
      hextile_tile.w = hextile_rect.x + hextile_rect.w - hextile_tile.x;
  } else if (hextile_tile.y + 16 < hextile_rect.y + hextile_rect.h) {
    /* First tile in the next row */
    hextile_tile.x = hextile_rect.x;
    hextile_tile.w = (hextile_rect.w < 16) ? hextile_rect.w : 16;
    hextile_tile.y += 16;
    if (hextile_tile.y + 16 < hextile_rect.y + hextile_rect.h)
      hextile_tile.h = 16;
    else
      hextile_tile.h = hextile_rect.y + hextile_rect.h - hextile_tile.y;
  } else {
    fbupdate_rect_done();       /* No more tiles */
    return;
  }
  aio_setread(rf_host_hextile_subenc, NULL, 1);
}


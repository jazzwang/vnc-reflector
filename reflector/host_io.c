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
 * $Id: host_io.c,v 1.36 2002/07/10 15:46:38 const Exp $
 * Asynchronous interaction with VNC host.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <zlib.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "logging.h"
#include "active.h"
#include "rect.h"
#include "translate.h"
#include "client_io.h"
#include "host_connect.h"
#include "host_io.h"
#include "encode.h"

static void host_really_activate(AIO_SLOT *slot);
static void fn_host_pass_newfbsize(AIO_SLOT *slot);

static void rf_host_msg(void);

static void rf_host_fbupdate_hdr(void);
static void rf_host_fbupdate_recthdr(void);
static void rf_host_fbupdate_raw(void);
static void rf_host_copyrect(void);
static void rf_host_hextile_subenc(void);
static void rf_host_hextile_raw(void);
static void rf_host_hextile_hex(void);
static void rf_host_hextile_subrects(void);
static void fn_host_add_client_rect(AIO_SLOT *slot);

static void rf_host_colormap_hdr(void);
static void rf_host_colormap_data(void);

static void rf_host_cuttext_hdr(void);
static void rf_host_cuttext_data(void);
static void fn_host_pass_cuttext(AIO_SLOT *slot);

static void fbs_open_file(HOST_SLOT *hs);
static void fbs_write_data(void *buf, size_t len);
static void fbs_close_file(void);

static void hextile_fill_tile(void);
static void hextile_fill_subrect(CARD8 pos, CARD8 dim);
static void hextile_next_tile(void);

static void reset_framebuffer(void);
static void fbupdate_rect_done(void);
static void request_update(int incr);

/*
 * Implementation
 */

static AIO_SLOT *s_host_slot = NULL;
static AIO_SLOT *s_new_slot = NULL;

static char *s_fbs_prefix = NULL;
static int s_join_sessions;
static int s_fbs_idx = 0;
static FILE *s_fbs_fp = NULL;
static CARD8 *s_fbs_buffer, *s_fbs_buffer_ptr;
static struct timeval s_fbs_start_time, s_fbs_time;
static struct timezone s_fbs_timezone;
static CARD16 s_fbs_fb_width, s_fbs_fb_height;


void host_set_fbs_prefix(char *fbs_prefix, int join_sessions)
{
  s_fbs_prefix = fbs_prefix;
  s_join_sessions = join_sessions;
  s_fbs_idx = 0;
}

/* Prepare host I/O slot for operating in main protocol phase */
void host_activate(void)
{
  if (s_host_slot == NULL) {
    /* Just activate */
    host_really_activate(cur_slot);
  } else {
    /* Let close hook do the work */
    s_new_slot = cur_slot;
    aio_close_other(s_host_slot, 0);
  }
}

/* On-close hook */
void host_close_hook(void)
{

  if (cur_slot->type == TYPE_HOST_ACTIVE_SLOT) {
    /* Close session file if open  */
    if (s_fbs_fp != NULL)
      fbs_close_file();

    /* Erase framebuffer contents, invalidate cache */
    /* FIXME: Don't reset if there is a new connection, so the
       framebuffer (of its new size) would be changed anyway? */
    reset_framebuffer();

    /* No active slot exist */
    s_host_slot = NULL;
  }

  if (cur_slot->errread_f) {
    if (cur_slot->io_errno) {
      log_write(LL_ERROR, "Host I/O error, read: %s",
                strerror(cur_slot->io_errno));
    } else {
      log_write(LL_ERROR, "Host I/O error, read");
    }
  } else if (cur_slot->errwrite_f) {
    if (cur_slot->io_errno) {
      log_write(LL_ERROR, "Host I/O error, write: %s",
                strerror(cur_slot->io_errno));
    } else {
      log_write(LL_ERROR, "Host I/O error, write");
    }
  } else if (cur_slot->errio_f) {
    log_write(LL_ERROR, "Host I/O error");
  }

  if (s_new_slot == NULL) {
    log_write(LL_WARN, "Closing connection to host");
    /* Exit event loop if framebuffer does not exist yet. */
    if (g_framebuffer == NULL)
      aio_close(1);
    remove_active_file();
  } else {
    log_write(LL_INFO, "Closing previous connection to host");
    host_really_activate(s_new_slot);
    s_new_slot = NULL;
  }
}

static void host_really_activate(AIO_SLOT *slot)
{
  AIO_SLOT *saved_slot = cur_slot;
  HOST_SLOT *hs = (HOST_SLOT *)slot;

  log_write(LL_MSG, "Activating new host connection");
  slot->type = TYPE_HOST_ACTIVE_SLOT;
  s_host_slot = slot;

  write_active_file();

  /* Allocate the framebuffer or extend its dimensions if necessary */
  if (!alloc_framebuffer(hs->fb_width, hs->fb_height)) {
    aio_close(1);
    return;
  }

  /* Set default desktop geometry for new client connections */
  g_screen_info.width = hs->fb_width;
  g_screen_info.height = hs->fb_height;

  /* If requested, open file to save this session and write the header */
  if (s_fbs_prefix != NULL)
    fbs_open_file(hs);

  cur_slot = slot;

  /* Request initial screen contents */
  log_write(LL_DETAIL, "Requesting full framebuffer update");
  request_update(0);
  aio_setread(rf_host_msg, NULL, 1);

  /* Notify clients about desktop geometry change */
  aio_walk_slots(fn_host_pass_newfbsize, TYPE_CL_SLOT);

  cur_slot = saved_slot;
}

/*
 * Inform a client about new desktop geometry.
 */

static void fn_host_pass_newfbsize(AIO_SLOT *slot)
{
  HOST_SLOT *hs = (HOST_SLOT *)cur_slot;
  FB_RECT r;

  r.enc = RFB_ENCODING_NEWFBSIZE;
  r.x = r.y = 0;
  r.w = hs->fb_width;
  r.h = hs->fb_height;
  fn_client_add_rect(slot, &r);
}

/***************************/
/* Processing RFB messages */
/***************************/

static void rf_host_msg(void)
{
  int msg_id;

  msg_id = (int)cur_slot->readbuf[0] & 0xFF;
  switch(msg_id) {
  case 0:                       /* FramebufferUpdate */
    aio_setread(rf_host_fbupdate_hdr, NULL, 3);
    break;
  case 1:                       /* SetColourMapEntries */
    aio_setread(rf_host_colormap_hdr, NULL, 5);
    break;
  case 2:                       /* Bell */
    log_write(LL_DETAIL, "Received Bell message from host");
    if (s_fbs_fp != NULL) {
      fbs_write_data(cur_slot->readbuf, 1);
    }
    aio_setread(rf_host_msg, NULL, 1);
    break;
  case 3:                       /* ServerCutText */
    aio_setread(rf_host_cuttext_hdr, NULL, 7);
    break;
  default:
    log_write(LL_ERROR, "Unknown server message type: %d", msg_id);
    aio_close(0);
  }
}

/********************************/
/* Handling framebuffer updates */
/********************************/

/* FIXME: Add state variables to the AIO_SLOT structure clone. */

static CARD16 rect_count;
static FB_RECT cur_rect;
static CARD16 rect_cur_row;

static CARD8 hextile_subenc;
static CARD8 hextile_num_subrects;
static CARD32 hextile_bg;
static CARD32 hextile_fg;
static FB_RECT hextile_rect;

/* FIXME: Evil/buggy servers can overflow this buffer */
static CARD32 hextile_buf[256 + 2];

static void rf_host_fbupdate_hdr(void)
{
  CARD8 hdr_buf[4];

  rect_count = buf_get_CARD16(&cur_slot->readbuf[1]);

  if (rect_count == 0xFFFF) {
    log_write(LL_DETAIL, "Receiving framebuffer update");
  } else {
    log_write(LL_DETAIL, "Receiving framebuffer update, %d rectangle(s)",
              rect_count);
  }

  if (s_fbs_fp != NULL) {
    hdr_buf[0] = 0;
    memcpy(&hdr_buf[1], cur_slot->readbuf, 3);
    fbs_write_data(hdr_buf, 4);
  }

  if (rect_count) {
    aio_setread(rf_host_fbupdate_recthdr, NULL, 12);
  } else {
    log_write(LL_DEBUG, "Requesting incremental framebuffer update");
    request_update(1);
    aio_setread(rf_host_msg, NULL, 1);
  }
}

static void rf_host_fbupdate_recthdr(void)
{
  HOST_SLOT *hs = (HOST_SLOT *)cur_slot;

  cur_rect.x = buf_get_CARD16(cur_slot->readbuf);
  cur_rect.y = buf_get_CARD16(&cur_slot->readbuf[2]);
  cur_rect.w = buf_get_CARD16(&cur_slot->readbuf[4]);
  cur_rect.h = buf_get_CARD16(&cur_slot->readbuf[6]);
  cur_rect.enc = buf_get_CARD32(&cur_slot->readbuf[8]);

  /* Copy data for saving in a file if necessary */
  if (s_fbs_fp != NULL) {
    memcpy(s_fbs_buffer, cur_slot->readbuf, 12);
    s_fbs_buffer_ptr = s_fbs_buffer + 12;
  }

  if (cur_rect.h == 0 || cur_rect.w == 0) {
    log_write(LL_WARN, "Zero-size rectangle %dx%d at %d,%d (ignoring)",
              (int)cur_rect.w, (int)cur_rect.h,
              (int)cur_rect.x, (int)cur_rect.y);
    /* Save data in a file if necessary */
    if (s_fbs_fp != NULL) {
      fbs_write_data(s_fbs_buffer, s_fbs_buffer_ptr - s_fbs_buffer);
    }
    fbupdate_rect_done();
    return;
  }

  /* Handle NewFBSize "encoding" first, as a special case */
  if (cur_rect.enc == RFB_ENCODING_NEWFBSIZE) {
    log_write(LL_INFO, "New host desktop geometry: %dx%d",
              (int)cur_rect.w, (int)cur_rect.h);
    g_screen_info.width = hs->fb_width = cur_rect.w;
    g_screen_info.height = hs->fb_height = cur_rect.h;

    /* Reallocate the framebuffer if necessary */
    if (!alloc_framebuffer(hs->fb_width, hs->fb_height)) {
      aio_close(1);
      return;
    }

    /* Respect the specification */
    cur_rect.x = cur_rect.y = 0;

    /* NewFBSize is always the last rectangle regardless of rect_count */
    rect_count = 1;
    fbupdate_rect_done();
    return;
  }

  /* Prevent overflow of the framebuffer */
  if (cur_rect.x >= g_fb_width || cur_rect.x + cur_rect.w > g_fb_width ||
      cur_rect.y >= g_fb_height || cur_rect.y + cur_rect.h > g_fb_height) {
    log_write(LL_ERROR, "Rectangle out of framebuffer bounds: %dx%d at %d,%d",
              (int)cur_rect.w, (int)cur_rect.h,
              (int)cur_rect.x, (int)cur_rect.y);
    aio_close(0);
    return;
  }

  /* Ok, now the rectangle seems correct */
  log_write(LL_DEBUG, "Receiving rectangle %dx%d at %d,%d",
            (int)cur_rect.w, (int)cur_rect.h,
            (int)cur_rect.x, (int)cur_rect.y);

  switch(cur_rect.enc) {
  case RFB_ENCODING_RAW:
    log_write(LL_DEBUG, "Receiving raw data, expecting %d byte(s)",
              cur_rect.w * cur_rect.h * sizeof(CARD32));
    rect_cur_row = 0;
    aio_setread(rf_host_fbupdate_raw,
                &g_framebuffer[cur_rect.y * (int)g_fb_width +
                               cur_rect.x],
                cur_rect.w * sizeof(CARD32));
    break;
  case RFB_ENCODING_HEXTILE:
    log_write(LL_DEBUG, "Receiving Hextile-encoded data");
    hextile_rect.x = cur_rect.x;
    hextile_rect.y = cur_rect.y;
    hextile_rect.w = (cur_rect.w < 16) ? cur_rect.w : 16;
    hextile_rect.h = (cur_rect.h < 16) ? cur_rect.h : 16;
    aio_setread(rf_host_hextile_subenc, NULL, sizeof(CARD8));
    break;
  case RFB_ENCODING_COPYRECT:
    log_write(LL_DEBUG, "Receiving CopyRect instruction");
    aio_setread(rf_host_copyrect, NULL, 4);
    break;
  default:
    log_write(LL_ERROR, "Unknown encoding: 0x%08lX",
              (unsigned long)cur_rect.enc);
    aio_close(0);
  }
}

static void rf_host_fbupdate_raw(void)
{
  int idx;

  /* Save data in a file if necessary */
  if (s_fbs_fp != NULL) {
    memcpy(s_fbs_buffer_ptr, cur_slot->readbuf, cur_rect.w * sizeof(CARD32));
    s_fbs_buffer_ptr += cur_rect.w * sizeof(CARD32);
  }

  if (++rect_cur_row < cur_rect.h) {
    /* Read next row */
    idx = (cur_rect.y + rect_cur_row) * (int)g_fb_width + cur_rect.x;
    aio_setread(rf_host_fbupdate_raw, &g_framebuffer[idx],
                cur_rect.w * sizeof(CARD32));
  } else {
    /* Done with this rectangle */
    fbupdate_rect_done();
  }
}

static void rf_host_copyrect(void)
{
  CARD32 *src_ptr;
  CARD32 *dst_ptr;
  int width = (int)g_fb_width;
  int row;

  /* Save data in a file if necessary */
  if (s_fbs_fp != NULL) {
    memcpy(s_fbs_buffer_ptr, cur_slot->readbuf, 4);
    s_fbs_buffer_ptr += 4;
  }

  cur_rect.src_x = buf_get_CARD16(cur_slot->readbuf);
  cur_rect.src_y = buf_get_CARD16(&cur_slot->readbuf[2]);

  if ( cur_rect.src_x >= g_fb_width ||
       cur_rect.src_x + cur_rect.w > g_fb_width ||
       cur_rect.src_y >= g_fb_height ||
       cur_rect.src_y + cur_rect.h > g_fb_height ) {
    log_write(LL_WARN,
              "CopyRect from outside of the framebuffer: %dx%d from %d,%d",
              (int)cur_rect.w, (int)cur_rect.h,
              (int)cur_rect.src_x, (int)cur_rect.src_y);
    return;
  }

  if (cur_rect.src_y > cur_rect.y) {
    /* Copy rows starting from top */
    src_ptr = &g_framebuffer[cur_rect.src_y * width + cur_rect.src_x];
    dst_ptr = &g_framebuffer[cur_rect.y * width + cur_rect.x];
    for (row = 0; row < cur_rect.h; row++) {
      memmove(dst_ptr, src_ptr, cur_rect.w * sizeof(CARD32));
      src_ptr += width;
      dst_ptr += width;
    }
  } else {
    /* Copy rows starting from bottom */
    src_ptr = &g_framebuffer[(cur_rect.src_y + cur_rect.h - 1) * width +
                             cur_rect.src_x];
    dst_ptr = &g_framebuffer[(cur_rect.y + cur_rect.h - 1) * width +
                             cur_rect.x];
    for (row = 0; row < cur_rect.h; row++) {
      memmove(dst_ptr, src_ptr, cur_rect.w * sizeof(CARD32));
      src_ptr -= width;
      dst_ptr -= width;
    }
  }

  fbupdate_rect_done();
}

static void rf_host_hextile_subenc(void)
{
  int data_size;

  /* Copy data for saving in a file if necessary */
  if (s_fbs_fp != NULL)
    *s_fbs_buffer_ptr++ = cur_slot->readbuf[0];

  hextile_subenc = cur_slot->readbuf[0];
  if (hextile_subenc & RFB_HEXTILE_RAW) {
    data_size = hextile_rect.w * hextile_rect.h * sizeof(CARD32);
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

  /* Copy data for saving in a file if necessary */
  if (s_fbs_fp != NULL) {
    memcpy(s_fbs_buffer_ptr, hextile_buf,
           hextile_rect.w * hextile_rect.h * sizeof(CARD32));
    s_fbs_buffer_ptr += hextile_rect.w * hextile_rect.h * sizeof(CARD32);
  }

  from_ptr = hextile_buf;
  fb_ptr = &g_framebuffer[hextile_rect.y * (int)g_fb_width + hextile_rect.x];

  /* Just copy raw data into the framebuffer */
  for (row = 0; row < hextile_rect.h; row++) {
    memcpy(fb_ptr, from_ptr, hextile_rect.w * sizeof(CARD32));
    from_ptr += hextile_rect.w;
    fb_ptr += g_fb_width;
  }

  hextile_next_tile();
}

static void rf_host_hextile_hex(void)
{
  CARD32 *from_ptr = hextile_buf;
  int data_size, size;

  /* Get background and foreground colors */
  if (hextile_subenc & RFB_HEXTILE_BG_SPECIFIED) {
    hextile_bg = *from_ptr++;
    hextile_fill_tile();
  }
  if (hextile_subenc & RFB_HEXTILE_FG_SPECIFIED) {
    hextile_fg = *from_ptr++;
  }

  /* Copy data for saving in a file if necessary */
  if (s_fbs_fp != NULL) {
    size = (from_ptr - hextile_buf) * sizeof(CARD32);
    if (hextile_subenc & RFB_HEXTILE_ANY_SUBRECTS)
      size++;
    memcpy(s_fbs_buffer_ptr, hextile_buf, size);
    s_fbs_buffer_ptr += size;
  }

  if (hextile_subenc & RFB_HEXTILE_ANY_SUBRECTS) {
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
  }

  hextile_next_tile();
}

/* FIXME: Not as efficient as it could be. */
static void rf_host_hextile_subrects(void)
{
  CARD8 *ptr;
  CARD8 pos, dim;
  int i, size;

  /* Copy data for saving in a file if necessary */
  if (s_fbs_fp != NULL) {
    size = (int)hextile_num_subrects;
    size *= (hextile_subenc & RFB_HEXTILE_SUBRECTS_COLOURED) ? 6 : 2;
    memcpy(s_fbs_buffer_ptr, cur_slot->readbuf, size);
    s_fbs_buffer_ptr += size;
  }

  ptr = cur_slot->readbuf;

  if (hextile_subenc & RFB_HEXTILE_SUBRECTS_COLOURED) {
    for (i = 0; i < (int)hextile_num_subrects; i++) {
      memcpy(&hextile_fg, ptr, sizeof(hextile_fg));
      ptr += sizeof(hextile_fg);
      pos = *ptr++;
      dim = *ptr++;
      hextile_fill_subrect(pos, dim);
    }
  } else {
    for (i = 0; i < (int)hextile_num_subrects; i++) {
      pos = *ptr++;
      dim = *ptr++;
      hextile_fill_subrect(pos, dim);
    }
  }

  hextile_next_tile();
}

static void fn_host_add_client_rect(AIO_SLOT *slot)
{
  fn_client_add_rect(slot, &cur_rect);
}

/*****************************************/
/* Handling SetColourMapEntries messages */
/*****************************************/

static void rf_host_colormap_hdr(void)
{
  CARD16 num_colors;

  log_write(LL_WARN, "Ignoring SetColourMapEntries message");

  num_colors = buf_get_CARD16(&cur_slot->readbuf[3]);
  if (num_colors > 0)
    aio_setread(rf_host_colormap_data, NULL, num_colors * 6);
  else
    aio_setread(rf_host_msg, NULL, 1);
}

static void rf_host_colormap_data(void)
{
  /* Nothing to do with colormap */
  aio_setread(rf_host_msg, NULL, 1);
}

/***********************************/
/* Handling ServerCutText messages */
/***********************************/

/* FIXME: Add state variables to the AIO_SLOT structure clone. */
static size_t cut_len;
static CARD8 *cut_text;

static void rf_host_cuttext_hdr(void)
{
  log_write(LL_DETAIL,
            "Receiving ServerCutText message from host, %lu byte(s)",
            (unsigned long)cut_len);

  cut_len = (size_t)buf_get_CARD32(&cur_slot->readbuf[3]);
  if (cut_len > 0)
    aio_setread(rf_host_cuttext_data, NULL, cut_len);
  else
    rf_host_cuttext_data();
}

static void rf_host_cuttext_data(void)
{
  cut_text = cur_slot->readbuf;
  aio_walk_slots(fn_host_pass_cuttext, TYPE_CL_SLOT);
  aio_setread(rf_host_msg, NULL, 1);
}

static void fn_host_pass_cuttext(AIO_SLOT *slot)
{
  fn_client_send_cuttext(slot, cut_text, cut_len);
}

/*************************************/
/* Functions called from client_io.c */
/*************************************/

/* FIXME: Function naming. Have to invent consistent naming rules. */

void pass_msg_to_host(CARD8 *msg, size_t len)
{
  AIO_SLOT *saved_slot = cur_slot;

  if (s_host_slot != NULL) {
    cur_slot = s_host_slot;
    aio_write(NULL, msg, len);
    cur_slot = saved_slot;
  }
}

void pass_cuttext_to_host(CARD8 *text, size_t len)
{
  AIO_SLOT *saved_slot = cur_slot;
  CARD8 client_cuttext_hdr[8] = {
    6, 0, 0, 0, 0, 0, 0, 0
  };

  if (s_host_slot != NULL) {
    buf_put_CARD32(&client_cuttext_hdr[4], (CARD32)len);

    cur_slot = s_host_slot;
    aio_write(NULL, client_cuttext_hdr, sizeof(client_cuttext_hdr));
    aio_write(NULL, text, len);
    cur_slot = saved_slot;
  }
}

/*****************************************/
/* Saving "framebuffer streams" in files */
/*****************************************/

static void fbs_open_file(HOST_SLOT *hs)
{
  int max_rect_size, w, h;
  CARD32 len;
  char fname[256];
  char fbs_header[256];
  char fbs_desktop_name[] = "RFB session archived by VNC Reflector";
  CARD8 fbs_newfbsize_msg[16] = {
    0, 0, 0, 1,                 /* msg header */
    0, 0, 0, 0, 0, 0, 0, 0,     /* x, y, w, h */
    0, 0, 0, 0                  /* encoding   */
  };

  /* Increment session number */
  s_fbs_idx++;
  if (s_fbs_idx > 999)
    s_fbs_idx = 0;

  /* Prepare file name optionally suffixed with session number */
  len = strlen(s_fbs_prefix);
  if (len + 4 > 255) {
    log_write(LL_WARN, "FBS filename prefix too long");
    s_fbs_prefix = NULL;
    return;
  }
  if (!s_join_sessions) {
    sprintf(fname, "%s.%03d", s_fbs_prefix, s_fbs_idx);
  } else {
    strcpy(fname, s_fbs_prefix);
  }

  if (!s_join_sessions || s_fbs_idx == 1) {

    /* Open the file */
    s_fbs_fp = fopen(fname, "w");
    if (s_fbs_fp == NULL) {
      log_write(LL_WARN, "Could not open FBS file for writing");
      s_fbs_prefix = NULL;
      return;
    }
    log_write(LL_MSG, "Opened FBS file for writing: %s", fname);

    /* Remember current time */
    gettimeofday(&s_fbs_start_time, &s_fbs_timezone);

    /* Write file header */
    if (fwrite("FBS 001.000\n", 1, 12, s_fbs_fp) != 12) {
      log_write(LL_WARN, "Could not write FBS file header");
      fclose(s_fbs_fp);
      s_fbs_fp = NULL;
      return;
    }

    /* Prepare stream header data */
    memcpy(fbs_header, "RFB 003.003\n", 12);
    buf_put_CARD32(&fbs_header[12], 1);
    buf_put_CARD16(&fbs_header[16], hs->fb_width);
    buf_put_CARD16(&fbs_header[18], hs->fb_height);
    buf_put_pixfmt(&fbs_header[20], &g_screen_info.pixformat);
    if (!s_join_sessions) {
      len = g_screen_info.name_length;
      if (len > 192)
        len = 192;
      buf_put_CARD32(&fbs_header[36], len);
      memcpy(&fbs_header[40], g_screen_info.name, len);
    } else {
      len = sizeof(fbs_desktop_name) - 1;
      buf_put_CARD32(&fbs_header[36], len);
      memcpy(&fbs_header[40], fbs_desktop_name, len);
    }

    /* Write stream header data */
    fbs_write_data(fbs_header, 40 + len);

  } else {                      /* Next session in the same file */

    /* Open the file */
    s_fbs_fp = fopen(fname, "a");
    if (s_fbs_fp == NULL) {
      log_write(LL_WARN, "Could not re-open FBS file for writing");
      s_fbs_prefix = NULL;
      return;
    }
    log_write(LL_MSG, "Re-opened FBS file for writing: %s", fname);

    /* Write NewFBSize rect if framebuffer dimensions have changed */
    if (s_fbs_fb_width != hs->fb_width || s_fbs_fb_height != hs->fb_height) {
      buf_put_CARD16(&fbs_newfbsize_msg[8], hs->fb_width);
      buf_put_CARD16(&fbs_newfbsize_msg[10], hs->fb_height);
      buf_put_CARD32(&fbs_newfbsize_msg[12], RFB_ENCODING_NEWFBSIZE);
      fbs_write_data(fbs_newfbsize_msg, sizeof(fbs_newfbsize_msg));
    }

  }

  /* Allocate memory to hold one rectangle of maximum size */
  if (s_fbs_fp != NULL) {
    w = (int)hs->fb_width;
    h = (int)hs->fb_height;
    max_rect_size = 12 + (w * h * 4) + ((w + 15) / 16) * ((h + 15) / 16);
    s_fbs_buffer = malloc(max_rect_size);
    if (s_fbs_buffer == NULL) {
      log_write(LL_WARN, "Memory allocation error, closing FBS file");
      fclose(s_fbs_fp);
      s_fbs_fp = NULL;
    } else {
      log_write(LL_DETAIL, "Allocated buffer to cache FBS data, %d bytes",
                max_rect_size);
      s_fbs_buffer_ptr = s_fbs_buffer;
    }
  }

  /* Remember framebuffer dimensions */
  if (s_fbs_fp != NULL) {
    s_fbs_fb_width = hs->fb_width;
    s_fbs_fb_height = hs->fb_height;
  }
}

static void fbs_write_data(void *buf, size_t len)
{
  CARD8 data_size_buf[4];
  CARD8 timestamp_buf[8];
  CARD32 timestamp;
  int padding;

  /* Calculate current timestamp */
  gettimeofday(&s_fbs_time, &s_fbs_timezone);
  if (s_fbs_time.tv_sec < s_fbs_start_time.tv_sec) {
    /* FIXME: not sure if this is correct. */
    s_fbs_time.tv_sec += 60 * 60 * 24;
  }
  timestamp = (CARD32)((s_fbs_time.tv_sec - s_fbs_start_time.tv_sec) * 1000 +
                       (s_fbs_time.tv_usec - s_fbs_start_time.tv_usec) / 1000);

  padding = 3 - ((len - 1) & 0x03);
  buf_put_CARD32(data_size_buf, (CARD32)len);
  buf_put_CARD32(&timestamp_buf[padding], timestamp);

  if (fwrite(data_size_buf, 1, 4, s_fbs_fp) != 4 ||
      fwrite(buf, 1, len, s_fbs_fp) != len ||
      fwrite(timestamp_buf, 1, 4 + padding, s_fbs_fp) != 4 + padding) {
    log_write(LL_WARN, "Could not write FBS file data");
    fbs_close_file();
  }
}

static void fbs_close_file(void)
{
  if (fclose(s_fbs_fp) != 0)
    log_write(LL_WARN, "Could not close FBS file");
  s_fbs_fp = NULL;
  free(s_fbs_buffer);
}

/********************/
/* Helper functions */
/********************/

static void hextile_fill_tile(void)
{
  int x, y;
  CARD32 *fb_ptr;

  fb_ptr = &g_framebuffer[hextile_rect.y * (int)g_fb_width + hextile_rect.x];

  /* Fill first row */
  for (x = 0; x < hextile_rect.w; x++)
    fb_ptr[x] = hextile_bg;

  /* Copy first row into other rows */
  for (y = 1; y < hextile_rect.h; y++)
    memcpy(&fb_ptr[y * g_fb_width], fb_ptr, hextile_rect.w * sizeof(CARD32));
}

static void hextile_fill_subrect(CARD8 pos, CARD8 dim)
{
  int pos_x, pos_y, dim_w, dim_h;
  int x, y, skip;
  CARD32 *fb_ptr;

  pos_x = pos >> 4 & 0x0F;
  pos_y = pos & 0x0F;
  fb_ptr = &g_framebuffer[(hextile_rect.y + pos_y) * (int)g_fb_width +
                          (hextile_rect.x + pos_x)];

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
  if (hextile_rect.x + 16 < cur_rect.x + cur_rect.w) {
    /* Next tile in the same row */
    hextile_rect.x += 16;
    if (hextile_rect.x + 16 < cur_rect.x + cur_rect.w)
      hextile_rect.w = 16;
    else
      hextile_rect.w = cur_rect.x + cur_rect.w - hextile_rect.x;
  } else if (hextile_rect.y + 16 < cur_rect.y + cur_rect.h) {
    /* First tile in the next row */
    hextile_rect.x = cur_rect.x;
    hextile_rect.w = (cur_rect.w < 16) ? cur_rect.w : 16;
    hextile_rect.y += 16;
    if (hextile_rect.y + 16 < cur_rect.y + cur_rect.h)
      hextile_rect.h = 16;
    else
      hextile_rect.h = cur_rect.y + cur_rect.h - hextile_rect.y;
  } else {
    fbupdate_rect_done();       /* No more tiles */
    return;
  }
  aio_setread(rf_host_hextile_subenc, NULL, 1);
}

static void reset_framebuffer(void)
{
  HOST_SLOT *hs = (HOST_SLOT *)cur_slot;
  FB_RECT r;

  log_write(LL_DETAIL, "Clearing framebuffer and cache");
  memset(g_framebuffer, 0, g_fb_width * g_fb_height * sizeof(CARD32));

  r.x = r.y = 0;
  r.w = g_fb_width;
  r.h = g_fb_height;

  invalidate_enc_cache(&r);

  /* Queue changed rectangle (the whole host screen) for each client */
  r.w = hs->fb_width;
  r.h = hs->fb_height;
   aio_walk_slots(fn_host_add_client_rect, TYPE_CL_SLOT);
}

static void fbupdate_rect_done(void)
{
  if (cur_rect.w != 0 && cur_rect.h != 0) {
    log_write(LL_DEBUG, "Received rectangle ok");

    /* Cached data for this rectangle is not valid any more */
    invalidate_enc_cache(&cur_rect);

    /* Save data in a file if necessary */
    if (s_fbs_fp != NULL)
      fbs_write_data(s_fbs_buffer, s_fbs_buffer_ptr - s_fbs_buffer);

    /* Queue this rectangle for each client */
    aio_walk_slots(fn_host_add_client_rect, TYPE_CL_SLOT);
  }

  if (--rect_count) {
    aio_setread(rf_host_fbupdate_recthdr, NULL, 12);
  } else {
    /* Done with the whole update */
    aio_walk_slots(fn_client_send_rects, TYPE_CL_SLOT);
    log_write(LL_DEBUG, "Requesting incremental framebuffer update");
    request_update(1);
    aio_setread(rf_host_msg, NULL, 1);
  }
}

/*
 * Send a FramebufferUpdateRequest for the whole screen
 */

static void request_update(int incr)
{
  HOST_SLOT *hs = (HOST_SLOT *)cur_slot;
  unsigned char fbupdatereq_msg[] = {
    3,                          /* Message id */
    0,                          /* Incremental if 1 */
    0, 0, 0, 0,                 /* X position, Y position */
    0, 0, 0, 0                  /* Width, height */
  };

  fbupdatereq_msg[1] = (incr) ? 1 : 0;
  buf_put_CARD16(&fbupdatereq_msg[6], hs->fb_width);
  buf_put_CARD16(&fbupdatereq_msg[8], hs->fb_height);

  log_write(LL_DEBUG, "Sending FramebufferUpdateRequest message");
  aio_write(NULL, fbupdatereq_msg, sizeof(fbupdatereq_msg));
}


/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_io.c,v 1.10 2001/08/06 23:30:31 const Exp $
 * Asynchronous interaction with VNC host.
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "logging.h"
#include "rect.h"
#include "client_io.h"
#include "host_io.h"

static void if_host(void);      /* Init hook */
static void cf_host(void);      /* Close hook */

static void rf_host_msg(void);

static void rf_host_fbupdate_hdr(void);
static void rf_host_fbupdate_recthdr(void);
static void rf_host_fbupdate_raw(void);
static void rf_host_hextile_subenc(void);
static void rf_host_hextile_raw(void);
static void rf_host_hextile_hex(void);
static void rf_host_hextile_subrects(void);
static void fn_host_add_client_rect(AIO_SLOT *slot);

static void rf_host_colormap_hdr(void);
static void rf_host_colormap_data(void);
static void rf_host_cuttext_hdr(void);
static void rf_host_cuttext_data(void);

static void hextile_fill_tile(void);
static void hextile_fill_subrect(CARD8 pos, CARD8 dim);
static void hextile_next_tile(void);
static void fbupdate_rect_done(void);
static void request_update(int incr);

/*
 * Implementation
 */

static AIO_SLOT *host_slot;

/* Initializing host I/O */
void init_host_io(int fd)
{
  aio_add_slot(fd, NULL, if_host, sizeof(AIO_SLOT));
}

/* Initializing I/O slot */
static void if_host(void)
{
  host_slot = cur_slot;
  aio_setclose(cf_host);

  log_write(LL_DETAIL, "Requesting full framebuffer update");
  request_update(0);

  aio_setread(rf_host_msg, NULL, 1);
}

/* On-close hook */
static void cf_host(void)
{
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
  }
  log_write(LL_WARN, "Closing connection to host");
  aio_close(1);
}

/* Processing RFB messages */
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
static CARD32 rect_enc;
static CARD16 rect_cur_row;

static CARD8 hextile_subenc;
static CARD8 hextile_num_subrects;
static CARD32 hextile_bg;
static CARD32 hextile_fg;
static FB_RECT hextile_rect;
static CARD32 hextile_buf[256];

static void rf_host_fbupdate_hdr(void)
{

  rect_count = buf_get_CARD16(&cur_slot->readbuf[1]);

  if (rect_count == 0xFFFF) {
    log_write(LL_DETAIL, "Receiving framebuffer update");
  } else {
    log_write(LL_DETAIL, "Receiving framebuffer update, %d rectangle(s)",
              rect_count);
  }

  aio_setread(rf_host_fbupdate_recthdr, NULL, 12);
}

static void rf_host_fbupdate_recthdr(void)
{

  cur_rect.x = buf_get_CARD16(cur_slot->readbuf);
  cur_rect.y = buf_get_CARD16(&cur_slot->readbuf[2]);
  cur_rect.w = buf_get_CARD16(&cur_slot->readbuf[4]);
  cur_rect.h = buf_get_CARD16(&cur_slot->readbuf[6]);
  rect_enc = buf_get_CARD32(&cur_slot->readbuf[8]);

  if (!cur_rect.h || !cur_rect.w) {
    log_write(LL_WARN, "Zero-size rectangle %dx%d at %d,%d (ignoring)",
              (int)cur_rect.w, (int)cur_rect.h,
              (int)cur_rect.x, (int)cur_rect.y);
    aio_setread(rf_host_fbupdate_recthdr, NULL, 12);
    return;
  }

  log_write(LL_DEBUG, "Receiving rectangle %dx%d at %d,%d",
            (int)cur_rect.w, (int)cur_rect.h,
            (int)cur_rect.x, (int)cur_rect.y);

  switch(rect_enc) {
  case RFB_ENCODING_RAW:
    log_write(LL_DEBUG, "Receiving raw-encoded data, expecting %d byte(s)",
              cur_rect.w * cur_rect.h * sizeof(CARD32));
    rect_cur_row = 0;
    aio_setread(rf_host_fbupdate_raw,
                &g_framebuffer[cur_rect.y * (int)g_screen_info->width +
                               cur_rect.x],
                cur_rect.w * sizeof(CARD32));
    break;
  case RFB_ENCODING_HEXTILE:
    log_write(LL_DEBUG, "Receiving hextile-encoded data");
    hextile_rect.x = cur_rect.x;
    hextile_rect.y = cur_rect.y;
    hextile_rect.w = (cur_rect.w < 16) ? cur_rect.w : 16;
    hextile_rect.h = (cur_rect.h < 16) ? cur_rect.h : 16;
    aio_setread(rf_host_hextile_subenc, NULL, sizeof(CARD8));
    break;
  default:
    log_write(LL_ERROR, "Unknown encoding: 0x%08lX", (unsigned long)rect_enc);
    aio_close(0);
  }
}

static void rf_host_fbupdate_raw(void)
{
  int idx;

  if (++rect_cur_row < cur_rect.h) {
    /* Read next row */
    idx = (cur_rect.y + rect_cur_row) * (int)g_screen_info->width + cur_rect.x;
    aio_setread(rf_host_fbupdate_raw, &g_framebuffer[idx],
                cur_rect.w * sizeof(CARD32));
  } else {
    /* Done with this rectangle */
    fbupdate_rect_done();
  }
}

static void rf_host_hextile_subenc(void)
{
  int data_size;

  log_write(LL_DEBUG, "Hextile rect %dx%d at %d,%d",
            (int)hextile_rect.w, (int)hextile_rect.h,
            (int)hextile_rect.x, (int)hextile_rect.y);

  hextile_subenc = cur_slot->readbuf[0];
  if (hextile_subenc & HEXTILE_RAW) {
    data_size = hextile_rect.w * hextile_rect.h * sizeof(CARD32);
    aio_setread(rf_host_hextile_raw, hextile_buf, data_size);
    return;
  }
  data_size = 0;
  if (hextile_subenc & HEXTILE_BG_SPECIFIED) {
    data_size += sizeof(CARD32);
  } else {
    hextile_fill_tile();
  }
  if (hextile_subenc & HEXTILE_FG_SPECIFIED)
    data_size += sizeof(CARD32);
  if (hextile_subenc & HEXTILE_ANY_SUBRECTS)
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

  from_ptr = hextile_buf;
  fb_ptr = &g_framebuffer[hextile_rect.y * (int)g_screen_info->width +
                          hextile_rect.x];

  /* Just copy raw data into the framebuffer */
  for (row = 0; row < hextile_rect.h; row++) {
    memcpy(fb_ptr, from_ptr, hextile_rect.w * sizeof(CARD32));
    from_ptr += hextile_rect.w;
    fb_ptr += g_screen_info->width;
  }

  hextile_next_tile();
}

static void rf_host_hextile_hex(void)
{
  CARD32 *from_ptr = hextile_buf;
  int data_size;

  if (hextile_subenc & HEXTILE_BG_SPECIFIED) {
    hextile_bg = *from_ptr++;
    hextile_fill_tile();
  }
  if (hextile_subenc & HEXTILE_FG_SPECIFIED)
    hextile_fg = *from_ptr++;
  if (hextile_subenc & HEXTILE_ANY_SUBRECTS) {
    hextile_num_subrects = *((CARD8 *)from_ptr);
    if (hextile_subenc & HEXTILE_SUBRECTS_COLOURED) {
      data_size = 6 * (unsigned int)hextile_num_subrects;
    } else {
      data_size = 2 * (unsigned int)hextile_num_subrects;
    }
    aio_setread(rf_host_hextile_subrects, NULL, data_size);
    return;
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

  if (hextile_subenc & HEXTILE_SUBRECTS_COLOURED) {
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
  aio_setread(rf_host_colormap_data, NULL, num_colors * 6);
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
static CARD32 cut_len;

static void rf_host_cuttext_hdr(void)
{
  cut_len = buf_get_CARD32(&cur_slot->readbuf[3]);

  log_write(LL_DETAIL,
            "Receiving ServerCutText message from host, %lu byte(s)",
            (unsigned long)cut_len);

  aio_setread(rf_host_cuttext_data, NULL, (size_t)cut_len);
}

static void rf_host_cuttext_data(void)
{
  if (cut_len <= 46) {
    log_write(LL_DEBUG, "Cut text: \"%.*s\"",
              (int)cut_len, cur_slot->readbuf);
  } else {
    log_write(LL_DEBUG, "Cut text: \"%.34s\" (truncated)",
              cur_slot->readbuf);
  }
  aio_setread(rf_host_msg, NULL, 1);
}

/*************************************/
/* Functions called from client_io.c */
/*************************************/

void pass_msg_to_host(CARD8 *msg, size_t len)
{
  AIO_SLOT *saved_slot = cur_slot;
  cur_slot = host_slot;
  aio_write(NULL, msg, len);
  cur_slot = saved_slot;
}

/********************/
/* Helper functions */
/********************/

static void hextile_fill_tile(void)
{
  int x, y;
  CARD32 *fb_ptr;

  fb_ptr = &g_framebuffer[hextile_rect.y * (int)g_screen_info->width +
                          hextile_rect.x];

  for (y = 0; y < hextile_rect.h; y++) {
    for (x = 0; x < hextile_rect.w; x++) {
      *fb_ptr++ = hextile_bg;
    }
    fb_ptr += g_screen_info->width - hextile_rect.w;
  }
}

/* FIXME: Not as efficient as it could be. */
static void hextile_fill_subrect(CARD8 pos, CARD8 dim)
{
  int pos_x, pos_y, dim_w, dim_h;
  int x, y;
  CARD32 *fb_ptr;

  pos_x = pos >> 4 & 0x0F;
  pos_y = pos & 0x0F;
  dim_w = (dim >> 4 & 0x0F) + 1;
  dim_h = (dim & 0x0F) + 1;

  fb_ptr = &g_framebuffer[(hextile_rect.y+pos_y) * (int)g_screen_info->width +
                          (hextile_rect.x+pos_x)];

  for (y = 0; y < dim_h; y++) {
    for (x = 0; x < dim_w; x++) {
      *fb_ptr++ = hextile_fg;
    }
    fb_ptr += g_screen_info->width - dim_w;
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
  aio_setread(rf_host_hextile_subenc, NULL, sizeof(CARD8));
}

static void fbupdate_rect_done(void)
{
  log_write(LL_DEBUG, "Received rectangle ok");

  /* Queue this rectangle for each client */
  aio_walk_slots(fn_host_add_client_rect, TYPE_CL_SLOT);

  if (--rect_count) {
    aio_setread(rf_host_fbupdate_recthdr, NULL, 12);
  } else {
    /* Done with the whole update */
    aio_walk_slots(fn_client_send_rects, TYPE_CL_SLOT);
    log_write(LL_DETAIL, "Requesting incremental framebuffer update");
    request_update(1);
    aio_setread(rf_host_msg, NULL, 1);
  }
}

/* Send a FramebufferUpdateRequest for the whole screen */
static void request_update(int incr)
{
  unsigned char fbupdatereq_msg[] = {
    3,                          /* Message id */
    0,                          /* Incremental if 1 */
    0, 0, 0, 0,                 /* X position, Y position */
    0, 0, 0, 0                  /* Width, height */
  };

  fbupdatereq_msg[1] = (incr) ? 1 : 0;
  buf_put_CARD16(&fbupdatereq_msg[6], g_screen_info->width);
  buf_put_CARD16(&fbupdatereq_msg[8], g_screen_info->height);

  log_write(LL_DEBUG, "Sending FramebufferUpdateRequest message");
  aio_write(NULL, fbupdatereq_msg, sizeof(fbupdatereq_msg));
}


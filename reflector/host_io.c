/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_io.c,v 1.4 2001/08/02 19:50:23 const Exp $
 * Asynchronous interaction with VNC host.
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "logging.h"
#include "host_io.h"

static void if_host(void);      /* Init hook */
static void cf_host(void);      /* Close hook */

static void rf_host_msg(void);
static void rf_host_fbupdate_hdr(void);
static void rf_host_fbupdate_recthdr(void);
static void rf_host_fbupdate_raw(void);
static void rf_host_colormap_hdr(void);
static void rf_host_colormap_data(void);
static void rf_host_cuttext_hdr(void);
static void rf_host_cuttext_data(void);

static void request_update(int incr);

/*
 * Implementation
 */

/* This is the only function visible from outside */
void init_host_io(int fd)
{
  aio_add_slot(fd, if_host, 0, sizeof(AIO_SLOT));
}

/* Initializing I/O slot */
static void if_host(void)
{
  aio_setclose(cf_host);

  log_write(LL_DETAIL, "Requesting full framebuffer update");
  request_update(0);

  aio_setread(rf_host_msg, NULL, 1);
}

/* On-close hook */
static void cf_host(void)
{
  if (cur_slot->errread_f) {
    log_write(LL_ERROR, "Error reading data from host");
  } else if (cur_slot->errwrite_f) {
    log_write(LL_ERROR, "Error sending data to host");
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
static CARD16 rect_x, rect_y, rect_w, rect_h;
static CARD32 rect_enc;
static CARD16 rect_cur_row;

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
  rect_x = buf_get_CARD16(cur_slot->readbuf);
  rect_y = buf_get_CARD16(&cur_slot->readbuf[2]);
  rect_w = buf_get_CARD16(&cur_slot->readbuf[4]);
  rect_h = buf_get_CARD16(&cur_slot->readbuf[6]);
  rect_enc = buf_get_CARD32(&cur_slot->readbuf[8]);

  if (!rect_h || !rect_w) {
    log_write(LL_WARN, "Zero-size rectangle %dx%d at %d,%d (ignoring)",
              (int)rect_w, (int)rect_h, (int)rect_x, (int)rect_y);
    aio_setread(rf_host_fbupdate_recthdr, NULL, 12);
    return;
  }

  log_write(LL_DEBUG, "Receiving rectangle %dx%d at %d,%d",
            (int)rect_w, (int)rect_h, (int)rect_x, (int)rect_y);

  switch(rect_enc) {
  case 0:
    log_write(LL_DEBUG, "Receiving raw-encoded data, expecting %d byte(s)",
              rect_w * rect_h * sizeof(CARD32));
    rect_cur_row = 0;
    aio_setread(rf_host_fbupdate_raw,
                &framebuffer[rect_y * desktop_info.width + rect_x],
                rect_w * sizeof(CARD32));
    break;
  default:
    log_write(LL_ERROR, "Unknown encoding: 0x%08lX", (unsigned long)rect_enc);
    aio_close(0);
  }
}

static void rf_host_fbupdate_raw(void)
{
  int idx;

  if (++rect_cur_row < rect_h) {
    /* Read next row */
    idx = (rect_y + rect_cur_row) * desktop_info.width + rect_x;
    aio_setread(rf_host_fbupdate_raw, &framebuffer[idx],
                rect_w * sizeof(CARD32));
  } else {
    /* Done with this rectangle */
    log_write(LL_DEBUG, "Received rectangle ok");

    if (--rect_count) {
      aio_setread(rf_host_fbupdate_recthdr, NULL, 12);
    } else {
      log_write(LL_DETAIL, "Requesting incremental framebuffer update");
      request_update(1);
      aio_setread(rf_host_msg, NULL, 1);
    }
  }
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

/********************/
/* Helper functions */
/********************/

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
  buf_put_CARD16(&fbupdatereq_msg[6], (CARD16)desktop_info.width);
  buf_put_CARD16(&fbupdatereq_msg[8], (CARD16)desktop_info.height);

  log_write(LL_DEBUG, "Sending FramebufferUpdateRequest message");
  aio_write(NULL, fbupdatereq_msg, sizeof(fbupdatereq_msg));
}


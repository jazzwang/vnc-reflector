/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.c,v 1.18 2001/08/08 12:56:51 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "rfblib.h"
#include "logging.h"
#include "async_io.h"
#include "reflector.h"
#include "rect.h"
#include "encode.h"
#include "host_io.h"
#include "client_io.h"
#include "d3des.h"

static unsigned char *s_password;

/*
 * Prototypes for static functions
 */

static void cf_client(void);
static void rf_client_ver(void);
static void rf_client_auth(void);
static void wf_client_auth_failed(void);
static void rf_client_initmsg(void);
static void rf_client_msg(void);
static void rf_client_pixfmt(void);
static void rf_client_colormap_hdr(void);
static void rf_client_colormap_data(void);
static void rf_client_encodings_hdr(void);
static void rf_client_encodings_data(void);
static void rf_client_updatereq(void);
static void wf_client_update_finished(void);
static void rf_client_keyevent(void);
static void rf_client_ptrevent(void);
static void rf_client_cuttext_hdr(void);
static void rf_client_cuttext_data(void);

static void send_update(void);

/*
 * Implementation
 */

void set_client_password(unsigned char *password)
{
  s_password = password;
}

void af_client_accept(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  /* FIXME: Function naming is bad (client_accept_hook?). */

  cur_slot->type = TYPE_CL_SLOT;
  cl->connected = 0;
  aio_setclose(cf_client);

  log_write(LL_MSG, "Accepted connection from %s", cur_slot->name);

  aio_write(NULL, "RFB 003.003\n", 12);
  aio_setread(rf_client_ver, NULL, 12);
}

static void cf_client(void)
{
  if (cur_slot->errread_f) {
    if (cur_slot->io_errno) {
      log_write(LL_WARN, "Error reading from %s: %s",
                cur_slot->name, strerror(cur_slot->io_errno));
    } else {
      log_write(LL_WARN, "Error reading from %s", cur_slot->name);
    }
  } else if (cur_slot->errwrite_f) {
    if (cur_slot->io_errno) {
      log_write(LL_WARN, "Error sending to %s: %s",
                cur_slot->name, strerror(cur_slot->io_errno));
    } else {
      log_write(LL_WARN, "Error sending to %s", cur_slot->name);
    }
  }
  log_write(LL_MSG, "Closing client connection %s", cur_slot->name);
}

static void rf_client_ver(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  int i;

  /* FIXME: Check protocol version. */

  /* FIXME: Functions like authentication should be available in
     separate modules, not in I/O part of the code. */
  /* FIXME: Higher level I/O functions should be implemented
     instead of things like buf_put_CARD32 + aio_write. */

  log_write(LL_DETAIL, "Client supports %.11s", cur_slot->readbuf);

  if (s_password[0]) {
    /* Request VNC authentication */
    buf_put_CARD32(cl->msg_buf, 2);

    /* Prepare "random" challenge */
    srandom((unsigned int)time(NULL));
    for (i = 0; i < 16; i++)
      cl->msg_buf[i + 4] = (unsigned char)random();

    /* Send both auth ID and challenge */
    aio_write(NULL, cl->msg_buf, 20);
    aio_setread(rf_client_auth, NULL, 16);
  } else {
    log_write(LL_WARN, "Not requesting authentication from %s",
              cur_slot->name);
    buf_put_CARD32(cl->msg_buf, 1);
    aio_write(NULL, cl->msg_buf, 4);
    aio_setread(rf_client_initmsg, NULL, 1);
  }
}

static void rf_client_auth(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  unsigned char key[8];
  unsigned char buf[16];

  memset(key, 0, 8);
  strncpy((char *)key, (char *)s_password, 8);

  /* Place correct crypted response to buf */
  deskey(key, EN0);
  des(cl->msg_buf + 4, buf);
  des(cl->msg_buf + 12, buf + 8);

  /* Compare client response with the correct one */
  /* FIXME: Implement "too many tries" functionality some day. */
  if (memcmp(cur_slot->readbuf, buf, 16) != 0) {
    log_write(LL_WARN, "Authentication failed for %s", cur_slot->name);
    buf_put_CARD32(buf, 1);
    aio_write(wf_client_auth_failed, buf, 4);
  } else {
    log_write(LL_MSG, "Authentication passed by %s", cur_slot->name);
    buf_put_CARD32(buf, 0);
    aio_write(NULL, buf, 4);
    aio_setread(rf_client_initmsg, NULL, 1);
  }
}

static void wf_client_auth_failed(void)
{
  aio_close(0);
}

static void rf_client_initmsg(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  FB_RECT rect;
  unsigned char msg_server_init[24];

  if (cur_slot->readbuf[0] == 0) {
    log_write(LL_WARN, "Non-shared session requested by %s", cur_slot->name);
    aio_close(0);
  }

  /* Send ServerInitialisation message */
  buf_put_CARD16(msg_server_init, g_screen_info->width);
  buf_put_CARD16(msg_server_init + 2, g_screen_info->height);
  buf_put_pixfmt(msg_server_init + 4, &g_screen_info->pixformat);
  buf_put_CARD32(msg_server_init + 20, g_screen_info->name_length);
  aio_write(NULL, msg_server_init, 24);
  aio_write(NULL, g_screen_info->name, g_screen_info->name_length);
  aio_setread(rf_client_msg, NULL, 1);

  /* Set up initial pixel format */
  memcpy(&cl->format, &g_screen_info->pixformat, sizeof(RFB_PIXEL_FORMAT));

  /* The client did not requested framebuffer updates yet */
  cl->update_requested = 0;
  cl->update_in_progress = 0;

  /* Add a rectangle covering the whole framebuffer to the list of
     pending rectangles */
  rect.x = 0;
  rect.y = 0;
  rect.w = g_screen_info->width;
  rect.h = g_screen_info->height;
  rlist_init(&cl->pending_rects);
  rlist_push_rect(&cl->pending_rects, &rect);

  /* We are connected. */
  cl->connected = 1;
}

static void rf_client_msg(void)
{
  int msg_id;

  msg_id = (int)cur_slot->readbuf[0] & 0xFF;
  switch(msg_id) {
  case 0:                       /* SetPixelFormat */
    aio_setread(rf_client_pixfmt, NULL, 3 + sizeof(RFB_PIXEL_FORMAT));
    break;
  case 1:                       /* FixColourMapEntries */
    aio_setread(rf_client_colormap_hdr, NULL, 5);
    break;
  case 2:                       /* SetEncodings */
    aio_setread(rf_client_encodings_hdr, NULL, 3);
    break;
  case 3:                       /* FramebufferUpdateRequest */
    aio_setread(rf_client_updatereq, NULL, 9);
    break;
  case 4:                       /* KeyEvent */
    aio_setread(rf_client_keyevent, NULL, 7);
    break;
  case 5:                       /* PointerEvent */
    aio_setread(rf_client_ptrevent, NULL, 5);
    break;
  case 6:                       /* ClientCutText */
    aio_setread(rf_client_cuttext_hdr, NULL, 7);
    break;
  default:
    log_write(LL_ERROR, "Unknown client message type %d from %s",
              msg_id, cur_slot->name);
    aio_close(0);
  }
}

static void rf_client_pixfmt(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  buf_get_pixfmt(&cur_slot->readbuf[3], &cl->format);
  log_write(LL_DETAIL, "Pixel format (%d bpp) set by %s",
            cl->format.bits_pixel, cur_slot->name);
  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_colormap_hdr(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  log_write(LL_WARN, "Ignoring FixColourMapEntries message from %s",
            cur_slot->name);

  cl->temp_count = buf_get_CARD16(&cur_slot->readbuf[3]);
  aio_setread(rf_client_colormap_data, NULL, cl->temp_count * 6);
}

static void rf_client_colormap_data(void)
{
  /* Nothing to do with FixColourMapEntries */
  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_encodings_hdr(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  cl->temp_count = buf_get_CARD16(&cur_slot->readbuf[1]);
  aio_setread(rf_client_encodings_data, NULL, cl->temp_count * sizeof(CARD32));
}

static void rf_client_encodings_data(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  int i;
  CARD32 enc;

  /* Reset encoding list (always enable raw encoding) */
  cl->enc_enable[0] = 1;
  for (i = 1; i < NUM_ENCODINGS; i++) {
    cl->enc_enable[i] = 0;
  }

  /* Read and store encoding list supplied by the client */
  for (i = 0; i < (int)cl->temp_count; i++) {
    enc = buf_get_CARD32(&cur_slot->readbuf[i * sizeof(CARD32)]);
    if (enc >= 0 && enc < NUM_ENCODINGS) {
      cl->enc_enable[enc] = 1;
    }
  }

  log_write(LL_DETAIL, "Encoding list set by %s", cur_slot->name);
  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_updatereq(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  FB_RECT rect;

  rect.x = buf_get_CARD16(&cur_slot->readbuf[1]);
  rect.y = buf_get_CARD16(&cur_slot->readbuf[3]);
  rect.w = buf_get_CARD16(&cur_slot->readbuf[5]);
  rect.h = buf_get_CARD16(&cur_slot->readbuf[7]);

  if (!cur_slot->readbuf[0]) {
    rlist_add_rect(&cl->pending_rects, &rect);
    log_write(LL_DEBUG, "Received framebuffer update request (full) from %s",
              cur_slot->name);
  } else {
    log_write(LL_DEBUG, "Received framebuffer update request from %s",
              cur_slot->name);
  }

  if (cl->update_in_progress || !cl->pending_rects.num_rects) {
    cl->update_requested = 1;
  } else {
    send_update();
    cl->update_in_progress = 1;
    cl->update_requested = 0;
  }

  aio_setread(rf_client_msg, NULL, 1);
}

static void wf_client_update_finished(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  log_write(LL_DEBUG, "Finished sending framebuffer update to %s",
            cur_slot->name);

  cl->update_in_progress = 0;
  if (cl->update_requested && cl->pending_rects.num_rects) {
    send_update();
    cl->update_in_progress = 1;
    cl->update_requested = 0;
  }
}

static void rf_client_keyevent(void)
{
  CARD8 msg[8];

  msg[0] = 4;                   /* KeyEvent */
  memcpy(&msg[1], cur_slot->readbuf, 7);
  pass_msg_to_host(msg, sizeof(msg));
  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_ptrevent(void)
{
  CARD8 msg[6];

  msg[0] = 5;                   /* PointerEvent */
  memcpy(&msg[1], cur_slot->readbuf, 5);
  pass_msg_to_host(msg, sizeof(msg));
  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_cuttext_hdr(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  log_write(LL_DEBUG, "Receiving ClientCutText message from %s",
            cur_slot->name);

  cl->cut_len = (int)buf_get_CARD32(&cur_slot->readbuf[3]);
  aio_setread(rf_client_cuttext_data, NULL, cl->cut_len);
}

static void rf_client_cuttext_data(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  pass_cuttext_to_host(cur_slot->readbuf, cl->cut_len);
  aio_setread(rf_client_msg, NULL, 1);
}

/*
 * Functions called from host_io.c
 */

void fn_client_add_rect(AIO_SLOT *slot, FB_RECT *rect)
{
  CL_SLOT *cl = (CL_SLOT *)slot;

  if (cl->connected)
    rlist_add_rect(&cl->pending_rects, rect);
}

void fn_client_send_rects(AIO_SLOT *slot)
{
  CL_SLOT *cl = (CL_SLOT *)slot;
  AIO_SLOT *saved_slot = cur_slot;

  if ( !cl->update_in_progress &&
       cl->update_requested &&
       cl->pending_rects.num_rects ) {
    cur_slot = slot;
    send_update();
    cl->update_in_progress = 1;
    cl->update_requested = 0;
    cur_slot = saved_slot;
  }
}

void fn_client_send_cuttext(AIO_SLOT *slot, CARD8 *text, size_t len)
{
  CL_SLOT *cl = (CL_SLOT *)slot;
  AIO_SLOT *saved_slot = cur_slot;
  CARD8 svr_cuttext_hdr[8] = {
    3, 0, 0, 0, 0, 0, 0, 0
  };

  if (cl->connected) {
    cur_slot = slot;
    log_write(LL_DEBUG, "Sending ServerCutText message to %s", cur_slot->name);
    buf_put_CARD32(&svr_cuttext_hdr[4], (CARD32)len);
    aio_write(NULL, svr_cuttext_hdr, 8);
    aio_write(NULL, text, (size_t)len);
    cur_slot = saved_slot;
  }
}

/*
 * Non-callback functions
 */

static void send_update(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  CARD8 msg_hdr[4] = {
    0, 0, 0, 1
  };
  CARD8 rect_hdr[12] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
  };
  FB_RECT rect;

  /* FIXME: This sends the whole framebuffer to the client! */

  log_write(LL_DEBUG, "Sending framebuffer update (%d rects) to %s",
            cl->pending_rects.num_rects, cl->s.name);

  buf_put_CARD16(&msg_hdr[2], cl->pending_rects.num_rects);
  aio_write(NULL, msg_hdr, 4);

  while (rlist_pick_rect(&cl->pending_rects, &rect)) {
    buf_put_CARD16(rect_hdr, rect.x);
    buf_put_CARD16(&rect_hdr[2], rect.y);
    buf_put_CARD16(&rect_hdr[4], rect.w);
    buf_put_CARD16(&rect_hdr[6], rect.h);
    aio_write(NULL, rect_hdr, 12);
    aio_write_nocopy(wf_client_update_finished,
                     rfb_encode_raw_block(&cl->format, &rect));
  }
}


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
 * $Id: client_io.c,v 1.39 2002/09/08 19:37:11 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <zlib.h>

#include "rfblib.h"
#include "logging.h"
#include "async_io.h"
#include "reflector.h"
#include "rect.h"
#include "host_io.h"
#include "translate.h"
#include "client_io.h"
#include "encode.h"

static unsigned char *s_password;
static unsigned char *s_password_ro;

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

static void set_trans_func(CL_SLOT *cl);
static void send_update(void);

/*
 * Implementation
 */

void set_client_passwords(unsigned char *password, unsigned char *password_ro)
{
  s_password = password;
  s_password_ro = password_ro;
}

void af_client_accept(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  int i;

  /* FIXME: Function naming is bad (client_accept_hook?). */

  cur_slot->type = TYPE_CL_SLOT;
  cl->connected = 0;
  cl->trans_table = NULL;
  aio_setclose(cf_client);

  for (i = 0; i < 4; i++)
    cl->zs_active[i] = 0;

  log_write(LL_MSG, "Accepted connection from %s", cur_slot->name);

  aio_write(NULL, "RFB 003.003\n", 12);
  aio_setread(rf_client_ver, NULL, 12);
}

static void cf_client(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  int i;

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
  } else if (cur_slot->errio_f) {
    log_write(LL_WARN, "I/O error, client %s", cur_slot->name);
  }
  log_write(LL_MSG, "Closing client connection %s", cur_slot->name);

  /* Free zlib streams.
     FIXME: Maybe put cleanup function in encoder. */
  for (i = 0; i < 4; i++) {
    if (cl->zs_active[i])
      deflateEnd(&cl->zs_struct[i]);
  }

  /* Free dynamically allocated memory. */
  if (cl->trans_table != NULL)
    free(cl->trans_table);
}

static void rf_client_ver(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  CARD8 msg[20];

  /* FIXME: Check protocol version. */

  /* FIXME: Functions like authentication should be available in
     separate modules, not in I/O part of the code. */
  /* FIXME: Higher level I/O functions should be implemented
     instead of things like buf_put_CARD32 + aio_write. */

  log_write(LL_DETAIL, "Client supports %.11s", cur_slot->readbuf);

  if (s_password[0]) {
    /* Request VNC authentication */
    buf_put_CARD32(msg, 2);

    /* Prepare "random" challenge */
    rfb_gen_challenge(cl->auth_challenge);
    memcpy(&msg[4], cl->auth_challenge, 16);

    /* Send both auth ID and challenge */
    aio_write(NULL, msg, 20);
    aio_setread(rf_client_auth, NULL, 16);
  } else {
    log_write(LL_WARN, "Not requesting authentication from %s",
              cur_slot->name);
    buf_put_CARD32(msg, 1);
    aio_write(NULL, msg, 4);
    aio_setread(rf_client_initmsg, NULL, 1);
  }
}

static void rf_client_auth(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  unsigned char resp_rw[16];
  unsigned char resp_ro[16];
  unsigned char msg[4];

  /* Place correct crypted responses to resp_rw, resp_ro */
  rfb_crypt(resp_rw, cl->auth_challenge, s_password);
  rfb_crypt(resp_ro, cl->auth_challenge, s_password_ro);

  /* Compare client response with correct ones */
  /* FIXME: Implement "too many tries" functionality some day. */
  if (memcmp(cur_slot->readbuf, resp_rw, 16) == 0) {
    cl->readonly = 0;
    log_write(LL_MSG, "Full-control authentication passed by %s",
              cur_slot->name);
  } else if (memcmp(cur_slot->readbuf, resp_ro, 16) == 0) {
    cl->readonly = 1;
    log_write(LL_MSG, "Read-only authentication passed by %s",
              cur_slot->name);
  } else {
    log_write(LL_WARN, "Authentication failed for %s", cur_slot->name);
    buf_put_CARD32(msg, 1);
    aio_write(wf_client_auth_failed, msg, 4);
    return;
  }

  buf_put_CARD32(msg, 0);
  aio_write(NULL, msg, 4);
  aio_setread(rf_client_initmsg, NULL, 1);
}

static void wf_client_auth_failed(void)
{
  aio_close(0);
}

static void rf_client_initmsg(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  unsigned char msg_server_init[24];

  if (cur_slot->readbuf[0] == 0) {
    log_write(LL_WARN, "Non-shared session requested by %s", cur_slot->name);
    aio_close(0);
  }

  /* Save initial desktop geometry for this client */
  cl->fb_width = g_screen_info.width;
  cl->fb_height = g_screen_info.height;
  cl->enable_newfbsize = 0;

  /* Send ServerInitialisation message */
  buf_put_CARD16(msg_server_init, cl->fb_width);
  buf_put_CARD16(msg_server_init + 2, cl->fb_height);
  buf_put_pixfmt(msg_server_init + 4, &g_screen_info.pixformat);
  buf_put_CARD32(msg_server_init + 20, g_screen_info.name_length);
  aio_write(NULL, msg_server_init, 24);
  aio_write(NULL, g_screen_info.name, g_screen_info.name_length);
  aio_setread(rf_client_msg, NULL, 1);

  /* Set up initial pixel format and encoders' parameters */
  memcpy(&cl->format, &g_screen_info.pixformat, sizeof(RFB_PIXEL_FORMAT));
  cl->trans_func = transfunc_null;
  cl->bgr233_f = 0;
  cl->compress_level = 6;       /* default compression level */
  cl->jpeg_quality = -1;        /* disable JPEG by default */

  /* The client did not requested framebuffer updates yet */
  cl->update_requested = 0;
  cl->update_in_progress = 0;
  rlist_init(&cl->pending_rects);

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
  if (cl->trans_table != NULL)
    free(cl->trans_table);

  log_write(LL_DETAIL, "Pixel format (%d bpp) set by %s",
            cl->format.bits_pixel, cur_slot->name);

  set_trans_func(cl);

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
  int preferred_enc_set = 0;
  CARD32 enc;

  /* Reset encoding list (always enable raw encoding) */
  cl->enc_enable[RFB_ENCODING_RAW] = 1;
  cl->enc_prefer = RFB_ENCODING_RAW;
  cl->compress_level = -1;
  cl->jpeg_quality = -1;
  cl->enable_lastrect = 0;
  cl->enable_newfbsize = 0;
  for (i = 1; i < NUM_ENCODINGS; i++)
    cl->enc_enable[i] = 0;

  /* Read and store encoding list supplied by the client */
  for (i = 0; i < (int)cl->temp_count; i++) {
    enc = buf_get_CARD32(&cur_slot->readbuf[i * sizeof(CARD32)]);
    if (!preferred_enc_set) {
      if ( enc == RFB_ENCODING_RAW ||
           enc == RFB_ENCODING_HEXTILE ||
           enc == RFB_ENCODING_TIGHT ) {
        cl->enc_prefer = enc;
        preferred_enc_set = 1;
      }
    }
    if (enc >= 0 && enc < NUM_ENCODINGS) {
      cl->enc_enable[enc] = 1;
    } else if (enc >= RFB_ENCODING_COMPESSLEVEL0 &&
               enc <= RFB_ENCODING_COMPESSLEVEL9 &&
               cl->compress_level == -1) {
      cl->compress_level = (int)(enc - RFB_ENCODING_COMPESSLEVEL0);
      log_write(LL_DETAIL, "Compression level %d requested by client %s",
                cl->compress_level, cur_slot->name);
    } else if (enc >= RFB_ENCODING_QUALITYLEVEL0 &&
               enc <= RFB_ENCODING_QUALITYLEVEL9 &&
               cl->jpeg_quality == -1) {
      cl->jpeg_quality = (int)(enc - RFB_ENCODING_QUALITYLEVEL0);
      log_write(LL_DETAIL, "JPEG quality level %d requested by client %s",
                cl->jpeg_quality, cur_slot->name);
    } else if (enc == RFB_ENCODING_LASTRECT) {
      log_write(LL_DETAIL, "Client %s supports LastRect markers",
                cur_slot->name);
      cl->enable_lastrect = 1;
    } else if (enc == RFB_ENCODING_NEWFBSIZE) {
      /* FIXME: Handle NewFBRect on->off _correctly_. */
      cl->enable_newfbsize = 1;
      log_write(LL_DETAIL, "Client %s supports desktop geometry changes",
                cur_slot->name);
    }
  }
  if (cl->compress_level < 0)
    cl->compress_level = 6;     /* default compression level */

  log_write(LL_DEBUG, "Encoding list set by %s", cur_slot->name);
  if (cl->enc_prefer == RFB_ENCODING_RAW) {
    log_write(LL_WARN, "Using raw encoding for client %s",
              cur_slot->name);
  } else if (cl->enc_prefer == RFB_ENCODING_TIGHT) {
    log_write(LL_DETAIL, "Using Tight encoding for client %s",
              cur_slot->name);
  } else if (cl->enc_prefer == RFB_ENCODING_HEXTILE) {
    log_write(LL_DETAIL, "Using Hextile encoding for client %s",
              cur_slot->name);
  }
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

  /* Not CopyRect or NewFBSize. */
  rect.enc = 0;

  if (!cur_slot->readbuf[0]) {
    log_write(LL_DEBUG, "Received framebuffer update request (full) from %s",
              cur_slot->name);
    if (cl->bgr233_f)
      rlist_add_rect(&cl->pending_rects, &rect);
    else
      rlist_push_rect(&cl->pending_rects, &rect);
  } else {
    cl->update_rect = rect;
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
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  CARD8 msg[8];

  if (!cl->readonly) {
    msg[0] = 4;                 /* KeyEvent */
    memcpy(&msg[1], cur_slot->readbuf, 7);
    pass_msg_to_host(msg, sizeof(msg));
  }

  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_ptrevent(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  CARD16 x, y;
  CARD8 msg[6];

  if (!cl->readonly) {
    msg[0] = 5;                 /* PointerEvent */
    msg[1] = cur_slot->readbuf[0];
    x = buf_get_CARD16(&cur_slot->readbuf[1]);
    y = buf_get_CARD16(&cur_slot->readbuf[3]);

    /* Pointer position should fit in the host screen */
    if (x >= g_screen_info.width)
      x = g_screen_info.width - 1;
    if (y >= g_screen_info.height)
      y = g_screen_info.height - 1;

    buf_put_CARD16(&msg[2], x);
    buf_put_CARD16(&msg[4], y);
    pass_msg_to_host(msg, sizeof(msg));
  }

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

  if (!cl->readonly)
    pass_cuttext_to_host(cur_slot->readbuf, cl->cut_len);

  aio_setread(rf_client_msg, NULL, 1);
}

/*
 * Functions called from host_io.c
 */

void fn_client_add_rect(AIO_SLOT *slot, FB_RECT *rect)
{
  CL_SLOT *cl = (CL_SLOT *)slot;

  if (!cl->connected)
    return;

  if (rect->enc == RFB_ENCODING_NEWFBSIZE) {
    if (rect->w != cl->fb_width || rect->h != cl->fb_height) {
      cl->fb_width = rect->w;
      cl->fb_height = rect->h;
      if (cl->enable_newfbsize) {
        rlist_push_rect(&cl->pending_rects, rect);
        /* FIXME: The line below is a HACK!
           This update_rect stuff should be redesigned. */
        cl->update_rect = *rect;
      }
    }
  } else {
    rlist_add_clipped_rect(&cl->pending_rects, rect, &cl->update_rect,
                           cl->bgr233_f);
  }
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
    if (len)
      aio_write(NULL, text, len);

    cur_slot = saved_slot;
  }
}

/*
 * Non-callback functions
 */

static void set_trans_func(CL_SLOT *cl)
{
  if (cl->trans_table != NULL) {
    free(cl->trans_table);
    cl->trans_table = NULL;
    cl->trans_func = transfunc_null;
  }

  cl->bgr233_f = 0;

  if ( cl->format.bits_pixel != g_screen_info.pixformat.bits_pixel ||
       cl->format.color_depth != g_screen_info.pixformat.color_depth ||
       cl->format.big_endian != g_screen_info.pixformat.big_endian ||
       ((cl->format.true_color != 0) !=
        (g_screen_info.pixformat.true_color != 0)) ||
       cl->format.r_max != g_screen_info.pixformat.r_max ||
       cl->format.g_max != g_screen_info.pixformat.g_max ||
       cl->format.b_max != g_screen_info.pixformat.b_max ||
       cl->format.r_shift != g_screen_info.pixformat.r_shift ||
       cl->format.g_shift != g_screen_info.pixformat.g_shift ||
       cl->format.b_shift != g_screen_info.pixformat.b_shift ) {

    cl->trans_table = gen_trans_table(&cl->format);
    switch(cl->format.bits_pixel) {
    case 8:
      cl->trans_func = transfunc8;
      if ( cl->format.r_max == 7 && cl->format.g_max == 7 &&
           cl->format.b_max == 3 && cl->format.r_shift == 0 &&
           cl->format.g_shift == 3 && cl->format.b_shift == 6 &&
           cl->format.true_color != 0 ) {
        cl->bgr233_f = 1;
      }
      break;
    case 16:
      cl->trans_func = transfunc16;
      break;
    case 32:
      cl->trans_func = transfunc32;
      break;
    }
    log_write(LL_DEBUG, "Pixel format translation tables prepared");

  } else {
    log_write(LL_DETAIL, "No pixel format translation needed");
  }
}

static void send_update(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  CARD8 msg_hdr[4] = {
    0, 0, 0, 1
  };
  CARD8 rect_hdr[12];
  FB_RECT rect;
  AIO_BLOCK *block;
  AIO_FUNCPTR fn = NULL;
  int raw_bytes = 0, hextile_bytes = 0;

  log_write(LL_DEBUG, "Sending framebuffer update (%d rects max) to %s",
            cl->pending_rects.num_rects, cur_slot->name);

  /* Prepare and send FramebufferUpdate message header */
  /* FIXME: Enable Tight encoding even if LastRect is not supported. */
  if (cl->enc_prefer == RFB_ENCODING_TIGHT && cl->enable_lastrect) {
    buf_put_CARD16(&msg_hdr[2], 0xFFFF);
  } else {
    buf_put_CARD16(&msg_hdr[2], cl->pending_rects.num_rects);
  }
  aio_write(NULL, msg_hdr, 4);

  /* For each pending rectangle: */
  while (rlist_pick_rect(&cl->pending_rects, &rect)) {
    log_write(LL_DEBUG, "Sending rectangle %dx%d at %d,%d to %s",
              (int)rect.w, (int)rect.h, (int)rect.x, (int)rect.y,
              cur_slot->name);

    if (rect.enc == RFB_ENCODING_NEWFBSIZE) {
      /* NewFBSize (cl->enable_newfbsize assumed to be not 0) */
      put_rect_header(rect_hdr, &rect);
      aio_write(wf_client_update_finished, rect_hdr, 12);
      return;                   /* Important! */
    } else if (rect.enc == RFB_ENCODING_COPYRECT &&
               cl->enc_enable[RFB_ENCODING_COPYRECT] ) {
      /* CopyRect */
      block = rfb_encode_copyrect_block(cl, &rect);
    } else if (cl->enc_prefer == RFB_ENCODING_TIGHT && cl->enable_lastrect) {
      /* Use Tight encoding */
      rect.enc = RFB_ENCODING_TIGHT;
      rfb_encode_tight(cl, &rect);
      continue;                 /* Important! */
    } else if ( cl->enc_prefer != RFB_ENCODING_RAW &&
                cl->enc_enable[RFB_ENCODING_HEXTILE] ) {
      /* Use Hextile encoding */
      rect.enc = RFB_ENCODING_HEXTILE;
      block = rfb_encode_hextile_block(cl, &rect);
      if (block != NULL) {
        hextile_bytes += block->data_size;
        raw_bytes += rect.w * rect.h * (cl->format.bits_pixel / 8);
      }
    } else {
      /* Use Raw encoding */
      rect.enc = RFB_ENCODING_RAW;
      block = rfb_encode_raw_block(cl, &rect);
    }

    /* If it's the last rectangle, install hook function which would
       be called after all data has been sent. But do not do that if
       we use Tight encoding since there would be one more rectangle
       (LastRect marker) */
    if (!cl->pending_rects.num_rects)
      fn = wf_client_update_finished;

    /* Send the rectangle.
       FIXME: Check for block == NULL? */
    aio_write_nocopy(fn, block);
  }

  /* Send LastRect marker. */
  if (cl->enc_prefer == RFB_ENCODING_TIGHT && cl->enable_lastrect) {
    rect.x = rect.y = rect.w = rect.h = 0;
    rect.enc = RFB_ENCODING_LASTRECT;
    put_rect_header(rect_hdr, &rect);
    aio_write(wf_client_update_finished, rect_hdr, 12);
  }
}


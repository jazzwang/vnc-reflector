/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.c,v 1.7 2001/08/04 17:29:34 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "rfblib.h"
#include "logging.h"
#include "async_io.h"
#include "client_io.h"
#include "reflector.h"
#include "encode.h"
#include "d3des.h"

static unsigned char *s_password;

/*
 * Prototypes for static functions
 */

static void cf_client(void);
static void rf_client_ver(void);
static void rf_client_auth(void);
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
  /* FIXME: Function naming is bad (client_accept_hook?). */
  /* FIXME: Store client address in an AIO_SLOT structure clone */

  aio_setclose(cf_client);

  log_write(LL_MSG, "Accepted connection from %s", cur_slot->name);

  aio_write(NULL, "RFB 003.003\n", 12);
  aio_setread(rf_client_ver, NULL, 12);
}

static void cf_client(void)
{
  if (cur_slot->errread_f) {
    log_write(LL_WARN, "Error reading from %s: %s",
              cur_slot->name, strerror(cur_slot->io_errno));
  } else if (cur_slot->errwrite_f) {
    log_write(LL_WARN, "Error sending to %s: %s",
              cur_slot->name, strerror(cur_slot->io_errno));
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

  /* Request VNC authentication */
  buf_put_CARD32(cl->msg_buf, 2);

  /* Prepare "random" challenge */
  srandom((unsigned int)time(NULL));
  for (i = 0; i < 16; i++)
    cl->msg_buf[i + 4] = (unsigned char)random();

  aio_write(NULL, cl->msg_buf, 20);
  aio_setread(rf_client_auth, NULL, 16);
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
    aio_write(NULL, buf, 4);
    aio_close(0);
  } else {
    log_write(LL_MSG, "Authentication passed by %s", cur_slot->name);
    buf_put_CARD32(buf, 0);
    aio_write(NULL, buf, 4);
    aio_setread(rf_client_initmsg, NULL, 1);
  }
}

static void rf_client_initmsg(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
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

  /* The client did not requested framebuffer updates yet */
  cl->update_requested = 0;
  cl->update_in_progress = 0;
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
  /* FIXME: Skip FixColourMapEntries properly. */
  aio_setread(rf_client_colormap_data, NULL, 1);
}

static void rf_client_colormap_data(void)
{
  /* FIXME: Skip FixColourMapEntries properly. */
  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_encodings_hdr(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  cl->enc_count = buf_get_CARD16(&cur_slot->readbuf[1]);
  aio_setread(rf_client_encodings_data, NULL, cl->enc_count * sizeof(CARD32));
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
  for (i = 0; i < (int)cl->enc_count; i++) {
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

  if (!cur_slot->readbuf[0]) {
    cl->update_full = 1;
    log_write(LL_DEBUG, "Received framebuffer update request (full) from %s",
              cur_slot->name);
  } else {
    log_write(LL_DEBUG, "Received framebuffer update request from %s",
              cur_slot->name);
  }

  if (cl->update_in_progress) {
    cl->update_requested = 1;
  } else {
    send_update();
    cl->update_in_progress = 1;
    cl->update_requested = 0;
    cl->update_full = 0;
  }

  aio_setread(rf_client_msg, NULL, 1);
}

static void wf_client_update_finished(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;

  log_write(LL_DEBUG, "Finished sending framebuffer update to %s",
            cur_slot->name);

  cl->update_in_progress = 0;
  if (cl->update_requested) {
    send_update();
    cl->update_in_progress = 1;
    cl->update_requested = 0;
    cl->update_full = 0;
  }
}

static void rf_client_keyevent(void)
{
  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_ptrevent(void)
{
  aio_setread(rf_client_msg, NULL, 1);
}

static void rf_client_cuttext_hdr(void)
{
  aio_setread(rf_client_cuttext_data, NULL, 1);
}

static void rf_client_cuttext_data(void)
{
  aio_setread(rf_client_msg, NULL, 1);
}

/*
 * Non-callback functions
 */

static void send_update(void)
{
  CL_SLOT *cl = (CL_SLOT *)cur_slot;
  unsigned char msg_hdr[16] = {
    0, 0, 0, 1,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
  };

  /* FIXME: This sends the whole framebuffer to the client! */

  log_write(LL_DEBUG, "Sending framebuffer update to %s",
            cur_slot->name);

  buf_put_CARD16(&msg_hdr[8], g_screen_info->width);
  buf_put_CARD16(&msg_hdr[10], g_screen_info->height);

  aio_write(NULL, msg_hdr, 16);
  aio_write_nocopy(wf_client_update_finished,
                   rfb_encode_raw_block(&cl->format, 0, 0,
                                        g_screen_info->width,
                                        g_screen_info->height));
}


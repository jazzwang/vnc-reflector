/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.c,v 1.5 2001/08/03 07:34:57 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "logging.h"
#include "async_io.h"
#include "client_io.h"
#include "d3des.h"

static unsigned char *s_password;

/*
 * Prototypes for static functions
 */

static void cf_client(void);
static void rf_client_ver(void);
static void rf_client_auth(void);
static void rf_client_initmsg(void);

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
    log_write(LL_WARN, "Error reading data from %s", cur_slot->name);
  } else if (cur_slot->errwrite_f) {
    log_write(LL_WARN, "Error sending data to %s", cur_slot->name);
  }
  log_write(LL_MSG, "Closing client connection %s", cur_slot->name);
}

static void rf_client_ver(void)
{
  CL_SLOT *csl = (CL_SLOT *)cur_slot;
  int i;

  /* FIXME: Check protocol version. */

  /* FIXME: Functions like authentication should be available in
     separate modules, not in I/O part of the code. */
  /* FIXME: Higher level I/O functions should be implemented
     instead of things like buf_put_CARD32 + aio_write. */

  log_write(LL_DETAIL, "Client supports %.11s", cur_slot->readbuf);

  /* Request VNC authentication */
  buf_put_CARD32(csl->msg_buf, 2);

  /* Prepare "random" challenge */
  srandom((unsigned int)time(NULL));
  for (i = 0; i < 16; i++)
    csl->msg_buf[i + 4] = (unsigned char)random();

  aio_write(NULL, csl->msg_buf, 20);
  aio_setread(rf_client_auth, NULL, 16);
}

static void rf_client_auth(void)
{
  CL_SLOT *csl = (CL_SLOT *)cur_slot;
  unsigned char key[8];
  unsigned char buf[16];

  memset(key, 0, 8);
  strncpy((char *)key, s_password, 8);

  /* Put correct crypted response to buf */
  deskey(key, EN0);
  des(csl->msg_buf + 4, buf);
  des(csl->msg_buf + 12, buf + 8);

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
  if (cur_slot->readbuf[0] == 0) {
    log_write(LL_WARN, "Non-shared session requested by %s", cur_slot->name);
    aio_close(0);
  }

  /* FIXME: Send ServerInitialisation message and so on... */
  aio_setread(rf_client_initmsg, NULL, 1);
}


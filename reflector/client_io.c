/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.c,v 1.2 2001/08/02 16:47:45 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "logging.h"
#include "async_io.h"
#include "d3des.h"

static unsigned char s_buf[20];
static unsigned char s_crypted[16];
static unsigned char *s_password;

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
  /* FIXME: Store client address in an AIO_SLOT structure clone */

  log_write(LL_MSG, "Accepted connection from [unknown address]");

  aio_write(NULL, "RFB 003.003\n", 12);
  aio_setread(rf_client_ver, NULL, 12);
}

static void rf_client_ver(void)
{
  int i;

  /* FIXME: Check protocol version. */

  /* FIXME: Functions like authentication should be available in
     separate modules, not in I/O part of the code. */
  /* FIXME: Higher level I/O functions should be implemented
     instead of things like buf_put_CARD32 + aio_write. */

  log_write(LL_DETAIL, "Client supports %.11s", cur_slot->readbuf);

  /* Request VNC authentication */
  buf_put_CARD32(s_buf, (CARD32)2);

  /* Prepare "random" challenge */
  srandom((unsigned int)time(NULL));
  for (i = 0; i < 16; i++)
    s_buf[i + 4] = (unsigned char)random();

  aio_write(NULL, s_buf, 20);
  aio_setread(rf_client_auth, NULL, 16);
}

static void rf_client_auth(void)
{
  unsigned char key[8];

  memset(key, 0, 8);
  strncpy((char *)key, s_password, 8);

  deskey(key, EN0);
  des(s_buf + 4, s_crypted);
  des(s_buf + 12, s_crypted + 8);

  if (memcmp(cur_slot->readbuf, s_crypted, 16) != 0) {
    log_write(LL_WARN, "Authentication failed for [unknown address]");
    /* FIXME: Implement "too many tries" functionality some day */
    buf_put_CARD32(s_buf, (CARD32)1);
    aio_write(NULL, s_buf, 4);
    /* FIXME: Disconnect the client */
    aio_setread(rf_client_ver, NULL, 16);
  } else {
    log_write(LL_MSG, "Authentication passed by [unknown address]");
    buf_put_CARD32(s_buf, (CARD32)0);
    aio_write(NULL, s_buf, 4);
    aio_setread(rf_client_initmsg, NULL, 1);
  }
}

static void rf_client_initmsg(void)
{
  if (cur_slot->readbuf[0] == 0) {
    log_write(LL_WARN, "Non-shared session requested by [unknown address]");
    /* FIXME: Disconnect the client, we accept only shared sessions. */
    aio_setread(rf_client_initmsg, NULL, 1);
  }

  /* FIXME: Send ServerInitialisation message and so on... */
  aio_setread(rf_client_initmsg, NULL, 1);
}


/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.c,v 1.1 2001/08/02 15:33:05 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#include <stdio.h>
#include <sys/types.h>

#include "rfblib.h"
#include "logging.h"
#include "async_io.h"

static void rf_client_ver(void);

/*
 * Implementation
 */

void af_client_accept(void)
{
  log_write(LL_MSG, "Accepted connection from [unknown address]");

  aio_write(NULL, "RFB 003.003\n", 12);
  aio_setread(rf_client_ver, NULL, 12);
}

static void rf_client_ver(void)
{
  log_write(LL_DETAIL, "Client supports %.11s", cur_slot->readbuf);
  aio_setread(rf_client_ver, NULL, 12);
}

/* VNC Reflector
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: control.c,v 1.2 2001/08/23 15:24:51 const Exp $
 * Processing signals to control reflector
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>

#include "rfblib.h"
#include "async_io.h"
#include "logging.h"
#include "reflector.h"
#include "host_connect.h"
#include "host_io.h"
#include "rect.h"
#include "translate.h"
#include "client_io.h"
#include "control.h"

#define FUNC_CL_DISCONNECT   0
#define FUNC_HOST_RECONNECT  1

static void sh_disconnect_clients(int signo);
static void sh_reconnect_to_host(int signo);
static void safe_disconnect_clients(void);
static void fn_disconnect_client(AIO_SLOT *slot);
static void safe_reconnect_to_host(void);
static void fn_reconnect_to_host(AIO_SLOT *slot);

/*
 * Function visible from outside
 */

void set_control_signals(void)
{
  signal(SIGHUP, sh_disconnect_clients);
  signal(SIGUSR1, sh_reconnect_to_host);
  signal(SIGUSR2, SIG_IGN);
}

/*
 * Signal handlers
 */

static void sh_disconnect_clients (int signo)
{
  aio_call_func(safe_disconnect_clients, FUNC_CL_DISCONNECT);
  signal(signo, sh_disconnect_clients);
}

static void sh_reconnect_to_host (int signo)
{
  aio_call_func(safe_reconnect_to_host, FUNC_HOST_RECONNECT);
  signal(signo, sh_reconnect_to_host);
}

/*
 * Disconnecting all clients on SIGHUP
 */

static void safe_disconnect_clients(void)
{
  log_write(LL_WARN, "Caught SIGHUP signal, disconnecting all clients");
  aio_walk_slots(fn_disconnect_client, TYPE_CL_SLOT);
}

static void fn_disconnect_client(AIO_SLOT *slot)
{
  AIO_SLOT *saved_slot = cur_slot;

  cur_slot = slot;
  aio_close(0);
  cur_slot = saved_slot;
}

static void safe_reconnect_to_host(void)
{
  log_write(LL_WARN, "Caught SIGUSR1 signal, re-connecting");

  /* If host connection is active, aio_walk_slots would return 1 and
     we would request re-connect after current host connection is
     closed (fn_reconnect_to_host function). Otherwise (if there is no
     host connection), just connect immediately. */

  if (aio_walk_slots(fn_reconnect_to_host, TYPE_HOST_SLOT) == 0)
    connect_to_host(NULL, 0);
}

static void fn_reconnect_to_host(AIO_SLOT *slot)
{
  AIO_SLOT *saved_slot = cur_slot;

  cur_slot = slot;
  aio_close(0);
  host_request_reconnect();
  cur_slot = saved_slot;
}

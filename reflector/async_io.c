/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: async_io.c,v 1.6 2001/08/04 12:26:03 const Exp $
 * Asynchronous file/socket I/O
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#include "async_io.h"

/*
 * Global variables
 */

AIO_SLOT *cur_slot;

/*
 * Static variables
 */

static fd_set s_fdset_read;
static fd_set s_fdset_write;
static int s_max_fd;
static AIO_FUNCPTR s_idle_func;
static AIO_SLOT *s_first_slot;
static AIO_SLOT *s_last_slot;

static int s_listen_fd;
static AIO_FUNCPTR s_accept_func;
static size_t s_new_slot_size;

static int s_close_f;

/*
 * Prototypes for static functions
 */

static void aio_process_input(AIO_SLOT *slot);
static void aio_process_output(AIO_SLOT *slot);
static void aio_accept_connection(void);
static void aio_destroy_slot(AIO_SLOT *slot, int fatal);


/*
 * Implementation
 */

/*
 * Initialize I/O sybsystem. This function should be called prior to
 * any other function herein and should NOT be called from within
 * event loop, from callback functions.
 */

void aio_init(void)
{
  signal(SIGPIPE, SIG_IGN);
  FD_ZERO(&s_fdset_read);
  FD_ZERO(&s_fdset_write);
  s_max_fd = 0;
  s_listen_fd = -1;
  s_idle_func = NULL;
  s_first_slot = NULL;
  s_last_slot = NULL;
  s_close_f = 0;
}

/*
 * Create I/O slot for existing connection (open file). After new slot
 * was created, initfunc would be called with cur_slot pointing to
 * that slot. To allow reading from provided descriptor, initfunc
 * should set some input handler using aio_setread() function.
 */

void aio_add_slot(int fd, char *name, AIO_FUNCPTR initfunc, size_t slot_size)
{
  size_t size;
  AIO_SLOT *slot, *saved_slot;

  /* FIXME: Check return value after calloc(). */

  size = (slot_size > sizeof(AIO_SLOT)) ? slot_size : sizeof(AIO_SLOT);
  slot = calloc(1, size);

  slot->type = 0;
  slot->fd = fd;
  slot->bytes_to_read = 0;
  slot->outqueue = NULL;
  slot->closefunc = NULL;
  slot->alloc_f = 0;
  slot->close_f = 0;
  slot->errread_f = 0;
  slot->errwrite_f = 0;
  slot->next = NULL;

  if (name != NULL) {
    slot->name = strdup(name);
  } else {
    slot->name = strdup("[unknown]");
  }

  if (s_last_slot == NULL) {
    /* This is the first slot */
    s_first_slot = slot;
    slot->prev = NULL;
  } else {
    /* Other slots exist */
    s_last_slot->next = slot;
    slot->prev = s_last_slot;
  }
  s_last_slot = slot;

  /* Put fd into non-blocking mode */
  /* FIXME: check return value? */
  fcntl (fd, F_SETFL, O_NONBLOCK);

  FD_SET(fd, &s_fdset_read);
  if (fd > s_max_fd)
    s_max_fd = fd;

  /* Saving cur_slot value, calling initfunc with different cur_slot */
  saved_slot = cur_slot;
  cur_slot = slot;
  (*initfunc)();
  cur_slot = saved_slot;
}

/*
 * Create listening socket. All connections would be accepted
 * automatically and initfunc would be called for each new slot
 * created on incoming connection.
 * NOTE: only one listening socket is supported at this time.
 */

int aio_listen(int port, AIO_FUNCPTR acceptfunc, size_t slot_size)
{
  struct sockaddr_in listen_addr;
  int optval = 1;

  /* acceptfunc must be provided, otherwise we don't know
     how to handle input and what to send as output. */
  if (acceptfunc == NULL)
    return 0;
  
  s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (s_listen_fd < 0)
    return 0;

  if (setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR,
                 &optval, sizeof(int)) != 0) {
    close(s_listen_fd);
    s_listen_fd = -1;
    return 0;
  }

  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  listen_addr.sin_port = htons((unsigned short)port);

  if ( bind(s_listen_fd, (struct sockaddr *)&listen_addr,
            sizeof(listen_addr)) != 0 ||
       fcntl(s_listen_fd, F_SETFL, O_NONBLOCK) != 0 ||
       listen (s_listen_fd, 5) != 0) {
    close(s_listen_fd);
    s_listen_fd = -1;
    return 0;
  }

  FD_SET(s_listen_fd, &s_fdset_read);
  if (s_listen_fd > s_max_fd)
    s_max_fd = s_listen_fd;

  s_accept_func = acceptfunc;
  s_new_slot_size = slot_size;

  return 1;
}

/*
 * Function to close connection slot. Operates on *cur_slot.
 * If fatal is not 0 then close all other slots and quit
 * event loop. Note that a slot would not be destroyed right
 * on this function call, this would be done later, at the end
 * of main loop cycle.
 */

/* FIXME: Implement "closing after all data has been sent" */

void aio_close(int fatal)
{
  cur_slot->close_f = 1;

  if (fatal)
    s_close_f = 1;
}

/*
 * Main event loop. It watches for possibility to perform I/O
 * operations on descriptors and dispatches results to custom
 * callback functions.
 */

/* FIXME: Implement configurable network timeout. */

void aio_mainloop(void)
{
  fd_set fdset_r, fdset_w;
  struct timeval timeout;
  AIO_SLOT *slot, *next_slot;

  while (!s_close_f) {
    memcpy(&fdset_r, &s_fdset_read, sizeof(fd_set));
    memcpy(&fdset_w, &s_fdset_write, sizeof(fd_set));
    timeout.tv_sec = 10;        /* Ten seconds timeout */
    timeout.tv_usec = 0;
    if (select (s_max_fd + 1, &fdset_r, &fdset_w, NULL, &timeout) > 0) {
      slot = s_first_slot;
      while (slot != NULL && !s_close_f) {
        next_slot = slot->next;
        if (FD_ISSET(slot->fd, &fdset_w))
          aio_process_output(slot);
        if (FD_ISSET(slot->fd, &fdset_r))
          aio_process_input(slot);
        if (slot->close_f)
          aio_destroy_slot(slot, 0);
        slot = next_slot;
      }
      if (FD_ISSET(s_listen_fd, &fdset_r) && !s_close_f)
        aio_accept_connection();
    } else {
      /* Do something in idle periods */
      if (s_idle_func != NULL)
        (*s_idle_func)();
    }
  }
  /* Stop listening, close all slots and exit */
  close(s_listen_fd);
  for (slot = s_first_slot; slot != NULL; slot = slot->next)
    aio_destroy_slot(slot, 1);
}

void aio_setread(AIO_FUNCPTR fn, void *inbuf, int bytes_to_read)
{
  /* FIXME: Check for close_f before the work? */

  if (cur_slot->alloc_f) {
    free(cur_slot->readbuf);
    cur_slot->alloc_f = 0;
  }

  /* NOTE: readfunc must be real, not NULL */
  cur_slot->readfunc = fn;

  if (inbuf != NULL) {
    cur_slot->readbuf = inbuf;
  } else {
    if (bytes_to_read <= sizeof(cur_slot->buf256)) {
      cur_slot->readbuf = cur_slot->buf256;
    } else {
      cur_slot->readbuf = malloc(bytes_to_read);
      if (cur_slot->readbuf != NULL) {
        cur_slot->alloc_f = 1;
      } else {
        cur_slot->close_f = 1;
      }
    }
  }
  cur_slot->bytes_to_read = bytes_to_read;
  cur_slot->bytes_ready = 0;
}

void aio_write(AIO_FUNCPTR fn, void *outbuf, int bytes_to_write)
{
  AIO_BLOCK *block;

  /* FIXME: Check for close_f before the work? */
  /* FIXME: Check return value after malloc(). */
  /* FIXME: Provide a function that do not use memcpy(). */
  /* FIXME: Join small blocks together? */
  /* FIXME: Support small static buffer as in reading? */

  /* By the way, writefunc may be NULL */
  cur_slot->writefunc = fn;

  block = malloc(sizeof(AIO_BLOCK) + bytes_to_write - 1);
  block->data_size = bytes_to_write;
  block->next = NULL;
  memcpy(block->data, outbuf, bytes_to_write);

  if (cur_slot->outqueue == NULL) {
    /* Output queue was empty */
    cur_slot->outqueue = block;
    cur_slot->bytes_written = 0;
    FD_SET(cur_slot->fd, &s_fdset_write);
  } else {
    /* Output queue was not empty */
    cur_slot->outqueue_last->next = block;
  }
  cur_slot->outqueue_last = block;
}

void aio_setclose(AIO_FUNCPTR closefunc)
{
  cur_slot->closefunc = closefunc;
}

/***************************
 * Static functions follow
 */

static void aio_process_input(AIO_SLOT *slot)
{
  int bytes;

  /* FIXME: Do not read anything if readfunc is not set?
     Or maybe skip everything we're receiving?
     Or better destroy the slot? -- I think yes. */

  if (!slot->close_f) {
    bytes = read(slot->fd, slot->readbuf + slot->bytes_ready,
                 slot->bytes_to_read - slot->bytes_ready);

    if (bytes > 0) {
      slot->bytes_ready += bytes;
      if (slot->bytes_ready == slot->bytes_to_read) {
        cur_slot = slot;
        (*slot->readfunc)();
      }
    } else {
      slot->close_f = 1;
      slot->errread_f = 1;
    }
  }
}

static void aio_process_output(AIO_SLOT *slot)
{
  int bytes;
  AIO_BLOCK *next;

  /* FIXME: Maybe write all blocks in a loop. */

  if (!slot->close_f) {
    bytes = write(slot->fd, slot->outqueue->data + slot->bytes_written,
                  slot->outqueue->data_size - slot->bytes_written);

    if (bytes > 0) {
      slot->bytes_written += bytes;
      if (slot->bytes_written == slot->outqueue->data_size) {
        next = slot->outqueue->next;
        if (next != NULL) {
          /* Block sent, free it and go to the next block */
          free(slot->outqueue);
          slot->outqueue = next;
          slot->bytes_written = 0;
        } else {
          /* Last block sent, free it and call writefunc */
          free(slot->outqueue);
          slot->outqueue = NULL;
          FD_CLR(slot->fd, &s_fdset_write);
          if (slot->writefunc != NULL) {
            cur_slot = slot;
            (*slot->writefunc)();
          }
        }
      }
    } else {
      slot->close_f = 1;
      slot->errwrite_f = 1;
    }
  }
}

static void aio_accept_connection(void)
{
  AIO_SLOT *slot;
  struct sockaddr_in client_addr;
  int len, fd;

  len = sizeof(client_addr);
  fd = accept(s_listen_fd, (struct sockaddr *) &client_addr, &len);
  if (fd < 0)
    return;

  aio_add_slot(fd, inet_ntoa(client_addr.sin_addr), s_accept_func,
               s_new_slot_size);
}

/*
 * Destroy a slot, free all its memory etc. If fatal != 0, assume all
 * slots would be removed one after another so do not care about such
 * things as correctness of slot list links, setfd_* masks etc.
 */

/* FIXME: Dangerous. Changes slot list while we might iterate over it. */

static void aio_destroy_slot(AIO_SLOT *slot, int fatal)
{
  AIO_BLOCK *block, *next_block;

  /* Call on-close hook */
  if (slot->closefunc != NULL) {
    cur_slot = slot;
    (*slot->closefunc)();
  }

  if (!fatal) {
    /* Remove from the slot list */
    if (slot->prev == NULL)
      s_first_slot = slot->next;
    else
      slot->prev->next = slot->next;
    if (slot->next == NULL)
      s_last_slot = slot->prev;
    else
      slot->next->prev = slot->prev;

    /* Remove references to descriptor */
    FD_CLR(slot->fd, &s_fdset_read);
    FD_CLR(slot->fd, &s_fdset_write);
    if (slot->fd == s_max_fd) {
      /* NOTE: Better way is to find _existing_ max fd */
      s_max_fd--;
    }
  }

  /* Free all memory in slave structures */
  block = slot->outqueue;
  while (block != NULL) {
    next_block = block->next;
    free(block);
    block = next_block;
  }
  free(slot->name);
  if (slot->alloc_f)
    free(slot->readbuf);

  /* Close the file and free the slot itself */
  close(slot->fd);
  free(slot);
}


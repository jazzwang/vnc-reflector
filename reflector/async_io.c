/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: async_io.c,v 1.2 2001/08/02 11:13:38 const Exp $
 * Asynchronous file/socket I/O
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>

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
static int s_listen_fd;
static AIO_FUNCPTR s_idle_func;
static AIO_SLOT *s_first_slot;
static AIO_SLOT *s_last_slot;

/*
 * Prototypes for static functions
 */

static void aio_process_input(AIO_SLOT *slot);
static void aio_process_output(AIO_SLOT *slot);

/*
 * Implementation
 */

void aio_init(void)
{
  /* NOTE: it should not be called after any other function is done. */

  FD_ZERO(&s_fdset_read);
  FD_ZERO(&s_fdset_write);
  s_max_fd = 0;
  s_listen_fd = -1;
  s_idle_func = NULL;
  s_first_slot = NULL;
  s_last_slot = NULL;
}

void aio_add_slot(int fd, AIO_FUNCPTR initfunc, int type, size_t slot_size)
{
  size_t size;
  AIO_SLOT *slot, *saved_slot;

  /* NOTE: initfunc must set input handler using aio_setread(). */
  /* FIXME: Check return value after calloc(). */

  size = (slot_size > sizeof(AIO_SLOT)) ? size : sizeof(AIO_SLOT);
  slot = calloc(1, size);

  slot->type = type;
  slot->fd = fd;
  slot->bytes_to_read = 0;
  slot->outqueue = NULL;
  slot->errorfunc = NULL;
  slot->alloc_f = 0;
  slot->error_f = 0;
  slot->next = NULL;

  if (s_last_slot == NULL) {
    /* This is the first slot */
    s_first_slot = slot;
  } else {
    /* Other slots exist */
    s_last_slot->next = slot;
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

void aio_mainloop(void)
{
  fd_set fdset_r, fdset_w;
  struct timeval timeout;
  AIO_SLOT *slot;

  while (1) {
    memcpy(&fdset_r, &s_fdset_read, sizeof(fd_set));
    memcpy(&fdset_w, &s_fdset_write, sizeof(fd_set));
    timeout.tv_sec = 10;        /* Ten seconds timeout */
    timeout.tv_usec = 0;
    if (select (s_max_fd + 1, &fdset_r, &fdset_w, NULL, &timeout) > 0) {
      for (slot = s_first_slot; slot != NULL; slot = slot->next) {
        if (FD_ISSET(slot->fd, &fdset_w))
          aio_process_output(slot);
        if (FD_ISSET(slot->fd, &fdset_r))
          aio_process_input(slot);
      }
    } else {
      /* Do something in idle periods */
      if (s_idle_func != NULL)
        (*s_idle_func)();
    }
  }
}

void aio_setread(AIO_FUNCPTR fn, void *inbuf, int bytes_to_read)
{
  /* FIXME: Check for error_f before the work? */

  if (cur_slot->alloc_f) {
    free(cur_slot->readbuf);
    cur_slot->alloc_f = 0;
  }

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
        cur_slot->error_f = 1;
      }
    }
  }
  cur_slot->bytes_to_read = bytes_to_read;
  cur_slot->bytes_ready = 0;
}

void aio_write(AIO_FUNCPTR fn, void *outbuf, int bytes_to_write)
{
  AIO_BLOCK *block;

  /* FIXME: Check for error_f before the work? */
  /* FIXME: Check return value after malloc(). */
  /* FIXME: Provide a function that do not use memcpy(). */

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

static void aio_process_input(AIO_SLOT *slot)
{
  int bytes = -1;

  /* FIXME: Do not read anything if readfunc is not set */

  if (!slot->error_f) {
    bytes = read(slot->fd, slot->readbuf + slot->bytes_ready,
                 slot->bytes_to_read - slot->bytes_ready);
  }
  if (bytes >= 0) {
    slot->bytes_ready += bytes;
    if (slot->bytes_ready == slot->bytes_to_read) {
      cur_slot = slot;
      (*slot->readfunc)();
    }
  } else {
    /* FIXME: Close the slot on error */
  }
}

static void aio_process_output(AIO_SLOT *slot)
{
  int bytes = -1;
  AIO_BLOCK *next;

  /* FIXME: Maybe write all blocks in a loop */

  if (!slot->error_f) {
    bytes = write(slot->fd, slot->outqueue->data + slot->bytes_written,
                  slot->outqueue->data_size - slot->bytes_written);
  }

  if (bytes >= 0) {
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
    /* FIXME: Close the slot on error */
  }
}


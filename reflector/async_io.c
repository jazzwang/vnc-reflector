/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: async_io.c,v 1.14 2001/08/23 10:52:32 const Exp $
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
#include <errno.h>

#ifdef USE_POLL
#include <sys/poll.h>
#define FD_ARRAY_MAXSIZE  10000
#endif

#include "async_io.h"

/*
 * Global variables
 */

AIO_SLOT *cur_slot;

/*
 * Static variables
 */

#ifdef USE_POLL
static struct pollfd s_fd_array[FD_ARRAY_MAXSIZE];
static unsigned int s_fd_array_size;
static int s_listen_fd_idx;
#else
static fd_set s_fdset_read;
static fd_set s_fdset_write;
static int s_max_fd;
#endif

static AIO_FUNCPTR s_idle_func;
static AIO_SLOT *s_first_slot;
static AIO_SLOT *s_last_slot;

static int s_listen_fd;
static AIO_FUNCPTR s_accept_func;
static size_t s_new_slot_size;

static int s_sig_func_set;
static AIO_FUNCPTR s_sig_func[10];

static int s_close_f;

/*
 * Prototypes for static functions
 */

static void aio_process_input(AIO_SLOT *slot);
static void aio_process_output(AIO_SLOT *slot);
static void aio_process_func_list(void);
static void aio_accept_connection(void);
static void aio_destroy_slot(AIO_SLOT *slot, int fatal);

static void sh_interrupt (int signo);


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
  int i;

#ifdef USE_POLL
  s_fd_array_size = 0;
#else
  FD_ZERO(&s_fdset_read);
  FD_ZERO(&s_fdset_write);
  s_max_fd = 0;
#endif
  s_listen_fd = -1;
  s_idle_func = NULL;
  s_first_slot = NULL;
  s_last_slot = NULL;
  s_close_f = 0;

  s_sig_func_set = 0;
  for (i = 0; i < 10; i++)
    s_sig_func[i] = NULL;
}

/*
 * Create I/O slot for existing connection (open file). After new slot
 * has been created, initfunc would be called with cur_slot pointing
 * to that slot. To allow reading from provided descriptor, initfunc
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
  slot->errio_f = 0;
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
  fcntl(fd, F_SETFL, O_NONBLOCK);

#ifdef USE_POLL
  /* FIXME: do something better if s_fd_array_size exceeds max size? */
  if (s_fd_array_size < FD_ARRAY_MAXSIZE) {
    slot->idx = s_fd_array_size++;
    s_fd_array[slot->idx].fd = fd;
    s_fd_array[slot->idx].events = POLLIN;
  }
#else
  FD_SET(fd, &s_fdset_read);
  if (fd > s_max_fd)
    s_max_fd = fd;
#endif

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
       listen(s_listen_fd, 5) != 0 ) {
    close(s_listen_fd);
    s_listen_fd = -1;
    return 0;
  }

#ifdef USE_POLL
  /* FIXME: do something better if s_fd_array_size exceeds max size? */
  if (s_fd_array_size < FD_ARRAY_MAXSIZE) {
    s_listen_fd_idx = s_fd_array_size++;
    s_fd_array[s_listen_fd_idx].fd = s_listen_fd;
    s_fd_array[s_listen_fd_idx].events = POLLIN;
  }
#else
  FD_SET(s_listen_fd, &s_fdset_read);
  if (s_listen_fd > s_max_fd)
    s_max_fd = s_listen_fd;
#endif

  s_accept_func = acceptfunc;
  s_new_slot_size = slot_size;

  return 1;
}

/*
 * Iterate over a list of connection slots with specified type.
 */

void aio_walk_slots(AIO_FUNCPTR fn, int type)
{
  AIO_SLOT *slot, *next_slot;

  slot = s_first_slot;
  while (slot != NULL && !s_close_f) {
    next_slot = slot->next;
    if (slot->type == type) {
      (*fn)(slot);
    }
    slot = next_slot;
  }
}

/*
 * This function should be called if we have to execute a function
 * when I/O state is consistent, but currently we are not sure if it's
 * safe (e.g. to be called from signal handlers). fn_type should be a
 * number in the range of 0..9 and if there are two or more functions
 * of the same fn_type set, only one of them would be called
 * (probably, the latest set).
 */

void aio_call_func(AIO_FUNCPTR fn, int fn_type)
{
  if (fn_type < 10) {
    s_sig_func[fn_type] = fn;
    s_sig_func_set = 1;
  }
}

/*
 * Function to close connection slot. Operates on *cur_slot.
 * If fatal is not 0 then close all other slots and quit
 * event loop. Note that a slot would not be destroyed right
 * on this function call, this would be done later, at the end
 * of main loop cycle.
 */

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
 *
 * Here are two versions, one uses poll(2) syscall, another uses
 * select(2) instead. Note that select(2) is more portable while
 * poll(2) is less limited.
 */

/* FIXME: Implement configurable network timeout. */

#ifdef USE_POLL

void aio_mainloop(void)
{
  AIO_SLOT *slot, *next_slot;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGTERM, sh_interrupt);
  signal(SIGINT, sh_interrupt);

  if (s_sig_func_set)
    aio_process_func_list();

  while (!s_close_f) {
    if (poll(s_fd_array, s_fd_array_size, 1000) > 0) {
      slot = s_first_slot;
      while (slot != NULL && !s_close_f) {
        next_slot = slot->next;
        if (s_sig_func_set)
          aio_process_func_list();
        if (s_fd_array[slot->idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
          slot->errio_f = 1;
          slot->close_f = 1;
        } else {
          if (s_fd_array[slot->idx].revents & POLLOUT)
            aio_process_output(slot);
          if ((s_fd_array[slot->idx].revents & POLLIN) && !slot->close_f)
            aio_process_input(slot);
        }
        if (slot->close_f)
          aio_destroy_slot(slot, 0);
        slot = next_slot;
      }
      if ( s_listen_fd != -1 &&
           (s_fd_array[s_listen_fd_idx].revents & POLLIN) &&
           !s_close_f )
        aio_accept_connection();
      if (s_sig_func_set)
        aio_process_func_list();
    } else {
      if (s_sig_func_set)
        aio_process_func_list();
      else if (s_idle_func != NULL)
        (*s_idle_func)();       /* Do something in idle periods */
    }
  }
  /* Stop listening, close all slots and exit */
  close(s_listen_fd);
  for (slot = s_first_slot; slot != NULL; slot = slot->next)
    aio_destroy_slot(slot, 1);
}

#else

void aio_mainloop(void)
{
  fd_set fdset_r, fdset_w;
  struct timeval timeout;
  AIO_SLOT *slot, *next_slot;

  if (s_sig_func_set)
    aio_process_func_list();

  while (!s_close_f) {
    memcpy(&fdset_r, &s_fdset_read, sizeof(fd_set));
    memcpy(&fdset_w, &s_fdset_write, sizeof(fd_set));
    timeout.tv_sec = 1;         /* One second timeout */
    timeout.tv_usec = 0;
    if (select(s_max_fd + 1, &fdset_r, &fdset_w, NULL, &timeout) > 0) {
      slot = s_first_slot;
      while (slot != NULL && !s_close_f) {
        next_slot = slot->next;
        if (s_sig_func_set)
          aio_process_func_list();
        if (FD_ISSET(slot->fd, &fdset_w))
          aio_process_output(slot);
        if (FD_ISSET(slot->fd, &fdset_r) && !slot->close_f)
          aio_process_input(slot);
        if (slot->close_f)
          aio_destroy_slot(slot, 0);
        slot = next_slot;
      }
      if (s_listen_fd != -1 && FD_ISSET(s_listen_fd, &fdset_r) && !s_close_f)
        aio_accept_connection();
      if (s_sig_func_set)
        aio_process_func_list();
    } else {
      if (s_sig_func_set)
        aio_process_func_list();
      else if (s_idle_func != NULL)
        (*s_idle_func)();       /* Do something in idle periods */
    }
  }
  /* Stop listening, close all slots and exit */
  close(s_listen_fd);
  for (slot = s_first_slot; slot != NULL; slot = slot->next)
    aio_destroy_slot(slot, 1);
}

#endif /* USE_POLL */

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

  /* FIXME: Join small blocks together? */
  /* FIXME: Support small static buffer as in reading? */

  block = malloc(sizeof(AIO_BLOCK) + bytes_to_write);
  if (block != NULL) {
    block->data_size = bytes_to_write;
    memcpy(block->data, outbuf, bytes_to_write);
    aio_write_nocopy(fn, block);
  }
}

void aio_write_nocopy(AIO_FUNCPTR fn, AIO_BLOCK *block)
{
  if (block != NULL) {
    /* By the way, fn may be NULL */
    block->func = fn;

    if (cur_slot->outqueue == NULL) {
      /* Output queue was empty */
      cur_slot->outqueue = block;
      cur_slot->bytes_written = 0;
#ifdef USE_POLL
      s_fd_array[cur_slot->idx].events |= POLLOUT;
#else
      FD_SET(cur_slot->fd, &s_fdset_write);
#endif
    } else {
      /* Output queue was not empty */
      cur_slot->outqueue_last->next = block;
    }

    cur_slot->outqueue_last = block;
    block->next = NULL;
  }
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
    errno = 0;
    bytes = read(slot->fd, slot->readbuf + slot->bytes_ready,
                 slot->bytes_to_read - slot->bytes_ready);

    if (bytes > 0) {
      slot->bytes_ready += bytes;
      if (slot->bytes_ready == slot->bytes_to_read) {
        cur_slot = slot;
        (*slot->readfunc)();
      }
    } else if (bytes == 0 || (bytes < 0 && errno != EAGAIN)) {
      slot->close_f = 1;
      slot->errio_f = 1;
      slot->errread_f = 1;
      slot->io_errno = errno;
    }
  }
}

static void aio_process_output(AIO_SLOT *slot)
{
  int bytes;
  AIO_BLOCK *next;

  /* FIXME: Maybe write all blocks in a loop. */

  if (!slot->close_f) {
    errno = 0;
    bytes = write(slot->fd, slot->outqueue->data + slot->bytes_written,
                  slot->outqueue->data_size - slot->bytes_written);

    if (bytes > 0) {
      slot->bytes_written += bytes;
      if (slot->bytes_written == slot->outqueue->data_size) {
        /* Block sent, call hook function if set */
        if (slot->outqueue->func != NULL) {
          cur_slot = slot;
          (*slot->outqueue->func)();
        }
        next = slot->outqueue->next;
        if (next != NULL) {
          /* There are other blocks to send */
          free(slot->outqueue);
          slot->outqueue = next;
          slot->bytes_written = 0;
        } else {
          /* Last block sent */
          free(slot->outqueue);
          slot->outqueue = NULL;
#ifdef USE_POLL
          s_fd_array[slot->idx].events &= (short)~POLLOUT;
#else
          FD_CLR(slot->fd, &s_fdset_write);
#endif
        }
      }
    } else if (bytes == 0 || (bytes < 0 && errno != EAGAIN)) {
      slot->close_f = 1;
      slot->errio_f = 1;
      slot->errwrite_f = 1;
      slot->io_errno = errno;
    }
  }
}

static void aio_process_func_list(void)
{
  int i;

  while (s_sig_func_set) {
    s_sig_func_set = 0;
    for (i = 0; i < 10; i++) {
      if (s_sig_func[i] != NULL) {
        (*s_sig_func[i])();
        s_sig_func[i] = NULL;
      }
    }
  }
}

static void aio_accept_connection(void)
{
  AIO_SLOT *slot;
  struct sockaddr_in client_addr;
  unsigned int len;
  int fd;

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
#ifdef USE_POLL
  AIO_SLOT *h_slot;
#endif

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
#ifdef USE_POLL
    if (s_fd_array_size - 1 > slot->idx) {
      memmove(&s_fd_array[slot->idx],
              &s_fd_array[slot->idx + 1],
              (s_fd_array_size - slot->idx - 1) * sizeof(struct pollfd));
      if (s_listen_fd_idx > slot->idx)
        s_listen_fd_idx--;
      for (h_slot = slot->next; h_slot != NULL; h_slot = h_slot->next)
        h_slot->idx--;
    }
    s_fd_array_size--;
#else
    FD_CLR(slot->fd, &s_fdset_read);
    FD_CLR(slot->fd, &s_fdset_write);
    if (slot->fd == s_max_fd) {
      /* NOTE: Better way is to find _existing_ max fd */
      s_max_fd--;
    }
#endif
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

/*
 * Signal handler catching SIGTERM and SIGINT signals
 */

static void sh_interrupt (int signo)
{
  s_close_f = 1;
  signal (signo, sh_interrupt);
}


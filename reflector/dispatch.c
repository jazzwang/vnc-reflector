/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: dispatch.c,v 1.2 2001/08/01 16:06:07 const Exp $
 * Watching I/O events, dispatching control flow.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>

#include "rfblib.h"
#include "reflector.h"
#include "dispatch.h"
#include "logging.h"

static fd_set fdset_used;
static int max_fd_used;

typedef void (*readfunc)();
static readfunc readfunc_host;
static char *readptr_host;
static char inbuf_host[16];
static int bytes_read_host;
static int bytes_left_host;

/* FIXME: Remove this. */
static int saved_fd;

static void get_write_fdset(fd_set *fdset);
static void process_host_input(int fd);
static void process_host_output(int fd);
void set_readfunc(readfunc fn, char *inbuf, int bytes_to_read);

static void rf_host_msg(void);
static void rf_host_fbupdate_hdr(void);
static void rf_host_fbupdate_recthdr(void);
static void rf_host_fbupdate_raw(void);
static void rf_host_colormap_hdr(void);
static void rf_host_cuttext(void);

/*
 * Implementation
 */

void mainloop(int fd_host, int listen_port)
{
  fd_set fdset_r, fdset_w;
  struct timeval timeout;

  /* Put fd_host in non-blocking mode */
  if (fcntl (fd_host, F_SETFL, O_NONBLOCK) < 0)
    log_write (LL_WARN, "Could not set non-blocking mode for fd %d", fd_host);

  /* FIXME: Remove this. */
  saved_fd = fd_host;

  FD_ZERO(&fdset_used);
  FD_SET(fd_host, &fdset_used);
  max_fd_used = fd_host;

  set_readfunc(rf_host_msg, inbuf_host, 1);

  while (1) {
    memcpy(&fdset_r, &fdset_used, sizeof (fd_set));
    get_write_fdset(&fdset_w);
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    if (select (max_fd_used + 1, &fdset_r, &fdset_w, NULL, &timeout) > 0) {
      if (FD_ISSET(fd_host, &fdset_r)) {
        process_host_input(fd_host);
      }
      if (FD_ISSET(fd_host, &fdset_w)) {
        process_host_output(fd_host);
      }
    } else {
      /* Do something in idle periods */
      log_reopen();
    }
  }
}

void get_write_fdset(fd_set *fdset)
{
  FD_ZERO(fdset);
}

void process_host_input(int fd)
{
  int bytes_read;

  bytes_read = read(fd, &readptr_host[bytes_read_host], bytes_left_host);
  log_write(LL_DEBUG, "Received %d byte(s)", bytes_read);
  if (bytes_read >= 0) {
    bytes_read_host += bytes_read;
    bytes_left_host -= bytes_read;
    if (!bytes_left_host) {
      (*readfunc_host)();
    }
  }
}

void process_host_output(int fd)
{

}

void set_readfunc(readfunc fn, char *inbuf, int bytes_to_read)
{
  readfunc_host = fn;
  readptr_host = inbuf;
  bytes_read_host = 0;
  bytes_left_host = bytes_to_read;
}

static void rf_host_msg(void)
{
  int msg_id;

  msg_id = (int)inbuf_host[0] & 0xFF;
  switch(msg_id) {
  case 0:                       /* FramebufferUpdate */
    set_readfunc(rf_host_fbupdate_hdr, &inbuf_host[1], 3);
    break;
  case 1:                       /* SetColourMapEntries */
    log_write(LL_WARN, "Ignoring SetColourMapEntries message");
    set_readfunc(rf_host_colormap_hdr, inbuf_host, 1);
    break;
  case 2:                       /* Bell */
    log_write(LL_DEBUG, "Receiving Bell message from host");
    set_readfunc(rf_host_msg, inbuf_host, 1);
    break;
  case 3:                       /* ServerCutText */
    log_write(LL_DEBUG, "Receiving ServerCutText message from host");
    set_readfunc(rf_host_cuttext, inbuf_host, 1);
    break;
  default:
    log_write(LL_ERROR, "Unknown server message type: %d", msg_id);
    set_readfunc(rf_host_msg, inbuf_host, 1);
  }
}

/********************************/
/* Handling framebuffer updates */
/********************************/

static CARD16 rect_count;
static CARD16 rect_x, rect_y, rect_w, rect_h;
static CARD32 rect_enc;

static void rf_host_fbupdate_hdr(void)
{
  rect_count = buf_get_CARD16(&inbuf_host[2]);

  if (rect_count == 0xFFFF) {
    log_write(LL_DEBUG, "Receiving framebuffer update");
  } else {
    log_write(LL_DEBUG, "Receiving framebuffer update, %d rectangle(s)",
              rect_count);
  }

  set_readfunc(rf_host_fbupdate_recthdr, inbuf_host, 12);
}

static void rf_host_fbupdate_recthdr(void)
{
  rect_x = buf_get_CARD16(inbuf_host);
  rect_y = buf_get_CARD16(&inbuf_host[2]);
  rect_w = buf_get_CARD16(&inbuf_host[4]);
  rect_h = buf_get_CARD16(&inbuf_host[6]);
  rect_enc = buf_get_CARD32(&inbuf_host[8]);

  log_write(LL_DEBUG, "Receiving rectangle %dx%d at %d,%d",
            (int)rect_w, (int)rect_h, (int)rect_x, (int)rect_y);

  switch(rect_enc) {
  case 0:
    log_write(LL_DEBUG, "Receiving raw-encoded data, expecting %d byte(s)",
              rect_w * rect_h * sizeof(CARD32));
    set_readfunc(rf_host_fbupdate_raw, (char *)framebuffer,
                 rect_w * rect_h * sizeof(CARD32));
    break;
  default:
    log_write(LL_ERROR, "Unknown encoding: %0x08lX", (unsigned long)rect_enc);
    set_readfunc(rf_host_msg, inbuf_host, 1);
  }
}

/* FIXME: Remove this. */
static int request_incr_update(int fd, int fb_width, int fb_height);

static void rf_host_fbupdate_raw(void)
{
  log_write(LL_DEBUG, "Received rectangle ok");

  if (--rect_count) {
    set_readfunc(rf_host_fbupdate_recthdr, inbuf_host, 12);
  } else {
    /* FIXME: use async I/O to send data */
    request_incr_update(saved_fd, desktop_info.width, desktop_info.height);
    set_readfunc(rf_host_msg, inbuf_host, 1);
  }
}

/* FIXME: Remove this. */
static int request_incr_update(int fd, int fb_width, int fb_height)
{
  unsigned char fbupdatereq_msg[] = {
    3,                          /* Message id */
    1,                          /* Incremental if 1 */
    0, 0, 0, 0,                 /* X position, Y position */
    0, 0, 0, 0                  /* Width, height */
  };

  buf_put_CARD16(&fbupdatereq_msg[6], (CARD16)fb_width);
  buf_put_CARD16(&fbupdatereq_msg[8], (CARD16)fb_height);

  log_write(LL_DEBUG, "Sending FramebufferUpdateRequest message");
  write(fd, fbupdatereq_msg, sizeof(fbupdatereq_msg));

  return 1;
}


/********************************/

static void rf_host_colormap_hdr(void)
{
  /* FIXME: add real processing */
  set_readfunc(rf_host_msg, inbuf_host, 1);
}

static void rf_host_cuttext(void)
{
  /* FIXME: add real processing */
  set_readfunc(rf_host_msg, inbuf_host, 1);
}


/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_connect.c,v 1.13 2001/08/24 00:50:47 const Exp $
 * Connecting to a VNC host
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#include "rfblib.h"
#include "reflector.h"
#include "logging.h"
#include "async_io.h"
#include "host_io.h"
#include "rect.h"
#include "translate.h"
#include "client_io.h"
#include "encode.h"
#include "host_connect.h"

static int parse_host_info(void);
static void host_init_hook(void);
static void rf_host_ver(void);
static void rf_host_auth(void);
static void rf_host_conn_failed(void);
static void rf_host_crypt(void);
static void rf_host_auth_done(void);
static void send_client_initmsg(void);
static void rf_host_initmsg(void);
static void rf_host_set_formats(void);
static int allocate_framebuffer(void);

static int s_reconnect_f;

static char *s_host_info_file;
static int s_cl_listen_port;

static char s_hostname[256];
static int s_host_port;
static unsigned char s_host_password[9];

static CARD32 s_len;

/* Connect to remote RFB host */
int connect_to_host(char *host_info_file, int cl_listen_port)
{
  int host_fd;
  struct hostent *phe;
  struct sockaddr_in host_addr;

  /* Save arguments in static variables, for later use */
  if (host_info_file != NULL)
    s_host_info_file = host_info_file;
  if (cl_listen_port != 0)
    s_cl_listen_port = cl_listen_port;

  /* Extract hostname, port and password from host info file */
  if (!parse_host_info())
    return 0;

  log_write(LL_MSG, "Connecting to %s, port %d", s_hostname, s_host_port);

  phe = gethostbyname(s_hostname);
  if (phe == NULL) {
    log_write(LL_ERROR, "Could not get host address: %s", strerror(errno));
    return 0;
  }

  host_addr.sin_family = AF_INET;
  memcpy(&host_addr.sin_addr.s_addr, phe->h_addr, phe->h_length);
  host_addr.sin_port = htons((unsigned short)s_host_port);

  host_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (host_fd == -1) {
    log_write(LL_ERROR, "Could not create socket: %s", strerror(errno));
    return 0;
  }

  if (connect(host_fd, (struct sockaddr *)&host_addr,
              sizeof(host_addr)) != 0) {
    log_write(LL_ERROR, "Could not connect: %s", strerror(errno));
    close(host_fd);
    return 0;
  }

  log_write(LL_MSG, "Connection established");

  aio_add_slot(host_fd, NULL, host_init_hook, sizeof(AIO_SLOT));
  return 1;
}

void host_request_reconnect(void)
{
  s_reconnect_f = 1;
}

void host_maybe_reconnect(void)
{
  if (s_reconnect_f) {
    connect_to_host(NULL, 0);
    s_reconnect_f = 0;
  }
}

static int parse_host_info(void)
{
  FILE *fp;
  char buf[256];
  char *pos, *colon_pos, *space_pos;
  int len;

  fp = fopen(s_host_info_file, "r");
  if (fp == NULL) {
    log_write(LL_ERROR, "Cannot open host info file: %s", s_host_info_file);
    return 0;
  }

  /* Read the file into buffer first */
  len = fread(buf, 1, 255, fp);
  buf[len] = '\0';
  fclose(fp);

  if (len == 0) {
    log_write(LL_ERROR, "Error reading host info file (is it empty?)");
    return 0;
  }

  /* Truncate at the end of first line */
  pos = strchr(buf, '\n');
  if (pos != NULL)
    *pos = '\0';

  /* FIXME: parsing code below is primitive */

  space_pos = strchr(buf, ' ');
  if (space_pos != NULL) {
    strncpy((char *)s_host_password, space_pos + 1, 8);
    s_host_password[8] = '\0';
    *space_pos = '\0';
  } else {
    log_write(LL_WARN, "Host password not specified, assuming no auth");
    s_host_password[0] = '\0';
  }

  colon_pos = strchr(buf, ':');
  if (colon_pos != NULL) {
    if (!isdigit(colon_pos[1])) {
      log_write(LL_ERROR, "Non-numeric host display number: %s",
                colon_pos + 1);
      return 0;
    }
    s_host_port = 5900 + atoi(colon_pos + 1);
    *colon_pos = '\0';
  } else {
    log_write(LL_WARN, "Host display not specified, assuming display :0");
    s_host_port = 5900;
  }

  strcpy(s_hostname, buf);

  return 1;
}

static void host_init_hook(void)
{
  s_reconnect_f = 0;

  cur_slot->type = TYPE_HOST_SLOT;
  aio_setclose(host_close_hook);
  aio_setread(rf_host_ver, NULL, 12);
}

static void rf_host_ver(void)
{
  char *buf = (char *)cur_slot->readbuf;
  int major = 3, minor = 3;
  int remote_major, remote_minor;
  char ver_msg[12];

  if ( strncmp(buf, "RFB ", 4) != 0 || !isdigit(buf[4]) ||
       !isdigit(buf[4]) || !isdigit(buf[5]) || !isdigit(buf[6]) ||
       buf[7] != '.' || !isdigit(buf[8]) || !isdigit(buf[9]) ||
       !isdigit(buf[10]) || buf[11] != '\n' ) {
    log_write(LL_ERROR, "Wrong greeting data received from host");
    aio_close(0);
    return;
  }

  remote_major = atoi(&buf[4]);
  remote_minor = atoi(&buf[8]);
  log_write(LL_INFO, "Remote RFB Protocol version is %d.%d",
            remote_major, remote_minor);

  if (remote_major != major) {
    log_write(LL_ERROR, "Wrong protocol version, expected %d.x", major);
    aio_close(0);
    return;
  } else if (remote_minor != minor) {
    log_write(LL_WARN, "Protocol sub-version does not match (ignoring)");
  }

  sprintf(ver_msg, "RFB %03d.%03d\n", abs(major) % 999, abs(minor) % 999);
  aio_write(NULL, ver_msg, 12);
  aio_setread(rf_host_auth, NULL, 4);
}

static void rf_host_auth(void)
{
  int success = 1;
  char *reason;
  CARD32 value32;

  value32 = buf_get_CARD32(cur_slot->readbuf);

  switch (value32) {
  case 0:
    s_len = buf_get_CARD32(&cur_slot->readbuf[4]);
    aio_setread(rf_host_conn_failed, NULL, s_len);
    break;
  case 1:
    log_write(LL_MSG, "No authentication required at host side");
    send_client_initmsg();
    break;
  case 2:
    log_write(LL_DETAIL, "VNC authentication requested by host");
    aio_setread(rf_host_crypt, NULL, 16);
    break;
  default:
    log_write(LL_ERROR, "Unknown authentication scheme requested by host");
    aio_close(0);
    return;
  }
}

static void rf_host_conn_failed(void)
{
  log_write(LL_ERROR, "VNC connection failed: %.*s",
            (int)s_len, cur_slot->readbuf);
  aio_close(0);
}

static void rf_host_crypt(void)
{
  unsigned char response[16];

  log_write(LL_DEBUG, "Received random challenge");

  rfb_crypt(response, cur_slot->readbuf, s_host_password);
  log_write(LL_DEBUG, "Sending DES-encrypted response");
  aio_write(NULL, response, 16);

  aio_setread(rf_host_auth_done, NULL, 4);
}

static void rf_host_auth_done(void)
{
  CARD32 value32;

  value32 = buf_get_CARD32(cur_slot->readbuf);

  switch(value32) {
  case 0:
    log_write(LL_MSG, "Authentication successful");
    send_client_initmsg();
    break;
  case 1:
    log_write(LL_ERROR, "Authentication failed");
    aio_close(0);
    break;
  case 2:
    log_write(LL_ERROR, "Authentication failed, too many tries");
    aio_close(0);
    break;
  default:
    log_write(LL_ERROR, "Unknown authentication result");
    aio_close(0);
  }
}

static void send_client_initmsg(void)
{
  CARD8 shared_session = 1;

  log_write(LL_DETAIL, "Requesting %s session",
            (shared_session != 0) ? "non-shared" : "shared");
  aio_write(NULL, &shared_session, 1);

  aio_setread(rf_host_initmsg, NULL, 24);
}

static void rf_host_initmsg(void)
{
  CARD16 width, height;

  log_write(LL_DEBUG, "Receiving host desktop parameters");

  width = buf_get_CARD16(cur_slot->readbuf);
  height = buf_get_CARD16(&cur_slot->readbuf[2]);
  log_write(LL_MSG, "Remote desktop geometry is %dx%d",
            (int)width, (int)height);

  if (g_screen_info->width == 0) {
    g_screen_info->width = width;
    g_screen_info->height = height;
  } else if (g_screen_info->width != width ||
             g_screen_info->height != height) {
    log_write(LL_ERROR, "Incompatible geometry of remote desktop");
    aio_close(0);
    return;
  }

  s_len = buf_get_CARD32(&cur_slot->readbuf[20]);
  aio_setread(rf_host_set_formats, NULL, s_len);
}

static void rf_host_set_formats(void)
{
  unsigned char setpixfmt_msg[4 + SZ_RFB_PIXEL_FORMAT];
  unsigned char setencodings_msg[] = {
    2,                          /* Message id */
    0,                          /* Padding -- not used */
    0, 3,                       /* Number of encodings */
    0, 0, 0, 5,                 /* Hextile encoding */
    0, 0, 0, 1,                 /* CopyRect encoding */
    0, 0, 0, 0                  /* Raw encoding */
  };

  log_write(LL_INFO, "Remote desktop name: %.*s",
            (int)s_len, cur_slot->readbuf);

  g_screen_info = realloc(g_screen_info,
                          sizeof(RFB_SCREEN_INFO) + (size_t)s_len);
  if (g_screen_info != NULL) {
    g_screen_info->name_length = s_len;
    memcpy(g_screen_info->name, cur_slot->readbuf, s_len);
  } else {
    g_screen_info->name_length = 1;
    g_screen_info->name[0] = '?';
  }

  log_write(LL_DETAIL, "Setting up pixel format and encodings");

  log_write(LL_DEBUG, "Sending SetPixelFormat message");
  memset(setpixfmt_msg, 0, 4);
  buf_put_pixfmt(&setpixfmt_msg[4], &g_screen_info->pixformat);
  aio_write(NULL, setpixfmt_msg, sizeof(setpixfmt_msg));

  log_write(LL_DEBUG, "Sending SetEncodings message");
  aio_write(NULL, setencodings_msg, sizeof(setencodings_msg));

  /* If there is no local framebuffer yet, allocate it and start
     listening for client connections. */
  if (g_framebuffer == NULL) {
    if (!allocate_framebuffer()) {
      aio_close(1);
      return;
    }
    if (!aio_listen(s_cl_listen_port, af_client_accept, sizeof(CL_SLOT))) {
      log_write(LL_ERROR, "Error creating listening socket: %s",
                strerror(errno));
      aio_close(1);
      return;
    }
  }

  host_activate();
}

static int allocate_framebuffer(void)
{
  int fb_size, tiles_x, tiles_y;

  fb_size = (int)g_screen_info->width * (int)g_screen_info->height;
  g_framebuffer = malloc(fb_size * sizeof(CARD32));
  if (g_framebuffer == NULL) {
    log_write(LL_ERROR, "Error allocating framebuffer");
    return 0;
  }
  log_write(LL_INFO, "Allocated framebuffer, %d bytes",
            fb_size * sizeof(CARD32));

  tiles_x = ((int)g_screen_info->width + 15) / 16;
  tiles_y = ((int)g_screen_info->height + 15) / 16;
  g_hints = calloc(tiles_x * tiles_y, sizeof(TILE_HINTS));
  if (g_hints == NULL) {
    log_write(LL_ERROR, "Error allocating memory for hextile hints");
    free(g_framebuffer);
    g_framebuffer = NULL;
    return 0;
  }
  log_write(LL_INFO, "Allocated memory for hextile hints, %d bytes",
            tiles_x * tiles_y * sizeof(TILE_HINTS));

  g_cache8 = malloc(fb_size);
  if (g_cache8 == NULL) {
    log_write(LL_ERROR, "Error allocating memory for hextile BGR233 cache");
    free(g_hints);
    g_hints = NULL;
    free(g_framebuffer);
    g_framebuffer = NULL;
    return 0;
  }
  log_write(LL_INFO, "Allocated memory for hextile BGR233 cache, %d bytes",
            fb_size);

  return 1;
}


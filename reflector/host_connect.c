/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_connect.c,v 1.2 2001/08/03 13:06:59 const Exp $
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
#include "logging.h"
#include "host_connect.h"
#include "d3des.h"

static int negotiate_ver(int fd, int major, int minor);
static int vnc_authenticate(int fd, char *password);
static int set_data_formats(int fd, RFB_PIXEL_FORMAT *pixfmt);

static int recv_data(int fd, void *buf, size_t len);
static int send_data(int fd, void *buf, size_t len);
static int send_CARD8(int fd, CARD8 data);
static int recv_CARD16(int fd, CARD16 *result);
static int recv_CARD32(int fd, CARD32 *result);
static char *recv_string(int fd);

int connect_to_host(char *host, int port)
{
  int host_fd;
  struct hostent *phe;
  struct sockaddr_in host_addr;

  log_write(LL_MSG, "Connecting to %s, port %d", host, port);

  phe = gethostbyname(host);
  if (phe == NULL) {
    log_write(LL_ERROR, "Could not get host address: %s", strerror(errno));
    return -1;
  }

  host_addr.sin_family = AF_INET;
  memcpy(&host_addr.sin_addr.s_addr, phe->h_addr, phe->h_length);
  host_addr.sin_port = htons((unsigned short)port);

  host_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (host_fd == -1) {
    log_write(LL_ERROR, "Could not create socket: %s", strerror(errno));
    return -1;
  }

  if (connect(host_fd, (struct sockaddr *)&host_addr,
              sizeof(host_addr)) != 0) {
    log_write(LL_ERROR, "Could not connect: %s", strerror(errno));
    close(host_fd);
    return -1;
  }

  log_write(LL_MSG, "Connection established");

  return host_fd;
}

int setup_session(int host_fd, char *password, RFB_SCREEN_INFO **scr)
{
  int success = 1;
  char *reason;
  CARD32 value32;

  if (!negotiate_ver(host_fd, 3, 3) || !recv_CARD32(host_fd, &value32))
    success = 0;

  if (success) {
    switch (value32) {
    case 0:
      reason = recv_string(host_fd);
      if (reason == NULL)
        reason = "unknown reason";
      log_write(LL_ERROR, "VNC connection failed: %s", reason);
      free(reason);
      success = 0;
      break;
    case 1:
      log_write(LL_MSG, "No authentication required");
      break;
    case 2:
      log_write(LL_DETAIL, "VNC authentication required");
      if (!vnc_authenticate(host_fd, password))
        success = 0;
      break;
    default:
      log_write(LL_ERROR, "Unknown authentication scheme");
      success = 0;
    }
  }

  if (success) {
    log_write(LL_DETAIL, "Requesting shared session");
    if (!send_CARD8(host_fd, 1))
      success = 0;
  }

  if (success) {
    CARD8 pixfmt_buf[16];
    CARD32 name_length;
    CARD8 *name;

    log_write(LL_DEBUG, "Receiving host desktop parameters");
    if ( recv_CARD16(host_fd, &(*scr)->width) &&
         recv_CARD16(host_fd, &(*scr)->height) &&
         recv_data(host_fd, pixfmt_buf, 16) ) {
      log_write(LL_MSG, "Remote desktop geometry is %dx%d",
                (int)(*scr)->width, (int)(*scr)->height);
      if ((name = recv_string(host_fd)) != NULL) {
        log_write(LL_INFO, "Remote desktop name: %s", name);
        name_length = (CARD32)strlen(name);
        *scr = realloc(*scr, sizeof(RFB_SCREEN_INFO) + name_length);
        if (*scr != NULL) {
          (*scr)->name_length = name_length;
          memcpy((*scr)->name, name, name_length);
        } else {
          (*scr)->name_length = 1;
          (*scr)->name[0] = '?';
        }
        free(name);
      } else {
        success = 0;
      }
    } else {
      success = 0;
    }
  }

  if (success) {
    log_write(LL_DEBUG, "Setting up pixel format and encodings");
    if (!set_data_formats(host_fd, &(*scr)->pixformat))
      success = 0;
  }

  if (!success) {
    log_write(LL_MSG, "Closing connection to host");
    shutdown(host_fd, 2);
    close(host_fd);
    return 0;
  }

  return 1;
}

static int negotiate_ver(int fd, int major, int minor)
{
  char buf[16];
  int remote_major, remote_minor;

  if (!recv_data(fd, buf, 12))
    return 0;

  if ( strncmp(buf, "RFB ", 4) != 0 || !isdigit(buf[4]) ||
       !isdigit(buf[4]) || !isdigit(buf[5]) || !isdigit(buf[6]) ||
       buf[7] != '.' || !isdigit(buf[8]) || !isdigit(buf[9]) ||
       !isdigit(buf[10]) || buf[11] != '\n' ) {
    log_write(LL_ERROR, "Wrong greeting data received from host");
    return 0;
  }

  remote_major = atoi(&buf[4]);
  remote_minor = atoi(&buf[8]);
  log_write(LL_INFO, "Remote RFB Protocol version is %d.%d",
            remote_major, remote_minor);

  if (remote_major != major) {
    log_write(LL_ERROR, "Wrong protocol version, expected %d.x", major);
    return 0;
  } else if (remote_minor != minor) {
    log_write(LL_WARN, "Protocol sub-version does not match (ignoring)");
  }

  sprintf(buf, "RFB %03d.%03d\n", abs(major) % 999, abs(minor) % 999);

  return send_data(fd, buf, 12);
}

static int vnc_authenticate(int fd, char *password)
{
  unsigned char key[8];
  unsigned char challenge[16];
  unsigned char response[16];
  CARD32 value32;

  log_write(LL_DEBUG, "Receiving random challenge");

  if (!recv_data(fd, challenge, 16))
    return 0;

  memset(key, 0, 8);
  strncpy((char *)key, password, 8);

  deskey(key, EN0);
  des(challenge, response);
  des(challenge + 8, response + 8);

  log_write(LL_DEBUG, "Sending DES-encrypted response");

  if (!send_data(fd, response, 16))
    return 0;

  if (!recv_CARD32(fd, &value32))
    return 0;

  switch(value32) {
  case 0:
    log_write(LL_MSG, "Authentication successful");
    break;
  case 1:
    log_write(LL_ERROR, "Authentication failed");
    return 0;
  case 2:
    log_write(LL_ERROR, "Authentication failed, too many tries");
    return 0;
  default:
    log_write(LL_ERROR, "Unknown authentication result");
    return 0;
  }

  return 1;
}

static int set_data_formats(int fd, RFB_PIXEL_FORMAT *pixfmt)
{
  unsigned char setpixfmt_msg[4 + SZ_RFB_PIXEL_FORMAT];
  unsigned char setencodings_msg[] = {
    2,                          /* Message id */
    0,                          /* Padding -- not used */
    0, 1,                       /* Number of encodings */
    0, 0, 0, 0                  /* Raw encoding */
  };

  log_write(LL_DEBUG, "Sending SetPixelFormat message");

  memset(setpixfmt_msg, 0, 4);
  buf_put_pixfmt(&setpixfmt_msg[4], pixfmt);
  if (!send_data(fd, setpixfmt_msg, sizeof(setpixfmt_msg)))
    return 0;

  log_write(LL_DEBUG, "Sending SetEncodings message");

  if (!send_data(fd, setencodings_msg, sizeof(setencodings_msg)))
    return 0;

  return 1;
}

static int recv_data(int fd, void *buf, size_t len)
{
  if ((size_t)recv(fd, buf, len, MSG_WAITALL) != len) {
    log_write(LL_ERROR, "Error receiving network data");
    return 0;
  }
  log_write(LL_DEBUG, "Received %d byte(s)", (int)len);
  return 1;
}

static int send_data(int fd, void *buf, size_t len)
{
  if ((size_t)send(fd, buf, len, 0) != len) {
    log_write(LL_ERROR, "Error sending network data");
    return 0;
  }
  log_write(LL_DEBUG, "Sent %d byte(s)", (int)len);
  return 1;
}

static int send_CARD8(int fd, CARD8 data)
{
  return send_data(fd, &data, 1);
}

static int recv_CARD16(int fd, CARD16 *result)
{
  unsigned char buf[2];

  if (!recv_data(fd, buf, 2))
    return 0;

  *result = buf_get_CARD16(buf);
  return 1;
}

static int recv_CARD32(int fd, CARD32 *result)
{
  unsigned char buf[4];

  if (!recv_data(fd, buf, 4))
    return 0;

  *result = buf_get_CARD32(buf);
  return 1;
}

static char *recv_string(int fd)
{
  CARD32 len;
  char *data;

  if (!recv_CARD32(fd, &len))
    return NULL;

  data = malloc(len + 1);
  if (data == NULL) {
    log_write(LL_ERROR, "Memory allocation error");
    return NULL;
  }

  if (!recv_data(fd, data, len)) {
    free(data);
    return NULL;
  }

  data[len] = '\0';
  return data;
}


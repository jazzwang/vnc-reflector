/* VNC Reflector
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: main.c,v 1.10 2001/08/02 16:47:45 const Exp $
 * Main module
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>

#include "rfblib.h"
#include "async_io.h"
#include "logging.h"
#include "reflector.h"
#include "host_connect.h"
#include "host_io.h"
#include "client_io.h"

/* Configuration options */
static int   opt_listen_port;
static char *opt_log_filename;
static char *opt_passwd_filename;
static int   opt_foreground;
static int   opt_stderr_loglevel;
static int   opt_file_loglevel;
static char  opt_hostname[256];
static int   opt_hostport;
static char  opt_password[9];

/* Framebuffer */
RFB_DESKTOP_INFO desktop_info;
CARD32 *framebuffer;

/* Functions local to this file */
static void parse_args(int argc, char **argv);
static void report_usage(char *program_name);
static int read_pasword_file(void);

int main(int argc, char **argv)
{
  int host_fd;

  /* Parse command line, exit on error */
  parse_args(argc, argv);

  if (!log_open(opt_log_filename, opt_file_loglevel,
                (opt_foreground) ? opt_stderr_loglevel : -1)) {
    fprintf(stderr, "%s: error opening log file (ignoring this error)\n",
            argv[0]);
  }
  log_write(LL_MSG, "Starting VNC Reflector %s", VERSION);

  /* Fork the process to the background if necessary */
  if (!opt_foreground) {
    if (getpid() != 1) {
      signal(SIGTTIN, SIG_IGN);
      signal(SIGTTOU, SIG_IGN);
      signal(SIGTSTP, SIG_IGN);
      if (fork ())
        return 0;
      setsid();
    }
    close(0);
    close(1);
    close(2);
    log_write(LL_INFO, "Switched to the background mode");
  }

  read_pasword_file();

  host_fd = connect_to_host(opt_hostname, opt_hostport);
  if (host_fd != -1 && setup_session(host_fd, opt_password, &desktop_info)) {
    /* Allocate framebuffer */
    framebuffer = malloc(desktop_info.width * desktop_info.height * 4);
    if (framebuffer == NULL) {
      log_write(LL_ERROR, "Error allocating framebuffer");
    } else {
      log_write(LL_DEBUG, "Allocated framebuffer, %d bytes",
                desktop_info.width * desktop_info.height * 4);

      set_client_password(opt_password);

      aio_init();

      if (!aio_listen(opt_listen_port, af_client_accept,
                      1, sizeof(AIO_SLOT))) {
        log_write(LL_ERROR, "Error creating listening socket: %s",
                  strerror(errno));
      } else {
        init_host_io(host_fd);
        aio_mainloop();
      }
      free(framebuffer);
    }
    free(desktop_info.name);
  }

  /* Close logs */
  if (!log_close() && opt_foreground) {
    fprintf(stderr, "%s: error closing log file (ignoring this error)\n",
            argv[0]);
  }

  /* Done */
  exit(0);
}

static void parse_args(int argc, char **argv)
{
  int err = 0;
  int c, len;
  char *pos;

  opt_foreground = 0;
  opt_stderr_loglevel = -1;
  opt_file_loglevel = -1;
  opt_passwd_filename = NULL;
  opt_log_filename = NULL;
  opt_listen_port = -1;

  while (!err && (c = getopt(argc, argv, "hv:f:p:g:l:")) != -1) {
    switch (c) {
    case 'h':
      err = 1;
      break;
    case 'v':
      if (opt_file_loglevel != -1)
        err = 1;
      else
        opt_file_loglevel = atoi(optarg);
      break;
    case 'f':
      opt_foreground = 1;
      if (opt_stderr_loglevel != -1)
        err = 1;
      else
        opt_stderr_loglevel = atoi(optarg);
      break;
    case 'p':
      if (opt_passwd_filename != NULL)
        err = 1;
      else
        opt_passwd_filename = optarg;
      break;
    case 'g':
      if (opt_log_filename != NULL)
        err = 1;
      else
        opt_log_filename = optarg;
      break;
    case 'l':
      if (opt_listen_port != -1)
        err = 1;
      else {
        opt_listen_port = atoi(optarg);
        if (opt_listen_port < 0)
          err = 1;
      }
      break;
    default:
      err = 1;
    }
  }

  /* Print usage help on error */
  if (err || optind != argc - 1) {
    report_usage(argv[0]);
    exit(1);
  }

  /* Provide reasonable defaults to options */
  if (opt_file_loglevel == -1)
    opt_file_loglevel = LL_INFO;
  if (opt_passwd_filename == NULL)
    opt_passwd_filename = "passwd";
  if (opt_log_filename == NULL)
    opt_log_filename = "reflector.log";
  if (opt_listen_port == -1)
    opt_listen_port = 5999;

  /* Separate host name and host display number if exists */
  pos = strchr(argv[optind], ':');
  if (pos == NULL) {
    opt_hostport = 5900;        /* Default to display :0 */
    len = strlen(argv[optind]);
  } else {
    opt_hostport = 5900 + atoi(&pos[1]);
    len = pos - argv[optind];
  }

  /* More diagnosis */
  if (len == 0) {
    fprintf(stderr, "%s: missing host name\n", argv[0]);
    exit(1);
  } else if (len > 255) {
    fprintf(stderr, "%s: host name too long\n", argv[0]);
    exit(1);
  }

  /* Save host name */
  strncpy(opt_hostname, argv[optind], len);
  opt_hostname[len] = '\0';
}

static void report_usage(char *program_name)
{
  fprintf(stderr, "\nUsage: %s [OPTIONS...] HOST[:DISPLAY]\n\n",
          program_name);

  fprintf(stderr,
          "Options:\n"
          "  -p PASSWD_FILE - read vncpasswd's password file"
          " [default: passwd]\n"
          "  -l LISTEN_PORT - port to listen for client connections"
          " [default: 5999]\n"
          "  -g LOG_FILE    - write logs to specified file"
          " [default: reflector.log]\n"
          "  -v LOG_LEVEL   - set verbosity level for log file (0..%d)"
          " [default: %d]\n"
          "  -f LOG_LEVEL   - run in foreground, show logs on stderr"
          " at specified\n"
          "                   verbosity level (0..%d)\n"
          "  -h             - print this help message\n"
          "\n"
          "Note: default host's display number is :0 (port 5900)\n\n",
          LL_DEBUG, LL_INFO, LL_DEBUG, LL_MSG);
}

static int read_pasword_file(void)
{
  FILE *passwd_fp;
  char buf[8];
  int len;

  log_write(LL_DETAIL, "Looking for a password in the file \"%s\"",
            opt_passwd_filename);

  passwd_fp = fopen(opt_passwd_filename, "r");
  if (passwd_fp == NULL) {
    log_write(LL_WARN, "Cannot open password file, assuming empty password");
    return 1;
  }

  len = fread(buf, 1, 8, passwd_fp);
  strncpy(opt_password, buf, len);
  opt_password[len] = '\0';
  len = strcspn(opt_password, "\n\r");
  opt_password[len] = '\0';

  if (len == 0)
    log_write(LL_WARN, "Got empty password, hoping that's ok");
  else
    log_write(LL_DETAIL, "Got the password");

  fclose(passwd_fp);
  return 1;
}


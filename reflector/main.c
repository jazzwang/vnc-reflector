/* VNC Reflector
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: main.c,v 1.1 2001/08/01 04:58:39 const Exp $
 * Main module
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "rfblib.h"
#include "reflector.h"
#include "logging.h"
#include "hostconnect.h"

/* Configuration options */
int   opt_listen_port;
char *opt_log_filename;
char *opt_passwd_filename;
int   opt_foreground;
char  opt_hostname[256];
int   opt_hostport;
char  opt_password[9];

RFB_DESKTOP_INFO desktop_info;

/* Functions local to this file */
static void parse_args(int argc, char **argv);
static void report_usage(char *program_name);
static int read_pasword_file(void);

int main(int argc, char **argv)
{
  int host_fd;

  /* Parse command line, exit on error */
  parse_args(argc, argv);

  /* Open logs */
  if (!log_open(opt_log_filename, LL_DEBUG,
                (opt_foreground) ? LL_DETAIL : -1)) {
    fprintf(stderr, "%s: error opening log file (ignoring this error)\n",
            argv[0]);
  }
  log_write(LL_MSG, "Starting VNC Reflector %s", VERSION);

  /* Read the password */
  read_pasword_file();

  /* Connect to host machine */
  host_fd = connect_to_host(opt_hostname, opt_hostport);
  if (host_fd != -1 && setup_session(host_fd, opt_password, &desktop_info)) {

    /* ... */

    free(desktop_info.name);
  }

  /* Close logs */
  if (!log_close()) {
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
  opt_passwd_filename = NULL;
  opt_log_filename = NULL;
  opt_listen_port = -1;

  while (!err && (c = getopt(argc, argv, "hfp:g:l:")) != -1) {
    switch (c) {
    case 'h':
      err = 1;
      break;
    case 'f':
      opt_foreground = 1;
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
          "  -f             - run in foreground, write log to stderr\n"
          "  -h             - print this help message\n"
          "\n"
          "Note: default display number is :0 (port 5900)\n\n");
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
    return 0;
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
}


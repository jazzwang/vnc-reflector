/* VNC Reflector
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: main.c,v 1.22 2001/08/23 10:52:32 const Exp $
 * Main module
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "rfblib.h"
#include "async_io.h"
#include "logging.h"
#include "reflector.h"
#include "host_connect.h"
#include "control.h"

/*
 * Configuration options
 */

static int   opt_cl_listen_port;
static char *opt_log_filename;
static char *opt_passwd_filename;
static int   opt_foreground;
static int   opt_stderr_loglevel;
static int   opt_file_loglevel;
static char  opt_hostname[256];
static int   opt_hostport;
static char  opt_pidfile[256];

static unsigned char opt_host_password[9];
static unsigned char opt_client_password[9];
static unsigned char opt_client_ro_password[9];

/*
 * Global variables
 */

/* FIXME: allocate desktop name only, not the whole g_screen_info. */
RFB_SCREEN_INFO *g_screen_info;
CARD32 *g_framebuffer;

/*
 * Functions local to this file
 */

static void parse_args(int argc, char **argv);
static void report_usage(char *program_name);
static int read_password_file(void);
static int init_screen_info(void);
static int write_pid_file(void);
static int remove_pid_file(void);

/*
 * Implementation
 */

int main(int argc, char **argv)
{

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

  /* Initialization */
  init_screen_info();
  read_password_file();
  set_client_passwords(opt_client_password, opt_client_ro_password);
  aio_init();

  /* Main work */
  if (connect_to_host(opt_hostname, opt_hostport, opt_cl_listen_port,
                      opt_host_password)) {
    if (write_pid_file()) {
      set_control_signals();
      aio_mainloop();
      remove_pid_file();
    }
  }

  /* Cleanup */
  if (g_framebuffer != NULL) {
    log_write(LL_DEBUG, "Freeing framebuffer");
    free(g_framebuffer);
  }
  free(g_screen_info);

  log_write(LL_MSG, "Terminating");

  /* Close logs */
  if (!log_close() && opt_foreground) {
    fprintf(stderr, "%s: error closing log file (ignoring this error)\n",
            argv[0]);
  }

  /* Done */
  exit(1);
}

static void parse_args(int argc, char **argv)
{
  int err = 0;
  int c, len;
  char *pos;
  char *temp_pidfile = NULL;
  char temp_buf[32];            /* 32 bytes should be more than enough */

  opt_foreground = 0;
  opt_stderr_loglevel = -1;
  opt_file_loglevel = -1;
  opt_passwd_filename = NULL;
  opt_log_filename = NULL;
  opt_cl_listen_port = -1;
  opt_pidfile[0] = '\0';

  while (!err && (c = getopt(argc, argv, "hv:f:p:g:l:i:")) != -1) {
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
      if (opt_cl_listen_port != -1)
        err = 1;
      else {
        opt_cl_listen_port = atoi(optarg);
        if (opt_cl_listen_port < 0)
          err = 1;
      }
      break;
    case 'i':
      if (temp_pidfile != NULL)
        err = 1;
      else
        temp_pidfile = optarg;
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

  /* Provide reasonable defaults for some options */
  if (opt_file_loglevel == -1)
    opt_file_loglevel = LL_INFO;
  if (opt_passwd_filename == NULL)
    opt_passwd_filename = "passwd";
  if (opt_log_filename == NULL)
    opt_log_filename = "reflector.log";
  if (opt_cl_listen_port == -1)
    opt_cl_listen_port = 5999;

  /* Append listening port number to pid filename */
  if (temp_pidfile != NULL) {
    sprintf(temp_buf, "%d", opt_cl_listen_port);
    sprintf(opt_pidfile, "%.*s.%s", 255 - strlen(temp_buf) - 1,
            temp_pidfile, temp_buf);
  }

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
          "  -i PID_FILE     - write pid file, appending listening port"
          " to filename\n"
          "  -p PASSWD_FILE  - read plaintext password file"
          " [default: passwd]\n"
          "  -l LISTEN_PORT  - port to listen for client connections"
          " [default: 5999]\n"
          "  -g LOG_FILE     - write logs to specified file"
          " [default: reflector.log]\n");
  fprintf(stderr,
          "  -v LOG_LEVEL    - set verbosity level for log file (0..%d)"
          " [default: %d]\n"
          "  -f LOG_LEVEL    - run in foreground, show logs on stderr"
          " at specified\n"
          "                    verbosity level (0..%d)\n"
          "  -h              - print this help message\n"
          "\n"
          "Default host's display number is :0 (port 5900)\n\n"
          "Password file may contain three lines with one password"
          " on each line:\n"
          "  host, client password, read-only client password.\n\n",
          LL_DEBUG, LL_INFO, LL_DEBUG, LL_MSG);
}

static int read_password_file(void)
{
  FILE *passwd_fp;
  unsigned char *password_ptr = opt_host_password;
  int line = 0, len = 0;
  int c;

  /* Fill passwords with zeros */
  memset(opt_host_password, 0, 9);
  memset(opt_client_password, 0, 9);
  memset(opt_client_ro_password, 0, 9);

  log_write(LL_DETAIL, "Looking for passwords in the file \"%s\"",
            opt_passwd_filename);

  passwd_fp = fopen(opt_passwd_filename, "r");
  if (passwd_fp == NULL) {
    log_write(LL_WARN,
              "Cannot open password file, assuming no authentication");
    return 1;
  }

  /* Read password file */
  while (line < 3) {
    c = getc(passwd_fp);
    if (c != '\n' && c != EOF && len < 8) {
      password_ptr[len++] = c;
    } else {
      password_ptr[len] = '\0';
      /* Truncate long passwords */
      if (len == 8 && c != '\n' && c != EOF) {
        log_write(LL_WARN, "Using only 8 first bytes of a longer password");
        do {
          c = getc(passwd_fp);
        } while (c != '\n' && c != EOF);
      }
      /* End of file */
      if (c == EOF)
        break;
      /* Empty password means no authentication */
      if (len == 0) {
        log_write(LL_WARN, "Got empty password, hoping no auth can be ok");
      }
      /* End of line */
      switch (++line) {
      case 1:
        password_ptr = opt_client_password;
        break;
      case 2:
        password_ptr = opt_client_ro_password;
        break;
      }
      len = 0;
    }
  }
  if (len == 0) {
    if (line == 0) {
      log_write(LL_WARN, "Got empty host password, assuming no auth");
    } else {
      line--;
    }
  }

  log_write(LL_DETAIL, "Got %d password(s) from file, including empty ones",
            line + 1);

  /* Provide reasonable defaults if not all three passwords set */
  if (line == 0) {
    log_write(LL_WARN, "Client password not specified, using host's one");
    strcpy((char *)opt_client_password, (char *)opt_host_password);
    line++;
  }
  if (line == 1) {
    log_write(LL_DETAIL, "Read-only client password not specified");
    strcpy((char *)opt_client_ro_password, (char *)opt_client_password);
  }

  fclose(passwd_fp);
  return 1;
}

static int init_screen_info(void)
{
  union _LITTLE_ENDIAN {
    CARD32 value32;
    CARD8 test;
  } little_endian;

  /* Allocate memory making sure all fields are zeroed */
  g_screen_info = calloc(1, sizeof(RFB_SCREEN_INFO));
  if (g_screen_info == NULL)
    return 0;

  /* Zero screen dimensions, set initial desktop name */
  g_screen_info->width = 0;
  g_screen_info->height = 0;
  g_screen_info->name_length = 1;
  g_screen_info->name[0] = '?';

  /* Fill in PIXEL_FORMAT structure */
  g_screen_info->pixformat.bits_pixel = 32;
  g_screen_info->pixformat.color_depth = 24;
  g_screen_info->pixformat.true_color = 1;
  g_screen_info->pixformat.r_max = 255;
  g_screen_info->pixformat.g_max = 255;
  g_screen_info->pixformat.b_max = 255;
  g_screen_info->pixformat.r_shift = 16;
  g_screen_info->pixformat.g_shift = 8;
  g_screen_info->pixformat.b_shift = 0;

  /* Set correct endian flag in PIXEL_FORMAT */
  little_endian.value32 = 1;
  if (little_endian.test) {
    log_write(LL_DEBUG, "Our machine is little endian");
  } else {
    log_write(LL_DEBUG, "Our machine is big endian");
    g_screen_info->pixformat.big_endian = 1;
  }

  /* Make sure we would not try to free framebuffer */
  g_framebuffer = NULL;

  return 1;
}

static int write_pid_file(void)
{
  int pid_fd, len;
  char buf[32];                 /* 32 bytes should be more than enough */

  if (opt_pidfile[0] == '\0')
    return 1;

  pid_fd = open(opt_pidfile, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (pid_fd == -1) {
    log_write(LL_ERROR, "Pid file exists, another instance may be running");
    return 0;
  }
  sprintf(buf, "%d\n", (int)getpid());
  len = strlen(buf);
  if (write(pid_fd, buf, len) != len) {
    close(pid_fd);
    log_write(LL_ERROR, "Error writing to pid file");
    return 0;
  }

  log_write(LL_DEBUG, "Wrote pid file: %s", opt_pidfile);

  close(pid_fd);
  return 1;
}

static int remove_pid_file(void)
{
  if (opt_pidfile[0] == '\0')
    return 1;

  if (unlink(opt_pidfile) == 0) {
    log_write(LL_DEBUG, "Removed pid file", opt_pidfile);
  } else {
    log_write(LL_WARN, "Error removing pid file: %s", opt_pidfile);
  }
}


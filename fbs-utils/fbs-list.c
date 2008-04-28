/*
 * FrameBuffer Stream Utilities.
 * Copyright (C) 2008 Wimba, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file
 * LICENSE, included.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "rfblib.h"
#include "version.h"
#include "fbs-io.h"

/* FIXME: Get rid of static and global variables. */
static RFB_SCREEN_INFO s_screen_info;
static void read_pixel_format(void *buf);

static void report_usage(char *program_name);
static int list_fbs(FILE *fp);
static int read_rfb_init(FBSTREAM *fbs);
static void report_rfb_init(char buf_version[12]);

int main (int argc, char *argv[])
{
  FILE *fp = stdin;
  int needClose = 0;
  int success = 0;

  if (argc == 2 && argv[1][0] != '-') {
    fp = fopen(argv[1], "rb");
    if (fp == NULL) {
      fprintf(stderr, "Error opening file: %s\n", argv[1]);
      return 1;
    }
  } else if (argc >= 2) {
    report_usage(argv[0]);
    return 1;
  }

  if (list_fbs(fp)) {
    success = 1;
  }

  if (needClose) {
    fclose(fp);
  }

  return success;
}

static void report_usage(char *program_name)
{
  fprintf(stderr, "fbs-list version %s.\n%s\n\n", VERSION, COPYRIGHT);

  fprintf(stderr, "Usage: %s [FBS_FILE]\n\n",
          program_name);
}

static int list_fbs(FILE *fp)
{
  FBSTREAM fbs;

  if (!fbs_init(&fbs, fp)) {
    return 0;
  }

  if (!read_rfb_init(&fbs)) {
    return 0;
  }

  free(s_screen_info.name);
  fbs_cleanup(&fbs);

  return 1;
}

static int read_rfb_init(FBSTREAM *fbs)
{
  char buf_version[12];
  char buf_sec_type[4];
  char buf_server_init[24];

  if (!fbs_read(fbs, buf_version, 12)) {
    return 0;
  }
  if (memcmp(buf_version, "RFB 003.003\n", 12) != 0) {
    fprintf(stderr, "Incorrect RFB protocol version\n");
    return 0;
  }
  if (!fbs_read(fbs, buf_sec_type, 4)) {
    return 0;
  }
  if (buf_get_CARD32(buf_sec_type) != 1) {
    fprintf(stderr, "Incorrect RFB protocol security type\n");
    return 0;
  }
  if (!fbs_read(fbs, buf_server_init, 24)) {
    return 0;
  }

  s_screen_info.width = buf_get_CARD16(&buf_server_init[0]);
  s_screen_info.height = buf_get_CARD16(&buf_server_init[2]);
  read_pixel_format(&buf_server_init[4]);

  s_screen_info.name_length = buf_get_CARD32(&buf_server_init[20]);
  /* FIXME: Check size. */
  s_screen_info.name = (CARD8 *)malloc(s_screen_info.name_length + 1);
  if (!fbs_read(fbs, (char *)s_screen_info.name, s_screen_info.name_length)) {
    return 0;
  }
  s_screen_info.name[s_screen_info.name_length] = '\0';

  report_rfb_init(buf_version);

  return 1;
}

static void report_rfb_init(char buf_version[12])
{
  printf("# Protocol version: %.11s\n", buf_version);
  printf("# Desktop size: %dx%d\n", s_screen_info.width, s_screen_info.height);
  printf("# Desktop name: %s\n", s_screen_info.name);
}

/* FIXME: Code duplication, see rfblib.c */
static void read_pixel_format(void *buf)
{
  CARD8 *bbuf = buf;
  RFB_PIXEL_FORMAT *format = &s_screen_info.pixformat;

  memcpy(format, buf, SZ_RFB_PIXEL_FORMAT);
  format->r_max = buf_get_CARD16(&bbuf[4]);
  format->g_max = buf_get_CARD16(&bbuf[6]);
  format->b_max = buf_get_CARD16(&bbuf[8]);
}

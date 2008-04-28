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

static void report_usage(char *program_name);
static int list_fbs(FILE *fp);
static int read_rfb_init(FBSTREAM *fbs);
static void report_rfb_init(char buf_version[12],
                            char buf_server_init[24],
                            char *ptr_desktop_name);

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

  fbs_cleanup(&fbs);
  return 1;
}

static int read_rfb_init(FBSTREAM *fbs)
{
  char buf_version[12];
  char buf_sec_type[4];
  char buf_server_init[24];
  CARD32 desktop_name_bytes;
  char *ptr_desktop_name;

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

  desktop_name_bytes = buf_get_CARD32(&buf_server_init[20]);
  /* FIXME: Check size. */
  ptr_desktop_name = (char *)malloc(desktop_name_bytes + 1);
  if (!fbs_read(fbs, ptr_desktop_name, desktop_name_bytes)) {
    return 0;
  }
  ptr_desktop_name[desktop_name_bytes] = '\0';

  report_rfb_init(buf_version, buf_server_init, ptr_desktop_name);

  free(ptr_desktop_name);

  return 1;
}

static void report_rfb_init(char buf_version[12],
                            char buf_server_init[24],
                            char *ptr_desktop_name)
{
  int width, height;

  printf("# Protocol version: %.11s\n", buf_version);

  width = buf_get_CARD16(&buf_server_init[0]);
  height = buf_get_CARD16(&buf_server_init[2]);
  printf("# Desktop size: %dx%d\n", width, height);

  printf("# Desktop name: %s\n", ptr_desktop_name);
}

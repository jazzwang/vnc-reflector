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

static const CARD32 MAX_DESKTOP_NAME_SIZE = 1024;

static void report_usage(char *program_name);
static int list_fbs(FILE *fp);
static int read_rfb_init(FBSTREAM *fbs, RFB_SCREEN_INFO *scr);
static int read_normal_protocol(FBSTREAM *fbs, RFB_SCREEN_INFO *scr);
static void read_pixel_format(RFB_SCREEN_INFO *scr, void *buf);
static int tight_24bits_format(RFB_SCREEN_INFO *scr);

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
  RFB_SCREEN_INFO screen;
  int success;

  if (!fbs_init(&fbs, fp)) {
    return 0;
  }

  if (!read_rfb_init(&fbs, &screen)) {
    return 0;
  }

  success = read_normal_protocol(&fbs, &screen);

  free(screen.name);
  fbs_cleanup(&fbs);

  return success;
}

static int read_rfb_init(FBSTREAM *fbs, RFB_SCREEN_INFO *scr)
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
  scr->width = buf_get_CARD16(&buf_server_init[0]);
  scr->height = buf_get_CARD16(&buf_server_init[2]);
  read_pixel_format(scr, &buf_server_init[4]);

  if (!tight_24bits_format(scr)) {
    fprintf(stderr, "Warning: Pixel format does not look good - ignoring\n");
  }

  scr->name_length = buf_get_CARD32(&buf_server_init[20]);
  if (scr->name_length > MAX_DESKTOP_NAME_SIZE) {
    fprintf(stderr, "Desktop name too long: %u bytes\n",
            (unsigned int)scr->name_length);
    return 0;
  }
  scr->name = (CARD8 *)malloc(scr->name_length + 1);
  if (!fbs_read(fbs, (char *)scr->name, scr->name_length)) {
    return 0;
  }
  scr->name[scr->name_length] = '\0';

  printf("# Protocol version: %.11s\n", buf_version);
  printf("# Desktop size: %dx%d\n", scr->width, scr->height);
  printf("# Desktop name: %s\n", scr->name);

  return 1;
}

static int read_normal_protocol(FBSTREAM *fbs, RFB_SCREEN_INFO *scr)
{
  return 1;
}

/* FIXME: Code duplication, see rfblib.c */
static void read_pixel_format(RFB_SCREEN_INFO *scr, void *buf)
{
  CARD8 *bbuf = buf;
  RFB_PIXEL_FORMAT *format = &scr->pixformat;

  memcpy(format, buf, SZ_RFB_PIXEL_FORMAT);
  format->r_max = buf_get_CARD16(&bbuf[4]);
  format->g_max = buf_get_CARD16(&bbuf[6]);
  format->b_max = buf_get_CARD16(&bbuf[8]);
}

static int tight_24bits_format(RFB_SCREEN_INFO *scr)
{
  RFB_PIXEL_FORMAT *fmt = &scr->pixformat;

  if (fmt->true_color &&
      fmt->bits_pixel == 32 &&
      fmt->color_depth == 24 &&
      fmt->r_max == 255 &&
      fmt->g_max == 255 &&
      fmt->b_max == 255)
    return 1;

  return 0;
}

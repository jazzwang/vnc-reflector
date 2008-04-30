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
static int fbs_check_success(FBSTREAM *fbs);
static void read_pixel_format(RFB_SCREEN_INFO *scr, void *buf);
static int check_24bits_format(RFB_SCREEN_INFO *scr);

static int read_normal_protocol(FBSTREAM *fbs, RFB_SCREEN_INFO *scr);

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
  CARD32 sec_type;
  char buf_pixformat[16];

  /* Read as much as possible, not checking for errors. */
  fbs_read(fbs, buf_version, 12);
  sec_type = fbs_read_U32(fbs);
  scr->width = fbs_read_U16(fbs);
  scr->height = fbs_read_U16(fbs);
  fbs_read(fbs, buf_pixformat, 16);
  scr->name_length = fbs_read_U32(fbs);

  /* Could we read everything? */
  if (!fbs_check_success(fbs)) {
    return 0;
  }

  /* Now examine what we have read. */
  if (memcmp(buf_version, "RFB 003.003\n", 12) != 0) {
    fprintf(stderr, "Incorrect RFB protocol version\n");
    return 0;
  }
  if (sec_type != 1) {
    fprintf(stderr, "Incorrect RFB protocol security type\n");
    return 0;
  }
  read_pixel_format(scr, buf_pixformat);
  if (!check_24bits_format(scr)) {
    fprintf(stderr, "Warning: Pixel format does not look good - ignoring\n");
  }
  if (scr->name_length > MAX_DESKTOP_NAME_SIZE) {
    fprintf(stderr, "Desktop name too long: %u bytes\n",
            (unsigned int)scr->name_length);
    return 0;
  }

  /* Finally, read the desktop name. */
  scr->name = (CARD8 *)malloc(scr->name_length + 1);
  fbs_read(fbs, (char *)scr->name, scr->name_length);
  if (!fbs_check_success(fbs)) {
    return 0;
  }
  scr->name[scr->name_length] = '\0';

  printf("# Protocol version: %.11s\n", buf_version);
  printf("# Desktop size: %dx%d\n", scr->width, scr->height);
  printf("# Desktop name: %s\n", scr->name);

  return 1;
}

static int fbs_check_success(FBSTREAM *fbs)
{
  if (fbs_error(fbs)) {
    /* No need to report errors -- already reported. */
    return 0;
  } else if (fbs_eof(fbs)) {
    fprintf(stderr, "Preliminary end of file\n");
    return 0;
  }
  return 1;
}


/* FIXME: Code duplication, see rfblib.c */
/* FIXME: Read directly from FBSTREAM. */
static void read_pixel_format(RFB_SCREEN_INFO *scr, void *buf)
{
  CARD8 *bbuf = buf;
  RFB_PIXEL_FORMAT *format = &scr->pixformat;

  memcpy(format, buf, SZ_RFB_PIXEL_FORMAT);
  format->r_max = buf_get_CARD16(&bbuf[4]);
  format->g_max = buf_get_CARD16(&bbuf[6]);
  format->b_max = buf_get_CARD16(&bbuf[8]);
}

static int check_24bits_format(RFB_SCREEN_INFO *scr)
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

/************************* Normal Protocol *************************/

static int handle_framebuffer_update(FBSTREAM *fbs);
static int handle_tight_rect(FBSTREAM *fbs, int rect_width, int rect_height);

static int handle_set_colormap_entries(FBSTREAM *fbs);
static int handle_bell(FBSTREAM *fbs);
static int handle_server_cut_text(FBSTREAM *fbs);

static int read_normal_protocol(FBSTREAM *fbs, RFB_SCREEN_INFO *scr)
{
  int msg_id;
  size_t filepos;
  size_t blksize;
  size_t offset;
  unsigned int timestamp;

  while (!fbs_eof(fbs) && !fbs_error(fbs)) {
    if (fbs_get_pos(fbs, &filepos, &blksize, &offset, &timestamp)) {
      printf("%u:\t%u/%u\t@%u\n",
             (unsigned int)filepos, (unsigned int)offset,
             (unsigned int)blksize, timestamp);
      if ((msg_id = fbs_getc(fbs)) >= 0) {
        switch(msg_id) {
        case 0:
          if (!handle_framebuffer_update(fbs)) {
            return 0;
          }
          break;
        case 1:
          if (!handle_set_colormap_entries(fbs)) {
            return 0;
          }
          break;
        case 2:
          if (!handle_bell(fbs)) {
            return 0;
          }
          break;
        case 3:
          if (!handle_server_cut_text(fbs)) {
            return 0;
          }
          break;
        default:
          fprintf(stderr, "Unknown server message type: %d\n", msg_id);
          return 0;
        }
      }
    }
  }

  return !fbs_error(fbs);
}

static int handle_framebuffer_update(FBSTREAM *fbs)
{
  CARD16 num_rects;
  int i;
  CARD16 x, y, w, h;
  INT32 encoding;

  fbs_read_U8(fbs);
  num_rects = fbs_read_U16(fbs);

  if (!fbs_eof(fbs) && !fbs_error(fbs)) {
    printf("  update, num_rects=%d\n", (int)num_rects);
    for (i = 0; i < (int)num_rects; i++) {
      x = fbs_read_U16(fbs);
      y = fbs_read_U16(fbs);
      w = fbs_read_U16(fbs);
      h = fbs_read_U16(fbs);
      encoding = fbs_read_U32(fbs);
      if (!fbs_check_success(fbs)) {
        return 0;
      }
      printf("    rect #%d\t%hux%hu\tat (%hu,%hu)\tencoding %d\n",
             i, w, h, x, y, encoding);

      /* Special case: LastRect. */
      if (encoding == -224)
        break;

      switch (encoding) {
      case 7:
        if (!handle_tight_rect(fbs, w, h)) {
          return 0;
        }
        break;
      default:
        fprintf(stderr, "Unknown encoding type\n");
        return 0;
      }
    }
  }

  return 1;
}

static int handle_tight_rect(FBSTREAM *fbs, int rect_width, int rect_height)
{
  CARD8 comp_ctl;
  int stream_id;
  char reset_str[5] = "----";
  int filter_id;
  size_t uncompressed_size;
  size_t compressed_size;
  int num_colors;
  int i;

/* Read the compression control byte. */
  comp_ctl = fbs_read_U8(fbs);
  if (!fbs_check_success(fbs)) {
    return 0;
  }

  /* Flush zlib streams if requested. */
  for (stream_id = 0; stream_id < 4; stream_id++) {
    if (comp_ctl & (1 << stream_id)) {
      reset_str[stream_id] = '0' + stream_id;
    }
  }
  comp_ctl &= 0xF0;             /* clear bits 3..0 */

  if (comp_ctl == RFB_TIGHT_FILL) {
    printf("      Tight/FILL\n");
    for (i = 0; i < 3; i++) {
      fbs_getc(fbs);
    }
    if (!fbs_check_success(fbs)) {
      return 0;
    }
  } else if (comp_ctl == RFB_TIGHT_JPEG) {
    fprintf(stderr, "Tight/JPEG not supported\n");
    return 0;
  } else if (comp_ctl > RFB_TIGHT_MAX_SUBENCODING) {
    fprintf(stderr, "Invalid sub-encoding in Tight-encoded data\n");
    return 0;
  }
  else {                        /* "basic" compression */
    stream_id = (comp_ctl >> 4) & 0x03;
    if (comp_ctl & RFB_TIGHT_EXPLICIT_FILTER) {
      filter_id = fbs_getc(fbs);
      if (!fbs_check_success(fbs)) {
        return 0;
      }
    } else {
      filter_id = RFB_TIGHT_FILTER_COPY;
    }
    if (filter_id == RFB_TIGHT_FILTER_COPY ||
        filter_id == RFB_TIGHT_FILTER_GRADIENT) {
      uncompressed_size = rect_width * rect_height * 3;
    } else if (filter_id == RFB_TIGHT_FILTER_PALETTE) {
      num_colors = fbs_getc(fbs) + 1;
      if (!fbs_check_success(fbs)) {
        return 0;
      }
      printf("      [palette, colors %d]\n", num_colors);
      for (i = 0; i < num_colors * 3; i++) {
        fbs_getc(fbs);
      }
      if (!fbs_check_success(fbs)) {
        return 0;
      }
      if (num_colors <= 2) {
        uncompressed_size = ((rect_width + 7) / 8) * rect_height;
      } else {
        uncompressed_size = rect_width * rect_height;
      }
    }
    if (uncompressed_size < RFB_TIGHT_MIN_TO_COMPRESS) {
      fprintf(stderr, "Tight/RAW not supported\n");
      return 0;
    } else {
      compressed_size = fbs_read_tight_len(fbs);
      printf("      Tight/ZLIB, filter %d, stream %d, zlib bytes %u\n",
             filter_id, stream_id, (unsigned int)compressed_size);
      if (!fbs_check_success(fbs)) {
        return 0;
      }
      for (i = 0; i < compressed_size; i++) {
        fbs_getc(fbs);
      }
      if (!fbs_check_success(fbs)) {
        return 0;
      }
    }
  }

  return 1;
}

static int handle_set_colormap_entries(FBSTREAM *fbs)
{
  printf("  colormap\n");
  fprintf(stderr, "SetColormapEntries message is not supported\n");
  return 0;
}

static int handle_bell(FBSTREAM *fbs)
{
  printf("  bell\n");
  return 1;
}

static int handle_server_cut_text(FBSTREAM *fbs)
{
  printf("  cuttext not supported\n");
  fprintf(stderr, "ServerCutText message is not supported\n");
  return 0;
}

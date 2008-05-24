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
#include "tight-decoder.h"
#include "version.h"
#include "fbsinput.h"

static const CARD32 MAX_DESKTOP_NAME_SIZE = 1024;

static void report_usage(char *program_name);
static int list_fbs(FILE *fp);
static int read_rfb_init(FBSTREAM *fbs, RFB_SCREEN_INFO *scr);
static int fbs_check_success(FBSTREAM *fbs);
static void read_pixel_format(RFB_SCREEN_INFO *scr, void *buf);
static int check_24bits_format(RFB_SCREEN_INFO *scr);

static int read_normal_protocol(FBSTREAM *fbs, RFB_SCREEN_INFO *scr,
                                TIGHT_DECODER *decoder);

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

  return success ? 0 : 1;
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
  TIGHT_DECODER decoder;
  int success;

  if (!fbs_init(&fbs, fp)) {
    return 0;
  }

  if (!read_rfb_init(&fbs, &screen)) {
    return 0;
  }

  if (!tight_decode_init(&decoder)) {
    fprintf(stderr, "Error initializing Tight decoder\n");
    return 0;
  }
  if (!tight_decode_set_framebuffer(&decoder, NULL,
                                    screen.width, screen.height, 0)) {
    fprintf(stderr, "Tight decoder: %s\n",
            tight_decode_get_error(&decoder));
    return 0;
  }

  success = read_normal_protocol(&fbs, &screen, &decoder);

  tight_decode_cleanup(&decoder);
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

/*
 * Skip bytes and report error message if got over end of file.
 */
static int fbs_skip_ex(FBSTREAM *fbs, size_t len)
{
  fbs_skip(fbs, len);
  return fbs_check_success(fbs);
}

static size_t fbs_tight_len_bytes(size_t len)
{
  if (len < 128) {
    return 1;
  } else if (len < 16384) {
    return 2;
  } else {
    return 3;
  }
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

static int read_message(FBSTREAM *fbs, TIGHT_DECODER *decoder);

static int handle_framebuffer_update(FBSTREAM *fbs, TIGHT_DECODER *decoder);
static int handle_newfbsize(TIGHT_DECODER *decoder, int w, int h);
static int handle_copyrect(FBSTREAM *fbs);
static int handle_tight_rect(FBSTREAM *fbs, TIGHT_DECODER *decoder,
                             int x, int y, int w, int h);
static int handle_cursor(FBSTREAM *fbs, int width, int height, int encoding);

static int handle_set_colormap_entries(FBSTREAM *fbs);
static int handle_bell(FBSTREAM *fbs);
static int handle_server_cut_text(FBSTREAM *fbs);

static int read_normal_protocol(FBSTREAM *fbs, RFB_SCREEN_INFO *scr,
                                TIGHT_DECODER *decoder)
{
  unsigned int idx, prev_idx = -1;
  size_t filepos;
  size_t blksize;
  size_t offset;
  unsigned int timestamp;

  while (!fbs_eof(fbs) && !fbs_error(fbs)) {
    if (fbs_get_pos(fbs, &idx, &filepos, &blksize, &offset, &timestamp)) {
      if (idx != prev_idx) {
        int not_listed = idx - prev_idx - 1;
        if (not_listed != 0) {
          printf("[blocks not listed: %d]\n", not_listed);
        }
        printf("block #%u (fpos %u, data size %u, timestamp %ums),"
               " offset %u\n",
               idx, (unsigned int)filepos, (unsigned int)blksize, timestamp,
               (unsigned int)offset);
        prev_idx = idx;
      }
      if (!read_message(fbs, decoder)) {
        return 0;
      }
    }
  }

  return !fbs_error(fbs);
}

static int read_message(FBSTREAM *fbs, TIGHT_DECODER *decoder)
{
  int msg_id;

  if ((msg_id = fbs_getc(fbs)) >= 0) {
    switch(msg_id) {
    case 0:
      if (!handle_framebuffer_update(fbs, decoder)) {
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

  return !fbs_error(fbs);
}

static int handle_framebuffer_update(FBSTREAM *fbs, TIGHT_DECODER *decoder)
{
  static int update_idx = 0;
  CARD16 num_rects;
  int i;
  CARD16 x, y, w, h;
  INT32 encoding;

  fbs_read_U8(fbs);
  num_rects = fbs_read_U16(fbs);

  if (!fbs_eof(fbs) && !fbs_error(fbs)) {
    printf("  update #%d, max rectangles %d\n", update_idx++, (int)num_rects);
    for (i = 0; i < (int)num_rects; i++) {
      x = fbs_read_U16(fbs);
      y = fbs_read_U16(fbs);
      w = fbs_read_U16(fbs);
      h = fbs_read_U16(fbs);
      encoding = fbs_read_U32(fbs);
      if (!fbs_check_success(fbs)) {
        return 0;
      }
      printf("    rect #%2d, (%4hu,%4hu) %4hu x%4hu, enc %d ",
             i, x, y, w, h, encoding);

      if (encoding == -224) {   /* RFB_ENCODING_LASTRECT */
        printf("(LastRect)\n");
        break;
      }
      if (encoding == -223) {   /* RFB_ENCODING_NEWFBSIZE */
        if (!handle_newfbsize(decoder, w, h)) {
          return 0;
        }
        break;
      }

      switch (encoding) {
      case RFB_ENCODING_COPYRECT:
        if (!handle_copyrect(fbs)) {
          return 0;
        }
        break;
      case RFB_ENCODING_TIGHT:
        if (!handle_tight_rect(fbs, decoder, x, y, w, h)) {
          return 0;
        }
        break;
      case -240:                /* RFB_ENCODING_XCURSOR */
      case -239:                /* RFB_ENCODING_RICHCURSOR */
        if (!handle_cursor(fbs, w, h, (int)encoding)) {
          return 0;
        }
        break;
      case -232:                /* RFB_ENCODING_POINTERPOS */
        printf("(PointerPos)\n");
        break;
      default:
        printf("(not supported)\n");
        fprintf(stderr, "Unknown encoding type\n");
        return 0;
      }
    }
  }

  return 1;
}

static int handle_newfbsize(TIGHT_DECODER *decoder, int w, int h)
{
  printf("(NewFBSize)\n");

  if (!tight_decode_set_framebuffer(decoder, NULL, w, h, 0)) {
    fprintf(stderr, "Tight decoder: %s\n",
            tight_decode_get_error(decoder));
    return 0;
  }

  return 1;
}

static int handle_copyrect(FBSTREAM *fbs)
{
  printf("(CopyRect)\n");
  fbs_read_U16(fbs);
  fbs_read_U16(fbs);
  return fbs_check_success(fbs);
}

static int handle_cursor(FBSTREAM *fbs, int width, int height, int encoding)
{
  int mask_size = ((width + 7) / 8) * height;
  int data_size;

  if (encoding == -239) {       /* RFB_ENCODING_RICHCURSOR */
    printf("(RichCursor)\n");
    data_size = mask_size + width * height * 4;
  } else {                      /* RFB_ENCODING_XCURSOR */
    printf("(XCursor)\n");
    data_size = ((width * height != 0) ? 6 : 0) + 2 * mask_size;
  }

  return fbs_skip_ex(fbs, data_size);
}

static int handle_tight_rect(FBSTREAM *fbs, TIGHT_DECODER *decoder,
                             int x, int y, int w, int h)
{
  int num_bytes;
  char *buf = NULL;

  num_bytes = tight_decode_start(decoder, x, y, w, h);
  while (num_bytes > 0) {
    /* FIXME: Don't allocate memory each time. */
    buf = malloc(num_bytes);
    if (buf == NULL) {
      fprintf(stderr, "Error allocating %d bytes\n", num_bytes);
      return 0;
    }
    fbs_read(fbs, buf, num_bytes);
    if (!fbs_check_success(fbs)) {
      free(buf);
      return 0;
    }
    num_bytes = tight_decode_continue(decoder, buf);
    free(buf);
  }
  if (num_bytes < 0) {
    printf("(Tight)\n");
    fprintf(stderr, "Tight decoder: %s\n", tight_decode_get_error(decoder));
    return 0;
  }

  /* Print details on Tight-encoded rectangle */
  {
    int reset_mask, zlib_stream_id, num_colors;
    char reset_str[5] = "----";
    char type_str[5] = "????";
    char z_str[4] = "+z?";
    int bytes1, bytes2, bytes3;

    reset_mask = tdstat_get_zlib_reset_mask(decoder);
    for (zlib_stream_id = 0; zlib_stream_id < 4; zlib_stream_id++) {
      if (reset_mask & (1 << zlib_stream_id)) {
        reset_str[zlib_stream_id] = '0' + zlib_stream_id;
      }
    }

    num_colors = tdstat_get_num_colors(decoder);
    if (num_colors == -2) {
      memcpy(type_str, "jpeg", 4);
    } else if (num_colors == -1) {
      memcpy(type_str, "grad", 4);
    } else if (num_colors == 0) {
      memcpy(type_str, "pure", 4);
    } else if (num_colors == 1) {
      memcpy(type_str, "fill", 4);
    } else if (num_colors == 2) {
      memcpy(type_str, "mono", 4);
    } else if (num_colors <= 256) {
      snprintf(type_str, 5, "i%03d", num_colors);
    }

    bytes3 = tdstat_get_num_compressed_bytes(decoder);
    bytes2 = (bytes3 > 0) ? fbs_tight_len_bytes(bytes3) : 0;
    bytes1 = tdstat_get_num_encoded_bytes(decoder) - bytes2 - bytes3;

    zlib_stream_id = tdstat_get_zlib_stream_id(decoder);
    if (zlib_stream_id < 0 || bytes3 == 0) {
      memcpy(z_str, "   ", 3);
    } else if (zlib_stream_id < 4) {
      z_str[2] = '0' + zlib_stream_id;
    }

    printf("(Tight: %s %s%s %d+%d+%d)\n",
           reset_str, type_str, z_str, bytes1, bytes2, bytes3);
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

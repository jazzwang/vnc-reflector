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
#include "fbsoutput.h"

typedef struct _FRAME_BUFFER {
  RFB_SCREEN_INFO info;
  TIGHT_DECODER decoder;
  u_int32_t *data;
} FRAME_BUFFER;

static const CARD32 MAX_DESKTOP_NAME_SIZE = 1024;

static void report_usage(char *program_name);
static int process_file(FILE *fp_input, FILE *fp_index, FILE *fp_keyframes);
static int read_rfb_init(FBSTREAM *fbs, RFB_SCREEN_INFO *scr);
static int write_rfb_init(FBSOUT *os, RFB_SCREEN_INFO *scr);
static int fbs_check_success(FBSTREAM *fbs);
static void read_pixel_format(RFB_SCREEN_INFO *scr, void *buf);
static int check_24bits_format(RFB_SCREEN_INFO *scr);

static int read_normal_protocol(FBSTREAM *fbs, FRAME_BUFFER *fb,
                                FBSOUT *fbk, FILE *fp_index);

int main (int argc, char *argv[])
{
  FILE *fp_input = stdin;
  int need_close_input = 0;
  FILE *fp_index = NULL;
  FILE *fp_keyframes = NULL;
  int success;

  if (argc == 2 && argv[1][0] != '-') {
    fp_input = fopen(argv[1], "rb");
    if (fp_input == NULL) {
      fprintf(stderr, "Error opening file: %s\n", argv[1]);
      return 1;
    }
  } else if (argc >= 2) {
    report_usage(argv[0]);
    return 1;
  }

  if ((fp_index = fopen("out.fbi", "wb")) == NULL ||
      (fp_keyframes = fopen("out.fbk", "wb")) == NULL) {
    fprintf(stderr, "Error creating output files\n");
    if (fp_index != NULL) {
      fclose(fp_index);
    }
    if (need_close_input) {
      fclose(fp_input);
    }
    return 1;
  }

  success = process_file(fp_input, fp_index, fp_keyframes);

  if (need_close_input) {
    fclose(fp_input);
  }
  fclose(fp_index);
  fclose(fp_keyframes);

  return success ? 0 : 1;
}

static void report_usage(char *program_name)
{
  fprintf(stderr, "fbs-list version %s.\n%s\n\n", VERSION, COPYRIGHT);

  fprintf(stderr, "Usage: %s [FBS_FILE]\n\n",
          program_name);
}

static int process_file(FILE *fp_input, FILE *fp_index, FILE *fp_keyframes)
{
  FBSTREAM fbs;
  FBSOUT fbk;
  FRAME_BUFFER fb;
  int w, h;
  int success;

  if (!fbs_init(&fbs, fp_input)) {
    return 0;
  }

  if (!fbsout_init(&fbk, fp_keyframes)) {
    fbs_cleanup(&fbs);
    return 0;
  }

  if (!read_rfb_init(&fbs, &fb.info)) {
    fbsout_cleanup(&fbk);
    fbs_cleanup(&fbs);
    return 0;
  }

  if (!write_rfb_init(&fbk, &fb.info)) {
    free(fb.info.name);
    fbsout_cleanup(&fbk);
    fbs_cleanup(&fbs);
    return 0;
  }

  w = fb.info.width;
  h = fb.info.height;

  fb.data = (u_int32_t *)malloc(w * h * 4);
  if (fb.data == NULL) {
    fprintf(stderr, "Error allocating memory (%d bytes)\n", w * h * 4);
    free(fb.info.name);
    fbsout_cleanup(&fbk);
    fbs_cleanup(&fbs);
    return 0;
  }

  if (!tight_decode_init(&fb.decoder)) {
    fprintf(stderr, "Error initializing Tight decoder\n");
    free(fb.data);
    free(fb.info.name);
    fbsout_cleanup(&fbk);
    fbs_cleanup(&fbs);
    return 0;
  }

  if (!tight_decode_set_framebuffer(&fb.decoder, fb.data, w, h, w)) {
    fprintf(stderr, "Tight decoder: %s\n",
            tight_decode_get_error(&fb.decoder));
    tight_decode_cleanup(&fb.decoder);
    free(fb.data);
    free(fb.info.name);
    fbsout_cleanup(&fbk);
    fbs_cleanup(&fbs);
    return 0;
  }

  success = read_normal_protocol(&fbs, &fb, &fbk, fp_index);

  tight_decode_cleanup(&fb.decoder);
  free(fb.data);
  free(fb.info.name);
  fbsout_cleanup(&fbk);
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

  return 1;
}

static int write_rfb_init(FBSOUT *os, RFB_SCREEN_INFO *scr)
{
  int sec_type = 1;

  fbs_write(os, "RFB 003.003\n", 12);
  fbs_write_U32(os, sec_type);
  fbs_write_U16(os, scr->width);
  fbs_write_U16(os, scr->height);

  fbs_write_U8(os, 32);         /* bits-per-pixel */
  fbs_write_U8(os, 24);         /* depth */
  fbs_write_U8(os, is_big_endian());
  fbs_write_U8(os, 1);          /* true-colour */
  fbs_write_U16(os, 255);       /* red-max */
  fbs_write_U16(os, 255);       /* green-max */
  fbs_write_U16(os, 255);       /* blue-max */
  fbs_write_U8(os, 16);         /* red-shift */
  fbs_write_U8(os, 8);          /* green-shift */
  fbs_write_U8(os, 0);          /* blue-shift */
  fbs_write_U8(os, 0);          /* padding1 */
  fbs_write_U8(os, 0);          /* padding2 */
  fbs_write_U8(os, 0);          /* padding3 */

  fbs_write_U32(os, scr->name_length);
  fbs_write(os, (char *)scr->name, scr->name_length);

  /* FIXME: Check write errors. */
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

static int read_message(FBSTREAM *fbs, FRAME_BUFFER *fb);

static int handle_framebuffer_update(FBSTREAM *fbs, FRAME_BUFFER *fb);
static int handle_newfbsize(FRAME_BUFFER *fb, int w, int h);
static int handle_copyrect(FBSTREAM *fbs);
static int handle_tight_rect(FBSTREAM *fbs, TIGHT_DECODER *decoder,
                             int x, int y, int w, int h);
static int handle_cursor(FBSTREAM *fbs, int width, int height, int encoding);

static int handle_set_colormap_entries(FBSTREAM *fbs);
static int handle_bell(FBSTREAM *fbs);
static int handle_server_cut_text(FBSTREAM *fbs);

static int write_keyframe(FRAME_BUFFER *fb, FBSOUT *fbk);

static int read_normal_protocol(FBSTREAM *fbs, FRAME_BUFFER *fb,
                                FBSOUT *fbk, FILE *fp_index)
{
  unsigned int idx;
  size_t filepos;
  size_t blksize;
  size_t offset;
  unsigned int prev_timestamp = 0;
  unsigned int timestamp;
  int interval = 10;            /* interval between keyframes, in seconds */

  while (!fbs_eof(fbs) && !fbs_error(fbs)) {
    if (fbs_get_pos(fbs, &idx, &filepos, &blksize, &offset, &timestamp)) {
      if (!read_message(fbs, fb)) {
        return 0;
      }
      if (timestamp > prev_timestamp + interval * 1000) {
        if (!fbsout_set_timestamp(fbk, timestamp, 1)) {
          return 0;
        }
        prev_timestamp = timestamp;
        if (!write_keyframe(fb, fbk)) {
          return 0;
        }
      }
    }
  }

  return !fbs_error(fbs);
}

static int read_message(FBSTREAM *fbs, FRAME_BUFFER *fb)
{
  int msg_id;

  if ((msg_id = fbs_getc(fbs)) >= 0) {
    switch(msg_id) {
    case 0:
      if (!handle_framebuffer_update(fbs, fb)) {
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

static int handle_framebuffer_update(FBSTREAM *fbs, FRAME_BUFFER *fb)
{
  CARD16 num_rects;
  int i;
  CARD16 x, y, w, h;
  INT32 encoding;

  fbs_read_U8(fbs);
  num_rects = fbs_read_U16(fbs);

  if (!fbs_eof(fbs) && !fbs_error(fbs)) {
    for (i = 0; i < (int)num_rects; i++) {
      x = fbs_read_U16(fbs);
      y = fbs_read_U16(fbs);
      w = fbs_read_U16(fbs);
      h = fbs_read_U16(fbs);
      encoding = fbs_read_U32(fbs);
      if (!fbs_check_success(fbs)) {
        return 0;
      }

      if (encoding == -224) {   /* RFB_ENCODING_LASTRECT */
        break;
      }
      if (encoding == -223) {   /* RFB_ENCODING_NEWFBSIZE */
        if (!handle_newfbsize(fb, w, h)) {
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
        if (!handle_tight_rect(fbs, &fb->decoder, x, y, w, h)) {
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
        break;
      default:
        fprintf(stderr, "Unknown encoding type\n");
        return 0;
      }
    }
  }

  return 1;
}

static int handle_newfbsize(FRAME_BUFFER *fb, int w, int h)
{
  fb->info.width = w;
  fb->info.height = h;
  fb->data = realloc(fb->data, w * h * 4);
  if (fb->data == NULL) {
    fprintf(stderr, "Error reallocating memory (%d bytes)\n", w * h * 4);
    return 0;
  }

  if (!tight_decode_set_framebuffer(&fb->decoder, fb->data, w, h, w)) {
    fprintf(stderr, "Tight decoder: %s\n",
            tight_decode_get_error(&fb->decoder));
    return 0;
  }

  return 1;
}

static int handle_copyrect(FBSTREAM *fbs)
{
  fbs_read_U16(fbs);
  fbs_read_U16(fbs);
  return fbs_check_success(fbs);
}

static int handle_cursor(FBSTREAM *fbs, int width, int height, int encoding)
{
  int mask_size = ((width + 7) / 8) * height;
  int data_size;

  if (encoding == -239) {       /* RFB_ENCODING_RICHCURSOR */
    data_size = mask_size + width * height * 4;
  } else {                      /* RFB_ENCODING_XCURSOR */
    data_size = ((width * height != 0) ? 6 : 0) + 2 * mask_size;
  }

  return fbs_skip_ex(fbs, data_size);
}

static int handle_tight_rect(FBSTREAM *fbs, TIGHT_DECODER *decoder,
                             int x, int y, int w, int h)
{
  int num_bytes;
  static char static_buf[1024];
  char *buf;
  int buf_allocated;

  num_bytes = tight_decode_start(decoder, x, y, w, h);
  while (num_bytes > 0) {
    if (num_bytes > sizeof(static_buf)) {
      buf = malloc(num_bytes);
      if (buf == NULL) {
        fprintf(stderr, "Error allocating %d bytes\n", num_bytes);
        return 0;
      }
      buf_allocated = 1;
    } else {
      buf = static_buf;
      buf_allocated = 0;
    }
    fbs_read(fbs, buf, num_bytes);
    if (!fbs_check_success(fbs)) {
      if (buf_allocated) {
        free(buf);
      }
      return 0;
    }
    num_bytes = tight_decode_continue(decoder, buf);
    if (buf_allocated) {
      free(buf);
    }
  }
  if (num_bytes < 0) {
    fprintf(stderr, "Tight decoder: %s\n", tight_decode_get_error(decoder));
    return 0;
  }

  return 1;
}

static int handle_set_colormap_entries(FBSTREAM *fbs)
{
  fprintf(stderr, "SetColormapEntries message is not supported\n");
  return 0;
}

static int handle_bell(FBSTREAM *fbs)
{
  return 1;
}

static int handle_server_cut_text(FBSTREAM *fbs)
{
  fprintf(stderr, "ServerCutText message is not supported\n");
  return 0;
}

static int write_keyframe(FRAME_BUFFER *fb, FBSOUT *fbk)
{
  fbs_write_U8(fbk, 0);         /* message-type = FramebufferUpdate */
  fbs_write_U8(fbk, 0);         /* padding */
  fbs_write_U16(fbk, 1);        /* number-of-rectangles */

  fbs_write_U16(fbk, 0);        /* x-position */
  fbs_write_U16(fbk, 0);        /* y-position */
  fbs_write_U16(fbk, fb->info.width);
  fbs_write_U16(fbk, fb->info.height);
  fbs_write_U32(fbk, 0);        /* encoding-type = Raw */

  fbs_write(fbk, (char *)fb->data, fb->info.width * fb->info.height * 4);

  /* FIXME: Check write errors. */
  return 1;
}


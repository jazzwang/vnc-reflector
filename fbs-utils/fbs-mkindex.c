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
#include <unistd.h>
#include <sys/types.h>

#include "rfblib.h"
#include "tight-decoder.h"
#include "version.h"
#include "fbsinput.h"
#include "fbsoutput.h"
#include "encode_tight.h"

typedef struct _FRAME_BUFFER {
  RFB_SCREEN_INFO info;
  TIGHT_DECODER decoder;
  u_int32_t *data;
} FRAME_BUFFER;

static const CARD32 MAX_DESKTOP_NAME_SIZE = 1024;

static void report_usage(char *program_name);
static int process_file(FILE *fp_input, FILE *fp_index, FILE *fp_keyframes,
                        int interval);
static int read_rfb_init(FBSTREAM *fbs, RFB_SCREEN_INFO *scr);
static char* construct_rfb_init(RFB_SCREEN_INFO *scr, size_t *plen);
static int write_rfb_init(FBSOUT *os, RFB_SCREEN_INFO *scr);
static int fbs_check_success(FBSTREAM *fbs);
static void read_pixel_format(RFB_SCREEN_INFO *scr, void *buf);
static int check_24bits_format(RFB_SCREEN_INFO *scr);

static int read_normal_protocol(FBSTREAM *fbs, FRAME_BUFFER *fb,
                                FBSOUT *fbk, FILE *fp_index, int interval);

int main (int argc, char *argv[])
{
  int err = 0;
  int c;
  int opt_interval = 10;
  int num_positional_args;
  char *output_fname_prefix = "out";
  int len;
  char *fname_index;
  char *fname_keyframes;
  FILE *fp_input = stdin;
  int need_close_input = 0;
  FILE *fp_index = NULL;
  FILE *fp_keyframes = NULL;
  int success;

  /* Parse the command line. */
  while (!err &&
         (c = getopt(argc, argv, "hi:")) != -1) {
    switch (c) {
    case 'h':
      err = 1;
      break;
    case 'i':
      opt_interval = atoi(optarg);
      break;
    default:
      err = 1;
    }
  }

  num_positional_args = argc - optind;

  /* Print usage help on error */
  if (err || num_positional_args < 0 || num_positional_args > 2) {
    report_usage(argv[0]);
    return 1;
  }

  /* Handle positional arguments, open input file if specified */
  if (num_positional_args > 0) {
    fp_input = fopen(argv[optind], "rb");
    if (fp_input == NULL) {
      fprintf(stderr, "Error opening file: %s\n", argv[optind]);
      return 1;
    }
    need_close_input = 1;
    if (num_positional_args > 1) {
      output_fname_prefix = argv[optind + 1];
    }
  }

  /* Form output file names */
  len = strlen(output_fname_prefix) + 5;
  fname_index = malloc(len);
  fname_keyframes = malloc(len);
  snprintf(fname_index, len, "%s.fbi", output_fname_prefix);
  snprintf(fname_keyframes, len, "%s.fbk", output_fname_prefix);

  /* Open output files */
  if ((fp_index = fopen(fname_index, "wb")) == NULL ||
      (fp_keyframes = fopen(fname_keyframes, "wb")) == NULL) {
    fprintf(stderr, "Error creating output files\n");
    if (fp_index != NULL) {
      fclose(fp_index);
    }
    if (need_close_input) {
      fclose(fp_input);
    }
    free(fname_index);
    free(fname_keyframes);
    return 1;
  }

  /* Do the work! */
  success = process_file(fp_input, fp_index, fp_keyframes, opt_interval);

  /* Cleanup */
  if (need_close_input) {
    fclose(fp_input);
  }
  fclose(fp_index);
  fclose(fp_keyframes);
  free(fname_index);
  free(fname_keyframes);

  return success ? 0 : 1;
}

static void report_usage(char *program_name)
{
  fprintf(stderr, "fbs-list version %s.\n%s\n\n", VERSION, COPYRIGHT);

  fprintf(stderr, "Usage: %s [OPTIONS...] [FBS_FILE [OUT_PREFIX]]\n\n",
          program_name);

  fprintf(stderr,
          "Output file name prefix defaults to `out', so the default\n"
          "file names are `out.fbi' (index) and `out.fbk' (key frames).\n\n");

  fprintf(stderr,
          "Options:\n"
          "  -i INTERVAL     - minimal time interval between keyframes,"
          " in seconds\n\n");
}

static int process_file(FILE *fp_input, FILE *fp_index, FILE *fp_keyframes,
                        int interval)
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

  success = read_normal_protocol(&fbs, &fb, &fbk, fp_index, interval);

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

static char* construct_rfb_init(RFB_SCREEN_INFO *scr, size_t *plen)
{
  size_t rfb_init_length;
  char *buf;
  int sec_type = 1;

  rfb_init_length = (12 +       /* RFB protocol version. */
                     4 +        /* security type */
                     24 +       /* ServerInit message */
                     scr->name_length);

  *plen = rfb_init_length;
  buf = malloc(rfb_init_length);

  if (buf != NULL) {
    memcpy(buf, "RFB 003.003\n", 12);
    buf_put_CARD32(&buf[12], sec_type);
    buf_put_CARD16(&buf[16], scr->width);
    buf_put_CARD16(&buf[18], scr->height);

    buf_put_CARD8(&buf[20], 32);    /* bits-per-pixel */
    buf_put_CARD8(&buf[21], 24);    /* depth */
    buf_put_CARD8(&buf[22], is_big_endian());
    buf_put_CARD8(&buf[23], 1);     /* true-colour */
    buf_put_CARD16(&buf[24], 255);  /* red-max */
    buf_put_CARD16(&buf[26], 255);  /* green-max */
    buf_put_CARD16(&buf[28], 255);  /* blue-max */
    buf_put_CARD8(&buf[30], 16);    /* red-shift */
    buf_put_CARD8(&buf[31], 8);     /* green-shift */
    buf_put_CARD8(&buf[32], 0);     /* blue-shift */
    buf_put_CARD8(&buf[33], 0);     /* padding1 */
    buf_put_CARD8(&buf[34], 0);     /* padding2 */
    buf_put_CARD8(&buf[35], 0);     /* padding3 */

    buf_put_CARD32(&buf[36], scr->name_length);
    memcpy(&buf[40], scr->name, scr->name_length);
  }

  return buf;
}

static int write_rfb_init(FBSOUT *os, RFB_SCREEN_INFO *scr)
{
  char* init_data;
  size_t init_data_len;
  int result;

  init_data = construct_rfb_init(scr, &init_data_len);
  result = fbs_write(os, init_data, init_data_len);
  free(init_data);

  return result;
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
                                FBSOUT *fbk, FILE *fp_index, int interval)
{
  char buf[20];
  unsigned int idx;
  size_t filepos;
  size_t blksize;
  size_t offset;
  unsigned int prev_timestamp = 0;
  unsigned int timestamp;
  static int num_keyframes = 0;
  CARD32 key_fpos, key_size;

  buf_put_CARD32(buf, 0xFFFFFFFF);
  if (fwrite("FBI 001.000\n", 1, 12, fp_index) != 12 ||
      fwrite(buf, 1, 4, fp_index) != 4) {
    fprintf(stderr, "Error writing .fbi file header\n");
    return 0;
  }
  printf("-------------------------------------------------------\n");
  printf(" timestamp | key_fpos | key_size | fbs_fpos | fbs_skip\n");
  printf("-----------+----------+----------+----------+----------\n");

  while (!fbs_eof(fbs) && !fbs_error(fbs)) {
    if (fbs_get_pos(fbs, &idx, &filepos, &blksize, &offset, &timestamp)) {
      if (!read_message(fbs, fb)) {
        return 0;
      }
      if (timestamp > prev_timestamp + interval * 1000) {
        /* Set new timestamp, flush .fbk output */
        if (!fbsout_set_timestamp(fbk, timestamp, 1)) {
          return 0;
        }
        /* Write keyframe, track file pointer position */
        key_fpos = fbsout_get_filepos(fbk);
        if (!write_keyframe(fb, fbk) || !fbsout_flush(fbk)) {
          return 0;
        }
        key_size = fbsout_get_filepos(fbk) - key_fpos;
        /* Write a record into the .fbi index file */
        buf_put_CARD32(buf, timestamp);
        buf_put_CARD32(buf + 4, key_fpos);
        buf_put_CARD32(buf + 8, key_size);
        buf_put_CARD32(buf + 12, filepos - 4);
        buf_put_CARD32(buf + 16, offset);
        if (fwrite(buf, 1, 20, fp_index) != 20) {
          fprintf(stderr, "Error writing to .fbi file\n");
          return 0;
        }
        /* Log to stdout */
        printf("%10u |%9u |%9u |%9u |%9u\n",
               timestamp,
               (unsigned int)key_fpos,
               (unsigned int)key_size,
               (unsigned int)(filepos - 4),
               (unsigned int)offset);
        /* Remember at which time point we wrote previous keyframe */
        prev_timestamp = timestamp;
        num_keyframes++;
      }
    }
  }

  /* Put correct number of records into the .fbi file */
  if (fseek(fp_index, 12, SEEK_SET) == 0) {
    buf_put_CARD32(buf, num_keyframes);
    if (fwrite(buf, 1, 4, fp_index) != 4) {
      fprintf(stderr, "Error rewriting .fbi file header\n");
      return 0;
    }
  }

  if (fbs_error(fbs)) {
    return 0;
  }

  printf("-------------------------------------------------------\n");

  return 1;
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
        fprintf(stderr, "Unknown encoding type: %d\n", (int)encoding);
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
  int num_rects;
  FB_RECT r;

  /* First, put an update with a single NewFBSize */

  fbs_write_U8(fbk, 0);         /* message-type = FramebufferUpdate */
  fbs_write_U8(fbk, 0);         /* padding */
  fbs_write_U16(fbk, 1);        /* number-of-rectangles */

  fbs_write_U16(fbk, 0);
  fbs_write_U16(fbk, 0);
  fbs_write_U16(fbk, fb->info.width);
  fbs_write_U16(fbk, fb->info.height);
  fbs_write_U32(fbk, RFB_ENCODING_NEWFBSIZE);

  /* Now, encode and write the whole framebuffer */

  configure_tight_encoder(fb->data, fb->info.width, fb->info.height, fbk);

  SET_RECT(&r, 0, 0, fb->info.width, fb->info.height);
  num_rects = num_rects_tight(&r);
  if (num_rects == 0) {
    num_rects = 0xFFFF;
  }

  fbs_write_U8(fbk, 0);         /* message-type = FramebufferUpdate */
  fbs_write_U8(fbk, 0);         /* padding */
  fbs_write_U16(fbk, num_rects);

  if (!rfb_encode_tight(&r)) {
    fprintf(stderr, "Tight encoder failed\n");
    return 0;
  }

  if (num_rects == 0xFFFF) {
    fbs_write_U16(fbk, 0);
    fbs_write_U16(fbk, 0);
    fbs_write_U16(fbk, 0);
    fbs_write_U16(fbk, 0);
    fbs_write_U32(fbk, RFB_ENCODING_LASTRECT);
  }

  /* FIXME: Check write errors. */
  return 1;
}


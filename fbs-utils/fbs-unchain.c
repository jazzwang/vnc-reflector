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

#include <zlib.h>

#include "rfblib.h"
#include "version.h"
#include "fbsinput.h"
#include "fbsoutput.h"

static const CARD32 MAX_DESKTOP_NAME_SIZE = 1024;

static void report_usage(char *program_name);
static int convert_fbs(FILE *fp);
static int copy_rfb_init(FBSTREAM *is, FBSOUT *os, RFB_SCREEN_INFO *scr);
static int fbs_check_success(FBSTREAM *is);
static int fbs_copy(FBSTREAM *is, FBSOUT *os, size_t len);
static void read_pixel_format(RFB_SCREEN_INFO *scr, void *buf);
static int check_24bits_format(RFB_SCREEN_INFO *scr);

static int read_normal_protocol(FBSTREAM *is, FBSOUT *os,
                                RFB_SCREEN_INFO *scr);

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

  if (convert_fbs(fp)) {
    success = 1;
  }

  if (needClose) {
    fclose(fp);
  }

  return success ? 0 : 1;
}

static void report_usage(char *program_name)
{
  fprintf(stderr, "fbs-unchain version %s.\n%s\n\n", VERSION, COPYRIGHT);

  fprintf(stderr, "Usage: %s [FBS_FILE] [> CONVERTED_FILE]\n\n",
          program_name);
}

static int convert_fbs(FILE *fp)
{
  FBSTREAM is;                  /* input stream */
  FBSOUT os;                    /* output stream */
  RFB_SCREEN_INFO screen;
  int success;

  if (!fbs_init(&is, fp) || !fbsout_init(&os, stdout)) {
    return 0;
  }

  if (!copy_rfb_init(&is, &os, &screen)) {
    return 0;
  }

  success = read_normal_protocol(&is, &os, &screen);

  free(screen.name);
  fbsout_cleanup(&os);
  fbs_cleanup(&is);

  return success;
}

static int copy_rfb_init(FBSTREAM *is, FBSOUT *os, RFB_SCREEN_INFO *scr)
{
  char buf_version[12];
  CARD32 sec_type;
  char buf_pixformat[16];

  /* Read as much as possible, not checking for errors. */
  fbs_read(is, buf_version, 12);
  sec_type = fbs_read_U32(is);
  scr->width = fbs_read_U16(is);
  scr->height = fbs_read_U16(is);
  fbs_read(is, buf_pixformat, 16);
  scr->name_length = fbs_read_U32(is);

  /* Could we read everything? */
  if (!fbs_check_success(is)) {
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
  fbs_read(is, (char *)scr->name, scr->name_length);
  if (!fbs_check_success(is)) {
    return 0;
  }
  scr->name[scr->name_length] = '\0';

  /* Copy the same data to the output stream. */
  fbs_write(os, buf_version, 12);
  fbs_write_U32(os, sec_type);
  fbs_write_U16(os, scr->width);
  fbs_write_U16(os, scr->height);
  fbs_write(os, buf_pixformat, 16);
  fbs_write_U32(os, scr->name_length);
  fbs_write(os, (char *)scr->name, scr->name_length);

  return !fbsout_error(os);
}

static int fbs_check_success(FBSTREAM *is)
{
  if (fbs_error(is)) {
    /* No need to report errors -- already reported. */
    return 0;
  } else if (fbs_eof(is)) {
    fprintf(stderr, "Preliminary end of file\n");
    return 0;
  }
  return 1;
}

/*
 * Copy bytes from input to output, check errors, report error message
 * if got over end of file.
 */
static int fbs_copy(FBSTREAM *is, FBSOUT *os, size_t len)
{
  char *buf;
  int success;

  buf = malloc(len);
  fbs_read(is, buf, len);
  success = fbs_check_success(is);
  if (success) {
    success = fbs_write(os, buf, len);
  }
  free(buf);

  return success;
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

static int handle_framebuffer_update(FBSTREAM *is, FBSOUT *os);
static int handle_copyrect(FBSTREAM *is, FBSOUT *os);
static int handle_tight(FBSTREAM *is, FBSOUT *os, int w, int h, int reset);
static int handle_cursor(FBSTREAM *is, FBSOUT *os, int w, int h, int encoding);

static int handle_set_colormap_entries(FBSTREAM *is, FBSOUT *os);
static int handle_bell(FBSTREAM *is, FBSOUT *os);
static int handle_server_cut_text(FBSTREAM *is, FBSOUT *os);

static int read_normal_protocol(FBSTREAM *is, FBSOUT *os, RFB_SCREEN_INFO *scr)
{
  int msg_id;
  unsigned int idx;
  size_t filepos;
  size_t blksize;
  size_t offset;
  unsigned int timestamp;

  while (!fbs_eof(is) && !fbs_error(is)) {
    if (fbs_get_pos(is, &idx, &filepos, &blksize, &offset, &timestamp)) {
      fbsout_set_timestamp(os, timestamp, 1);
      if ((msg_id = fbs_getc(is)) >= 0) {
        fbs_putc(os, msg_id);
        switch(msg_id) {
        case 0:
          if (!handle_framebuffer_update(is, os)) {
            return 0;
          }
          break;
        case 1:
          if (!handle_set_colormap_entries(is, os)) {
            return 0;
          }
          break;
        case 2:
          if (!handle_bell(is, os)) {
            return 0;
          }
          break;
        case 3:
          if (!handle_server_cut_text(is, os)) {
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

  return !fbs_error(is);
}

static int handle_framebuffer_update(FBSTREAM *is, FBSOUT *os)
{
  CARD8 padding;
  CARD16 num_rects;
  int i;
  CARD16 x, y, w, h;
  INT32 encoding;
  int reset_zlib = 1;

  padding = fbs_read_U8(is);
  num_rects = fbs_read_U16(is);

  if (!fbs_eof(is) && !fbs_error(is)) {

    fbs_write_U8(os, padding);
    fbs_write_U16(os, num_rects);

    for (i = 0; i < (int)num_rects; i++) {
      x = fbs_read_U16(is);
      y = fbs_read_U16(is);
      w = fbs_read_U16(is);
      h = fbs_read_U16(is);
      encoding = fbs_read_U32(is);
      if (!fbs_check_success(is)) {
        return 0;
      }

      fbs_write_U16(os, x);
      fbs_write_U16(os, y);
      fbs_write_U16(os, w);
      fbs_write_U16(os, h);
      fbs_write_U32(os, encoding);

      if (encoding == -224) {   /* RFB_ENCODING_LASTRECT */
        break;
      }
      if (encoding == -223) {   /* RFB_ENCODING_NEWFBSIZE */
        break;
      }

      switch (encoding) {
      case RFB_ENCODING_COPYRECT:
        if (!handle_copyrect(is, os)) {
          return 0;
        }
        break;
      case RFB_ENCODING_TIGHT:
        if (!handle_tight(is, os, w, h, reset_zlib)) {
          return 0;
        }
        reset_zlib = 0;
        break;
      case -240:                /* RFB_ENCODING_XCURSOR */
      case -239:                /* RFB_ENCODING_RICHCURSOR */
        if (!handle_cursor(is, os, w, h, (int)encoding)) {
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

static int handle_copyrect(FBSTREAM *is, FBSOUT *os)
{
  CARD16 src_x, src_y;

  src_x = fbs_read_U16(is);
  src_y = fbs_read_U16(is);
  if (!fbs_check_success(is)) {
    return 0;
  }

  fbs_write_U16(os, src_x);
  fbs_write_U16(os, src_y);

  return 1;
}

static int handle_cursor(FBSTREAM *is, FBSOUT *os, int w, int h, int encoding)
{
  int mask_size = ((w + 7) / 8) * h;
  int data_size;

  if (encoding == -239) {       /* RFB_ENCODING_RICHCURSOR */
    data_size = mask_size + w * h * 4;
  } else {                      /* RFB_ENCODING_XCURSOR */
    data_size = ((w * h != 0) ? 6 : 0) + 2 * mask_size;
  }

  return fbs_copy(is, os, data_size);
}

static void zlib_reset_stream_in(int stream_id);
static void zlib_reset_streams_out(void);

static int zlib_convert(FBSTREAM *is, FBSOUT *os, int zlib_stream_id,
                        size_t raw_size);

static int handle_tight(FBSTREAM *is, FBSOUT *os, int w, int h, int reset)
{
  CARD8 comp_ctl;
  int stream_id;
  int filter_id;
  size_t uncompressed_size;
  size_t compressed_size;
  int num_colors;

  /* Read the compression control byte. */
  comp_ctl = fbs_read_U8(is);
  if (!fbs_check_success(is)) {
    return 0;
  }

  /* Reset input zlib streams if requested. */
  for (stream_id = 0; stream_id < 4; stream_id++) {
    if (comp_ctl & (1 << stream_id)) {
      zlib_reset_stream_in(stream_id);
    }
  }
  comp_ctl &= 0xF0;             /* clear bits 3..0 */

  /* Reset output zlib streams if needed. */
  if (reset) {
    fbs_write_U8(os, comp_ctl | 0x0F);
    zlib_reset_streams_out();
  } else {
    fbs_write_U8(os, comp_ctl);
  }

  if (comp_ctl == RFB_TIGHT_FILL) {
    return fbs_copy(is, os, 3);
  }

  if (comp_ctl == RFB_TIGHT_JPEG) {
    compressed_size = fbs_read_tight_len(is);
    if (!fbs_check_success(is)) {
      return 0;
    }
    fbs_write_tight_len(os, compressed_size);
    return fbs_copy(is, os, compressed_size);
  }

  if (comp_ctl > RFB_TIGHT_MAX_SUBENCODING) {
    fprintf(stderr, "Invalid sub-encoding in Tight-encoded data\n");
    return 0;
  }

  /* "Basic" compression. First, get zlib stream id and filter type. */
  stream_id = (comp_ctl >> 4) & 0x03;
  if (comp_ctl & RFB_TIGHT_EXPLICIT_FILTER) {
    filter_id = fbs_getc(is);
    if (!fbs_check_success(is)) {
      return 0;
    }
    fbs_write_U8(os, filter_id);
  } else {
    filter_id = RFB_TIGHT_FILTER_COPY;
  }

  if (filter_id == RFB_TIGHT_FILTER_COPY) {
    uncompressed_size = w * h * 3;
  } else if (filter_id == RFB_TIGHT_FILTER_GRADIENT) {
    uncompressed_size = w * h * 3;
  } else if (filter_id == RFB_TIGHT_FILTER_PALETTE) {
    num_colors = fbs_getc(is) + 1;
    if (!fbs_check_success(is)) {
      return 0;
    }
    fbs_write_U8(os, num_colors - 1);
    if (!fbs_copy(is, os, num_colors * 3)) {
      return 0;
    }
    if (num_colors <= 2) {
      uncompressed_size = ((w + 7) / 8) * h;
    } else {
      uncompressed_size = w * h;
    }
  } else {
    fprintf(stderr, "Invalid filter id in Tight-encoded data\n");
    return 0;
  }
  if (uncompressed_size < RFB_TIGHT_MIN_TO_COMPRESS) {
    return fbs_copy(is, os, uncompressed_size);
  } else {
    return zlib_convert(is, os, stream_id, uncompressed_size);
  }
}

static int handle_set_colormap_entries(FBSTREAM *is, FBSOUT *os)
{
  fprintf(stderr, "SetColormapEntries message is not supported\n");
  return 0;
}

static int handle_bell(FBSTREAM *is, FBSOUT *os)
{
  return 1;
}

static int handle_server_cut_text(FBSTREAM *is, FBSOUT *os)
{
  fprintf(stderr, "ServerCutText message is not supported\n");
  return 0;
}

/************************* Zlib Conversion *************************/

static z_stream s_zstream_in[4];
static int s_zstream_in_active[4] = { 0, 0, 0, 0 };

static z_stream s_zstream_out[4];
static int s_zstream_out_active[4] = { 0, 0, 0, 0 };

static void zlib_reset_stream_in(int stream_id)
{
  if (s_zstream_in_active[stream_id]) {
    if (inflateEnd(&s_zstream_in[stream_id]) != Z_OK) {
      if (s_zstream_in[stream_id].msg != NULL) {
        fprintf(stderr, "inflateEnd() failed: %s\n",
                s_zstream_in[stream_id].msg);
      } else {
        fprintf(stderr, "inflateEnd() failed\n");
      }
    }
    s_zstream_in_active[stream_id] = 0;
  }
}

static void zlib_reset_streams_out(void)
{
  int stream_id;

  for (stream_id = 0; stream_id < 4; stream_id++) {
    if (s_zstream_out_active[stream_id]) {
      if (deflateReset(&s_zstream_out[stream_id]) != Z_OK) {
        if (s_zstream_out[stream_id].msg != NULL) {
          fprintf(stderr, "deflateEnd() failed: %s\n",
                  s_zstream_out[stream_id].msg);
        } else {
          fprintf(stderr, "deflateEnd() failed\n");
        }
      }
      /* s_zstream_out_active[stream_id] = 0; */
    }
  }
}

static int zlib_convert(FBSTREAM *is, FBSOUT *os, int stream_id,
                        size_t raw_size)
{
  z_streamp zs;
  int err;
  size_t zlib_in_size;
  size_t zlib_out_size;
  char *zlib_in_data;
  char *zlib_out_data;
  char *raw_data;
  int success;

  zlib_in_size = fbs_read_tight_len(is);
  if (!fbs_check_success(is)) {
    return 0;
  }

  /* Initialize decompression stream if needed */

  zs = &s_zstream_in[stream_id];
  if (!s_zstream_in_active[stream_id]) {
    zs->zalloc = Z_NULL;
    zs->zfree = Z_NULL;
    zs->opaque = Z_NULL;
    err = inflateInit(zs);
    if (err != Z_OK) {
      if (zs->msg != NULL) {
        fprintf(stderr, "inflateInit() failed: %s\n", zs->msg);
      } else {
        fprintf(stderr, "inflateInit() failed\n");
      }
      return 0;
    }
    s_zstream_in_active[stream_id] = 1;
  }

  /* Allocate buffers to put zlib and raw data into */

  zlib_in_data = malloc(zlib_in_size);
  raw_data = malloc(raw_size);

  /* Read the data. */

  if (!fbs_read(is, zlib_in_data, zlib_in_size)) {
    free(zlib_in_data);
    free(raw_data);
    return 0;
  }

  /* Decompress the data */

  zs->next_in = (Bytef *)zlib_in_data;
  zs->avail_in = zlib_in_size;
  zs->next_out = (Bytef *)raw_data;
  zs->avail_out = raw_size;

  err = inflate(zs, Z_SYNC_FLUSH);
  if (err != Z_OK && err != Z_STREAM_END) {
    if (zs->msg != NULL) {
      fprintf(stderr, "inflate() failed: %s\n", zs->msg);
    } else {
      fprintf(stderr, "inflate() failed: %d\n", err);
    }
    free(zlib_in_data);
    free(raw_data);
    return 0;
  }

  if (zs->avail_out > 0)
    fprintf(stderr, "Decompressed data size is less than expected\n");

  /* Initialize compression stream if needed. */

  zs = &s_zstream_out[stream_id];
  if (!s_zstream_out_active[stream_id]) {
    zs->zalloc = Z_NULL;
    zs->zfree = Z_NULL;
    zs->opaque = Z_NULL;

    err = deflateInit2(zs, Z_BEST_COMPRESSION, Z_DEFLATED, MAX_WBITS,
                       MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (err != Z_OK) {
      free(zlib_in_data);
      free(raw_data);
      return 0;
    }

    s_zstream_out_active[stream_id] = 1;
  }

  /* Allocate buffer for compressed data. */

  zlib_out_size = deflateBound(zs, raw_size) + 1;
  zlib_out_data = malloc(zlib_out_size);

  /* Prepare buffer pointers. */

  zs->next_in = (Bytef *)raw_data;
  zs->avail_in = raw_size;
  zs->next_out = (Bytef *)zlib_out_data;
  zs->avail_out = zlib_out_size;

  /* Actual compression. */

  if (deflate(zs, Z_SYNC_FLUSH) != Z_OK ||
      zs->avail_in != 0 || zs->avail_out == 0) {
    free(zlib_in_data);
    free(raw_data);
    free(zlib_out_data);
    return 0;
  }

  /* Write compressed data. */

  zlib_out_size -= zs->avail_out;
  fbs_write_tight_len(os, zlib_out_size);
  success = fbs_write(os, zlib_out_data, zlib_out_size);

  /* Free memory. */

  free(zlib_in_data);
  free(raw_data);
  free(zlib_out_data);

  return success;
}

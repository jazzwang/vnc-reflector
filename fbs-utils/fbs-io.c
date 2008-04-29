/*
 * FrameBuffer Stream Utilities.
 * Copyright (C) 2008 Wimba, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file
 * LICENSE, included.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "rfblib.h"
#include "fbs-io.h"

static int fbs_read_block(FBSTREAM *fbs);
static void fbs_free_block(FBSTREAM *fbs);

/************************* Public Functions *************************/

int fbs_init(FBSTREAM *fbs, FILE *fp)
{
  char version_msg[12];

  /* Initialize the structure. */
  memset(fbs, 0, sizeof(FBSTREAM));
  fbs->fp = fp;

  if (fread(version_msg, 1, 12, fbs->fp) != 12) {
    fprintf(stderr, "Error reading the file header\n");
    return 0;
  }

  if (memcmp(version_msg, "FBS 001.000\n", 12) != 0) {
    fprintf(stderr, "Bad file header\n");
    return 0;
  }

  if (!fbs_read_block(fbs)) {
    return 0;
  }

  return 1;
}

void fbs_cleanup(FBSTREAM *fbs)
{
  fbs_free_block(fbs);
}

int fbs_getc(FBSTREAM *fbs)
{
  int c;

  if (fbs->block_data == NULL) {
    return -1;
  }

  c = fbs->block_data[fbs->block_offset] & 0xFF;
  fbs->block_offset++;
  fbs->file_offset++;
  if (fbs->block_offset >= fbs->block_size) {
    fbs_read_block(fbs);
  }
  return c;
}

/*
 * FIXME: Using fbs_getc() to read each byte is inefficient.
 */
int fbs_read(FBSTREAM *fbs, char *buf, size_t len)
{
  int i, c;

  for (i = 0; i < len; i++) {
    c = fbs_getc(fbs);
    if (c < 0) {
      return 0;
    }
    buf[i] = (char)c;
  }

  return 1;
}

CARD8 fbs_read_U8(FBSTREAM *fbs)
{
  return (CARD8)fbs_getc(fbs);
}

CARD16 fbs_read_U16(FBSTREAM *fbs)
{
  CARD16 hi, lo;

  hi = (CARD16)fbs_getc(fbs);
  lo = (CARD16)fbs_getc(fbs);

  return hi << 8 | lo;
}

CARD32 fbs_read_U32(FBSTREAM *fbs)
{
  CARD32 b3, b2, b1, b0;

  b3 = (CARD32)fbs_getc(fbs);
  b2 = (CARD32)fbs_getc(fbs);
  b1 = (CARD32)fbs_getc(fbs);
  b0 = (CARD32)fbs_getc(fbs);

  return b3 << 24 | b2 << 16 | b1 << 8 | b0;
}

INT8 fbs_read_S8(FBSTREAM *fbs)
{
  return (INT8)fbs_getc(fbs);
}

INT16 fbs_read_S16(FBSTREAM *fbs)
{
  INT16 hi, lo;

  hi = (INT8)fbs_getc(fbs);
  lo = (INT16)fbs_getc(fbs);

  return hi << 8 | lo;
}

INT32 fbs_read_S32(FBSTREAM *fbs)
{
  INT32 b3, b2, b1, b0;

  b3 = (INT8)fbs_getc(fbs);
  b2 = (INT32)fbs_getc(fbs);
  b1 = (INT32)fbs_getc(fbs);
  b0 = (INT32)fbs_getc(fbs);

  return b3 << 24 | b2 << 16 | b1 << 8 | b0;
}

size_t fbs_get_block_size(FBSTREAM *fbs)
{
  return fbs->block_size;
}

size_t fbs_get_block_offset(FBSTREAM *fbs)
{
  return fbs->block_offset;
}

size_t fbs_get_file_offset(FBSTREAM *fbs)
{
  return fbs->file_offset;
}

unsigned long fbs_get_last_byte_timestamp(FBSTREAM *fbs)
{
  return (fbs->block_offset > 0) ? fbs->timestamp : fbs->prev_timestamp;
}

unsigned long fbs_get_next_byte_timestamp(FBSTREAM *fbs)
{
  return fbs->timestamp;
}

int fbs_eof(FBSTREAM *fbs)
{
  return fbs->end_reached;
}

int fbs_error(FBSTREAM *fbs)
{
  return (fbs->block_data == NULL && !fbs_eof(fbs));
}

/************************* Private Code *************************/

static const size_t MAX_BLOCK_SIZE = 1024 * 1024;

static int fbs_read_block(FBSTREAM *fbs)
{
  char buf[4];
  size_t buf_size;
  int n;

  fbs_free_block(fbs);

  n = fread(buf, 1, 4, fbs->fp);
  if (n == 0 && feof(fbs->fp)) {
    fbs->end_reached = 1;
    return 1;
  }
  if (n != 4) {
    fprintf(stderr, "Error reading block header\n");
    return 0;
  }

  fbs->block_size = buf_get_CARD32(buf);
  if (fbs->block_size == 0 || fbs->block_size > MAX_BLOCK_SIZE) {
    fprintf(stderr, "Bad block size: %d\n", fbs->block_size);
    return 0;
  }

  /* Data is padded to multiple of 4 bytes. */
  buf_size = (fbs->block_size + 3) & (~3);

  fbs->block_offset = 0;
  fbs->block_data = malloc(buf_size);

  if (fread(fbs->block_data, 1, buf_size, fbs->fp) != buf_size) {
    fprintf(stderr, "Error reading data\n");
    fbs_free_block(fbs);
    return 0;
  }

  if (fread(buf, 1, 4, fbs->fp) != 4) {
    fprintf(stderr, "Error reading block timestamp\n");
    fbs_free_block(fbs);
    return 0;
  }

  fbs->prev_timestamp = fbs->timestamp;
  fbs->timestamp = buf_get_CARD32(buf);

  return 1;
}

static void fbs_free_block(FBSTREAM *fbs)
{
  if (fbs->block_data != NULL) {
    free(fbs->block_data);
    fbs->block_data = NULL;
  }
}

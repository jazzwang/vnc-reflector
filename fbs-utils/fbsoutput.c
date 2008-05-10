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
#include "fbsoutput.h"

/* FIXME: Replace with reasonable values. */
static const size_t INITIAL_BUFFER_SIZE = 4;
static const size_t BUFFER_SIZE_INCREMENT = 8;

/************************* Public Functions *************************/

int fbsout_init(FBSOUT *fbs, FILE *fp)
{
  char version_msg[] = "FBS 001.000\n";

  /* Initialize the structure. */
  memset(fbs, 0, sizeof(FBSOUT));
  fbs->fp = fp;

  /* Allocate buffer. */
  fbs->block_size = INITIAL_BUFFER_SIZE;
  fbs->block_data = malloc(fbs->block_size);

  /* Leave the place for 4 bytes of byte count. */
  fbs->offset_in_block = 4;

  /* Write file signature. */
  if (fwrite(version_msg, 1, 12, fbs->fp) != 12) {
    fprintf(stderr, "Error writing file header\n");
    fbs->error = 1;
    return 0;
  }

  return 1;
}

void fbsout_cleanup(FBSOUT *fbs)
{
  fbsout_flush(fbs);
  if (fbs->block_data != NULL) {
    free(fbs->block_data);
    fbs->block_data = NULL;
  }
}

int fbs_putc(FBSOUT *fbs, int c)
{
  if (fbs->error) {
    return -1;
  }

  /* Make sure there is place for one byte of data,
     3 bytes padding, and 4 bytes of timestamp. */
  if (fbs->offset_in_block + 7 >= fbs->block_size) {
    size_t new_size;
    void *new_data;

    new_size = fbs->block_size + BUFFER_SIZE_INCREMENT;
    new_data = realloc(fbs->block_data, new_size);
    if (new_data == NULL) {
      fprintf(stderr, "Error allocating memory\n");
      return -1;
    } else {
      fbs->block_size = new_size;
      fbs->block_data = new_data;
    }
  }

  fbs->block_data[fbs->offset_in_block++] = (char)c;

  return c & 0xFF;
}

/* FIXME: Using fbs_putc() to write each byte is inefficient. */

int fbs_write(FBSOUT *fbs, char *buf, size_t len)
{
  int i, c;

  for (i = 0; i < len; i++) {
    c = buf[i] & 0xFF;
    if (fbs_putc(fbs, c) < 0) {
      return 0;
    }
  }

  return 1;
}

int fbs_write_U8(FBSOUT *fbs, CARD8 value)
{
  if (fbs_putc(fbs, (int)value) < 0) {
    return 0;
  }
  return 1;
}

int fbs_write_U16(FBSOUT *fbs, CARD16 value)
{
  if (fbs_putc(fbs, (int)value >> 8) < 0 ||
      fbs_putc(fbs, (int)value) < 0) {
    return 0;
  }
  return 1;
}

int fbs_write_U32(FBSOUT *fbs, CARD32 value)
{
  if (fbs_putc(fbs, (int)value >> 24) < 0 ||
      fbs_putc(fbs, (int)value >> 16) < 0 ||
      fbs_putc(fbs, (int)value >> 8) < 0 ||
      fbs_putc(fbs, (int)value) < 0) {
    return 0;
  }
  return 1;
}

int fbs_write_S8(FBSOUT *fbs, INT8 value)
{
  return fbs_write_U8(fbs, (CARD8)value);
}

int fbs_write_S16(FBSOUT *fbs, INT16 value)
{
  return fbs_write_U16(fbs, (CARD16)value);
}

int fbs_write_S32(FBSOUT *fbs, INT32 value)
{
  return fbs_write_U32(fbs, (CARD32)value);
}

int fbs_write_tight_len(FBSOUT *fbs, size_t value)
{
  char buf[3];
  size_t len_bytes = 0;

  buf[len_bytes++] = (char)(value & 0x7F);
  if (value > 0x7F) {
    buf[len_bytes-1] |= 0x80;
    buf[len_bytes++] = (char)(value >> 7 & 0x7F);
    if (value > 0x3FFF) {
      buf[len_bytes-1] |= 0x80;
      buf[len_bytes++] = (char)(value >> 14 & 0xFF);
    }
  }

  return fbs_write(fbs, buf, len_bytes);
}

int fbsout_set_timestamp(FBSOUT *fbs, unsigned int timestamp, int can_flush)
{
  int success = 1;

  if (can_flush && timestamp > fbs->timestamp) {
    success = fbsout_flush(fbs);
  }
  fbs->timestamp = timestamp;

  return success;
}

int fbsout_flush(FBSOUT *fbs)
{
  unsigned int len, ts;
  char *ptr;

  if (fbs->error) {
    return -1;
  }

  /* Check if there is something to flush. */
  if (fbs->offset_in_block > 4) {

    /* Store byte counter in the beginning of buffer. */
    len = (unsigned int)(fbs->offset_in_block - 4);
    ptr = fbs->block_data;
    ptr[0] = (char)(len >> 24);
    ptr[1] = (char)(len >> 16);
    ptr[2] = (char)(len >> 8);
    ptr[3] = (char)(len);

    /* Pad data with zeroes to next multiple of 32 bits. */
    ptr = fbs->block_data + fbs->offset_in_block;
    while ((len & 3) != 0) {
      *ptr++ = (char)0;
      len++;
    }

    /* Append timestamp at the end. */
    ts = fbs->timestamp;
    ptr[0] = (char)(ts >> 24);
    ptr[1] = (char)(ts >> 16);
    ptr[2] = (char)(ts >> 8);
    ptr[3] = (char)(ts);

    /* Write everything at once. */
    if (fwrite(fbs->block_data, 1, len + 8, fbs->fp) != len + 8) {
      fprintf(stderr, "Error writing data\n");
      fbs->error = 1;
      return 0;
    }

    /* DEBUG: */
    if (fbs->block_size < len + 8) {
      fprintf(stderr, "DEBUG: Buffer overflow\n");
    }

    /* Reset position in the buffer. */
    fbs->offset_in_block = 4;

  }

  return 1;
}


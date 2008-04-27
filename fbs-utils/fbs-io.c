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

static int read_block(FBSTREAM *fbs);
static void free_block(FBSTREAM *fbs);

/************************* Public Functions *************************/

int open_fbstream(FBSTREAM *fbs, FILE *fp)
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

  if (!read_block(fbs)) {
    return 0;
  }

  return 1;
}

void close_fbstream(FBSTREAM *fbs)
{
  /* NOTE: We do not close the file itself! */
  free_block(fbs);
}

size_t get_block_size(FBSTREAM *fbs)
{
  return fbs->block_size;
}

size_t get_block_offset(FBSTREAM *fbs)
{
  return fbs->block_offset;
}


size_t get_file_offset(FBSTREAM *fbs)
{
  return fbs->file_offset;
}

unsigned long get_last_byte_timestamp(FBSTREAM *fbs)
{
  return (fbs->block_offset > 0) ? fbs->timestamp : fbs->prev_timestamp;
}

unsigned long get_next_byte_timestamp(FBSTREAM *fbs)
{
  return fbs->timestamp;
}

/************************* Private Code *************************/

static int read_block(FBSTREAM *fbs)
{
  char buf[4];
  size_t buf_size;

  free_block(fbs);

  if (fread(buf, 1, 4, fbs->fp) != 4) {
    fprintf(stderr, "Error reading block header\n");
    return 0;
  }

  fbs->block_size = buf_size = buf_get_CARD32(buf);
  fbs->block_offset = 0;
  fbs->block_data = malloc(fbs->block_size);

  /* Data is padded to multiple of 4 bytes. */
  buf_size = (buf_size + 3) & (~3);

  if (fread(fbs->block_data, 1, buf_size, fbs->fp) != buf_size) {
    fprintf(stderr, "Error reading data\n");
    return 0;
  }

  if (fread(buf, 1, 4, fbs->fp) != 4) {
    fprintf(stderr, "Error reading block header\n");
    return 0;
  }

  fbs->prev_timestamp = fbs->timestamp;
  fbs->timestamp = buf_get_CARD32(buf);

  return 1;
}

static void free_block(FBSTREAM *fbs)
{
  if (fbs->block_data != NULL) {
    free(fbs->block_data);
    fbs->block_data = NULL;
  }
}

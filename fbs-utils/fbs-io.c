/*
 * FrameBuffer Stream Utilities.
 * Copyright (C) 2008 Wimba, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file
 * LICENSE, included.
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>

#include "fbs-io.h"

static int read_block_header(FBSTREAM *fbs);

/************************* Public functions *************************/

int open_fbstream(FBSTREAM *fbs, FILE *fp)
{
  char version_msg[12];

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

  if (!read_block_header(fbs)) {
    return 0;
  }

  return 1;
}

size_t get_block_consumed(FBSTREAM *fbs)
{
  return fbs->consumed;
}

size_t get_block_available(FBSTREAM *fbs)
{
  return fbs->available;
}


size_t get_total_consumed(FBSTREAM *fbs)
{
  return fbs->total_consumed;
}

unsigned long get_last_byte_timestamp(FBSTREAM *fbs)
{
  return fbs->last_timestamp;
}

unsigned long get_next_byte_timestamp(FBSTREAM *fbs)
{
  return fbs->next_timestamp;
}

/************************* Private functions *************************/

static int read_block_header(FBSTREAM *fbs)
{
  fprintf(stderr, "Not implemented yet\n");
  return 0;
}

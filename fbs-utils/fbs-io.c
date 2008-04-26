/*
 * FrameBuffer Stream Utilities.
 * Copyright (C) 2008 Wimba, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file
 * LICENSE, included.
 */

#include "fbs-io.h"

int new_fbstream(FBSTREAM *fbs, FILE *fp)
{
  fbs->fp = fp;
  fbs->consumed = 0;
  fbs->available = 0;
  fbs->total_consumed = 0;
  fbs->last_timestamp = 0;
  fbs->next_timestamp = 0;
  fbs->end_reached = 0;

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

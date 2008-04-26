/*
 * FrameBuffer Stream Utilities.
 * Copyright (C) 2008 Wimba, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file
 * LICENSE, included.
 */

#ifndef _FBSUTIL_IO_H
#define _FBSUTIL_IO_H

#include <stdio.h>
#include <sys/types.h>

typedef struct _FBSTREAM {
  FILE *fp;
  size_t consumed;
  size_t available;
  size_t total_consumed;
  unsigned long last_timestamp;
  unsigned long next_timestamp;
  int end_reached;
} FBSTREAM;

extern int open_fbstream(FBSTREAM *fbs, FILE *fp);

extern size_t get_block_consumed(FBSTREAM *fbs);
extern size_t get_block_available(FBSTREAM *fbs);
extern size_t get_total_consumed(FBSTREAM *fbs);

extern unsigned long get_last_byte_timestamp(FBSTREAM *fbs);
extern unsigned long get_next_byte_timestamp(FBSTREAM *fbs);

#endif /* defined(_FBSUTIL_IO_H) */

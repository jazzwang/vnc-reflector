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
  char *block_data;
  size_t block_size;
  size_t block_offset;
  size_t file_offset;
  unsigned int timestamp;
  unsigned int prev_timestamp;
  int end_reached;
} FBSTREAM;

extern int open_fbstream(FBSTREAM *fbs, FILE *fp);
extern void close_fbstream(FBSTREAM *fbs);

extern size_t get_block_size(FBSTREAM *fbs);
extern size_t get_block_offset(FBSTREAM *fbs);
extern size_t get_file_offset(FBSTREAM *fbs);

extern unsigned long get_last_byte_timestamp(FBSTREAM *fbs);
extern unsigned long get_next_byte_timestamp(FBSTREAM *fbs);

#endif /* defined(_FBSUTIL_IO_H) */

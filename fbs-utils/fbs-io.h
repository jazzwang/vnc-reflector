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

extern int fbs_init(FBSTREAM *fbs, FILE *fp);
extern void fbs_cleanup(FBSTREAM *fbs);

extern int fbs_getc(FBSTREAM *fbs);
extern int fbs_read(FBSTREAM *fbs, char *buf, size_t len);

extern size_t fbs_get_block_size(FBSTREAM *fbs);
extern size_t fbs_get_block_offset(FBSTREAM *fbs);
extern size_t fbs_get_file_offset(FBSTREAM *fbs);

extern unsigned long fbs_get_last_byte_timestamp(FBSTREAM *fbs);
extern unsigned long fbs_get_next_byte_timestamp(FBSTREAM *fbs);

extern int fbs_end_reached(FBSTREAM *fbs);

#endif /* defined(_FBSUTIL_IO_H) */

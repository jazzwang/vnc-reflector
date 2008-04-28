/*
 * FrameBuffer Stream Utilities.
 * Copyright (C) 2008 Wimba, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file
 * LICENSE, included.
 */

/*
 * The .fbs data stream reading API.
 *
 * These functions construct a programming interface that allows
 * reading data streams with included timing information, known as
 * .fbs (FrameBuffer Stream) files.
 *
 * The file format is compatible with rfbproxy utility by Tim Waugh,
 * and is documented in the rfbproxy source, file rfbproxy.c. The
 * original utility can be found here:
 *
 *   http://cyberelk.net/tim/software/rfbproxy/
 */

#ifndef _FBSUTIL_IO_H
#define _FBSUTIL_IO_H

#include <stdio.h>
#include <sys/types.h>

/*
 * The FBSTREAM data structure is used to maintain the state of a
 * particular .fbs data stream. Once initialized with fbs_init(), then
 * it will be passed as a first argument to functions that work with
 * .fbs streams.
 */
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

/*
 * fbs_init() function initializes the FBSTREAM structure (referenced
 * by fbs), and associates the given file pointer (fp) with the .fbs
 * data stream.
 *
 * The FBSTREAM structure should be allocated by the caller. The
 * function reads the header and the first data block from the
 * file. For each successful call to fbs_init(), there should be a
 * corresponding call to fbs_cleanup() which is .
 *
 * The return value is 1 for success, and 0 for a failure. If the
 * function fails, it prints an error message on stderr.
 */
extern int fbs_init(FBSTREAM *fbs, FILE *fp);

/*
 * fbs_cleanup() deallocates resources that might have been allocated
 * while initializing and/or reading the data stream.
 *
 * The FBSTREAM structure itself is not deallocated, and the
 * associated file is not closed by this function. The caller is
 * responsible for such cleanup.
 */
extern void fbs_cleanup(FBSTREAM *fbs);

/*
 * Read one byte from the data stream referenced by fbs.
 *
 * The return value is either the byte sucessfully read (as an
 * unsigned char converted to an int), or -1 if there was an error or
 * the end of file has been reached.
 */
extern int fbs_getc(FBSTREAM *fbs);

/*
 * Read exactly the specified number of bytes (len) from the data
 * stream (referenced by fbs) to the specified buffer (pointed to by
 * buf).
 *
 * The return value is 1 if all the bytes have been copied into the
 * buffer, and 0 if there was a failure or end of stream has been
 * reached. If the return value is 0, the buffer contents should be
 * considered undefined, as some unknown amount of data may have been
 * written to it.
 */
extern int fbs_read(FBSTREAM *fbs, char *buf, size_t len);

/*
 * fbs_get_block_size() returns the size of current data block. Data
 * block is the sequence of bytes with the same timestamp. Current
 * data block is one that includes the byte that would be returned by
 * the next fbs_getc() call.
 */
extern size_t fbs_get_block_size(FBSTREAM *fbs);

/*
 * fbs_get_block_offset() returns the offset of current byte in
 * current data block. Current byte is one that would be returned by
 * the next fbs_getc() call.
 */
extern size_t fbs_get_block_offset(FBSTREAM *fbs);

/*
 * fbs_get_file_offset() returns global offset of current byte in the
 * .fbs data stream referenced by the fbs pointer. Note that this is
 * not just an offset in the file, it's an offset in the data stream
 * saved in that .fbs file. The data stream does not include file
 * header, bytes counters, timestamps and padding bytes.
 */
extern size_t fbs_get_file_offset(FBSTREAM *fbs);

/*
 * fbs_get_last_byte_timestamp() returns the timestamp of the most
 * recent byte read from the data stream. The timestamp is the number
 * of milliseconds since the beginning of data capture.
 */
extern unsigned long fbs_get_last_byte_timestamp(FBSTREAM *fbs);

/*
 * fbs_get_next_byte_timestamp() returns the timestamp of current byte
 * that would be returned by the next fbs_getc() call. The timestamp
 * is the number of milliseconds since the beginning of data capture.
 */
extern unsigned long fbs_get_next_byte_timestamp(FBSTREAM *fbs);

extern int fbs_end_reached(FBSTREAM *fbs);

#endif /* defined(_FBSUTIL_IO_H) */
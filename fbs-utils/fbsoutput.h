/*
 * FrameBuffer Stream Utilities.
 * Copyright (C) 2008 Wimba, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file
 * LICENSE, included.
 */

/*
 * The .fbs data stream writing API.
 *
 * These functions construct a programming interface that allows
 * writing data streams with integrated timing information, known as
 * .fbs (FrameBuffer Stream) files.
 *
 * The file format is compatible with rfbproxy utility by Tim Waugh,
 * and is documented in the rfbproxy source, file rfbproxy.c. The
 * original utility can be found here:
 *
 *   http://cyberelk.net/tim/software/rfbproxy/
 */

#ifndef _FBSOUTPUT_H
#define _FBSOUTPUT_H

#include <stdio.h>
#include <sys/types.h>

/*
 * The FBSOUT data structure is used to maintain the state of a
 * particular .fbs output stream. It should be initialized by calling
 * fbsout_init().
 */
typedef struct _FBSOUT {
  FILE *fp;
  char *block_data;
  size_t block_size;
  size_t offset_in_block;
  unsigned int timestamp;
  int error;
} FBSOUT;

/*
 * fbsout_init() function initializes the FBSOUT structure (referenced
 * by fbs), and associates the given file pointer (fp) with the .fbs
 * output stream.
 *
 * The FBSOUT structure should be allocated by the caller. The
 * contents will be overwritten by fbsout_init(). For each successful
 * call to fbsout_init(), there must be a corresponding call to
 * fbsout_cleanup() which should be called when the structure is not
 * needed any more.
 *
 * The return value is 1 for success, and 0 for a failure. If the
 * function fails, it prints an error message on stderr.
 */
extern int fbsout_init(FBSOUT *fbs, FILE *fp);

/*
 * fbsout_cleanup() deallocates resources that might have been
 * allocated while initializing and/or writing to the output stream.
 *
 * The FBSOUT structure itself is not deallocated, and the associated
 * file is not closed by this function. The caller is responsible for
 * such cleanup.
 */
extern void fbsout_cleanup(FBSOUT *fbs);

extern int fbs_putc(FBSOUT *fbs, int c);
extern int fbs_write(FBSOUT *fbs, char *buf, size_t len);
extern int fbs_write_U8(FBSOUT *fbs, CARD8 value);
extern int fbs_write_U16(FBSOUT *fbs, CARD16 value);
extern int fbs_write_U32(FBSOUT *fbs, CARD32 value);
extern int fbs_write_S8(FBSOUT *fbs, INT8 value);
extern int fbs_write_S16(FBSOUT *fbs, INT16 value);
extern int fbs_write_S32(FBSOUT *fbs, INT32 value);
extern int fbs_write_tight_len(FBSOUT *fbs, size_t value);

extern int fbsout_set_timestamp(FBSOUT *fbs, unsigned int timestamp,
                                int can_flush);
extern int fbsout_flush(FBSOUT *fbs);

extern CARD32 fbsout_get_filepos(FBSOUT *fbs);

#endif /* defined(_FBSOUTPUT_H) */

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
 * reading data streams with integrated timing information, known as
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
 * particular .fbs data stream. It should be initialized by calling
 * fbs_init().
 */
typedef struct _FBSTREAM {
  FILE *fp;
  char *block_data;
  unsigned int block_idx;
  size_t block_fpos;
  size_t next_block_fpos;
  size_t block_size;
  size_t offset_in_block;
  size_t num_bytes_read;
  unsigned int timestamp;
  int eof;
  int error;
} FBSTREAM;

/*
 * fbs_init() function initializes the FBSTREAM structure (referenced
 * by fbs), and associates the given file pointer (fp) with the .fbs
 * data stream.
 *
 * The FBSTREAM structure should be allocated by the caller. The
 * contents will be overwritten by fbs_init(). For each successful
 * call to fbs_init(), there must be a corresponding call to
 * fbs_cleanup() which should be called when the structure is not
 * needed any more.
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
 *
 * On error, fbs_getc() will print error message on stderr.
 */
extern int fbs_getc(FBSTREAM *fbs);

/*
 * Read exactly the specified number of bytes (len) from the data
 * stream (referenced by fbs) to the specified buffer (pointed to by
 * buf).
 *
 * The return value is 1 if all the bytes have been read into the
 * buffer, and 0 if there was an error or end of stream has been
 * reached. If the return value is 0, the buffer contents should be
 * considered undefined, as some unknown amount of data may have been
 * written to it.
 *
 * On error, fbs_read() will print error message on stderr.
 */
extern int fbs_read(FBSTREAM *fbs, char *buf, size_t len);

/*
 * Read and discard the specified number of bytes (len) from the data
 * stream (referenced by fbs).
 *
 * The return value is 1 on success, and 0 if there was an error or
 * end of stream has been reached. Note that fbs_skip() not just
 * positions the file pointer, but may actually read some data. That's
 * because .fbs files are block-based, and all previous block headers
 * should be read to locate the beginning of the next block.
 *
 * On error, fbs_skip() will print error message on stderr.
 */
extern int fbs_skip(FBSTREAM *fbs, size_t len);

/*
 * These functions read and return different types of integer values
 * from the .fbs data stream referenced by fbs. The functions read 8-,
 * 16- and 32-bit values, interpreted as unsigned and signed integers.
 * Multi-byte values are expected to be in the big-endian byte order.
 *
 * The return value does not provide an indication of error or eof
 * condition. The caller should use fbs_error() and fbs_eof() to
 * detect errors or end of file. However, on error, the functions will
 * print error messages on stderr.
 */
extern CARD8 fbs_read_U8(FBSTREAM *fbs);
extern CARD16 fbs_read_U16(FBSTREAM *fbs);
extern CARD32 fbs_read_U32(FBSTREAM *fbs);
extern INT8 fbs_read_S8(FBSTREAM *fbs);
extern INT16 fbs_read_S16(FBSTREAM *fbs);
extern INT32 fbs_read_S32(FBSTREAM *fbs);

/*
 * fbs_read_tight_len() reads an integer in compact representation
 * (1..3 bytes). Such format is used as a part of the Tight encoding.
 *
 * The return value is the decoded integer. Its type is size_t because
 * normally it should designate data length. The return value does not
 * provide an indication of error or eof condition. The caller should
 * use fbs_error() and fbs_eof() to detect errors or end of file.
 * However, on error, error message will be printed on stderr.
 */
extern size_t fbs_read_tight_len(FBSTREAM *fbs);

/*
 * fbs_get_pos() provides various information related to current
 * position in the .fbs file. If current position is on the block
 * boundary (that is, between bytes with different timestamps), then
 * fbs_get_pos() advances to the next data block.
 *
 * The arguments are pointers to variables where the results will be
 * stored on successful execution. NULL value may be passed instead of
 * any pointer, and in that case the corresponding value will not be
 * stored. The arguments designate the following attributes:
 *
 *   block_idx -  the number of data blocks before current one in the
 *                .fbs file.
 *
 *   block_fpos - position of the beginning of current data block in
 *                the .fbs file. This is an offset from the file
 *                beginning, so that it may be used with lseek(2) and
 *                fseek(3). The byte at this offset is the first byte
 *                of RFB data within the data block.
 *
 *   block_size - the data size in current data block, not counting
 *                padding or any attributes such as byte counter or
 *                timestamp.
 *
 *   offset_in_block - offset of current byte in current data block,
 *                (byte counter is not counted as a part of the data
 *                block). Current byte is one that would be returned
 *                by the next fbs_getc() call.
 *
 *   timestamp -  timestamp corresponding to current byte in the .fbs
 *                data stream. Current byte is one that would be
 *                returned by the next fbs_getc() call. Timestamp is
 *                the number of milliseconds since the beginning of
 *                data recording.
 *
 * Return value is 1 on success, or 0 if there was an error or end of
 * file was reached. Variables referenced by the arguments are not
 * changed if the return value is 0.
 *
 * On error, fbs_get_pos() will print error message on stderr.
 */
extern int fbs_get_pos(FBSTREAM *fbs,
                       unsigned int *block_idx,
                       size_t *block_fpos,
                       size_t *block_size,
                       size_t *offset_in_block,
                       unsigned int *timestamp);

/*
 * fbs_num_bytes_read() returns the total number of bytes read from
 * the .fbs data stream referenced by the fbs pointer. Note that the
 * data stream does not include file header (signature), byte
 * counters, timestamps and padding bytes.
 */
extern size_t fbs_num_bytes_read(FBSTREAM *fbs);

/*
 * fbs_eof() returns 1 if there was an attempt to read beyond the end
 * of file. Otherwise, it returns 0 (even if there are no more data to
 * read).
 */
extern int fbs_eof(FBSTREAM *fbs);

/*
 * fbs_error() returns 1 if there was an error, 0 otherwise. End of
 * file condition is not considered an error.
 */
extern int fbs_error(FBSTREAM *fbs);

#endif /* defined(_FBSUTIL_IO_H) */

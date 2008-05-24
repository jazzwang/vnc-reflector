/*
 * Tight Encoding (efficient encoding for true-color pixel data)
 *
 * Copyright (C) 2000-2008 Constantin Kaplinsky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tight-decoder.h - Decoding Tight-encoded rectangles.
 */

#ifndef _TIGHT_DECODER_H_INCLUDED_
#define _TIGHT_DECODER_H_INCLUDED_

#include <sys/types.h>
#include <zlib.h>

struct _TIGHT_DECODER;

typedef int (*TIGHT_DECODE_FUNC)(struct _TIGHT_DECODER *td,
                                 unsigned char *buf);

/*
 * The TIGHT_DECODER data structure is used to maintain the state of
 * the decoder. It should be initialized by calling tight_decode_init().
 */
typedef struct _TIGHT_DECODER {
  TIGHT_DECODE_FUNC func;
  z_stream zstream[4];
  int zstream_active[4];
  int zlib_reset_mask;
  int zlib_stream_id;
  u_int32_t *fb;
  int fb_width;
  int fb_height;
  int fb_stride;
  int rect_x;
  int rect_y;
  int rect_w;
  int rect_h;
  int filter_id;
  int num_colors;
  u_int32_t palette[256];
  int compressed_size;
  int uncompressed_size;
  int num_bytes;
  char error_msg[256];
} TIGHT_DECODER;

/************************ Decoder Functions *************************/

extern int tight_decode_init(TIGHT_DECODER *td);
extern int tight_decode_set_framebuffer(TIGHT_DECODER *td, u_int32_t *fb,
                                        int width, int height, int stride);
extern void tight_decode_cleanup(TIGHT_DECODER *td);

/*
 * Both tight_decode_start() and tight_decode_continue() functions
 * return -1 on error, 0 if the decoding is complete, or a number of
 * bytes to read before the next call to tight_decode_continue().
 */
extern int tight_decode_start(TIGHT_DECODER *td, int x, int y, int w, int h);
extern int tight_decode_continue(TIGHT_DECODER *td, char *buf);

extern char *tight_decode_get_error(TIGHT_DECODER *td);


/********************* Informational Functions **********************/

extern int tdstat_get_zlib_reset_mask(TIGHT_DECODER *td);
extern int tdstat_get_zlib_stream_id(TIGHT_DECODER *td);
extern int tdstat_get_num_colors(TIGHT_DECODER *td);
extern int tdstat_get_num_raw_bytes(TIGHT_DECODER *td);
extern int tdstat_get_num_encoded_bytes(TIGHT_DECODER *td);
extern int tdstat_get_num_compressed_bytes(TIGHT_DECODER *td);

#endif /* _TIGHT_DECODER_H_INCLUDED_ */

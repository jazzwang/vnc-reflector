/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: encode.c,v 1.1 2001/08/04 17:29:34 const Exp $
 * Encoding screen rectangles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "client_io.h"
#include "encode.h"

static void rfb_encode_raw(void *outbuf, RFB_PIXEL_FORMAT *fmt,
                           int x, int y, int w, int h);

AIO_BLOCK *rfb_encode_raw_block(RFB_PIXEL_FORMAT *fmt,
                                int x, int y, int w, int h)
{
  AIO_BLOCK *block;

  block = malloc(sizeof(AIO_BLOCK) + w * h * (fmt->bits_pixel / 8));
  if (block) {
    rfb_encode_raw(&block->data, fmt, x, y, w, h);
    block->data_size = w * h * (fmt->bits_pixel / 8);
  }

  return block;
}

static void rfb_encode_raw(void *outbuf, RFB_PIXEL_FORMAT *fmt,
                           int x, int y, int w, int h)
{
  CARD32 *fb_ptr;
  CARD8 *outbuf8 = (CARD8 *)outbuf;
  CARD16 *outbuf16 = (CARD16 *)outbuf;
  CARD32 *outbuf32 = (CARD32 *)outbuf;
  int skip, cx, cy;

  fb_ptr = &g_framebuffer[x * g_screen_info->width + y];
  skip =  (g_screen_info->width - w) * sizeof(CARD32);

  switch (fmt->bits_pixel) {
  case 8:
    for (cy = 0; cy < h; cy++) {
      for (cx = 0; cx < w; cx++) {
        *outbuf8++ = TRANSLATE_PIXEL(*fb_ptr, fmt);
        fb_ptr++;
      }
      fb_ptr += skip;
    }
    break;
  case 16:
    for (cy = 0; cy < h; cy++) {
      for (cx = 0; cx < w; cx++) {
        *outbuf16++ = TRANSLATE_PIXEL(*fb_ptr, fmt);
        fb_ptr++;
      }
      fb_ptr += skip;
    }
    break;
  case 32:
    for (cy = 0; cy < h; cy++) {
      for (cx = 0; cx < w; cx++) {
        *outbuf32++ = TRANSLATE_PIXEL(*fb_ptr, fmt);
        fb_ptr++;
      }
      fb_ptr += skip;
    }
    break;
  }
}


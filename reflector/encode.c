/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: encode.c,v 1.2 2001/08/04 21:58:57 const Exp $
 * Encoding screen rectangles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "reflector.h"
#include "async_io.h"
#include "rect.h"
#include "client_io.h"
#include "encode.h"

static void rfb_encode_raw(void *outbuf, RFB_PIXEL_FORMAT *fmt, FB_RECT *r);

AIO_BLOCK *rfb_encode_raw_block(RFB_PIXEL_FORMAT *fmt, FB_RECT *r)
{
  AIO_BLOCK *block;

  block = malloc(sizeof(AIO_BLOCK) + r->w * r->h * (fmt->bits_pixel / 8));
  if (block) {
    rfb_encode_raw(&block->data, fmt, r);
    block->data_size = r->w * r->h * (fmt->bits_pixel / 8);
  }

  return block;
}

static void rfb_encode_raw(void *outbuf, RFB_PIXEL_FORMAT *fmt, FB_RECT *r)
{
  CARD32 *fb_ptr;
  CARD8 *outbuf8 = (CARD8 *)outbuf;
  CARD16 *outbuf16 = (CARD16 *)outbuf;
  CARD32 *outbuf32 = (CARD32 *)outbuf;
  int skip, x, y;

  fb_ptr = &g_framebuffer[r->y * g_screen_info->width + r->x];
  skip = g_screen_info->width - r->w;

  switch (fmt->bits_pixel) {
  case 8:
    for (y = 0; y < r->h; y++) {
      for (x = 0; x < r->w; x++) {
        *outbuf8++ = TRANSLATE_PIXEL(*fb_ptr, fmt);
        fb_ptr++;
      }
      fb_ptr += skip;
    }
    break;
  case 16:
    for (y = 0; y < r->h; y++) {
      for (x = 0; x < r->w; x++) {
        *outbuf16++ = TRANSLATE_PIXEL(*fb_ptr, fmt);
        fb_ptr++;
      }
      fb_ptr += skip;
    }
    break;
  case 32:
    for (y = 0; y < r->h; y++) {
      for (x = 0; x < r->w; x++) {
        *outbuf32++ = TRANSLATE_PIXEL(*fb_ptr, fmt);
        fb_ptr++;
      }
      fb_ptr += skip;
    }
    break;
  }
}


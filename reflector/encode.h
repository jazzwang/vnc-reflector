/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: encode.h,v 1.2 2001/08/04 21:58:57 const Exp $
 * Encoding screen rectangles.
 */

#ifndef _REFLIB_ENCODE_H
#define _REFLIB_ENCODE_H

#define TRANSLATE_PIXEL(pixel, fmt)                                          \
  (((((pixel) >> 16 & 0xFF) * (fmt)->r_max + 127) / 255) << (fmt)->r_shift | \
   ((((pixel) >> 8 & 0xFF) * (fmt)->g_max + 127) / 255) << (fmt)->g_shift |  \
   ((((pixel) & 0xFF) * (fmt)->b_max + 127) / 255) << (fmt)->b_shift)

AIO_BLOCK *rfb_encode_raw_block(RFB_PIXEL_FORMAT *fmt, FB_RECT *r);

#endif /* _REFLIB_ENCODE_H */

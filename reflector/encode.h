/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: encode.h,v 1.3 2001/08/11 02:49:04 const Exp $
 * Encoding screen rectangles.
 */

#ifndef _REFLIB_ENCODE_H
#define _REFLIB_ENCODE_H

#define TRANSLATE_PIXEL(pixel, fmt)                                          \
  (((((pixel) >> 16 & 0xFF) * (fmt)->r_max + 127) / 255) << (fmt)->r_shift | \
   ((((pixel) >> 8 & 0xFF) * (fmt)->g_max + 127) / 255) << (fmt)->g_shift |  \
   ((((pixel) & 0xFF) * (fmt)->b_max + 127) / 255) << (fmt)->b_shift)

#define BUF_PUT_PIXEL8(buf, pixel)  *(buf) = (pixel)

#define BUF_PUT_PIXEL16(buf, pixel)             \
{                                               \
  (buf)[0] = ((CARD8 *)&(pixel))[0];            \
  (buf)[1] = ((CARD8 *)&(pixel))[1];            \
}

#define BUF_PUT_PIXEL32(buf, pixel)             \
{                                               \
  (buf)[0] = ((CARD8 *)&(pixel))[0];            \
  (buf)[1] = ((CARD8 *)&(pixel))[1];            \
  (buf)[2] = ((CARD8 *)&(pixel))[2];            \
  (buf)[3] = ((CARD8 *)&(pixel))[3];            \
}

AIO_BLOCK *rfb_encode_raw_block(RFB_PIXEL_FORMAT *fmt, FB_RECT *r);
AIO_BLOCK *rfb_encode_hextile_block(RFB_PIXEL_FORMAT *fmt, FB_RECT *r);

#endif /* _REFLIB_ENCODE_H */

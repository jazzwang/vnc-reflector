/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: encode.h,v 1.4 2001/08/19 13:56:42 const Exp $
 * Encoding screen rectangles.
 */

#ifndef _REFLIB_ENCODE_H
#define _REFLIB_ENCODE_H

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

AIO_BLOCK *rfb_encode_raw_block(CL_SLOT *cl, FB_RECT *r);
AIO_BLOCK *rfb_encode_hextile_block(CL_SLOT *cl, FB_RECT *r);

#endif /* _REFLIB_ENCODE_H */

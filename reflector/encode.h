/* VNC Reflector
 * Copyright (C) 2001 HorizonLive.com, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file LICENSE,
 * included.  HorizonLive provides e-Learning and collaborative synchronous
 * presentation solutions in a totally Web-based environment.  For more
 * information about HorizonLive, please see our website at
 * http://www.horizonlive.com.
 *
 * This software was authored by Constantin Kaplinsky <const@ce.cctpu.edu.ru>
 * and sponsored by HorizonLive.com, Inc.
 *
 * $Id: encode.h,v 1.7 2001/10/05 10:36:19 const Exp $
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

int put_rect_header(CARD8 *buf, FB_RECT *r, CARD32 enc);

AIO_BLOCK *rfb_encode_raw_block(CL_SLOT *cl, FB_RECT *r);
AIO_BLOCK *rfb_encode_copyrect_block(CL_SLOT *cl, FB_RECT *r);
AIO_BLOCK *rfb_encode_hextile_block(CL_SLOT *cl, FB_RECT *r);

void get_hextile_caching_stats(long *hits, long *misses);

int rfb_encode_tight8(CL_SLOT *cl, FB_RECT *r);

#endif /* _REFLIB_ENCODE_H */

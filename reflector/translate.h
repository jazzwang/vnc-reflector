/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: translate.h,v 1.1 2001/08/18 10:44:27 const Exp $
 * Pixel format translation.
 */

#ifndef _REFLIB_TRANSLATE_H
#define _REFLIB_TRANSLATE_H

void *gen_trans_table(RFB_PIXEL_FORMAT *fmt);

void transfunc_null(CARD32 *dst_buf, RECT *r);

#endif /* _REFLIB_TRANSLATE_H */

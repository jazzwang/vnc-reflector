/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: translate.h,v 1.3 2001/08/26 15:09:53 const Exp $
 * Pixel format translation.
 */

#ifndef _REFLIB_TRANSLATE_H
#define _REFLIB_TRANSLATE_H

/* Not used at this moment */
/*
#define TRANSLATE_PIXEL(pixel, table, bpp)              \
  ((CARD##bpp)(tbl_ptr[pixel >> 16 & 0xFF] |            \
               tbl_ptr[256 + (pixel >> 8 & 0xFF)] |     \
               tbl_ptr[512 + (pixel & 0xFF)]))
*/

typedef void (*TRANSFUNC_PTR)(void *dst_buf, FB_RECT *r, void *table);

void *gen_trans_table(RFB_PIXEL_FORMAT *fmt);

void transfunc_null(void *dst_buf, FB_RECT *r, void *table);
void transfunc8(void *dst_buf, FB_RECT *r, void *table);
void transfunc16(void *dst_buf, FB_RECT *r, void *table);
void transfunc32(void *dst_buf, FB_RECT *r, void *table);

#endif /* _REFLIB_TRANSLATE_H */

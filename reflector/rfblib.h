/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: rfblib.h,v 1.5 2001/08/03 13:06:59 const Exp $
 * RFB protocol definitions
 */

#ifndef _REFLIB_RFBLIB_H
#define _REFLIB_RFBLIB_H

/*
 * Define data types used in the RFB protocol.
 */

#ifndef CARD32

#define INT8    int8_t
#define CARD8   u_int8_t
#define INT16   int16_t
#define CARD16  u_int16_t
#define INT32   int32_t
#define CARD32  u_int32_t

#endif /* CARD32 */

/* FIXME: Non-consistent names. */

/*
 * RFB_PIXEL_FORMAT structure correspond to PIXEL_FORMAT as defined in
 * the RFB protocol specification. It describes pixel format used in
 * framebuffer updates sent by server to client.
 */

typedef struct _RFB_PIXEL_FORMAT {
  CARD8 bits_pixel;
  CARD8 color_depth;
  CARD8 big_endian;
  CARD8 true_color;
  CARD16 r_max;
  CARD16 g_max;
  CARD16 b_max;
  CARD8 r_shift;
  CARD8 g_shift;
  CARD8 b_shift;
  CARD8 unused[3];
} RFB_PIXEL_FORMAT;

/* Size of the pixel format structure as defined in the RFB protocol */
#define SZ_RFB_PIXEL_FORMAT  16

/*
 * RFB_SCREEN_INFO structure describes dimensions and format of the
 * screen (framebuffer).
 */

/* FIXME: Keep name separately. Do not keep format similar to
   ServerInitialisation message. */

typedef struct _RFB_SCREEN_INFO {
  CARD16 width;
  CARD16 height;
  RFB_PIXEL_FORMAT pixformat;
  CARD32 name_length;
  CARD8 name[1];
} RFB_SCREEN_INFO;

/*
 * Functions to compose/decompose bigger values from/into byte arrays.
 */

CARD16 buf_get_CARD16(void *buf);
CARD32 buf_get_CARD32(void *buf);
void buf_put_CARD16(void *buf, CARD16 value);
void buf_put_CARD32(void *buf, CARD32 value);

void buf_get_pixfmt(void *buf, RFB_PIXEL_FORMAT *format);
void buf_put_pixfmt(void *buf, RFB_PIXEL_FORMAT *format);

#endif /* _REFLIB_RFBLIB_H */

/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: rfblib.c,v 1.2 2001/08/03 13:06:59 const Exp $
 * RFB protocol helper functions
 */

#include <string.h>
#include <sys/types.h>

#include "rfblib.h"

CARD16 buf_get_CARD16(void *buf)
{
  unsigned char *bbuf = buf;

  return ((CARD16)bbuf[0] << 8 |
          (CARD16)bbuf[1]);
}

CARD32 buf_get_CARD32(void *buf)
{
  unsigned char *bbuf = buf;

  return ((CARD32)bbuf[0] << 24 |
          (CARD32)bbuf[1] << 16 |
          (CARD32)bbuf[2] << 8  |
          (CARD32)bbuf[3]);
}

void buf_put_CARD16(void *buf, CARD16 value)
{
  unsigned char *bbuf = buf;

  bbuf[0] = (unsigned char)(value >> 8);
  bbuf[1] = (unsigned char)value;
}

void buf_put_CARD32(void *buf, CARD32 value)
{
  unsigned char *bbuf = buf;

  bbuf[0] = (unsigned char)(value >> 24);
  bbuf[1] = (unsigned char)(value >> 16);
  bbuf[2] = (unsigned char)(value >> 8);
  bbuf[3] = (unsigned char)value;
}

void buf_get_pixfmt(void *buf, RFB_PIXEL_FORMAT *format)
{
  unsigned char *bbuf = buf;

  memcpy(format, buf, SZ_RFB_PIXEL_FORMAT);
  format->r_max = buf_get_CARD16(&bbuf[4]);
  format->g_max = buf_get_CARD16(&bbuf[6]);
  format->b_max = buf_get_CARD16(&bbuf[8]);
}

void buf_put_pixfmt(void *buf, RFB_PIXEL_FORMAT *format)
{
  unsigned char *bbuf = buf;

  memcpy(buf, format, SZ_RFB_PIXEL_FORMAT);
  buf_put_CARD16(&bbuf[4], format->r_max);
  buf_put_CARD16(&bbuf[6], format->g_max);
  buf_put_CARD16(&bbuf[8], format->b_max);
}


/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: rfblib.c,v 1.3 2001/08/11 02:47:47 const Exp $
 * RFB protocol helper functions
 */

#include <string.h>
#include <sys/types.h>

#include "rfblib.h"

/*
 * The following functions are not used, replaced by macros
 *

CARD16 buf_get_CARD16(void *buf)
{
  return ((CARD16)((CARD8 *)buf)[0] << 8 |
          (CARD16)((CARD8 *)buf)[1]);
}

CARD32 buf_get_CARD32(void *buf)
{
  CARD8 *bbuf = buf;

  return ((CARD32)bbuf[0] << 24 |
          (CARD32)bbuf[1] << 16 |
          (CARD32)bbuf[2] << 8  |
          (CARD32)bbuf[3]);
}

void buf_put_CARD16(void *buf, CARD16 value)
{
  CARD8 *bbuf = buf;

  bbuf[0] = (CARD8)(value >> 8);
  bbuf[1] = (CARD8)value;
}

void buf_put_CARD32(void *buf, CARD32 value)
{
  CARD8 *bbuf = buf;

  bbuf[0] = (CARD8)(value >> 24);
  bbuf[1] = (CARD8)(value >> 16);
  bbuf[2] = (CARD8)(value >> 8);
  bbuf[3] = (CARD8)value;
}

 *
 * End of comments
 */

void buf_get_pixfmt(void *buf, RFB_PIXEL_FORMAT *format)
{
  CARD8 *bbuf = buf;

  memcpy(format, buf, SZ_RFB_PIXEL_FORMAT);
  format->r_max = buf_get_CARD16(&bbuf[4]);
  format->g_max = buf_get_CARD16(&bbuf[6]);
  format->b_max = buf_get_CARD16(&bbuf[8]);
}

void buf_put_pixfmt(void *buf, RFB_PIXEL_FORMAT *format)
{
  CARD8 *bbuf = buf;

  memcpy(buf, format, SZ_RFB_PIXEL_FORMAT);
  buf_put_CARD16(&bbuf[4], format->r_max);
  buf_put_CARD16(&bbuf[6], format->g_max);
  buf_put_CARD16(&bbuf[8], format->b_max);
}


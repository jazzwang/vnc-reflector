/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: rfblib.c,v 1.1 2001/08/01 16:06:07 const Exp $
 * RFB protocol helper functions
 */

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


/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: rfblib.h,v 1.4 2001/08/02 11:53:00 const Exp $
 * RFB protocol definitions
 */

#ifndef _REFLIB_RFBLIB_H
#define _REFLIB_RFBLIB_H

#ifndef CARD32

#define INT8    int8_t
#define CARD8   u_int8_t
#define INT16   int16_t
#define CARD16  u_int16_t
#define INT32   int32_t
#define CARD32  u_int32_t

#endif /* CARD32 */

typedef struct _RFB_DESKTOP_INFO {
  int width;
  int height;
  char *name;
} RFB_DESKTOP_INFO;

CARD16 buf_get_CARD16(void *buf);
CARD32 buf_get_CARD32(void *buf);
void buf_put_CARD16(void *buf, CARD16 value);
void buf_put_CARD32(void *buf, CARD32 value);

#endif /* _REFLIB_RFBLIB_H */

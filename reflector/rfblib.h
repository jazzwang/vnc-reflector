/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: rfblib.h,v 1.2 2001/08/01 11:29:36 const Exp $
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
  int bytes_row;
  char *name;
} RFB_DESKTOP_INFO;

#endif /* _REFLIB_RFBLIB_H */

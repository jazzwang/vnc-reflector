/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.h,v 1.4 2001/08/03 13:06:59 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#ifndef _REFLIB_CLIENT_IO_H
#define _REFLIB_CLIENT_IO_H

#define NUM_ENCODINGS 10

/* Extension to AIO_SLOT structure to hold client state */
typedef struct _CL_SLOT {
  AIO_SLOT s;

  RFB_PIXEL_FORMAT format;
  CARD16 enc_count;
  unsigned char enc_enable[NUM_ENCODINGS];
  unsigned char msg_buf[20];
} CL_SLOT;

void set_client_password(unsigned char *password);
void af_client_accept(void);

#endif /* _REFLIB_CLIENT_IO_H */

/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.h,v 1.5 2001/08/04 17:29:34 const Exp $
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
  unsigned int update_requested   :1;
  unsigned int update_in_progress :1;
  unsigned int update_full        :1;
} CL_SLOT;

void set_client_password(unsigned char *password);
void af_client_accept(void);

#endif /* _REFLIB_CLIENT_IO_H */

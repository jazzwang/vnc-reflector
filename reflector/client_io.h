/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.h,v 1.9 2001/08/08 11:17:21 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#ifndef _REFLIB_CLIENT_IO_H
#define _REFLIB_CLIENT_IO_H

#define TYPE_CL_SLOT    1

#define NUM_ENCODINGS  10

/* Extension to AIO_SLOT structure to hold client state */
typedef struct _CL_SLOT {
  AIO_SLOT s;

  RFB_PIXEL_FORMAT format;
  FB_RECT_LIST pending_rects;
  CARD16 temp_count;
  unsigned char enc_enable[NUM_ENCODINGS];
  unsigned char msg_buf[20];
  unsigned int connected          :1;
  unsigned int update_requested   :1;
  unsigned int update_in_progress :1;
} CL_SLOT;

void set_client_password(unsigned char *password);
void af_client_accept(void);

/* Functions called from host_io.c */
void fn_client_add_rect(AIO_SLOT *slot, FB_RECT *rect);
void fn_client_send_rects(AIO_SLOT *slot);
void fn_client_send_cuttext(AIO_SLOT *slot, CARD8 *text, CARD32 len);

#endif /* _REFLIB_CLIENT_IO_H */

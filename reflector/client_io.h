/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: client_io.h,v 1.2 2001/08/02 16:47:45 const Exp $
 * Asynchronous interaction with VNC clients.
 */

#ifndef _REFLIB_CLIENT_IO_H
#define _REFLIB_CLIENT_IO_H

void set_client_password(unsigned char *password);
void af_client_accept(void);

#endif /* _REFLIB_CLIENT_IO_H */

/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_io.h,v 1.3 2001/08/08 12:34:01 const Exp $
 * Asynchronous interaction with VNC host.
 */

#ifndef _REFLIB_HOST_IO_H
#define _REFLIB_HOST_IO_H

void init_host_io(int fd);

void pass_msg_to_host(CARD8 *msg, size_t len);
void pass_cuttext_to_host(CARD8 *text, size_t len);

#endif /* _REFLIB_HOST_IO_H */

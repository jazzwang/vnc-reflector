/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_io.h,v 1.6 2001/08/23 21:19:44 const Exp $
 * Asynchronous interaction with VNC host.
 */

#ifndef _REFLIB_HOST_IO_H
#define _REFLIB_HOST_IO_H

#define TYPE_HOST_SLOT  2

void host_set_fbs_prefix(char *fbs_prefix);
void host_activate(void);
void host_close_hook(void);

void pass_msg_to_host(CARD8 *msg, size_t len);
void pass_cuttext_to_host(CARD8 *text, size_t len);

#endif /* _REFLIB_HOST_IO_H */

/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_connect.h,v 1.5 2001/08/23 15:24:51 const Exp $
 * Connecting to a VNC host
 */

#ifndef _REFLIB_HOSTCONNECT_H
#define _REFLIB_HOSTCONNECT_H

int connect_to_host(char *host_info_file, int cl_listen_port);
void host_request_reconnect(void);
void host_maybe_reconnect(void);

#endif /* _REFLIB_HOSTCONNECT_H */

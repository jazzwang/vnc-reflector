/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_connect.h,v 1.4 2001/08/22 22:35:40 const Exp $
 * Connecting to a VNC host
 */

#ifndef _REFLIB_HOSTCONNECT_H
#define _REFLIB_HOSTCONNECT_H

int connect_to_host(char *host, int port, int cl_listen_port,
                    unsigned char *password);

#endif /* _REFLIB_HOSTCONNECT_H */

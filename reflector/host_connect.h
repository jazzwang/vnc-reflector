/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_connect.h,v 1.6 2001/08/28 17:28:47 const Exp $
 * Connecting to a VNC host
 */

#ifndef _REFLIB_HOSTCONNECT_H
#define _REFLIB_HOSTCONNECT_H

int connect_to_host(char *host_info_file, int cl_listen_port);

#endif /* _REFLIB_HOSTCONNECT_H */

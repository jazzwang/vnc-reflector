/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: host_connect.h,v 1.2 2001/08/03 13:06:59 const Exp $
 * Connecting to a VNC host
 */

#ifndef _REFLIB_HOSTCONNECT_H
#define _REFLIB_HOSTCONNECT_H

int connect_to_host(char *host, int port);
int setup_session(int fd, char *password, RFB_SCREEN_INFO **scr);

#endif /* _REFLIB_HOSTCONNECT_H */

/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: dispatch.h,v 1.1 2001/08/01 11:49:39 const Exp $
 * Watching I/O events, dispatching control flow.
 */

#ifndef _REFLIB_DISPATCH_H
#define _REFLIB_DISPATCH_H

void mainloop(int host_fd, int listen_port);

#endif /* _REFLIB_DISPATCH_H */

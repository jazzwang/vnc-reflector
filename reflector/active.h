/* VNC Reflector
 * Copyright (C) 2001,2002 HorizonLive.com, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file LICENSE,
 * included.  HorizonLive provides e-Learning and collaborative synchronous
 * presentation solutions in a totally Web-based environment.  For more
 * information about HorizonLive, please see our website at
 * http://www.horizonlive.com.
 *
 * This software was authored by Constantin Kaplinsky <const@ce.cctpu.edu.ru>
 * and sponsored by HorizonLive.com, Inc.
 *
 * $Id: active.h,v 1.1 2002/07/10 15:46:38 const Exp $
 * Active file marker functionality
 */

#ifndef _REFLIB_ACTIVE_H
#define _REFLIB_ACTIVE_H

/* Functions */
int write_active_file(void);
int remove_active_file(void);
int set_active_file(char *file_path);

#endif /* _REFLIB_ACTIVE_H */

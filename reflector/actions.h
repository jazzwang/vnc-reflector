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
 * $Id: actions.h,v 1.1 2002/07/25 16:59:48 const Exp $
 * Performing actions on events
 */

#ifndef _REFLIB_ACTIONS_H
#define _REFLIB_ACTIONS_H

/* Functions */
int set_actions_file(char *file_path);
int perform_action(char *action_str);

#endif /* _REFLIB_ACTIONS_H */

/* VNC Reflector
 * Copyright (C) 2001 HorizonLive.com, Inc.  All rights reserved.
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
 * $Id: reflector.h,v 1.25 2002/09/03 10:48:17 const Exp $
 * Global include file
 */

#ifndef _REF_REFLECTOR_H
#define _REF_REFLECTOR_H

#define VERSION  "1.1.9"

/* FIXME: Too many header files with too many dependencies */

/* Framebuffer and related metadata */

extern RFB_SCREEN_INFO g_screen_info;
extern CARD32 *g_framebuffer;
extern CARD16 g_fb_width, g_fb_height;

/* actions.c */

int set_actions_file(char *file_path);
int perform_action(char *action_str);

/* active.c */

int write_active_file(void);
int remove_active_file(void);
int set_active_file(char *file_path);

/* control.c */

void set_control_signals(void);

#endif /* _REF_REFLECTOR_H */

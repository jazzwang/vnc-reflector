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
 * $Id: reflector.h,v 1.22 2002/03/07 22:04:24 const Exp $
 * Global include file
 */

#ifndef _REF_REFLECTOR_H
#define _REF_REFLECTOR_H

#define VERSION  "1.1.7"

/* FIXME: Too many header files with too many dependencies */

/* Framebuffer and related metadata */

extern RFB_SCREEN_INFO g_screen_info;
extern CARD32 *g_framebuffer;
extern CARD16 g_fb_width, g_fb_height;

#endif /* _REF_REFLECTOR_H */

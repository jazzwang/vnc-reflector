/* VNC Reflector
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: reflector.h,v 1.1 2001/08/01 04:58:39 const Exp $
 * Global include file
 */

#ifndef _REF_REFLECTOR_H
#define _REF_REFLECTOR_H

#define VERSION  "0.2"

/*
 * Global configuration options
 * FIXME: Actually they should not be global
 */

extern int   opt_listen_port;
extern char *opt_log_filename;
extern char *opt_passwd_filename;
extern int   opt_foreground;
extern char  opt_hostname[256];
extern int   opt_hostport;
extern char  opt_password[9];

/*
 * Prototypes for global functions
 */

#endif /* _REF_REFLECTOR_H */

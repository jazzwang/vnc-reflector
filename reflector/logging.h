/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: logging.h,v 1.1 2001/08/01 04:58:39 const Exp $
 * Logging functionality
 */

#ifndef _REFLIB_LOGGING_H
#define _REFLIB_LOGGING_H

/* Log levels */
#define LL_INTERR  0
#define LL_ERROR   1
#define LL_WARN    2
#define LL_MSG     3
#define LL_INFO    4
#define LL_DETAIL  5
#define LL_DEBUG   6

/* Functions */
int log_open(char *filename, int file_level, int stderr_level);
int log_reopen(void);
int log_close(void);
void log_write(int level, char *fmt, ...);

#endif /* _REFLIB_LOGGING_H */

/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: rect.h,v 1.2 2001/08/08 12:56:51 const Exp $
 * Operations with rectangle structures.
 */

#ifndef _REFLIB_RECT_H
#define _REFLIB_RECT_H

typedef struct _FB_RECT {
  CARD16 x;
  CARD16 y;
  CARD16 w;
  CARD16 h;
} FB_RECT;

typedef struct _FB_RECT_NODE {
  FB_RECT r;
  struct _FB_RECT_NODE *next;
} FB_RECT_NODE;

typedef struct _FB_RECT_LIST {
  FB_RECT_NODE *first_rect;
  FB_RECT_NODE *last_rect;
  int num_rects;
} FB_RECT_LIST;

/*
 * Functions
 */

void rlist_init(FB_RECT_LIST *rlist);
void rlist_clear(FB_RECT_LIST *rlist);
void rlist_push_rect(FB_RECT_LIST *rlist, FB_RECT *rect);
void rlist_add_rect(FB_RECT_LIST *rlist, FB_RECT *rect);
int rlist_pick_rect(FB_RECT_LIST *rlist, FB_RECT *rect);
int rects_combine(FB_RECT *one, FB_RECT *another);

#endif /* _REFLIB_RECT_H */

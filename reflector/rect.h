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
 * $Id: rect.h,v 1.8 2002/09/09 07:35:31 const Exp $
 * Operations with rectangle structures.
 */

#ifndef _REFLIB_RECT_H
#define _REFLIB_RECT_H

typedef struct _FB_RECT {
  CARD16 x;
  CARD16 y;
  CARD16 w;
  CARD16 h;
  CARD32 enc;                   /* Encoding type. */
  CARD16 src_x;                 /* CopyRect X source position. */
  CARD16 src_y;                 /* CopyRect Y source position. */
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
 * A macro to set x, y, w, h values in an FB_RECT structure.
 */

#define SET_RECT(rr, xx, yy, ww, hh)            \
{                                               \
  (rr)->x = (xx);                               \
  (rr)->y = (yy);                               \
  (rr)->w = (ww);                               \
  (rr)->h = (hh);                               \
}

/*
 * Functions
 */

void rlist_init(FB_RECT_LIST *rlist);
void rlist_clear(FB_RECT_LIST *rlist);
void rlist_push_rect(FB_RECT_LIST *rlist, FB_RECT *rect);
void rlist_add_rect(FB_RECT_LIST *rlist, FB_RECT *rect);
void rlist_add_clipped_rect(FB_RECT_LIST *rlist, FB_RECT *rect,
                            FB_RECT *clip, int split_f);
int rlist_pick_rect(FB_RECT_LIST *rlist, FB_RECT *rect);

int rects_combine(FB_RECT *one, FB_RECT *another);
void rects_intersect(FB_RECT *one, FB_RECT *another);

#endif /* _REFLIB_RECT_H */

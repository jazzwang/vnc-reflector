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
 * $Id: rect.c,v 1.8 2001/12/02 08:30:07 const Exp $
 * Operations with rectangle structures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "rect.h"

/* NOTE: These constants are not used yet */
#define COMBINE_RECTS_LIMIT   16
#define COMBINE_MAX_OVERHEAD  16

#define my_min(a,b)  (((a) < (b)) ? (a) : (b))
#define my_max(a,b)  (((a) > (b)) ? (a) : (b))

/*
 * Initialize rectangle list.
 */

void rlist_init(FB_RECT_LIST *rlist)
{
  rlist->first_rect = NULL;
  rlist->last_rect = NULL;
  rlist->num_rects = 0;
}

/*
 * Clear rectangle list removing all rectangles.
 */

void rlist_clear(FB_RECT_LIST *rlist)
{
  FB_RECT_NODE *rnode, *next_rnode;

  rnode = rlist->first_rect;
  while (rnode) {
    next_rnode = rnode->next;
    free(rnode);
    rnode = next_rnode;
  }
  rlist_init(rlist);
}

/*
 * Add a rectangle to a rectangle list.
 */

void rlist_push_rect(FB_RECT_LIST *rlist, FB_RECT *rect)
{
  FB_RECT_NODE *rnode;

  rnode = malloc(sizeof(FB_RECT_NODE));
  if (rnode != NULL) {
    rnode->r = *rect;
    rnode->next = NULL;
    if (rlist->last_rect == NULL) {
      rlist->first_rect = rnode;
    } else {
      rlist->last_rect->next = rnode;
    }
    rlist->last_rect = rnode;
    rlist->num_rects++;
  }
}

/*
 * Add a rectangle to a rectangle list splitting it the way that would
 * help to cache hextile-encoded data.
 */

void rlist_add_rect(FB_RECT_LIST *rlist, FB_RECT *rect)
{
  CARD16 x_aligned, y_aligned;
  FB_RECT temp;

  if (rect->enc != RFB_ENCODING_COPYRECT && rect->w * rect->h > 256) {
    x_aligned = (rect->x + 15) & 0xFFF0;
    y_aligned = (rect->y + 15) & 0xFFF0;

    if ( rect->w - (x_aligned - rect->x) >= 16 &&
         rect->h - (y_aligned - rect->y) >= 16 ) {
      temp.enc = rect->enc;
      if (y_aligned != rect->y) {
        temp.x = rect->x;
        temp.y = rect->y;
        temp.w = rect->w;
        temp.h = y_aligned - rect->y;
        rlist_push_rect(rlist, &temp);
      }
      if (x_aligned != rect->x) {
        temp.x = rect->x;
        temp.y = y_aligned;
        temp.w = x_aligned - rect->x;
        temp.h = rect->h - (y_aligned - rect->y);
        rlist_push_rect(rlist, &temp);
      }
      temp.x = x_aligned;
      temp.y = y_aligned;
      temp.w = rect->w - (x_aligned - rect->x);
      temp.h = rect->h - (y_aligned - rect->y);
      rlist_push_rect(rlist, &temp);
      return;
    }
  }

  rlist_push_rect(rlist, rect);
}

/*
 * Intersect a rectangle with another and add the result to a
 * rectangle list (maybe adding more than one rectangle). If first
 * rectangle is a CopyRect one, it should be processed correctly by
 * this function. If split_f flag is non-zero, rlist_add_rect function
 * will be used, and rlist_push_rect otherwise.
 */

void rlist_add_clipped_rect(FB_RECT_LIST *rlist, FB_RECT *rect,
                            FB_RECT *clip, int split_f)
{
  FB_RECT temp;
  temp = *rect;
  rects_intersect(&temp, clip);

  if (rect->enc != RFB_ENCODING_COPYRECT) {
    if (temp.w * temp.h) {
      if (split_f)
        rlist_add_rect(rlist, &temp);
      else
        rlist_push_rect(rlist, &temp);
    }
  } else {
    /* We have clipped CopyRect destination already, so we should
       remove part of source location corresponding to disappeared
       part of destination. */
    if (temp.x != rect->x)
      temp.src_x += temp.x - rect->x;
    if (temp.y != rect->y)
      temp.src_y += temp.y - rect->y;

    /* Now, clip source location of CopyRect operation, and if some
       part is gone, substitute CopyRect destination rectangle with
       one smaller CopyRect and a set of non-CopyRect rectangles */
    /* FIXME: Implement that. */
    if (temp.w * temp.h)
      rlist_push_rect(rlist, &temp);
  }
}

/*
 * Move first rectangle out from a list. Returns 1 on success, 0 if
 * the list was empty.
 */

int rlist_pick_rect(FB_RECT_LIST *rlist, FB_RECT *rect)
{
  FB_RECT_NODE *new_first_rect;

  if (rlist->first_rect == NULL)
    return 0;

  *rect = rlist->first_rect->r;
  new_first_rect = rlist->first_rect->next;
  free(rlist->first_rect);
  if (new_first_rect == NULL) {
    rlist->last_rect = NULL;
  }
  rlist->first_rect = new_first_rect;
  rlist->num_rects--;
  return 1;
}

/*
 * Combine two rectangles into bigger one covering both.
 * Returns difference between the size of resulting rectangle
 * and sum of sizes of those two ones. This function does not respect
 * CopyRect-related fields of source FB_RECT structures.
 */

int rects_combine(FB_RECT *one, FB_RECT *another)
{
  int x, y, w, h, overhead;

  x = my_min(one->x, another->x);
  y = my_min(one->y, another->y);
  w = my_max(one->x + one->w, another->x + another->w) - x;
  h = my_max(one->y + one->h, another->y + another->h) - y;
  overhead = w * h - one->w * one->h - another->w * another->h;

  one->x = x, one->y = y, one->w = w, one->h = h;

  return overhead;
}

/*
 * Intersect two rectangles saving result in first one. This function
 * does not respect CopyRect-related fields of source FB_RECT
 * structures.
 */

void rects_intersect(FB_RECT *one, FB_RECT *another)
{
  int x, y, w, h;

  x = my_max(one->x, another->x);
  y = my_max(one->y, another->y);
  w = my_min(one->x + one->w, another->x + another->w) - x;
  h = my_min(one->y + one->h, another->y + another->h) - y;

  if (w < 0 || h < 0) {
    w = 0, h = 0;
  }
  one->x = x, one->y = y, one->w = w, one->h = h;
}


/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: rect.c,v 1.4 2001/08/24 05:55:50 const Exp $
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
    memcpy(&rnode->r, rect, sizeof(FB_RECT));
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

  if (rect->src_x == 0xFFFF && rect->w * rect->h > 256) {
    x_aligned = (rect->x + 15) & 0xFFF0;
    y_aligned = (rect->y + 15) & 0xFFF0;

    if ( rect->w - (x_aligned - rect->x) >= 16 &&
         rect->h - (y_aligned - rect->y) >= 16 ) {
      temp.src_x = temp.src_y = 0xFFFF;
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
 * Move first rectangle out from a list. Returns 1 on success, 0 if
 * the list was empty.
 */

int rlist_pick_rect(FB_RECT_LIST *rlist, FB_RECT *rect)
{
  FB_RECT_NODE *new_first_rect;

  if (rlist->first_rect == NULL)
    return 0;

  memcpy(rect, &rlist->first_rect->r, sizeof(FB_RECT));
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
   CopyRect-related fields of source FB_RECT structures.
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
  one->src_x = 0xFFFF; one->src_y = 0xFFFF;

  return overhead;
}


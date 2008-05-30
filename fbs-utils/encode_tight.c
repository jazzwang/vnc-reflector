/* VNC Reflector
 * Copyright (C) 2001-2004 HorizonLive.com, Inc.  All rights reserved.
 * Copyright (C) 2000,2001 Constantin Kaplinsky.  All rights reserved.
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
 * $Id: encode_tight.c,v 1.7 2004/08/08 15:23:35 const_k Exp $
 * Tight encoder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <zlib.h>

#include "rfblib.h"
#include "fbsoutput.h"
#include "encode_tight.h"

CARD32 *s_framebuffer;
CARD16 s_fb_width, s_fb_height;
FBSOUT *s_fbs;
int s_reset_mask = 0x0F;

static z_stream g_zs_struct[4];
static int g_zs_active[4] = { 0, 0, 0, 0 };

/* These parameters may be adjusted. */
#define MIN_SPLIT_RECT_SIZE     4096
#define MIN_SOLID_SUBRECT_SIZE  2048
#define MAX_SPLIT_TILE_SIZE       16

/* Compression level stuff. The following array contains various
   encoder parameters for each of 10 compression levels (0..9). Last
   three parameters correspond to JPEG quality levels (0..9). NOTE:
   This implementation does not include "gradient" filtering because
   it can be inefficient with many client connections. That is why
   gradientMinRectSize, gradientZlibLevel, gradientThreshold and
   gradientThreshold24 fields are not used. Another field,
   jpegThreshold, is also not used because we operate on the 24-bpp
   framebuffer and thus always use jpegThreshold24. */

typedef struct TIGHT_CONF_s {
  int maxRectSize, maxRectWidth;
  int monoMinRectSize, gradientMinRectSize;
  int idxZlibLevel, monoZlibLevel, rawZlibLevel, gradientZlibLevel;
  int gradientThreshold, gradientThreshold24;
  int idxMaxColorsDivisor;
  int jpegQuality, jpegThreshold, jpegThreshold24;
} TIGHT_CONF;

static TIGHT_CONF tightConf[10] = {
  {   512,   32,   6, 65536, 0, 0, 0, 0,   0,   0,   4,  5, 10000, 23000 },
  {  2048,  128,   6, 65536, 1, 1, 1, 0,   0,   0,   8, 10,  8000, 18000 },
  {  6144,  256,   8, 65536, 3, 3, 2, 0,   0,   0,  24, 15,  6500, 15000 },
  { 10240, 1024,  12, 65536, 5, 5, 3, 0,   0,   0,  32, 25,  5000, 12000 },
  { 16384, 2048,  12, 65536, 6, 6, 4, 0,   0,   0,  32, 37,  4000, 10000 },
  { 32768, 2048,  12,  4096, 7, 7, 5, 4, 150, 380,  32, 50,  3000,  8000 },
  { 65536, 2048,  16,  4096, 7, 7, 6, 4, 170, 420,  48, 60,  2000,  5000 },
  { 65536, 2048,  16,  4096, 8, 8, 7, 5, 180, 450,  64, 70,  1000,  2500 },
  { 65536, 2048,  32,  8192, 9, 9, 8, 6, 190, 475,  64, 75,   500,  1200 },
  { 65536, 2048,  32,  8192, 9, 9, 9, 6, 200, 500,  96, 80,   200,   500 }
};

static int compressLevel = 9;

/* Stuff dealing with palettes. */

typedef struct COLOR_LIST_s {
  struct COLOR_LIST_s *next;
  int idx;
  CARD32 rgb;
} COLOR_LIST;

typedef struct PALETTE_ENTRY_s {
  COLOR_LIST *listNode;
  int numPixels;
} PALETTE_ENTRY;

typedef struct PALETTE_s {
  PALETTE_ENTRY entry[256];
  COLOR_LIST *hash[256];
  COLOR_LIST list[256];
} PALETTE;

static int paletteNumColors, paletteMaxColors;
static CARD32 monoBackground, monoForeground;
static PALETTE palette;

/* Pointers to dynamically-allocated buffers. */

static int tightBeforeBufSize = 0;
static CARD8 *tightBeforeBuf = NULL;

static int tightAfterBufSize = 0;
static CARD8 *tightAfterBuf = NULL;

/* Prototypes for static functions. */

static void FindBestSolidArea (FB_RECT *r, CARD32 colorValue, FB_RECT *result);
static void ExtendSolidArea   (FB_RECT *r, CARD32 colorValue, FB_RECT *result);
static int  CheckSolidTile    (FB_RECT *r, CARD32 *colorPtr,
                               int needSameColor);

static int  SendRectSimple    (FB_RECT *r);
static int  SendSubrect       (FB_RECT *r);
static void SendTightHeader   (FB_RECT *r);

static void SendSolidRect     (void);
static int  SendMonoRect      (int w, int h);
static int  SendIndexedRect   (int w, int h);
static int  SendFullColorRect (int w, int h);

static int  CompressData(int streamId, int dataLen,
                         int zlibLevel, int zlibStrategy);
static void SendCompressedData(int compressedLen);

static void FillPalette8(int count);
static void FillPalette16(int count);
static void FillPalette32(int count);

static void PaletteReset(void);
static int  PaletteInsert(CARD32 rgb, int numPixels, int bpp);

static void Pack24(CARD8 *buf, int count);

static void EncodeIndexedRect16(CARD8 *buf, int count);
static void EncodeIndexedRect32(CARD8 *buf, int count);

static void EncodeMonoRect8(CARD8 *buf, int w, int h);
static void EncodeMonoRect16(CARD8 *buf, int w, int h);
static void EncodeMonoRect32(CARD8 *buf, int w, int h);

void transfunc_null(void *dst_buf, FB_RECT *r, void *table)
{
  CARD32 *fb_ptr;
  CARD32 *dst_ptr = (CARD32 *)dst_buf;
  int y;

  fb_ptr = &s_framebuffer[r->y * s_fb_width + r->x];

  for (y = 0; y < r->h; y++) {
    memcpy(dst_ptr, fb_ptr, r->w * sizeof(CARD32));
    fb_ptr += s_fb_width;
    dst_ptr += r->w;
  }
}

static void reset_zlib_streams(void)
{
  int stream_id;

  for (stream_id = 0; stream_id < 4; stream_id++) {
    if (g_zs_active[stream_id]) {
      deflateReset(&g_zs_struct[stream_id]);
    }
  }
}

/*
 * Tiny function to fill in rectangle header in an RFB update
 */

int put_rect_header(char *buf, FB_RECT *r)
{

  buf_put_CARD16(buf, r->x);
  buf_put_CARD16(&buf[2], r->y);
  buf_put_CARD16(&buf[4], r->w);
  buf_put_CARD16(&buf[6], r->h);
  buf_put_CARD32(&buf[8], r->enc);

  return 12;                    /* 12 bytes written */
}

/*
 * Tight encoding implementation.
 */

int
num_rects_tight(FB_RECT *r)
{
  int maxRectSize, maxRectWidth;
  int subrectMaxWidth, subrectMaxHeight;

  /* No matter how many rectangles we will send if LastRect markers
     are used to terminate rectangle stream. */
  if (r->w * r->h >= MIN_SPLIT_RECT_SIZE)
    return 0;

  maxRectSize = tightConf[compressLevel].maxRectSize;
  maxRectWidth = tightConf[compressLevel].maxRectWidth;

  if (r->w > maxRectWidth || r->w * r->h > maxRectSize) {
    subrectMaxWidth = (r->w > maxRectWidth) ? maxRectWidth : r->w;
    subrectMaxHeight = maxRectSize / subrectMaxWidth;
    return (((r->w - 1) / maxRectWidth + 1) *
            ((r->h - 1) / subrectMaxHeight + 1));
  } else {
    return 1;
  }
}

void
configure_tight_encoder(CARD32 *framebuffer,
                        CARD16 fb_width, CARD16 fb_height,
                        FBSOUT *fbs)
{
  s_framebuffer = framebuffer;
  s_fb_width = fb_width;
  s_fb_height = fb_height;
  s_fbs = fbs;

  s_reset_mask = 0x0F;
  reset_zlib_streams();
}

int
rfb_encode_tight(FB_RECT *r)
{
  int nMaxRows;
  CARD32 colorValue;
  FB_RECT rtile, rbest, rtemp;
  int t;

  if (r->w * r->h < MIN_SPLIT_RECT_SIZE)
    return SendRectSimple(r);

  /* Make sure we can write at least one pixel into tightBeforeBuf. */

  if (tightBeforeBufSize < 4) {
    tightBeforeBufSize = 4;
    if (tightBeforeBuf == NULL)
      tightBeforeBuf = malloc(tightBeforeBufSize);
    else
      tightBeforeBuf = realloc(tightBeforeBuf, tightBeforeBufSize);
  }

  /* Calculate maximum number of rows in one non-solid rectangle. */

  {
    int maxRectSize, maxRectWidth, nMaxWidth;

    maxRectSize = tightConf[compressLevel].maxRectSize;
    maxRectWidth = tightConf[compressLevel].maxRectWidth;
    nMaxWidth = (r->w > maxRectWidth) ? maxRectWidth : r->w;
    nMaxRows = maxRectSize / nMaxWidth;
  }

  /* Try to find large solid-color areas and send them separately. */

  for (rtile.y = r->y; rtile.y < r->y + r->h;
       rtile.y += MAX_SPLIT_TILE_SIZE) {

    /* If a rectangle becomes too large, send its upper part now. */

    if (rtile.y - r->y >= nMaxRows) {
      t = r->h - nMaxRows;
      r->h = nMaxRows;
      if (!SendRectSimple(r))
        return 0;
      r->y += nMaxRows;
      r->h = t;
    }

    rtile.h = (rtile.y + MAX_SPLIT_TILE_SIZE <= r->y + r->h) ?
      MAX_SPLIT_TILE_SIZE : (r->y + r->h - rtile.y);

    for (rtile.x = r->x; rtile.x < r->x + r->w;
         rtile.x += MAX_SPLIT_TILE_SIZE) {

      rtile.w = (rtile.x + MAX_SPLIT_TILE_SIZE <= r->x + r->w) ?
        MAX_SPLIT_TILE_SIZE : (r->x + r->w - rtile.x);

      if (CheckSolidTile(&rtile, &colorValue, 0)) {

        /* Get dimensions of solid-color area. */

        SET_RECT(&rtemp, rtile.x, rtile.y,
                 r->w - (rtile.x - r->x),
                 r->h - (rtile.y - r->y));
        FindBestSolidArea(&rtemp, colorValue, &rbest);

        /* Make sure a solid rectangle is large enough
           (or the whole rectangle is of the same color). */

        if (rbest.w * rbest.h != r->w * r->h &&
            rbest.w * rbest.h < MIN_SOLID_SUBRECT_SIZE)
          continue;

        /* Try to extend solid rectangle to maximum size. */

        ExtendSolidArea(r, colorValue, &rbest);

        /* Send rectangles at top and left to solid-color area. */

        SET_RECT(&rtemp, r->x, r->y, r->w, rbest.y - r->y);
        if (rbest.y != r->y && !SendRectSimple(&rtemp))
          return 0;
        SET_RECT(&rtemp, r->x, rbest.y, rbest.x - r->x, rbest.h);
        if (rbest.x != r->x && !rfb_encode_tight(&rtemp))
          return 0;

        /* Send solid-color rectangle. */

        SendTightHeader(&rbest);

        SET_RECT(&rtemp, rbest.x, rbest.y, 1, 1);
        transfunc_null(tightBeforeBuf, &rtemp, NULL);

        SendSolidRect();

        /* Send remaining rectangles (at right and bottom). */

        SET_RECT(&rtemp, rbest.x + rbest.w, rbest.y,
                 r->w - (rbest.x - r->x) - rbest.w, rbest.h);
        if (rbest.x + rbest.w != r->x + r->w &&
            !rfb_encode_tight(&rtemp))
          return 0;
        SET_RECT(&rtemp, r->x, rbest.y + rbest.h,
                 r->w, r->h - (rbest.y - r->y) - rbest.h);
        if (rbest.y + rbest.h != r->y + r->h &&
            !rfb_encode_tight(&rtemp))
          return 0;

        /* Return after all recursive calls are done. */

        return 1;
      }

    }

  }

  /* No suitable solid-color rectangles found. */

  return SendRectSimple(r);
}

static void
FindBestSolidArea(FB_RECT *r, CARD32 colorValue, FB_RECT *result)
{
  FB_RECT rc;
  int w_prev;
  int w_best = 0, h_best = 0;

  w_prev = r->w;

  for (rc.y = r->y; rc.y < r->y + r->h; rc.y += MAX_SPLIT_TILE_SIZE) {

    rc.h = (rc.y + MAX_SPLIT_TILE_SIZE <= r->y + r->h) ?
      MAX_SPLIT_TILE_SIZE : (r->y + r->h - rc.y);
    rc.w = (w_prev > MAX_SPLIT_TILE_SIZE) ?
      MAX_SPLIT_TILE_SIZE : w_prev;

    rc.x = r->x;
    if (!CheckSolidTile(&rc, &colorValue, 1))
      break;

    for (rc.x = r->x + rc.w; rc.x < r->x + w_prev;) {
      rc.w = (rc.x + MAX_SPLIT_TILE_SIZE <= r->x + w_prev) ?
        MAX_SPLIT_TILE_SIZE : (r->x + w_prev - rc.x);
      if (!CheckSolidTile(&rc, &colorValue, 1))
        break;
      rc.x += rc.w;
    }

    w_prev = rc.x - r->x;
    if (w_prev * (rc.y + rc.h - r->y) > w_best * h_best) {
      w_best = w_prev;
      h_best = rc.y + rc.h - r->y;
    }
  }

  SET_RECT(result, r->x, r->y, w_best, h_best);
}

static void
ExtendSolidArea(FB_RECT *r_bounds, CARD32 colorValue, FB_RECT *r)
{
  FB_RECT rtemp;

  rtemp.x = r->x;
  rtemp.w = r->w;
  rtemp.h = 1;

  /* Try to extend the area upwards. */
  if (r->y > 0) {
    for (rtemp.y = r->y - 1; rtemp.y >= r_bounds->y; rtemp.y--) {
      if (!CheckSolidTile(&rtemp, &colorValue, 1))
        break;
    }
    r->h += r->y - (rtemp.y + 1);
    r->y = rtemp.y + 1;
  }

  /* ... downwards. */
  for (rtemp.y = r->y + r->h; rtemp.y < r_bounds->y + r_bounds->h; rtemp.y++) {
    if (!CheckSolidTile(&rtemp, &colorValue, 1))
      break;
  }
  r->h += rtemp.y - (r->y + r->h);

  rtemp.y = r->y;
  rtemp.h = r->h;
  rtemp.w = 1;

  /* ... to the left. */
  if (r->x > 0) {
    for (rtemp.x = r->x - 1; rtemp.x >= r_bounds->x; rtemp.x--) {
      if (!CheckSolidTile(&rtemp, &colorValue, 1))
        break;
    }
    r->w += r->x - (rtemp.x + 1);
    r->x = rtemp.x + 1;
  }

  /* ... to the right. */
  for (rtemp.x = r->x + r->w; rtemp.x < r_bounds->x + r_bounds->w; rtemp.x++) {
    if (!CheckSolidTile(&rtemp, &colorValue, 1))
      break;
  }
  r->w += rtemp.x - (r->x + r->w);
}

/*
 * Check if a rectangle is all of the same color. If needSameColor is
 * set to non-zero, then also check that its color equals to the
 * *colorPtr value. The result is 1 if the test is successfull, and in
 * that case new color will be stored in *colorPtr.
 */

static int
CheckSolidTile(FB_RECT *r, CARD32 *colorPtr, int needSameColor)
{
  CARD32 *fb_ptr;
  CARD32 colorValue;
  int dx, dy;

  fb_ptr = &s_framebuffer[r->y * s_fb_width + r->x];

  colorValue = *fb_ptr;
  if (needSameColor && colorValue != *colorPtr)
    return 0;

  /* Check the first row. */
  for (dx = 0; dx < r->w; dx++) {
    if (colorValue != fb_ptr[dx])
      return 0;
  }

  /* Check other rows -- memcmp() does it faster. */
  for (dy = 1; dy < r->h; dy++) {
    if (memcmp(fb_ptr, &fb_ptr[dy * s_fb_width], r->w * sizeof(CARD32)) != 0)
      return 0;
  }

  *colorPtr = colorValue;
  return 1;
}

static int
SendRectSimple(FB_RECT *r)
{
  int maxBeforeSize, maxAfterSize;
  int maxRectSize, maxRectWidth;
  int subrectMaxWidth, subrectMaxHeight;
  FB_RECT sr;

  maxRectSize = tightConf[compressLevel].maxRectSize;
  maxRectWidth = tightConf[compressLevel].maxRectWidth;

  maxBeforeSize = maxRectSize * 4;
  maxAfterSize = maxBeforeSize + (maxBeforeSize + 99) / 100 + 12;

  if (tightBeforeBufSize < maxBeforeSize) {
    tightBeforeBufSize = maxBeforeSize;
    if (tightBeforeBuf == NULL)
      tightBeforeBuf = malloc(tightBeforeBufSize);
    else
      tightBeforeBuf = realloc(tightBeforeBuf, tightBeforeBufSize);
  }

  if (tightAfterBufSize < maxAfterSize) {
    tightAfterBufSize = maxAfterSize;
    if (tightAfterBuf == NULL)
      tightAfterBuf = malloc(tightAfterBufSize);
    else
      tightAfterBuf = realloc(tightAfterBuf, tightAfterBufSize);
  }

  if (tightBeforeBuf == NULL || tightAfterBuf == NULL)
    return 0;

  if (r->w > maxRectWidth || r->w * r->h > maxRectSize) {
    subrectMaxWidth = (r->w > maxRectWidth) ? maxRectWidth : r->w;
    subrectMaxHeight = maxRectSize / subrectMaxWidth;

    for (sr.y = r->y; sr.y < r->y + r->h; sr.y += subrectMaxHeight) {
      for (sr.x = r->x; sr.x < r->x + r->w; sr.x += maxRectWidth) {
        sr.w = (sr.x - r->x + maxRectWidth < r->w) ?
          maxRectWidth : r->x + r->w - sr.x;
        sr.h = (sr.y - r->y + subrectMaxHeight < r->h) ?
          subrectMaxHeight : r->y + r->h - sr.y;
        if (!SendSubrect(&sr))
          return 0;
      }
    }
  } else {
    if (!SendSubrect(r))
      return 0;
  }

  return 1;
}

static int SendSubrect(FB_RECT *r)
{
  int success = 0;

  SendTightHeader(r);

  transfunc_null(tightBeforeBuf, r, NULL);

  paletteMaxColors =
    r->w * r->h / tightConf[compressLevel].idxMaxColorsDivisor;
  if ( paletteMaxColors < 2 &&
       r->w * r->h >= tightConf[compressLevel].monoMinRectSize ) {
    paletteMaxColors = 2;
  }
  FillPalette32(r->w * r->h);

  switch (paletteNumColors) {
  case 0:
    /* Truecolor image */
    success = SendFullColorRect(r->w, r->h);
    break;
  case 1:
    /* Solid rectangle */
    SendSolidRect();
    success = 1;
    break;
  case 2:
    /* Two-color rectangle */
    success = SendMonoRect(r->w, r->h);
    break;
  default:
    /* Up to 256 different colors */
    success = SendIndexedRect(r->w, r->h);
  }
  return success;
}

static void
SendTightHeader(FB_RECT *r)
{
  char rect_hdr[12];

  r->enc = RFB_ENCODING_TIGHT;
  put_rect_header(rect_hdr, r);
  fbs_write(s_fbs, rect_hdr, sizeof(rect_hdr));
}

/*
 * Subencoding implementations.
 */

static void
SendSolidRect(void)
{
  char buf[5];
  int len;

  Pack24(tightBeforeBuf, 1);
  len = 3;

  buf[0] = RFB_TIGHT_FILL | s_reset_mask;
  s_reset_mask = 0;
  memcpy(&buf[1], tightBeforeBuf, len);
  fbs_write(s_fbs, buf, 1 + len);
}

static int
SendMonoRect(int w, int h)
{
  char buf[11];
  int streamId = 1;
  int paletteLen, dataLen;

  /* Prepare tight encoding header. */
  dataLen = (w + 7) / 8;
  dataLen *= h;

  buf[0] = RFB_TIGHT_EXPLICIT_FILTER | (streamId << 4) | s_reset_mask;
  s_reset_mask = 0;
  buf[1] = RFB_TIGHT_FILTER_PALETTE;
  buf[2] = 1;                   /* number of colors - 1 */

  /* Prepare palette, convert image. */
  EncodeMonoRect32((CARD8 *)tightBeforeBuf, w, h);

  ((CARD32 *)tightAfterBuf)[0] = monoBackground;
  ((CARD32 *)tightAfterBuf)[1] = monoForeground;
  Pack24(tightAfterBuf, 2);
  paletteLen = 6;

  memcpy(&buf[3], tightAfterBuf, paletteLen);
  fbs_write(s_fbs, buf, 3 + paletteLen);

  return CompressData(streamId, dataLen,
                      tightConf[compressLevel].monoZlibLevel,
                      Z_DEFAULT_STRATEGY);
}

static int
SendIndexedRect(int w, int h)
{
  char buf[3 + 256*4];
  int streamId = 2;
  int i, entryLen;

  buf[0] = RFB_TIGHT_EXPLICIT_FILTER | (streamId << 4) | s_reset_mask;
  s_reset_mask = 0;
  buf[1] = RFB_TIGHT_FILTER_PALETTE;
  buf[2] = (CARD8)(paletteNumColors - 1);

  /* Prepare palette, convert image. */
  EncodeIndexedRect32((CARD8 *)tightBeforeBuf, w * h);

  for (i = 0; i < paletteNumColors; i++) {
    ((CARD32 *)tightAfterBuf)[i] =
      palette.entry[i].listNode->rgb;
  }
  Pack24(tightAfterBuf, paletteNumColors);
  entryLen = 3;

  memcpy(&buf[3], tightAfterBuf, paletteNumColors * entryLen);
  fbs_write(s_fbs, buf, 3 + paletteNumColors * entryLen);

  return CompressData(streamId, w * h,
                      tightConf[compressLevel].idxZlibLevel,
                      Z_DEFAULT_STRATEGY);
}

static int
SendFullColorRect(int w, int h)
{
  char buf[1];
  int streamId = 0;
  int len;

  buf[0] = s_reset_mask;        /* stream id = 0, no filter */
  s_reset_mask = 0;
  fbs_write(s_fbs, buf, 1);

  Pack24(tightBeforeBuf, w * h);
  len = 3;

  return CompressData(streamId, w * h * len,
                      tightConf[compressLevel].rawZlibLevel,
                      Z_DEFAULT_STRATEGY);
}

static int
CompressData(int streamId, int dataLen,
             int zlibLevel, int zlibStrategy)
{
  z_streamp pz;
  int err;

  if (dataLen < RFB_TIGHT_MIN_TO_COMPRESS) {
    fbs_write(s_fbs, (char *)tightBeforeBuf, dataLen);
    return 1;
  }

  pz = &g_zs_struct[streamId];

  /* Initialize compression stream if needed. */
  if (!g_zs_active[streamId]) {
    pz->zalloc = Z_NULL;
    pz->zfree = Z_NULL;
    pz->opaque = Z_NULL;

    err = deflateInit2 (pz, zlibLevel, Z_DEFLATED, MAX_WBITS,
                        MAX_MEM_LEVEL, zlibStrategy);
    if (err != Z_OK)
      return 0;

    g_zs_active[streamId] = 1;
  }

  /* Prepare buffer pointers. */
  pz->next_in = (Bytef *)tightBeforeBuf;
  pz->avail_in = dataLen;
  pz->next_out = (Bytef *)tightAfterBuf;
  pz->avail_out = tightAfterBufSize;

  /* Actual compression. */
  if ( deflate (pz, Z_SYNC_FLUSH) != Z_OK ||
       pz->avail_in != 0 || pz->avail_out == 0 ) {
    return 0;
  }

  SendCompressedData(tightAfterBufSize - pz->avail_out);
  return 1;
}

static void SendCompressedData(int compressedLen)
{
  char buf[3];
  int len_bytes = 0;

  buf[len_bytes++] = compressedLen & 0x7F;
  if (compressedLen > 0x7F) {
    buf[len_bytes-1] |= 0x80;
    buf[len_bytes++] = compressedLen >> 7 & 0x7F;
    if (compressedLen > 0x3FFF) {
      buf[len_bytes-1] |= 0x80;
      buf[len_bytes++] = compressedLen >> 14 & 0xFF;
    }
  }
  fbs_write(s_fbs, buf, len_bytes);
  fbs_write(s_fbs, (char *)tightAfterBuf, compressedLen);
}

/*
 * Code to determine how many different colors are used in a rectangle.
 */

static void
FillPalette8(int count)
{
    CARD8 *data = (CARD8 *)tightBeforeBuf;
    CARD8 c0, c1;
    int i, n0, n1;

    paletteNumColors = 0;

    c0 = data[0];
    for (i = 1; i < count && data[i] == c0; i++);
    if (i == count) {
        paletteNumColors = 1;
        return;                 /* Solid rectangle */
    }

    if (paletteMaxColors < 2)
        return;

    n0 = i;
    c1 = data[i];
    n1 = 0;
    for (i++; i < count; i++) {
        if (data[i] == c0) {
            n0++;
        } else if (data[i] == c1) {
            n1++;
        } else
            break;
    }
    if (i == count) {
        if (n0 > n1) {
            monoBackground = (CARD32)c0;
            monoForeground = (CARD32)c1;
        } else {
            monoBackground = (CARD32)c1;
            monoForeground = (CARD32)c0;
        }
        paletteNumColors = 2;   /* Two colors */
    }
}

#define DEFINE_FILL_PALETTE_FUNCTION(bpp)                               \
                                                                        \
static void                                                             \
FillPalette##bpp(int count)                                             \
{                                                                       \
    CARD##bpp *data = (CARD##bpp *)tightBeforeBuf;                      \
    CARD##bpp c0, c1, ci;                                               \
    int i, n0, n1, ni;                                                  \
                                                                        \
    c0 = data[0];                                                       \
    for (i = 1; i < count && data[i] == c0; i++);                       \
    if (i >= count) {                                                   \
        paletteNumColors = 1;   /* Solid rectangle */                   \
        return;                                                         \
    }                                                                   \
                                                                        \
    if (paletteMaxColors < 2) {                                         \
        paletteNumColors = 0;   /* Full-color encoding preferred */     \
        return;                                                         \
    }                                                                   \
                                                                        \
    n0 = i;                                                             \
    c1 = data[i];                                                       \
    n1 = 0;                                                             \
    for (i++; i < count; i++) {                                         \
        ci = data[i];                                                   \
        if (ci == c0) {                                                 \
            n0++;                                                       \
        } else if (ci == c1) {                                          \
            n1++;                                                       \
        } else                                                          \
            break;                                                      \
    }                                                                   \
    if (i >= count) {                                                   \
        if (n0 > n1) {                                                  \
            monoBackground = (CARD32)c0;                                \
            monoForeground = (CARD32)c1;                                \
        } else {                                                        \
            monoBackground = (CARD32)c1;                                \
            monoForeground = (CARD32)c0;                                \
        }                                                               \
        paletteNumColors = 2;   /* Two colors */                        \
        return;                                                         \
    }                                                                   \
                                                                        \
    PaletteReset();                                                     \
    PaletteInsert (c0, (CARD32)n0, bpp);                                \
    PaletteInsert (c1, (CARD32)n1, bpp);                                \
                                                                        \
    ni = 1;                                                             \
    for (i++; i < count; i++) {                                         \
        if (data[i] == ci) {                                            \
            ni++;                                                       \
        } else {                                                        \
            if (!PaletteInsert (ci, (CARD32)ni, bpp))                   \
                return;                                                 \
            ci = data[i];                                               \
            ni = 1;                                                     \
        }                                                               \
    }                                                                   \
    PaletteInsert (ci, (CARD32)ni, bpp);                                \
}

DEFINE_FILL_PALETTE_FUNCTION(16)
DEFINE_FILL_PALETTE_FUNCTION(32)


/*
 * Functions to operate with palette structures.
 */

#define HASH_FUNC16(rgb) ((int)((((rgb) >> 8) + (rgb)) & 0xFF))
#define HASH_FUNC32(rgb) ((int)((((rgb) >> 16) + ((rgb) >> 8)) & 0xFF))

static void
PaletteReset(void)
{
    paletteNumColors = 0;
    memset(palette.hash, 0, 256 * sizeof(COLOR_LIST *));
}

static int
PaletteInsert(CARD32 rgb, int numPixels, int bpp)
{
    COLOR_LIST *pnode;
    COLOR_LIST *prev_pnode = NULL;
    int hash_key, idx, new_idx, count;

    hash_key = (bpp == 16) ? HASH_FUNC16(rgb) : HASH_FUNC32(rgb);

    pnode = palette.hash[hash_key];

    while (pnode != NULL) {
        if (pnode->rgb == rgb) {
            /* Such palette entry already exists. */
            new_idx = idx = pnode->idx;
            count = palette.entry[idx].numPixels + numPixels;
            if (new_idx && palette.entry[new_idx-1].numPixels < count) {
                do {
                    palette.entry[new_idx] = palette.entry[new_idx-1];
                    palette.entry[new_idx].listNode->idx = new_idx;
                    new_idx--;
                }
                while (new_idx && palette.entry[new_idx-1].numPixels < count);
                palette.entry[new_idx].listNode = pnode;
                pnode->idx = new_idx;
            }
            palette.entry[new_idx].numPixels = count;
            return paletteNumColors;
        }
        prev_pnode = pnode;
        pnode = pnode->next;
    }

    /* Check if palette is full. */
    if (paletteNumColors == 256 || paletteNumColors == paletteMaxColors) {
        paletteNumColors = 0;
        return 0;
    }

    /* Move palette entries with lesser pixel counts. */
    for ( idx = paletteNumColors;
          idx > 0 && palette.entry[idx-1].numPixels < numPixels;
          idx-- ) {
        palette.entry[idx] = palette.entry[idx-1];
        palette.entry[idx].listNode->idx = idx;
    }

    /* Add new palette entry into the freed slot. */
    pnode = &palette.list[paletteNumColors];
    if (prev_pnode != NULL) {
        prev_pnode->next = pnode;
    } else {
        palette.hash[hash_key] = pnode;
    }
    pnode->next = NULL;
    pnode->idx = idx;
    pnode->rgb = rgb;
    palette.entry[idx].listNode = pnode;
    palette.entry[idx].numPixels = numPixels;

    return (++paletteNumColors);
}


/*
 * Convert 32-bit color samples into 24-bit colors, in place. Source
 * data should be in the server's pixel format which is RGB888.
 */

static void Pack24(CARD8 *buf, int count)
{
    CARD32 *buf32;
    CARD32 pix;

    buf32 = (CARD32 *)buf;

    while (count--) {
        pix = *buf32++;
        *buf++ = (CARD8)(pix >> 16);
        *buf++ = (CARD8)(pix >> 8);
        *buf++ = (CARD8)pix;
    }
}


/*
 * Converting truecolor samples into palette indices.
 */

#define DEFINE_IDX_ENCODE_FUNCTION(bpp)                                 \
                                                                        \
static void                                                             \
EncodeIndexedRect##bpp(CARD8 *buf, int count)                           \
{                                                                       \
    COLOR_LIST *pnode;                                                  \
    CARD##bpp *src;                                                     \
    CARD##bpp rgb;                                                      \
    int rep = 0;                                                        \
                                                                        \
    src = (CARD##bpp *) buf;                                            \
                                                                        \
    while (count--) {                                                   \
        rgb = *src++;                                                   \
        while (count && *src == rgb) {                                  \
            rep++, src++, count--;                                      \
        }                                                               \
        pnode = palette.hash[HASH_FUNC##bpp(rgb)];                      \
        while (pnode != NULL) {                                         \
            if ((CARD##bpp)pnode->rgb == rgb) {                         \
                *buf++ = (CARD8)pnode->idx;                             \
                while (rep) {                                           \
                    *buf++ = (CARD8)pnode->idx;                         \
                    rep--;                                              \
                }                                                       \
                break;                                                  \
            }                                                           \
            pnode = pnode->next;                                        \
        }                                                               \
    }                                                                   \
}

DEFINE_IDX_ENCODE_FUNCTION(16)
DEFINE_IDX_ENCODE_FUNCTION(32)

#define DEFINE_MONO_ENCODE_FUNCTION(bpp)                                \
                                                                        \
static void                                                             \
EncodeMonoRect##bpp(CARD8 *buf, int w, int h)                           \
{                                                                       \
    CARD##bpp *ptr;                                                     \
    CARD##bpp bg;                                                       \
    unsigned int value, mask;                                           \
    int aligned_width;                                                  \
    int x, y, bg_bits;                                                  \
                                                                        \
    ptr = (CARD##bpp *) buf;                                            \
    bg = (CARD##bpp) monoBackground;                                    \
    aligned_width = w - w % 8;                                          \
                                                                        \
    for (y = 0; y < h; y++) {                                           \
        for (x = 0; x < aligned_width; x += 8) {                        \
            for (bg_bits = 0; bg_bits < 8; bg_bits++) {                 \
                if (*ptr++ != bg)                                       \
                    break;                                              \
            }                                                           \
            if (bg_bits == 8) {                                         \
                *buf++ = 0;                                             \
                continue;                                               \
            }                                                           \
            mask = 0x80 >> bg_bits;                                     \
            value = mask;                                               \
            for (bg_bits++; bg_bits < 8; bg_bits++) {                   \
                mask >>= 1;                                             \
                if (*ptr++ != bg) {                                     \
                    value |= mask;                                      \
                }                                                       \
            }                                                           \
            *buf++ = (CARD8)value;                                      \
        }                                                               \
                                                                        \
        mask = 0x80;                                                    \
        value = 0;                                                      \
        if (x >= w)                                                     \
            continue;                                                   \
                                                                        \
        for (; x < w; x++) {                                            \
            if (*ptr++ != bg) {                                         \
                value |= mask;                                          \
            }                                                           \
            mask >>= 1;                                                 \
        }                                                               \
        *buf++ = (CARD8)value;                                          \
    }                                                                   \
}

DEFINE_MONO_ENCODE_FUNCTION(8)
DEFINE_MONO_ENCODE_FUNCTION(16)
DEFINE_MONO_ENCODE_FUNCTION(32)


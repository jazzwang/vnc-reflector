/*
 * FrameBuffer Stream Utilities.
 * Copyright (C) 2008 Wimba, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file
 * LICENSE, included.
 */

#include <stdio.h>

#include "version.h"
#include "fbs-io.h"

static void report_usage(char *program_name);
static int list_fbs(FILE *fp);

int main (int argc, char *argv[])
{
  FILE *fp = stdin;
  int needClose = 0;
  int success = 0;

  if (argc == 2 && argv[1][0] != '-') {
    fp = fopen(argv[1], "rb");
    if (fp == NULL) {
      fprintf(stderr, "Error opening file: %s\n", argv[1]);
      return 1;
    }
  } else if (argc >= 2) {
    report_usage(argv[0]);
    return 1;
  }

  if (list_fbs(fp)) {
    success = 1;
  }

  if (needClose) {
    fclose(fp);
  }

  return success;
}

static void report_usage(char *program_name)
{
  fprintf(stderr, "fbs-list version %s.\n%s\n\n", VERSION, COPYRIGHT);

  fprintf(stderr, "Usage: %s [FBS_FILE]\n\n",
          program_name);
}

static int list_fbs(FILE *fp)
{
  FBSTREAM fbs;

  if (!open_fbstream(&fbs, fp))
    return 0;

  return 1;
}

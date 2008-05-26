# FrameBuffer Stream Utilities.
# Copyright (C) 2008 Wimba, Inc.  All rights reserved.
#
# This software is released under the terms specified in the file LICENSE,
# included.
#
# Variables you might want to edit: CFLAGS, CONFFLAGS

IFLAGS =	-I../lib

# Production
CFLAGS =	-O2 $(IFLAGS)
# Debug (strict)
#CFLAGS =	-g -pedantic -Wall $(IFLAGS)
# Debug (profiling)
#CFLAGS =	-g -pg $(IFLAGS)
# Debug (normal)
#CFLAGS =	-g $(IFLAGS)

CONFFLAGS =

PROG_FBS_LIST = fbs-list
OBJS_FBS_LIST = fbs-list.o fbsinput.o
LDFLAGS_FBS_LIST = -L/usr/local/lib -L../lib -lvref -lz

PROG_FBS_UNCHAIN = fbs-unchain
OBJS_FBS_UNCHAIN = fbs-unchain.o fbsinput.o fbsoutput.o
LDFLAGS_FBS_UNCHAIN = -L/usr/local/lib -lz

PROG_FBS_MKINDEX = fbs-mkindex
OBJS_FBS_MKINDEX = fbs-mkindex.o fbsinput.o fbsoutput.o encode_tight.o
LDFLAGS_FBS_MKINDEX = -L/usr/local/lib -L../lib -lvref -lz

SRCS = fbs-list.c fbs-unchain.c fbs-mkindex.c fbsinput.c fbsoutput.c \
	encode_tight.c

CC = gcc
MAKEDEPEND = makedepend
MAKEDEPFLAGS = -Y

default: $(PROG_FBS_LIST) $(PROG_FBS_UNCHAIN) $(PROG_FBS_MKINDEX)

$(PROG_FBS_LIST): $(OBJS_FBS_LIST)
	$(CC) $(CFLAGS) -o $(PROG_FBS_LIST) $(OBJS_FBS_LIST) \
		$(LDFLAGS_FBS_LIST)

$(PROG_FBS_UNCHAIN): $(OBJS_FBS_UNCHAIN)
	$(CC) $(CFLAGS) -o $(PROG_FBS_UNCHAIN) $(OBJS_FBS_UNCHAIN) \
		$(LDFLAGS_FBS_UNCHAIN)

$(PROG_FBS_MKINDEX): $(OBJS_FBS_MKINDEX)
	$(CC) $(CFLAGS) -o $(PROG_FBS_MKINDEX) $(OBJS_FBS_MKINDEX) \
		$(LDFLAGS_FBS_MKINDEX)

clean: 
	rm -f $(OBJS) *~ *.bak *.o \
		$(PROG_FBS_LIST) $(PROG_FBS_UNCHAIN) $(PROG_FBS_MKINDEX)

depend:
	$(MAKEDEPEND) $(MAKEDEPFLAGS) $(IFLAGS) $(SRCS) 2> /dev/null

.c.o:
	$(CC) $(CFLAGS) $(CONFFLAGS) -c $<


# DO NOT DELETE

fbs-list.o: ../lib/rfblib.h ../lib/tight-decoder.h version.h fbsinput.h
fbs-unchain.o: ../lib/rfblib.h version.h fbsinput.h fbsoutput.h
fbs-mkindex.o: ../lib/rfblib.h ../lib/tight-decoder.h version.h fbsinput.h
fbs-mkindex.o: fbsoutput.h
fbsinput.o: ../lib/rfblib.h fbsinput.h
fbsoutput.o: ../lib/rfblib.h fbsoutput.h
encode_tight.o: ../lib/rfblib.h fbsoutput.h encode_tight.h

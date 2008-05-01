# FrameBuffer Stream Utilities.
# Copyright (C) 2008 Wimba, Inc.  All rights reserved.
#
# This software is released under the terms specified in the file LICENSE,
# included.
#
# Variables you might want to edit: CFLAGS, CONFFLAGS

IFLAGS =	-I../reflector

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
LDFLAGS_FBS_LIST = -L/usr/local/lib

PROG_FBS_UNCHAIN = fbs-unchain
OBJS_FBS_UNCHAIN = fbs-unchain.o fbsinput.o fbsoutput.o
LDFLAGS_FBS_UNCHAIN = -L/usr/local/lib -lz

SRCS = fbs-list.c fbs-unchain.c fbsinput.c fbsoutput.c

CC = gcc
MAKEDEPEND = makedepend
MAKEDEPFLAGS = -Y

default: $(PROG_FBS_LIST) $(PROG_FBS_UNCHAIN)

$(PROG_FBS_LIST): $(OBJS_FBS_LIST)
	$(CC) $(CFLAGS) -o $(PROG_FBS_LIST) $(OBJS_FBS_LIST) \
		$(LDFLAGS_FBS_LIST)

$(PROG_FBS_UNCHAIN): $(OBJS_FBS_UNCHAIN)
	$(CC) $(CFLAGS) -o $(PROG_FBS_UNCHAIN) $(OBJS_FBS_UNCHAIN) \
		$(LDFLAGS_FBS_UNCHAIN)

clean: 
	rm -f $(OBJS) *~ *.bak *.o $(PROG_FBS_LIST) $(PROG_FBS_UNCHAIN)

depend:
	$(MAKEDEPEND) $(MAKEDEPFLAGS) $(IFLAGS) $(SRCS) 2> /dev/null

.c.o:
	$(CC) $(CFLAGS) $(CONFFLAGS) -c $<


# DO NOT DELETE

fbs-list.o: ../reflector/rfblib.h version.h fbsinput.h
fbs-unchain.o: ../reflector/rfblib.h version.h fbsinput.h
fbsinput.o: ../reflector/rfblib.h fbsinput.h
fbsoutput.o: ../reflector/rfblib.h fbsoutput.h

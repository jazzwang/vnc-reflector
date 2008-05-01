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

# Link with zlib library
LDFLAGS =	-L/usr/local/lib -lz

PROG = 	fbs-list

OBJS = 	fbs-list.o fbsinput.o

SRCS =	fbs-list.c fbsinput.c

CC = gcc
MAKEDEPEND = makedepend
MAKEDEPFLAGS = -Y

default: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) *core* ./*~ ./*.bak $(PROG)

depend: $(SRCS)
	$(MAKEDEPEND) $(MAKEDEPFLAGS) $(IFLAGS) $(SRCS) 2> /dev/null

.c.o:
	$(CC) $(CFLAGS) $(CONFFLAGS) -c $<


# DO NOT DELETE

fbs-list.o: ../reflector/rfblib.h version.h fbsinput.h
fbsinput.o: ../reflector/rfblib.h fbsinput.h

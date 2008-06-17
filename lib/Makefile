# VNC Reflector
# Copyright (C) 2001-2004 HorizonLive.com, Inc.  All rights reserved.
#
# This software is released under the terms specified in the file LICENSE,
# included.  HorizonLive provides e-Learning and collaborative synchronous
# presentation solutions in a totally Web-based environment.  For more
# information about HorizonLive, please see our website at
# http://www.horizonlive.com.
#
# This software was authored by Constantin Kaplinsky <const@ce.cctpu.edu.ru>
# and sponsored by HorizonLive.com, Inc.
#
# $Id: Makefile,v 1.40 2004/08/08 15:23:35 const_k Exp $
#
# Variables you might want to edit: CFLAGS, CONFFLAGS

IFLAGS =	-I.

# Production
CFLAGS =	-O2 $(IFLAGS)
# Debug (strict)
#CFLAGS =	-g -pedantic -Wall $(IFLAGS)
# Debug (profiling)
#CFLAGS =	-g -pg $(IFLAGS)
# Debug (normal)
#CFLAGS =	-g $(IFLAGS)

LIBRARY = libvref.a

OBJS = rfblib.o d3des.o tight-decoder.o

SRCS = rfblib.c d3des.c tight-decoder.c

CC = gcc
AR = ar cq
RANLIB = ranlib
MAKEDEPEND = makedepend
MAKEDEPFLAGS = -Y

default: $(LIBRARY)

$(LIBRARY): $(OBJS)
	rm -f $(LIBRARY)
	$(AR) $(LIBRARY) $(OBJS)
	$(RANLIB) $(LIBRARY)

clean: 
	rm -f $(LIBRARY) $(OBJS) *core* ./*~ ./*.bak $(PROG)

depend: $(SRCS)
	$(MAKEDEPEND) $(MAKEDEPFLAGS) -I. $(SRCS) 2> /dev/null

.c.o:
	$(CC) $(CFLAGS) $(CONFFLAGS) -c $<


# DO NOT DELETE

rfblib.o: rfblib.h d3des.h
d3des.o: d3des.h
tight-decoder.o: tight-decoder.h

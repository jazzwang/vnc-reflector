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

IFLAGS =	-I../lib

# Production
CFLAGS =	-O2 $(IFLAGS)
# Debug (strict)
#CFLAGS =	-g -pedantic -Wall $(IFLAGS)
# Debug (profiling)
#CFLAGS =	-g -pg $(IFLAGS)
# Debug (normal)
#CFLAGS =	-g $(IFLAGS)

# Use poll(2) syscall in async I/O instead of select(2)
CONFFLAGS =	-DUSE_POLL

# Link with ../lib/libvref.a, zlib and JPEG libraries
LDFLAGS =	../lib/libvref.a -L/usr/local/lib -lz -ljpeg

PROG = 	vncreflector

OBJS = 	main.o logging.o active.o actions.o host_connect.o \
	async_io.o host_io.o client_io.o encode.o region.o translate.o \
	control.o encode_tight.o decode_hextile.o decode_tight.o \
	decode_cursor.o fbs_files.o region_more.o

SRCS =	main.c logging.c active.c actions.c host_connect.c \
	async_io.c host_io.c client_io.c encode.c region.c translate.c \
	control.c encode_tight.c decode_hextile.c decode_tight.c \
	decode_cursor.c fbs_files.c region_more.c

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

main.o: ../lib/rfblib.h async_io.h logging.h reflector.h host_connect.h
main.o: translate.h host_io.h client_io.h region.h encode.h
logging.o: logging.h
active.o: ../lib/rfblib.h reflector.h logging.h
actions.o: ../lib/rfblib.h reflector.h logging.h
host_connect.o: ../lib/rfblib.h reflector.h logging.h async_io.h host_io.h
host_connect.o: translate.h client_io.h region.h encode.h host_connect.h
async_io.o: async_io.h
host_io.o: ../lib/rfblib.h reflector.h async_io.h logging.h translate.h
host_io.o: client_io.h region.h host_connect.h host_io.h encode.h
client_io.o: ../lib/rfblib.h logging.h async_io.h reflector.h host_io.h
client_io.o: translate.h client_io.h region.h encode.h
encode.o: ../lib/rfblib.h reflector.h async_io.h translate.h client_io.h
encode.o: region.h encode.h
region.o: ../lib/rfblib.h region.h
translate.o: ../lib/rfblib.h reflector.h async_io.h translate.h client_io.h
translate.o: region.h
control.o: ../lib/rfblib.h async_io.h logging.h reflector.h host_connect.h
control.o: host_io.h translate.h client_io.h region.h
encode_tight.o: ../lib/rfblib.h reflector.h async_io.h translate.h
encode_tight.o: client_io.h region.h encode.h
decode_hextile.o: ../lib/rfblib.h reflector.h async_io.h logging.h host_io.h
decode_tight.o: ../lib/rfblib.h reflector.h async_io.h logging.h host_io.h
decode_cursor.o: ../lib/rfblib.h logging.h async_io.h translate.h client_io.h
decode_cursor.o: region.h host_io.h reflector.h
fbs_files.o: ../lib/rfblib.h reflector.h logging.h
region_more.o: ../lib/rfblib.h region.h logging.h

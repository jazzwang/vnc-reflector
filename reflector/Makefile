# VNC Reflector
# Copyright (C) 2001 Const Kaplinsky
#
# $Id: Makefile,v 1.22 2001/08/23 10:06:54 const Exp $
#
# Variables you might want to edit: CFLAGS, CONFFLAGS.

IFLAGS =	-I.

# Production
#CFLAGS =	-O2 $(IFLAGS)
# Debug (strict)
CFLAGS =	-g -pedantic $(IFLAGS)
# Debug (profiling)
#CFLAGS =	-g -pg $(IFLAGS)
# Debug (normal)
#CFLAGS =	-g $(IFLAGS)

# Use poll(2) syscall in async I/O instead of select(2)
CONFFLAGS =	-DUSE_POLL

LDFLAGS =

PROG = 	vncreflector

OBJS = 	main.o logging.o host_connect.o d3des.o rfblib.o async_io.o \
	host_io.o client_io.o encode.o rect.o translate.o

SRCS =	main.c logging.c host_connect.c d3des.c rfblib.c async_io.c \
	host_io.c client_io.c encode.c rect.c translate.c

CC = gcc
MAKEDEPEND = gccmakedep

default: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) *core* ./*~ ./*.bak $(PROG)

depend: $(SRCS)
	$(MAKEDEPEND) -I. $(SRCS)

.c.o:
	$(CC) $(CFLAGS) $(CONFFLAGS) -c $<


# DO NOT DELETE
main.o: main.c rfblib.h async_io.h logging.h reflector.h rect.h \
  host_connect.h
logging.o: logging.c logging.h
host_connect.o: host_connect.c rfblib.h reflector.h logging.h async_io.h \
  host_io.h rect.h translate.h client_io.h host_connect.h
d3des.o: d3des.c d3des.h
rfblib.o: rfblib.c rfblib.h d3des.h
async_io.o: async_io.c async_io.h
host_io.o: host_io.c rfblib.h reflector.h async_io.h logging.h rect.h \
  translate.h client_io.h host_io.h
client_io.o: client_io.c rfblib.h logging.h async_io.h reflector.h rect.h \
  host_io.h translate.h client_io.h encode.h
encode.o: encode.c rfblib.h reflector.h async_io.h rect.h translate.h \
  client_io.h encode.h
rect.o: rect.c rfblib.h rect.h
translate.o: translate.c rfblib.h reflector.h rect.h async_io.h translate.h \
  client_io.h

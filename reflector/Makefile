IFLAGS =	-I.

# Production
#CFLAGS =	-O2 $(IFLAGS)
# Debug (strict)
CFLAGS =	-g -pedantic $(IFLAGS)
# Debug (profiling)
#CFLAGS =	-g -pg $(IFLAGS)
# Debug (normal)
#CFLAGS =	-g $(IFLAGS)

LDFLAGS =

PROG = 	vncreflector

OBJS = 	main.o logging.o host_connect.o d3des.o rfblib.o async_io.o \
	host_io.o client_io.o encode.o rect.o

SRCS =	main.c logging.c host_connect.c d3des.c rfblib.c async_io.c \
	host_io.c client_io.c encode.c rect.c

CC = gcc
MAKEDEPEND = gccmakedep

default: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) *core* ./*~ ./*.bak $(PROG)

depend: $(SRCS)
	$(MAKEDEPEND) -Y -I. $(SRCS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<


# DO NOT DELETE
main.o: main.c rfblib.h async_io.h logging.h reflector.h rect.h \
  host_connect.h host_io.h client_io.h
logging.o: logging.c logging.h
host_connect.o: host_connect.c rfblib.h logging.h host_connect.h d3des.h
d3des.o: d3des.c d3des.h
rfblib.o: rfblib.c rfblib.h
async_io.o: async_io.c async_io.h
host_io.o: host_io.c rfblib.h reflector.h async_io.h logging.h rect.h \
  client_io.h host_io.h
client_io.o: client_io.c rfblib.h logging.h async_io.h reflector.h rect.h \
  encode.h host_io.h client_io.h d3des.h
encode.o: encode.c rfblib.h reflector.h async_io.h rect.h client_io.h encode.h
rect.o: rect.c rfblib.h rect.h

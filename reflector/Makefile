IFLAGS =	-I.

# Production
#CFLAGS =	-O2 $(IFLAGS)
# Debug (strict)
#CFLAGS =	-g -pedantic $(IFLAGS)
# Debug (normal)
CFLAGS =	-g $(IFLAGS)

LDFLAGS =

PROG = 	vncreflector

OBJS = 	main.o logging.o host_connect.o d3des.o rfblib.o async_io.o \
	host_io.o client_io.o

SRCS =	main.c logging.c host_connect.c d3des.c rfblib.c async_io.c \
	host_io.c client_io.c

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


# DO NOT DELETE THIS LINE -- make depend depends on it.


async_io.o: async_io.h
client_io.o: async_io.h client_io.h d3des.h logging.h reflector.h rfblib.h
d3des.o: d3des.h
host_connect.o: d3des.h host_connect.h logging.h rfblib.h
host_io.o: async_io.h host_io.h logging.h reflector.h rfblib.h
logging.o: logging.h
main.o: async_io.h client_io.h host_connect.h host_io.h logging.h reflector.h
main.o: rfblib.h
rfblib.o: rfblib.h

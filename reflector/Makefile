IFLAGS =	-I.

# Production
#CFLAGS =	-O2 $(IFLAGS)
# Debug
CFLAGS =	-g -pedantic $(IFLAGS)

LDFLAGS =

PROG = 	vncreflector

OBJS = 	main.o logging.o hostconnect.o d3des.o rfblib.o async_io.o \
	host_io.o

SRCS =	main.c logging.c hostconnect.c d3des.c rfblib.c async_io.c \
	host_io.c

CC = gcc

default: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LDFLAGS)

clean: 
	rm -f $(OBJS) *core* ./*~ ./*.bak $(PROG)

depend: $(SRCS)
	makedepend -Y -I. $(SRCS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<


# DO NOT DELETE THIS LINE -- make depend depends on it.


async_io.o: async_io.h
d3des.o: d3des.h
hostconnect.o: d3des.h hostconnect.h logging.h rfblib.h
host_io.o: async_io.h host_io.h logging.h reflector.h rfblib.h
logging.o: logging.h
main.o: async_io.h hostconnect.h host_io.h logging.h reflector.h rfblib.h
rfblib.o: rfblib.h

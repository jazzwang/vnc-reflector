IFLAGS =	-I.

# Production
#CFLAGS =	-O2 $(IFLAGS)
# Debug
CFLAGS =	-g -pedantic $(IFLAGS)

LDFLAGS =

PROG = 	vncreflector

OBJS = 	main.o logging.o hostconnect.o d3des.o

SRCS =	main.c logging.c hostconnect.c d3des.c

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


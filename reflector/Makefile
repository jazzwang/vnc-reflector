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


d3des.o: d3des.h
hostconnect.o: hostconnect.h logging.h rfblib.h /usr/include/asm/errno.h
hostconnect.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
hostconnect.o: /usr/include/bits/byteswap.h /usr/include/bits/endian.h
hostconnect.o: /usr/include/bits/errno.h /usr/include/bits/in.h
hostconnect.o: /usr/include/bits/local_lim.h /usr/include/bits/netdb.h
hostconnect.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
hostconnect.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
hostconnect.o: /usr/include/bits/select.h /usr/include/bits/siginfo.h
hostconnect.o: /usr/include/bits/sigset.h /usr/include/bits/sockaddr.h
hostconnect.o: /usr/include/bits/socket.h /usr/include/bits/stdio_lim.h
hostconnect.o: /usr/include/bits/time.h /usr/include/bits/types.h
hostconnect.o: /usr/include/bits/uio.h /usr/include/bits/wchar.h
hostconnect.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
hostconnect.o: /usr/include/endian.h /usr/include/errno.h
hostconnect.o: /usr/include/features.h /usr/include/_G_config.h
hostconnect.o: /usr/include/gconv.h /usr/include/gnu/stubs.h
hostconnect.o: /usr/include/libio.h /usr/include/limits.h
hostconnect.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
hostconnect.o: /usr/include/netdb.h /usr/include/netinet/in.h
hostconnect.o: /usr/include/rpc/netdb.h /usr/include/stdint.h
hostconnect.o: /usr/include/stdio.h /usr/include/string.h
hostconnect.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
hostconnect.o: /usr/include/sys/socket.h /usr/include/sys/sysmacros.h
hostconnect.o: /usr/include/sys/types.h /usr/include/sys/uio.h
hostconnect.o: /usr/include/time.h /usr/include/wchar.h /usr/include/xlocale.h
hostconnect.o: /usr/lib/gcc-lib/i386-asplinux-linux/2.96/include/limits.h
hostconnect.o: /usr/lib/gcc-lib/i386-asplinux-linux/2.96/include/stdarg.h
hostconnect.o: /usr/lib/gcc-lib/i386-asplinux-linux/2.96/include/stddef.h
hostconnect.o: /usr/lib/gcc-lib/i386-asplinux-linux/2.96/include/syslimits.h
logging.o: logging.h /usr/include/alloca.h /usr/include/bits/endian.h
logging.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
logging.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
logging.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
logging.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
logging.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
logging.o: /usr/include/endian.h /usr/include/features.h
logging.o: /usr/include/_G_config.h /usr/include/gconv.h
logging.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
logging.o: /usr/include/stdlib.h /usr/include/string.h
logging.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
logging.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
logging.o: /usr/include/time.h /usr/include/wchar.h /usr/include/xlocale.h
logging.o: /usr/lib/gcc-lib/i386-asplinux-linux/2.96/include/stdarg.h
logging.o: /usr/lib/gcc-lib/i386-asplinux-linux/2.96/include/stddef.h
main.o: hostconnect.h logging.h reflector.h /usr/include/alloca.h
main.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
main.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
main.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
main.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
main.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
main.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
main.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
main.o: /usr/include/bits/wordsize.h /usr/include/endian.h
main.o: /usr/include/features.h /usr/include/_G_config.h /usr/include/gconv.h
main.o: /usr/include/getopt.h /usr/include/gnu/stubs.h /usr/include/libio.h
main.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
main.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
main.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
main.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
main.o: /usr/include/xlocale.h
main.o: /usr/lib/gcc-lib/i386-asplinux-linux/2.96/include/stdarg.h
main.o: /usr/lib/gcc-lib/i386-asplinux-linux/2.96/include/stddef.h

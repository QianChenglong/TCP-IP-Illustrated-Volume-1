# Change the following as required:
CC	= gcc
CFLAGS	=

# Following line for SVR4, Solaris 2.x
LIBS	= /usr/ucblib/libucb.a -lsocket -lnsl

# Following line for 4.4BSD, BSD/386, SunOS 4.x, AIX 3.2.2
#LIBS	=

PROGS =	traceroute.lsrr traceroute.pmtu
OBJS = traceroute.lsrr.o traceroute.pmtu.o

all:	${PROGS}

traceroute.pmtu:	traceroute.pmtu.o
			${CC} ${CFLAGS} -o $@ traceroute.pmtu.o ${LIBS}

traceroute.lsrr:	traceroute.lsrr.o
			${CC} ${CFLAGS} -o $@ traceroute.lsrr.o ${LIBS}

clean:
	rm -f ${PROGS} core core.* *.o temp.* *.out typescript*

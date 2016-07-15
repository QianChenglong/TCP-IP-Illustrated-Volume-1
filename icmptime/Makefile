# Change the following as required:
CC	= gcc
CFLAGS	=

# Following line for SVR4, Solaris 2.x
LIBS	= /usr/ucblib/libucb.a -lsocket -lnsl

# Following line for 4.4BSD, BSD/386, SunOS 4.x, AIX 3.2.2
#LIBS	=

PROGS =	icmptime
OBJS = icmptime.o

all:	${PROGS}

icmptime:	${OBJS}
		${CC} ${CFLAGS} -o $@ ${OBJS} ${LIBS}

clean:
		rm -f ${PROGS} core core.* *.o temp.* *.out typescript*

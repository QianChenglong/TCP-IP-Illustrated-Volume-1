/*
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The author makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/uio.h>
#include	<sys/time.h>		/* struct timeval for select() */
#ifdef	_AIX
#include	<sys/select.h>
#endif
#include	<netinet/in.h>
#ifdef	__bsdi__
#include	<machine/endian.h>	/* required before tcp.h, for BYTE_ORDER */
#endif
#include	<netinet/tcp.h>		/* TCP_NODELAY */
#include	<netdb.h>			/* getservbyname(), gethostbyname() */
#include	<arpa/inet.h>		/* inet_addr() */
#include	<errno.h>
#include	<signal.h>
#include	"ourhdr.h"

				/* declare global variables */
extern int		bindport;
extern int		broadcast;
extern int		cbreak;
extern int		client;
extern int		crlf;
extern int		debug;
extern int		dofork;
extern char		foreignip[];
extern int		foreignport;
extern int		halfclose;
extern int		keepalive;
extern long		linger;
extern int		listenq;
extern int		nodelay;
extern int		nbuf;
extern int		pauseclose;
extern int		pauseinit;
extern int		pauselisten;
extern int		pauserw;
extern int		reuseaddr;
extern int		readlen;
extern int		writelen;
extern int		recvdstaddr;
extern int		rcvbuflen;
extern int		sndbuflen;
extern char	   *rbuf;
extern char	   *wbuf;
extern int		server;
extern int		sourcesink;
extern int		udp;
extern int		urgwrite;
extern int		verbose;

#ifndef	INADDR_NONE
#define	INADDR_NONE	0xffffffff	/* should be in <netinet/in.h> */
#endif

/* Earlier versions of gcc under SunOS 4.x have problems passing arguments
   that are structs (as opposed to pointers to structs).  This shows up
   with inet_ntoa, whose argument is a "struct in_addr". */

#if	defined(sun) && defined(__GNUC__) && defined(GCC_STRUCT_PROBLEM)
#define	INET_NTOA(foo)	inet_ntoa(&foo)
#else
#define	INET_NTOA(foo)	inet_ntoa(foo)
#endif

				/* function prototypes */
void	bcopy(const void *, void *, size_t);
void	bzero(void *, size_t);

void	buffers(int);
int		cliopen(char *host, char *port);
int		crlf_add(char *dst, int dstsize, const char *src, int lenin);
int		crlf_strip(char *dst, int dstsize, const char *src, int lenin);
void	loop(int);
int		servopen(char *host, char *port);
void	sink(int sockfd);
void	sockopts(int sockfd, int doall);

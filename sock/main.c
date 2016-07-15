/*
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The author makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

#include	"sock.h"

			/* define global variables */
char	*host;
char	*port;

int		bindport;			/* 0 or TCP or UDP port number to bind */
int		broadcast;			/* SO_BROADCAST */
int		cbreak;				/* set terminal to cbreak mode */
int		client = 1;			/* acting as client is the default */
int		crlf;				/* convert newline to CR/LF & vice versa */
int		debug;				/* SO_DEBUG */
int		dofork;				/* concurrent server, do a fork() */
char	foreignip[32];		/* foreign IP address, dotted-decimal string */
int		foreignport;		/* foreign port number */
int		halfclose;			/* TCP half close option */
int		keepalive;			/* SO_KEEPALIVE */
long	linger = -1;		/* 0 or positive turns on option */
int		listenq = 5;		/* listen queue for TCP Server */
int		nodelay;			/* TCP_NODELAY (Nagle algorithm) */
int		nbuf = 1024;		/* number of buffers to write (sink mode) */
int		pauseclose;			/* seconds to sleep after recv FIN, before close */
int		pauseinit;			/* seconds to sleep before first read */
int		pauselisten;		/* seconds to sleep after listen() */
int		pauserw;			/* seconds to sleep before each read or write */
int		reuseaddr;			/* SO_REUSEADDR */
int		readlen = 1024;		/* default read length for socket */
int		writelen = 1024;	/* default write length for socket */
int		recvdstaddr;		/* IP_RECVDSTADDR option */
int		rcvbuflen;			/* size for SO_RCVBUF */
int		sndbuflen;			/* size for SO_SNDBUF */
char   *rbuf;				/* pointer that is malloc'ed */
char   *wbuf;				/* pointer that is malloc'ed */
int		server;				/* to act as server requires -s option */
int		sourcesink;			/* source/sink mode */
int		udp;				/* use UDP instead of TCP */
int		urgwrite;			/* write urgent byte after this write */
int		verbose;

static void	usage(const char *);

int
main(int argc, char *argv[])
{
	int		c, fd;
	char	*ptr;

	if (argc < 2)
		usage("");

	opterr = 0;		/* don't want getopt() writing to stderr */
	while ( (c = getopt(argc, argv, "b:cf:hin:p:q:r:suvw:ABCDEFKL:NO:P:Q:R:S:U:")) != EOF) {
		switch (c) {
		case 'b':
			bindport = atoi(optarg);
			break;

		case 'c':			/* convert newline to CR/LF & vice versa */
			crlf = 1;
			break;

		case 'f':			/* foreign IP address and port#: a.b.c.d.p */
			if ( (ptr = strrchr(optarg, '.')) == NULL)
				usage("invalid -f option");

			*ptr++ = 0;					/* null replaces final period */
			foreignport = atoi(ptr);	/* port number */
			strcpy(foreignip, optarg);	/* save dotted-decimal IP */
			break;

		case 'h':			/* TCP half-close option */
			halfclose = 1;
			break;

		case 'i':			/* source/sink option */
			sourcesink = 1;
			break;

		case 'n':			/* number of buffers to write */
			nbuf = atol(optarg);
			break;

		case 'p':			/* pause before each read or write */
			pauserw = atoi(optarg);
			break;

		case 'q':			/* listen queue for TCP server */
			listenq = atoi(optarg);
			break;

		case 'r':			/* read() length */
			readlen = atoi(optarg);
			break;

		case 's':			/* server */
			server = 1;
			client = 0;
			break;

		case 'u':			/* use UDP instead of TCP */
			udp = 1;
			break;

		case 'v':			/* output what's going on */
			verbose = 1;
			break;

		case 'w':			/* write() length */
			writelen = atoi(optarg);
			break;

		case 'A':			/* SO_REUSEADDR socket option */
			reuseaddr = 1;
			break;

		case 'B':			/* SO_BROADCAST socket option */
			broadcast = 1;
			break;

		case 'C':			/* set standard input to cbreak mode */
			cbreak = 1;
			break;

		case 'D':			/* SO_DEBUG socket option */
			debug = 1;
			break;

		case 'E':			/* IP_RECVDSTADDR socket option */
			recvdstaddr = 1;
			break;

		case 'F':			/* concurrent server, do a fork() */
			dofork = 1;
			break;

		case 'K':			/* SO_KEEPALIVE socket option */
			keepalive = 1;
			break;

		case 'L':			/* SO_LINGER socket option */
			linger = atol(optarg);
			break;

		case 'N':			/* SO_NODELAY socket option */
			nodelay = 1;
			break;

		case 'O':			/* pause before listen(), before first accept() */
			pauselisten = atoi(optarg);
			break;

		case 'P':			/* pause before first read() */
			pauseinit = atoi(optarg);
			break;

		case 'Q':			/* pause after receiving FIN, but before close() */
			pauseclose = atoi(optarg);
			break;

		case 'R':			/* SO_RCVBUF socket option */
			rcvbuflen = atoi(optarg);
			break;

		case 'S':			/* SO_SNDBUF socket option */
			sndbuflen = atoi(optarg);
			break;

		case 'U':			/* when to write urgent byte */
			urgwrite = atoi(optarg);
			break;

		case '?':
			usage("unrecognized option");
		}
	}

		/* check for options that don't make sense */
	if (udp && halfclose)
		usage("can't specify -h and -u");
	if (udp && debug)
		usage("can't specify -D and -u");
	if (udp && linger >= 0)
		usage("can't specify -L and -u");
	if (udp && nodelay)
		usage("can't specify -N and -u");
	if (udp == 0 && broadcast)
		usage("can't specify -B with TCP");
	if (udp == 0 && foreignip[0] != 0)
		usage("can't specify -f with TCP");

	if (client) {
		if (optind != argc-2)
			usage("missing <hostname> and/or <port>");
		host = argv[optind];
		port = argv[optind+1];

	} else {
			/* If server specifies host and port, then local address is
			   bound to the "host" argument, instead of being wildcarded. */
		if (optind == argc-2) {
			host = argv[optind];
			port = argv[optind+1];
		} else if (optind == argc-1) {
			host = NULL;
			port = argv[optind];
		} else
			usage("missing <port>");
	}

	if (client)
		fd = cliopen(host, port);
	else
		fd = servopen(host, port);

	if (sourcesink)
		sink(fd);			/* ignore stdin/stdout */
	else
		loop(fd);			/* copy stdin/stdout to/from socket */

	exit(0);
}

static void
usage(const char *msg)
{
	err_msg(
"usage: sock [ options ] <host> <port>              (for client; default)\n"
"       sock [ options ] -s [ <IPaddr> ] <port>     (for server)\n"
"       sock [ options ] -i <host> <port>           (for \"source\" client)\n"
"       sock [ options ] -i -s [ <IPaddr> ] <port>  (for \"sink\" server)\n"
"options: -b n	bind n as client's local port number\n"
"         -c    convert newline to CR/LF & vice versa\n"
"         -f a.b.c.d.p  foreign IP address = a.b.c.d, foreign port# = p\n"
"         -h    issue TCP half close on standard input EOF\n"
"         -i    \"source\" data to socket, \"sink\" data from socket (w/-s)\n"
"         -n n  #buffers to write for \"source\" client (default 1024)\n"
"         -p n  #seconds to pause before each read or write (source/sink)\n"
"         -q n  size of listen queue for TCP server (default 5)\n"
"         -r n  #bytes per read() for \"sink\" server (default 1024)\n"
"         -s    operate as server instead of client\n"
"         -u    use UDP instead of TCP\n"
"         -v    verbose\n"
"         -w n  #bytes per write() for \"source\" client (default 1024)\n"
"         -A    SO_REUSEADDR option\n"
"         -B    SO_BROADCAST option\n"
"         -C    set terminal to cbreak mode\n"
"         -D    SO_DEBUG option\n"
"         -E    IP_RECVDSTADDR option\n"
"         -F    fork after connection accepted (TCP concurrent server)\n"
"         -K    SO_KEEPALIVE option\n"
"         -L n  SO_LINGER option, n = linger time\n"
"         -N    TCP_NODELAY option\n"
"         -O n  #seconds to pause after listen, but before first accept\n"
"         -P n  #seconds to pause before first read or write (source/sink)\n"
"         -Q n  #seconds to pause after receiving FIN, but before close\n"
"         -R n  SO_RCVBUF option\n"
"         -S n  SO_SNDBUF option\n"
"         -U n  enter urgent mode after write number n (source only)"
);

	if (msg[0] != 0)
		err_quit("%s", msg);
	exit(1);
}

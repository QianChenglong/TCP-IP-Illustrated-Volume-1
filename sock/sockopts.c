/*
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The author makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

#include	"sock.h"

void
sockopts(int sockfd, int doall)
{
    int 			option, optlen;
	struct linger	ling;

	/* "doall" is 0 for a server's listening socket (i.e., before
	   accept() has returned.)  Some socket options such as SO_KEEPALIVE
	   don't make sense at this point, while others like SO_DEBUG do. */

    if (debug) {
        option = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_DEBUG,
								(char *) &option, sizeof(option)) < 0)
            err_sys("SO_DEBUG setsockopt error");

        option = 0;
		optlen = sizeof(option);
        if (getsockopt(sockfd, SOL_SOCKET, SO_DEBUG,
								(char *) &option, &optlen) < 0)
            err_sys("SO_DEBUG getsockopt error");
		if (option == 0)
			err_quit("SO_DEBUG not set (%d)", option);

		if (verbose)
			fprintf(stderr, "SO_DEBUG set\n");
    }

    if (broadcast && doall) {
        option = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
								(char *) &option, sizeof(option)) < 0)
            err_sys("SO_BROADCAST setsockopt error");

        option = 0;
		optlen = sizeof(option);
        if (getsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
								(char *) &option, &optlen) < 0)
            err_sys("SO_BROADCAST getsockopt error");
		if (option == 0)
			err_quit("SO_BROADCAST not set (%d)", option);

		if (verbose)
			fprintf(stderr, "SO_BROADCAST set\n");
    }

    if (keepalive && doall && udp == 0) {
        option = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
								(char *) &option, sizeof(option)) < 0)
            err_sys("SO_KEEPALIVE setsockopt error");

        option = 0;
		optlen = sizeof(option);
        if (getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
								(char *) &option, &optlen) < 0)
            err_sys("SO_KEEPALIVE getsockopt error");
		if (option == 0)
			err_quit("SO_KEEPALIVE not set (%d)", option);

		if (verbose)
			fprintf(stderr, "SO_KEEPALIVE set\n");
    }

    if (nodelay && doall && udp == 0) {
        option = 1;
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
								(char *) &option, sizeof(option)) < 0)
            err_sys("TCP_NODELAY setsockopt error");

        option = 0;
		optlen = sizeof(option);
        if (getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
								(char *) &option, &optlen) < 0)
            err_sys("TCP_NODELAY getsockopt error");
		if (option == 0)
			err_quit("TCP_NODELAY not set (%d)", option);

		if (verbose)
			fprintf(stderr, "TCP_NODELAY set\n");
    }

    if (doall && verbose && udp == 0) {	/* just print MSS if verbose */
        option = 0;
		optlen = sizeof(option);
        if (getsockopt(sockfd, IPPROTO_TCP, TCP_MAXSEG,
								(char *) &option, &optlen) < 0)
            err_sys("TCP_MAXSEG getsockopt error");

		fprintf(stderr, "TCP_MAXSEG = %d\n", option);
    }

    if (linger >= 0 && doall && udp == 0) {
        ling.l_onoff = 1;
        ling.l_linger = linger;		/* 0 for abortive disconnect */
        if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER,
								(char *) &ling, sizeof(ling)) < 0)
            err_sys("SO_LINGER setsockopt error");

		ling.l_onoff = 0;
		ling.l_linger = -1;
        optlen = sizeof(struct linger);
        if (getsockopt(sockfd, SOL_SOCKET, SO_LINGER,
								(char *) &ling, &optlen) < 0)
            err_sys("SO_LINGER getsockopt error");
		if (ling.l_onoff == 0 || ling.l_linger != linger)
			err_quit("SO_LINGER not set (%d, %d)", ling.l_onoff, ling.l_linger);

        if (verbose)
            fprintf(stderr, "linger %s, time = %d\n",
                            ling.l_onoff ? "on" : "off", ling.l_linger);
    }

    if (recvdstaddr && udp) {
#ifdef	IP_RECVDSTADDR
        option = 1;
        if (setsockopt(sockfd, IPPROTO_IP, IP_RECVDSTADDR,
								(char *) &option, sizeof(option)) < 0)
            err_sys("IP_RECVDSTADDR setsockopt error");

        option = 0;
		optlen = sizeof(option);
        if (getsockopt(sockfd, IPPROTO_IP, IP_RECVDSTADDR,
								(char *) &option, &optlen) < 0)
            err_sys("IP_RECVDSTADDR getsockopt error");
		if (option == 0)
			err_quit("IP_RECVDSTADDR not set (%d)", option);

		if (verbose)
			fprintf(stderr, "IP_RECVDSTADDR set\n");
#else
		fprintf(stderr, "warning: IP_RECVDSTADDR not supported by host\n");
#endif
    }
}

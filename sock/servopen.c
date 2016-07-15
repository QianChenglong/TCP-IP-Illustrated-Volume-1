/*
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The author makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

#include	"sock.h"

int
servopen(char *host, char *port)
{
	int					fd, newfd, i, on, pid;
	char				*protocol;
	unsigned long		inaddr;
	struct sockaddr_in	cli_addr, serv_addr;
	struct servent		*sp;

	protocol = udp ? "udp" : "tcp";

		/* Initialize the socket address structure */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family      = AF_INET;

		/* Caller normally wildcards the local Internet address, meaning
		   a connection will be accepted on any connected interface.
		   We only allow an IP address for the "host", not a name. */
	if (host == NULL)
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);		/* wildcard */
	else {
		if ( (inaddr = inet_addr(host)) == INADDR_NONE)
			err_quit("invalid host name for server: %s", host);
		serv_addr.sin_addr.s_addr = inaddr;
	}

		/* See if "port" is a service name or number */
	if ( (i = atoi(port)) == 0) {
		if ( (sp = getservbyname(port, protocol)) == NULL)
			err_ret("getservbyname() error for: %s/%s", port, protocol);

		serv_addr.sin_port = sp->s_port;
	} else
		serv_addr.sin_port = htons(i);

	if ( (fd = socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0)) < 0)
		err_sys("socket() error");

	if (reuseaddr) {
		on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
										(char *) &on, sizeof (on)) < 0)
			err_sys("setsockopt of SO_REUSEADDR error");
	}

		/* Bind our well-known port so the client can connect to us. */
	if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		err_sys("can't bind local address");

	if (udp) {
		buffers(fd);

		if (foreignip[0] != 0) {	/* connect to foreignip/port# */
			bzero((char *) &cli_addr, sizeof(cli_addr));
			cli_addr.sin_family      = AF_INET;
			cli_addr.sin_addr.s_addr = inet_addr(foreignip);
			cli_addr.sin_port        = htons(foreignport);
				/* connect() for datagram socket doesn't appear to allow
				   wildcarding of either IP address or port number */

			if (connect(fd, (struct sockaddr *) &cli_addr, sizeof(cli_addr))
																		  < 0)
				err_sys("connect() error");
			
		}

		sockopts(fd, 1);

		return(fd);		/* nothing else to do */
	}

	buffers(fd);		/* may set receive buffer size; must do here to get
						   correct window advertised on SYN */
	sockopts(fd, 0);	/* only set some socket options for fd */

	listen(fd, listenq);

	if (pauselisten)
		sleep(pauselisten);		/* lets connection queue build up */

	if (dofork)
		TELL_WAIT();			/* initialize synchronization primitives */

	for ( ; ; ) {
		i = sizeof(cli_addr);
		if ( (newfd = accept(fd, (struct sockaddr *) &cli_addr, &i)) < 0)
			err_sys("accept() error");

		if (dofork) {
			if ( (pid = fork()) < 0)
				err_sys("fork error");

			if (pid > 0) {
				close(newfd);	/* parent closes connected socket */
				WAIT_CHILD();	/* wait for child to output to terminal */
				continue;		/* and back to for(;;) for another accept() */
			} else {
				close(fd);		/* child closes listening socket */
			}
		}

			/* child (or iterative server) continues here */
		if (verbose) {
				/* Call getsockname() to find local address bound to socket:
				   local internet address is now determined (if multihomed). */
			i = sizeof(serv_addr);
			if (getsockname(newfd, (struct sockaddr *) &serv_addr, &i) < 0)
				err_sys("getsockname() error");

						/* Can't do one fprintf() since inet_ntoa() stores
						   the result in a static location. */
			fprintf(stderr, "connection on %s.%d ",
					INET_NTOA(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
			fprintf(stderr, "from %s.%d\n",
					INET_NTOA(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		}

		buffers(newfd);		/* setsockopt() again, in case it didn't propagate
							   from listening socket to connected socket */
		sockopts(newfd, 1);	/* can set all socket options for this socket */

		if (dofork)
			TELL_PARENT(getppid());	/* tell parent we're done with terminal */

		return(newfd);
	}
}

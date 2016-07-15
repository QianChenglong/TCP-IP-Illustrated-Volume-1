/*
 * Issue an ICMP address mask request and print the reply.
 *
 * This program originated from the public domain ping program written
 * by Mike Muuss.
 *
 * You must be superuser to run this program (or it must be suid to root)
 * since it requires a raw socket.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/signal.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#define	DEFDATALEN	(12)	/* default data length */
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPACKET	(65536 - 60 - 8)/* max packet size */

struct sockaddr	whereto;	/* who to send request to */
int		datalen = DEFDATALEN;
int		s;
u_char		outpack[MAXPACKET];
char		*hostname;
u_long		inet_addr();
char		*inet_ntoa();
void		sig_alrm(int);
int		response = 0;

main(argc, argv)
int	argc;
char	**argv;
{
	int			i, ch, fdmask, hold, packlen, preload;
	extern int		errno;
	struct hostent		*hp;
	struct sockaddr_in	*to;
	struct protoent		*proto;
	u_char			*packet;
	char			*target, hnamebuf[MAXHOSTNAMELEN], *malloc();

	if (argc != 2)
		exit(1);

	target = argv[1];

	bzero((char *)&whereto, sizeof(struct sockaddr));
	to = (struct sockaddr_in *)&whereto;
	to->sin_family = AF_INET;

		/* try to convert as dotted decimal address,
		   else if that fails assume it's a hostname */
	to->sin_addr.s_addr = inet_addr(target);
	if (to->sin_addr.s_addr != (u_int)-1)
		hostname = target;
	else {
		hp = gethostbyname(target);
		if (!hp) {
			fprintf(stderr, "unknown host %s\n", target);
			exit(1);
		}
		to->sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (caddr_t)&to->sin_addr, hp->h_length);
		strncpy(hnamebuf, hp->h_name, sizeof(hnamebuf) - 1);
		hostname = hnamebuf;
	}

	packlen = datalen + MAXIPLEN + MAXICMPLEN;
	if ( (packet = (u_char *)malloc((u_int)packlen)) == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}

	if ( (proto = getprotobyname("icmp")) == NULL) {
		fprintf(stderr, "unknown protocol icmp\n");
		exit(1);
	}

	if ( (s = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
		perror("socket");	/* probably not running as superuser */
		exit(1);
	}

	/*
	 * We send one request, then wait 5 seconds, printing any
	 * replies that come back.  This lets us send a request to
	 * a broadcast address and print multiple replies.
	 */

	signal(SIGALRM, sig_alrm);
	alarm(5);	/* 5 second time limit */

	sender();	/* send the request */

	for (;;) {
		struct sockaddr_in	from;
		int			cc, fromlen;

		fromlen = sizeof(from);
		if ( (cc = recvfrom(s, (char *)packet, packlen, 0,
		    		    (struct sockaddr *)&from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
			perror("recvfrom error");
			continue;
		}
		procpack((char *)packet, cc, &from);
	}
}

/*
 * Send the ICMP address mask request.
 */

sender()
{
	int		i, cc;
	struct icmp	*icp;

	icp = (struct icmp *)outpack;
	icp->icmp_type = ICMP_MASKREQ;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;	/* compute checksum below */
	icp->icmp_seq = 12345;	/* seq and id must be reflected */
	icp->icmp_id = getpid();

	icp->icmp_mask = 0;

	cc = ICMP_MASKLEN;	/* 12 = 8 bytes of header, 4 bytes of mask */

		/* compute ICMP checksum here */
	icp->icmp_cksum = in_cksum((u_short *)icp, cc);

	i = sendto(s, (char *)outpack, cc, 0, &whereto,
	    				sizeof(struct sockaddr));
	if (i < 0 || i != cc)  {
		if (i < 0)
			perror("sendto error");
		printf("wrote %s %d chars, ret=%d\n", hostname, cc, i);
	}
}

/*
 * Process a received ICMP message.
 */

procpack(buf, cc, from)
char			*buf;
int			cc;
struct sockaddr_in	*from;
{
	int		i, hlen;
	struct icmp	*icp;
	struct ip	*ip;
	struct timeval	tvdelta;

		/* Check the IP header */
	ip = (struct ip *)buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		fprintf(stderr, "packet too short (%d bytes) from %s\n", cc,
			  inet_ntoa(*(struct in_addr *)&from->sin_addr.s_addr));
		return;
	}

		/* Now the ICMP part */
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);

		/* With a raw ICMP socket we get all ICMP packets that
		   come into the kernel. */

	if (icp->icmp_type == ICMP_MASKREPLY) {
		if (cc != ICMP_MASKLEN)
			printf("cc = %d, expected cc = 12\n", cc);
		if (icp->icmp_seq != 12345)
			printf("received sequence # %d\n", icp->icmp_seq);
		if (icp->icmp_id != getpid())
			printf("received id %d\n", icp->icmp_id);

		printf("received mask = %08x, from %s\n",
			ntohl(icp->icmp_mask),
			inet_ntoa(*(struct in_addr *) &from->sin_addr.s_addr));
		response++;
	}
	/* We ignore all other types of ICMP messages */
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
in_cksum(addr, len)
	u_short *addr;
	int len;
{
	register int nleft = len;
	register u_short *w = addr;
	register int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}

void
sig_alrm(int signo)
{
	if (response == 0) {
		printf("timeout\n");
		exit(1);
	}
	exit(0);	/* we got one or more responses */
}

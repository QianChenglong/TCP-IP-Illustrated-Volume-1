/*
 * Fetch the current time from another host using the ICMP timestamp
 * request/reply.
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

struct sockaddr	whereto;	/* who to ping */
int		datalen = DEFDATALEN;
int		s;
u_char		outpack[MAXPACKET];
char		*hostname;

struct timeval	tvorig, tvrecv;	/* originate & receive timeval structs */
long		tsorig, tsrecv;	/* originate & receive timestamps */
			/* timestamp = #millisec since midnight UTC */
			/* both host byte ordered */
long		tsdiff;		/* adjustment must also be signed */

u_long		inet_addr();
char		*inet_ntoa();
void		sig_alrm(int);

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
		perror("socket");
		exit(1);
	}

	/*
	 * We send one request and wait up to 30 seconds for a reply.
	 */

	signal(SIGALRM, sig_alrm);
	alarm(30);	/* 30 second time limit */

	sender();	/* send the request */

	for (;;) {
		struct sockaddr_in from;
		register int cc;
		int fromlen;

		fromlen = sizeof(from);
		if ( (cc = recvfrom(s, (char *)packet, packlen, 0,
		    (struct sockaddr *)&from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
			perror("recvfrom error");
			continue;
		}

			/* process received ICMP message */
		if (procpack((char *)packet, cc, &from) == 0)
			exit(0);	/* terminate if reply received */
	}
}

/*
 * Send the request.
 */

sender()
{
	int		i, cc;
	struct icmp	*icp;

	icp = (struct icmp *)outpack;
	icp->icmp_type = ICMP_TSTAMP;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;	/* compute checksum below */
	icp->icmp_seq = 12345;	/* seq and id must be reflected */
	icp->icmp_id = getpid();

		/* fill in originate timestamp: have to convert tv_sec
		   from seconds since the Epoch to milliseconds since
		   midnight, then add in microseconds */

	gettimeofday(&tvorig, (struct timezone *)NULL);
	tsorig = (tvorig.tv_sec % (24*60*60)) * 1000 + tvorig.tv_usec / 1000;
	icp->icmp_otime = htonl(tsorig);
	icp->icmp_rtime = 0;		/* filled in by receiver */
	icp->icmp_ttime = 0;		/* filled in by receiver */

	cc = datalen + 8;	/* 8 bytes of header, 12 bytes of data */

					/* compute ICMP checksum */
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
 * Process a received ICMP message.  Since we receive *all* ICMP messages
 * that the kernel receives, we may receive other than the timestamp
 * reply we're waiting for.
 */

int
procpack(buf, cc, from)
char			*buf;
int			cc;
struct sockaddr_in	*from;
{
	int		i, hlen;
	struct icmp	*icp;
	struct ip	*ip;
	struct timeval	tvdelta;

	/* We could call gettimeofday() again to measure the RTT and use
	 * that in computing the offset, but some (most?) systems only
	 * have 100 Hz resoultion for this function and the RTT on a
	 * LAN is usually less than this (i.e., it wouldn't buy us
	 * anything). */

#ifdef	notdef
	gettimeofday(&tvrecv, (struct timezone *)NULL);
#endif

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

	if (icp->icmp_type == ICMP_TSTAMPREPLY) {
		if (ntohl(icp->icmp_otime) != tsorig)
			printf("originate timestamp not echoed: sent $lu, received %lu\n",
						tsorig, ntohl(icp->icmp_otime));
		if (cc != 20)
			printf("cc = %d, expected cc = 20\n", cc);
		if (icp->icmp_seq != 12345)
			printf("received sequence # %d\n", icp->icmp_seq);
		if (icp->icmp_id != getpid())
			printf("received id %d\n", icp->icmp_id);

		/* tsorig and tsrecv are both signed longs.  The icmp_[ort]time
		 * members in the structure are unsigned, but the max value
		 * for the #millisec since midnight is (24*60*60*1000 - 1)
		 * or 85,399,999, which easily fits into a signed long.
		 * We want them signed to compute a signed difference. */

		tsrecv = ntohl(icp->icmp_rtime);
		tsdiff = tsrecv - tsorig;	/* difference in millisec */

		printf("orig = %ld, recv = %ld\n",
				ntohl(icp->icmp_otime), ntohl(icp->icmp_rtime));
		printf("adjustment = %ld ms\n", tsdiff);
		tvdelta.tv_sec  = tsdiff / 1000;	/* normally 0 */
		tvdelta.tv_usec = (tsdiff % 1000) * 1000;
		printf("correction = %ld sec, %ld usec\n",
				tvdelta.tv_sec, tvdelta.tv_usec);
		if (adjtime(&tvdelta, (struct timeval *) 0) < 0) {
			perror("adjtime error");
			exit(1);
		}
		return(0);	/* done */
	} else
		return(-1);	/* some other type of ICMP message */
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
	printf("timeout\n");
	exit(1);
}

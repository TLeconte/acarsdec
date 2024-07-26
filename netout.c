#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include <errno.h>

#include "acarsdec.h"

static int sockfd = -1;
static struct sockaddr *netOutputAddr = NULL;
static int netOutputAddrLen = 0;

int Netoutinit(char *Rawaddr)
{
	static char tmpAddr[256] = { 0 };
	char *addr;
	char *port;
	struct addrinfo hints, *servinfo, *p;
	int rv;

	if (Rawaddr)
		strncpy(tmpAddr, Rawaddr, 255);
	else if (0 == tmpAddr[0])
		return -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	addr = tmpAddr;
	port = strstr(addr, ":");
	if (port == NULL)
		port = "5555";
	else {
		*port = 0;
		port++;
	}

	if (R.verbose)
		fprintf(stderr, "Attempting to resolve '%s:%s'.\n", addr, port);

	if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "Invalid/unknown error '%s' resolving '%s:%s', retrying later.\n", gai_strerror(rv), addr, port);
		return -1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			continue;

		netOutputAddrLen = p->ai_addrlen;
		netOutputAddr = malloc(netOutputAddrLen);
		memcpy(netOutputAddr, p->ai_addr, netOutputAddrLen);

		freeaddrinfo(servinfo);
		return 0;
	}

	fprintf(stderr, "failed to resolve: '%s:%s'\n", addr, port);

	freeaddrinfo(servinfo);

	return -1;
}

static void Netwrite(const void *buf, size_t count)
{
	int res;

	if (!netOutputAddrLen) {
		/* The destination address hasn't yet been succesfully resolved. */
		if (R.verbose)
			fprintf(stderr, "retrying DNS resolution.\n");

		res = Netoutinit(NULL);
		if (!res)	/* Resolution failed, so we'll drop this message and try again next time. */
			return;
	}

	res = sendto(sockfd, buf, count, 0, netOutputAddr, netOutputAddrLen);
	if (R.verbose && res < 0)
		fprintf(stderr, "error on sendto(): %s, ignoring.\n", strerror(errno));
}

void Netoutpp(acarsmsg_t *msg)
{
	static char pkt[3600]; // max. 16 blocks * 220 characters + extra space for msg prefix
	char *pstr;
	int res;

	char *txt = strdup(msg->txt);
	for (pstr = txt; *pstr != 0; pstr++)
		if (*pstr == '\n' || *pstr == '\r')
			*pstr = ' ';

	res = snprintf(pkt, sizeof(pkt), "AC%1c %7s %1c %2s %1c %4s %6s %s",
		       msg->mode, msg->addr, msg->ack, msg->label, msg->bid ? msg->bid : '.', msg->no,
		       msg->fid, txt);

	free(txt);
	Netwrite(pkt, res);
}

void Netoutsv(acarsmsg_t *msg, char *idstation, int chn, struct timeval tv)
{
	static char pkt[3600]; // max. 16 blocks * 220 characters + extra space for msg prefix
	struct tm tmp;
	int res;

	gmtime_r(&(tv.tv_sec), &tmp);

	res = snprintf(pkt, sizeof(pkt),
		       "%8s %1d %02d/%02d/%04d %02d:%02d:%02d %1d %03d %1c %7s %1c %2s %1c %4s %6s %s",
		       idstation, chn + 1, tmp.tm_mday, tmp.tm_mon + 1,
		       tmp.tm_year + 1900, tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
		       msg->err, (int)(msg->lvl), msg->mode, msg->addr, msg->ack, msg->label,
		       msg->bid ? msg->bid : '.', msg->no, msg->fid, msg->txt);

	Netwrite(pkt, res);
}

void Netoutjson(char *jsonbuf)
{
	Netwrite(jsonbuf, strlen(jsonbuf));
}

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
static char *netOutputRawaddr = NULL;


int Netoutinit(char *Rawaddr)
{
	char *addr;
	char *port;
	struct addrinfo hints, *servinfo, *p;
	int rv;

	netOutputRawaddr = Rawaddr;

	memset(&hints, 0, sizeof hints);
	if (Rawaddr[0] == '[') {
		hints.ai_family = AF_INET6;
		addr = Rawaddr + 1;
		port = strstr(addr, "]");
		if (port == NULL) {
			fprintf(stderr, "Invalid IPV6 address\n");
			return -1;
		}
		*port = 0;
		port++;
		if (*port != ':')
			port = "5555";
		else
			port++;
	} else {
		hints.ai_family = AF_UNSPEC;
		addr = Rawaddr;
		port = strstr(addr, ":");
		if (port == NULL)
			port = "5555";
		else {
			*port = 0;
			port++;
		}
	}

	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "Invalid/unknown address %s\n", addr);
		return -1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
		continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "failed to connect\n");
		return -1;
	}

	freeaddrinfo(servinfo);

	return 0;
}

static int Netwrite(const void *buf, size_t count) {
    if (!netOutputRawaddr) {
        return -1;
    }

    int res;
    res = write(sockfd, buf, count);
    if (res == -1) {
        perror("Netwrite");
        close(sockfd);
        // retry the write if the reconnect succeeds
        if (Netoutinit(netOutputRawaddr) == 0) {
            res = write(sockfd, buf, count);
        }
    }
    return res;
}


void Netoutpp(acarsmsg_t * msg)
{
	char pkt[3600]; // max. 16 blocks * 220 characters + extra space for msg prefix
	char *pstr;
	int res;

	char *txt = strdup(msg->txt);
	for (pstr = txt; *pstr != 0; pstr++)
		if (*pstr == '\n' || *pstr == '\r')
			*pstr = ' ';

	snprintf(pkt, sizeof(pkt), "AC%1c %7s %1c %2s %1c %4s %6s %s",
		msg->mode, msg->addr, msg->ack, msg->label, msg->bid ? msg->bid : '.', msg->no,
		msg->fid, txt);

	if (netOutputRawaddr) {
		res=Netwrite(pkt, strlen(pkt));
	}
	free(txt);
}

void Netoutsv(acarsmsg_t * msg, char *idstation, int chn, struct timeval tv)
{
	char pkt[3600]; // max. 16 blocks * 220 characters + extra space for msg prefix
	struct tm tmp;
	int res;

	gmtime_r(&(tv.tv_sec), &tmp);

	snprintf(pkt, sizeof(pkt),
		"%8s %1d %02d/%02d/%04d %02d:%02d:%02d %1d %03d %1c %7s %1c %2s %1c %4s %6s %s",
		idstation, chn + 1, tmp.tm_mday, tmp.tm_mon + 1,
		tmp.tm_year + 1900, tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
		msg->err, (int)(msg->lvl), msg->mode, msg->addr, msg->ack, msg->label,
		msg->bid ? msg->bid : '.', msg->no, msg->fid, msg->txt);

	if (netOutputRawaddr) {
		res=Netwrite(pkt, strlen(pkt));
	}
}

void Netoutjson(char *jsonbuf)
{
	char pkt[3600];
	int res;

	snprintf(pkt, sizeof(pkt), "%s\n", jsonbuf);
	if (netOutputRawaddr) {
		res=Netwrite(pkt, strlen(pkt));
	}
}



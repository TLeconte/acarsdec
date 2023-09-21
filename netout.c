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

/* This is used to store a duplicate of the supplied argv destination addres */
static char *netOutputRawaddr = NULL;


static struct sockaddr *netOutputAddr = NULL;
static int netOutputAddrLen = 0;

int Netoutinit(char *Rawaddr)
{
        char tmpAddr[256];
	char *addr;
	char *port;
	struct addrinfo hints, *servinfo, *p;
	int rv;

        if (Rawaddr) {
            netOutputRawaddr = strdup(Rawaddr);
        }

        memset(tmpAddr, 0, 256);
        strncpy(tmpAddr, netOutputRawaddr, 255);

	memset(&hints, 0, sizeof hints);
	if (tmpAddr[0] == '[') {
		hints.ai_family = AF_INET6;
		addr = tmpAddr + 1;
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
		addr = tmpAddr;
		port = strstr(addr, ":");
		if (port == NULL)
			port = "5555";
		else {
			*port = 0;
			port++;
		}
	}

        if (verbose) {
            fprintf(stderr, "Attempting to resolve '%s:%s'.\n", addr, port);
        }

	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
            if (rv == EAI_AGAIN) {
                fprintf(stderr, "Invalid/unknown error '%s' resolving '%s:%s', retrying later.\n", gai_strerror(rv), addr, port);
                return 0;
            } else if (rv == EAI_FAIL) {
                fprintf(stderr, "Invalid/unknown failure '%s' resolving '%s:%s', aborting.\n", gai_strerror(rv), addr, port);
                return -1;
            }

            fprintf(stderr, "Invalid/unknown error '%s' resolving '%s:%s', retrying later.\n", gai_strerror(rv), addr, port);
            return -1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                    continue;
		}


                netOutputAddrLen = p->ai_addrlen;
                netOutputAddr = malloc(netOutputAddrLen);
                memcpy(netOutputAddr, p->ai_addr, netOutputAddrLen);

                if (verbose) {
                    char txtaddress[256];
                    char txtport[256];
                    memset(txtaddress, 0, 256);
                    memset(txtport, 0, 256);
                    if (getnameinfo(netOutputAddr, netOutputAddrLen, txtaddress, 255, txtport, 255, NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM)) {
                        fprintf(stderr, "Fail in getnameinfo().\n");
                        return -1;
                    }

                    fprintf(stderr, "Successfully resolved '%s:%s' to %s:%s.\n", addr, port, txtaddress, txtport);
                }
                freeaddrinfo(servinfo);
                return 0;
	}

        fprintf(stderr, "failed to resolve: '%s:%s'\n", addr, port);

	freeaddrinfo(servinfo);

	return -1;
}

static int Netwrite(const void *buf, size_t count) {
    int res;
    if (!netOutputRawaddr) {
        return -1;
    }

    if (!netOutputAddrLen) {
        /* The destination address hasn't yet been succesfully resolved. */
        if (verbose) {
            fprintf(stderr, "retrying DNS resolution.\n");
        }
        res = Netoutinit(NULL);
        if(!res) {
            /* Resolution failed, so we'll drop this message and try again next time. */
            return res;
        }
    }

    res = sendto(sockfd, buf, count, 0, netOutputAddr, netOutputAddrLen);
    if (res != count) {
        if (errno == EAGAIN || errno == ECONNRESET || errno == EINTR) {
            fprintf(stderr, "error on sendto(): %s, retrying.\n", strerror(errno));
            /* Automatically retry these errors once and drop message if retry fails. */
            return sendto(sockfd, buf, count, 0, netOutputAddr, netOutputAddrLen);
        } else {
            fprintf(stderr, "error on sendto(): %s, ignoring.\n", strerror(errno));
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



#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "acarsserv.h"

#define DFLTPORT "5555"

const char *regpre1[] =
    { "C", "B", "F", "D", "2", "I", "P", "M", "G", "Z", "" };
const char *regpre2[] = {
	"YA", "ZA", "7T", "C3", "D2", "VP", "V2", "LV", "LQ", "EK", "P4", "VH",
	    "OE", "4K", "C6",
	"S2", "8P", "EW", "OO", "V3", "TY", "VQ", "A5", "CP", "T9", "E7", "A2",
	    "PP", "PR", "PT",
	"PU", "V8", "LZ", "XT", "9U", "XU", "TJ", "D4", "TL", "TT", "CC", "HJ",
	    "HK", "D6", "TN",
	"E5", "9Q", "TI", "TU", "9A", "CU", "5B", "OK", "OY", "J2", "J7", "HI",
	    "4W", "HC", "SU",
	"YS", "3C", "E3", "ES", "ET", "DQ", "OH", "TR", "C5", "4L", "9G", "SX",
	    "J3", "TG", "3X",
	"J5", "8R", "HH", "HR", "HA", "TF", "VT", "PK", "EP", "YI", "EI", "4X",
	    "6Y", "JY", "Z6",
	"UP", "5Y", "T3", "HL", "9K", "EX", "YL", "OD", "7P", "A8", "5A", "HB",
	    "LY", "LX", "Z3",
	"5R", "7Q", "9M", "8Q", "TZ", "9H", "V7", "5T", "3B", "XA", "XB", "XC",
	    "V6", "ER", "3A",
	"JU", "4O", "CN", "C9", "XY", "XZ", "V5", "C2", "9N", "PH", "PJ", "ZK",
	    "YN", "5U", "LN",
	"AP", "AR", "SU", "E4", "HP", "P2", "ZP", "OB", "RP", "SP", "SN", "CR",
	    "CS", "A7", "YR",
	"RA", "RF", "V4", "J6", "J8", "5W", "T7", "S9", "HZ", "6V", "YU", "S7",
	    "9L", "9V", "OM",
	"S5", "H4", "6O", "ZS", "ZT", "ZU", "EC", "4R", "ST", "PZ", "3D", "SE",
	    "HB", "YK", "EY",
	"5H", "HS", "5V", "A3", "9Y", "TS", "TC", "EZ", "T2", "5X", "UR", "A6",
	    "4U", "CX", "UK",
	"YJ", "VN", "7O", "9J", ""
};
const char *regpre3[] = { "A9C", "A4O", "9XR", "" };

int verbose = 0;
int station = 0;
int allmess = 0;
int dupmess = 0;

void fixreg(unsigned char *reg, unsigned char *add)
{
	unsigned char *p, *t;
	int i, f;
	for (p = add; *p == '.'; p++) ;

	if (strlen(p) >= 4) {
		t = NULL;
		for (i = 0; regpre3[i][0] != 0; i++)
			if (memcmp(p, regpre3[i], 3) == 0) {
				t = p + 3;
				break;
			}
		if (t == NULL) {
			for (i = 0; regpre2[i][0] != 0; i++)
				if (memcmp(p, regpre2[i], 2) == 0) {
					t = p + 2;
					break;
				}
		}
		if (t == NULL) {
			for (i = 0; regpre1[i][0] != 0; i++)
				if (*p == regpre1[i][0]) {
					t = p + 1;
					break;
				}
		}
		if (t && *t != '-') {
			memcpy(reg, p, t - p);
			reg[t - p] = 0;
			strncat(reg, "-", 7);
			strncat(reg, t, 7);
			reg[6] = 0;
			return;
		}
	}

	strncpy(reg, p, 7);
	reg[6] = 0;

}

static void processpkt(acarsmsg_t * msg, char *ipaddr)
{
	char *pr, *pf;
	int lm;

	pr = strtok(msg->reg, " ");
	pf = strtok(msg->fid, " ");
	strtok(msg->no, " ");

	lm = 0;
	if (msg->mode < 0x5d && pr && pf)
		lm = 1;

	if ((lm || station) &&
	    (allmess
	     || (strcmp(msg->label, "Q0") != 0
		 && strcmp(msg->label, "_d") != 0))
	    )
		lm = lm + 2;

	if (dupmess)
		lm = lm + 4;

	updatedb(msg, lm, ipaddr);
}

static char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
	switch (sa->sa_family) {
	case AF_INET:
		inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s,
			  maxlen);
		break;

	case AF_INET6:
		inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
			  s, maxlen);
		break;

	default:
		strncpy(s, "Unknown AF", maxlen);
		return NULL;
	}

	return s;
}

static void usage(void)
{
        fprintf(stderr,
                "Acarsdec/acarsserv 3.1 Copyright (c) 2015 Thierry Leconte \n\n");
	fprintf(stderr,
		"Usage: acarsserv [-v][-N address:port ][-b database path][-s][-d][-a]\n\n");
	fprintf(stderr, "	-v		: verbose\n");
	fprintf(stderr,
		"	-b filepath	: use filepath as sqlite3 database file (default : ./acaraserv.sqb)\n");
	fprintf(stderr,
		"	-N address:port : listen on given addresse:port (default : *:5555)\n");
	fprintf(stderr,
		"	-s		: store acars messages comming from base station (default : don't store )\n");
	fprintf(stderr,
		"	-d		: store duplicate acars messages (default : don't store )\n");
	fprintf(stderr,
		"	-a		: store Q0 and _d (just pings and acks without data) acars messages (default : don't store )\n");
	fprintf(stderr, "\n");

}

static int sockfd = -1;
int bindsock(char *argaddr)
{
	char *bindaddr;
	char *addr;
	char *port;
	struct addrinfo hints, *servinfo, *p;
	int rv;

	if (argaddr == NULL)
		return 0;

	bindaddr = strdup(argaddr);

	memset(&hints, 0, sizeof hints);
	if (bindaddr[0] == '[') {
		hints.ai_family = AF_INET6;
		addr = bindaddr + 1;
		port = strstr(addr, "]");
		if (port == NULL) {
			fprintf(stderr, "Invalid IPV6 address\n");
			return -1;
		}
		*port = 0;
		port++;
		if (*port != ':')
			port = DFLTPORT;
		else
			port++;
	} else {
		hints.ai_family = AF_UNSPEC;
		addr = bindaddr;
		port = strstr(addr, ":");
		if (port == NULL)
			port = DFLTPORT;
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
		if ((sockfd =
		     socket(p->ai_family, p->ai_socktype,
			    p->ai_protocol)) == -1) {
			continue;
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			continue;
		}
		break;
	}
	if (p == NULL) {
		return -1;
	}

	freeaddrinfo(servinfo);
	free(bindaddr);
	return 0;
}

#define MAXACARSLEN 500
int main(int argc, char **argv)
{
	unsigned char pkt[MAXACARSLEN];
	char *basename = "acarsserv.sqb";
	char *bindaddr = "[::]";
	int c;
	int res;

	while ((c = getopt(argc, argv, "vb:N:asd")) != EOF) {

		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'N':
			bindaddr = optarg;
			break;
		case 'b':
			basename = optarg;
			break;
		case 'a':
			allmess = 1;
			break;
		case 's':
			station = 1;
			break;
		case 'd':
			dupmess = 1;
			break;
		default:
			usage();
			exit(1);
		}
	}

	if (bindsock(bindaddr)) {
		fprintf(stderr, "failed to connect\n");
		exit(1);
	}

	if (initdb(basename)) {
		fprintf(stderr, "could not init sql base %s\n", basename);
		exit(1);
	}

	do {
		int len;
		acarsmsg_t *msg;
		struct tm tmp;
		char reg[8];
		char ipaddr[INET6_ADDRSTRLEN];
		struct sockaddr_in6 src_addr;
		socklen_t addrlen;
		char *mpt, *bpt;

		msg = calloc(sizeof(acarsmsg_t), 1);

		addrlen = sizeof(src_addr);
		bzero(&src_addr, addrlen);
		len =
		    recvfrom(sockfd, pkt, MAXACARSLEN, 0,
			     (struct sockaddr *)&src_addr, &addrlen);
		if (len <= 0) {
			fprintf(stderr, "read %d\n", len);
			continue;
		}

		if (len < 31) {
			continue;
		}
		pkt[len] = 0;

		mpt = pkt;
		bpt = mpt + 8;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		strcpy(msg->idst, mpt);
		mpt = bpt + 1;
		bpt = mpt + 1;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		msg->chn = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 2;
		if (*bpt != '/')
			continue;
		*bpt = '\0';
		tmp.tm_mday = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 2;
		if (*bpt != '/')
			continue;
		*bpt = '\0';
		tmp.tm_mon = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 4;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		tmp.tm_year = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 2;
		if (*bpt != ':')
			continue;
		*bpt = '\0';
		tmp.tm_hour = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 2;
		if (*bpt != ':')
			continue;
		*bpt = '\0';
		tmp.tm_min = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 2;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		tmp.tm_sec = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 1;
		if (*bpt != ' ')
			continue;
		msg->err = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 3;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		msg->lvl = atoi(mpt);
		mpt = bpt + 1;
		bpt = mpt + 1;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		msg->mode = *mpt;
		mpt = bpt + 1;
		bpt = mpt + 7;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		strcpy(reg, mpt);
		mpt = bpt + 1;
		bpt = mpt + 1;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		msg->ack = *mpt;
		mpt = bpt + 1;
		bpt = mpt + 2;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		strcpy(msg->label, mpt);
		mpt = bpt + 1;
		bpt = mpt + 1;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		msg->bid = *mpt;
		mpt = bpt + 1;
		bpt = mpt + 4;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		strcpy(msg->no, mpt);
		mpt = bpt + 1;
		bpt = mpt + 6;
		if (*bpt != ' ')
			continue;
		*bpt = '\0';
		strcpy(msg->fid, mpt);
		mpt = bpt + 1;
		strncpy(msg->txt, mpt, 220);
		msg->txt[220] = '\0';


		fixreg(msg->reg, reg);

		tmp.tm_isdst = 0;
		tmp.tm_mon -= 1;
		tmp.tm_year -= 1900;
		msg->tm = timegm(&tmp);

		get_ip_str((struct sockaddr *)&src_addr, ipaddr,
			   INET6_ADDRSTRLEN);

		if (verbose)
			fprintf(stdout,
				"MSG %s %1d %1c %7s %1c %2s %1c %4s %6s %s\n",
				ipaddr, msg->chn, msg->mode, msg->reg, msg->ack,
				msg->label, msg->bid, msg->no, msg->fid,
				msg->txt);

		processpkt(msg, ipaddr);

		free(msg);
	} while (1);
}

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include "acarsdec.h"

extern int inmode;

typedef struct {
	unsigned char mode;
	unsigned char addr[8];
	unsigned char ack;
	unsigned char label[3];
	unsigned char bid;
	unsigned char no[5];
	unsigned char fid[7];
	unsigned char bs, be;
	unsigned char txt[250];
	int err, lvl;
} acarsmsg_t;

static int sockfd = -1;
static FILE *fdout;

int initOutput(char *logfilename, char *Rawaddr)
{
	char *addr;
	char *port;
	struct addrinfo hints, *servinfo, *p;
	int rv;

	if (logfilename) {
		fdout = fopen(logfilename, "a+");
		if (fdout == NULL) {
			fprintf(stderr, "Could not open : %s\n", logfilename);
			return -1;
		}
	} else
		fdout = stdout;

	if (Rawaddr == NULL)
		return 0;

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
		if ((sockfd =
		     socket(p->ai_family, p->ai_socktype,
			    p->ai_protocol)) == -1) {
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

static void printtime(time_t t)
{
	struct tm *tmp;

	if (t == 0)
		return;

	tmp = gmtime(&t);

	fprintf(fdout, "%02d/%02d/%04d %02d:%02d:%02d",
		tmp->tm_mday, tmp->tm_mon + 1, tmp->tm_year + 1900,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}

void outpp(acarsmsg_t * msg)
{
	char pkt[500];
	char txt[250];
	char *pstr;

	strcpy(txt, msg->txt);
	for (pstr = txt; *pstr != 0; pstr++)
		if (*pstr == '\n' || *pstr == '\r')
			*pstr = ' ';

	sprintf(pkt, "AC%1c %7s %1c %2s %1c %4s %6s %s",
		msg->mode, msg->addr, msg->ack, msg->label, msg->bid, msg->no,
		msg->fid, txt);

	write(sockfd, pkt, strlen(pkt));
}

void outsv(acarsmsg_t * msg, int chn, time_t tm)
{
	char pkt[500];
	struct tm *tmp;

	tmp = gmtime(&tm);

	sprintf(pkt,
		"%8s %1d %02d/%02d/%04d %02d:%02d:%02d %1d %03d %1c %7s %1c %2s %1c %4s %6s %s",
		idstation, chn + 1, tmp->tm_mday, tmp->tm_mon + 1,
		tmp->tm_year + 1900, tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
		msg->err, msg->lvl, msg->mode, msg->addr, msg->ack, msg->label,
		msg->bid, msg->no, msg->fid, msg->txt);

	write(sockfd, pkt, strlen(pkt));
}

static void printmsg(acarsmsg_t * msg, int chn, time_t t)
{
#if defined (WITH_RTL) || defined (WITH_AIR)
	if (inmode >= 3)
		fprintf(fdout, "\n[#%1d (F:%3.3f L:%3d E:%1d) ", chn + 1,
			channel[chn].Fr / 1000000.0, msg->lvl, msg->err);
	else
#endif
		fprintf(fdout, "\n[#%1d (E:%1d) ", chn + 1, msg->err);
	if (inmode != 2)
		printtime(t);
	fprintf(fdout, " --------------------------------\n");
	if(msg->mode < 0x5d) {
		fprintf(fdout, "Aircraft reg: %s ", msg->addr);
		fprintf(fdout, "Flight id: %s", msg->fid);
		fprintf(fdout, "\n");
	}
	fprintf(fdout, "Mode: %1c ", msg->mode);
	fprintf(fdout, "Msg. label: %s\n", msg->label);
	fprintf(fdout, "Block id: %c ", msg->bid);
	fprintf(fdout, "Ack: %c\n", msg->ack);
	fprintf(fdout, "Msg. no: %s\n", msg->no);
	fprintf(fdout, "Message :\n%s\n", msg->txt);
	if (verbose && msg->be == 0x17)
		fprintf(fdout, "Block End\n");

	fflush(fdout);
}

static void printoneline(acarsmsg_t * msg, int chn, time_t t)
{
	char txt[30];
	char *pstr;

	strncpy(txt, msg->txt, 29);
	txt[29] = 0;
	for (pstr = txt; *pstr != 0; pstr++)
		if (*pstr == '\n' || *pstr == '\r')
			*pstr = ' ';

	if (inmode >= 3)
		fprintf(fdout, "#%1d (L:%3d E:%1d) ", chn + 1, msg->lvl,
			msg->err);
	else
		fprintf(fdout, "#%1d (E:%1d) ", chn + 1, msg->err);
	if (inmode != 2)
		printtime(t);
	fprintf(fdout, " %7s %6s %1c %2s %4s ", msg->addr, msg->fid, msg->mode,
		msg->label, msg->no);
	fprintf(fdout, "%s", txt);
	fprintf(fdout, "\n");
	fflush(fdout);
}

void outputmsg(const msgblk_t * blk)
{
	acarsmsg_t msg;
	int i, k;

	/* fill msg struct */
	msg.lvl = blk->lvl;
	msg.err = blk->err;

	k = 0;
	msg.mode = blk->txt[k];
	k++;

	for (i = 0; i < 7; i++, k++) {
		msg.addr[i] = blk->txt[k];
	}
	msg.addr[7] = '\0';

	/* ACK/NAK */
	msg.ack = blk->txt[k];
	if (msg.ack == 0x15)
		msg.ack = '!';
	k++;

	msg.label[0] = blk->txt[k];
	k++;
	msg.label[1] = blk->txt[k];
	if (msg.label[1] == 0x7f)
		msg.label[1] = 'd';
	k++;
	msg.label[2] = '\0';

	msg.bid = blk->txt[k];
	if (msg.bid == 0)
		msg.bid = ' ';
	k++;

	/* txt start  */
	msg.bs = blk->txt[k];
	k++;

	msg.no[0] = '\0';
	msg.fid[0] = '\0';
	msg.txt[0] = '\0';

	if ((msg.bs == 0x03 || msg.mode > 'Z' || msg.bid > '9') && airflt)
		return;

	if (msg.bs != 0x03) {
		if (msg.mode <= 'Z' && msg.bid <= '9') {
			/* message no */
			for (i = 0; i < 4 && k < blk->len - 1; i++, k++) {
				msg.no[i] = blk->txt[k];
			}
			msg.no[i] = '\0';

			/* Flight id */
			for (i = 0; i < 6 && k < blk->len - 1; i++, k++) {
				msg.fid[i] = blk->txt[k];
			}
			msg.fid[i] = '\0';
		}

		/* Message txt */
		for (i = 0; k < blk->len - 1; i++, k++)
			msg.txt[i] = blk->txt[k];
		msg.txt[i] = 0;
	}

	/* txt end */
	msg.be = blk->txt[blk->len - 1];

	if (sockfd > 0) {
		if (netout == 0)
			outpp(&msg);
		else
			outsv(&msg, blk->chn, blk->tm);
	}

	switch (outtype) {
	case 0:
		break;
	case 1:
		printoneline(&msg, blk->chn, blk->tm);
		break;
	case 2:
		printmsg(&msg, blk->chn, blk->tm);
		break;
	}
}

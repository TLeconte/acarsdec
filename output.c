#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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

static void printdate(time_t t)
{
	struct tm *tmp;

	if (t == 0)
		return;

	tmp = gmtime(&t);

	fprintf(fdout, "%02d/%02d/%04d %02d:%02d:%02d",
		tmp->tm_mday, tmp->tm_mon + 1, tmp->tm_year + 1900,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}

static void printtime(time_t t)
{
	struct tm *tmp;

	tmp = gmtime(&t);

	fprintf(fdout, "%02d:%02d:%02d",
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
		fprintf(fdout, "\n[#%1d (F:%3.3f L:%4d E:%1d) ", chn + 1,
			channel[chn].Fr / 1000000.0, msg->lvl, msg->err);
	else
#endif
		fprintf(fdout, "\n[#%1d ( L:%4d E:%1d) ", chn + 1, msg->lvl, msg->err);
	if (inmode != 2)
		printdate(t);
	fprintf(fdout, " --------------------------------\n");
	fprintf(fdout, "Mode : %1c ", msg->mode);
	fprintf(fdout, "Label : %2s ", msg->label);
	if(msg->bid) {
		fprintf(fdout, "Id : %1c ", msg->bid);
		if(msg->ack==0x15) fprintf(fdout, "Nak\n"); else fprintf(fdout, "Ack : %1c\n", msg->ack);
		fprintf(fdout, "Aircraft reg: %s ", msg->addr);
		if(msg->mode <= 'Z') {
			fprintf(fdout, "Flight id: %s\n", msg->fid);
			fprintf(fdout, "No: %4s", msg->no);
		}
	}
	fprintf(fdout, "\n");
	if(msg->txt[0]) fprintf(fdout, "%s\n", msg->txt);
	if (msg->be == 0x17) fprintf(fdout, "ETB\n");

	fflush(fdout);
}

static void printbinarystringasjson(unsigned char* start,unsigned char* end)
{
	unsigned char* pos;
	char special=0;
	for (pos=start;pos<end;pos++)
	{
		unsigned char ch=*pos;
		if (ch==0) {
			end=pos;
			break;
		}
		else {
			switch (ch)
			{
			case '\\':
			case '/':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				break;
			default:
				if ((ch<32)||(ch>=127))
				{
					special=1;
				}
				break;
			}
		}
	}
	if (special)
	{
		fprintf(fdout, "[");
		for (pos=start;pos<end;pos++)
		{
			if (pos!=start) fprintf(fdout, ",");
			fprintf(fdout, "%d",*pos);
		}
		fprintf(fdout, "]");
	}
	else
	{
		fprintf(fdout, "\"");
		for (pos=start;pos<end;pos++)
		{
			unsigned char ch=*pos;
			switch (ch)
			{
			case '\\':
				fprintf(fdout, "\\\\");
				break;
			case '/':
				fprintf(fdout, "\\/");
				break;
			case '\b':
				fprintf(fdout, "\\b");
				break;
			case '\f':
				fprintf(fdout, "\\f");
				break;
			case '\n':
				fprintf(fdout, "\\n");
				break;
			case '\r':
				fprintf(fdout, "\\r");
				break;
			case '\t':
				fprintf(fdout, "\\t");
				break;
			default:
				fprintf(fdout, "%c", ch);
				break;
			}
		}
		fprintf(fdout, "\"");
	}
}

#define PRINTC(X) printbinarystringasjson(&(X),&(X)+1)
#define PRINTS(X) printbinarystringasjson(&(X)[0],&(X)[0]+sizeof(X))

static void printjson(acarsmsg_t * msg, int chn, time_t t)
{
	fprintf(fdout,"{\"timestamp\":%lf,\"channel\":%d,\"level\":%d,\"error\":%d", (double)t, chn, msg->lvl, msg->err);
	fprintf(fdout,",\"mode\":");
	PRINTC(msg->mode);
	fprintf(fdout,",\"label\":");
	PRINTS(msg->label);
	if(msg->bid) {
		fprintf(fdout, ",\"block_id\":");
		PRINTC(msg->bid);
		fprintf(fdout, ",\"ack\":");
		if(msg->ack==0x15) {
			fprintf(fdout, "false");
		} else {
			PRINTC(msg->ack);
		}
		fprintf(fdout, ",\"tail\":");
		PRINTS(msg->addr);
		if(msg->mode <= 'Z') {
			fprintf(fdout, ",\"flight\":");
			PRINTS(msg->fid);
			fprintf(fdout, ",\"msgno\":");
			PRINTS(msg->no);
		}
	}
	fprintf(fdout, ",\"text\":");
	PRINTS(msg->txt);
	if (msg->be == 0x17)
		fprintf(fdout, ",\"end\":true");
	fprintf(fdout,"}\n");
	fflush(fdout);
}

#undef PRINTC
#undef PRINTS

static void printoneline(acarsmsg_t * msg, int chn, time_t t)
{
	char txt[30];
	char *pstr;

	strncpy(txt, msg->txt, 29);
	txt[29] = 0;
	for (pstr = txt; *pstr != 0; pstr++)
		if (*pstr == '\n' || *pstr == '\r')
			*pstr = ' ';

	fprintf(fdout, "#%1d (L:%4d E:%1d) ", chn + 1, msg->lvl, msg->err);

	if (inmode != 2)
		printdate(t);
	fprintf(fdout, " %7s %6s %1c %2s %4s ", msg->addr, msg->fid, msg->mode, msg->label, msg->no);
	fprintf(fdout, "%s", txt);
	fprintf(fdout, "\n");
	fflush(fdout);
}

typedef struct flight_s flight_t;
struct flight_s {
	flight_t *next;
	char addr[8];
	char fid[7];
	time_t ts,tl;
	int chm;
	int nbm;
};
static flight_t  *flight_head=NULL;

static void addFlight(acarsmsg_t * msg, int chn, time_t t)
{
	flight_t *fl,*flp;

	fl=flight_head;
	flp=NULL;
	while(fl) {
		if(strcmp(msg->addr,fl->addr)==0) break;
		flp=fl;
		fl=fl->next;
	}

	if(fl==NULL) {
		fl=malloc(sizeof(flight_t));
		fl->nbm=0;
		fl->ts=t;
		fl->chm=0;
		strncpy(fl->addr,msg->addr,8);
		strncpy(fl->fid,msg->fid,7);
		fl->next=NULL;
	}
	fl->tl=t;
	fl->chm|=(1<<chn);
	fl->nbm+=1;

	if(flp) {
		flp->next=fl->next;
		fl->next=flight_head;
	}
	flight_head=fl;

	flp=NULL;
	while(fl) {
		if(fl->tl<(t-mdly)) {
			if(flp) {
				flp->next=fl->next;
				free(fl);
				fl=flp->next;
			} else {
				flight_head=fl->next;
				free(fl);
				fl=flight_head;
			}
		} else {
			flp=fl;
			fl=fl->next;
		}
	}
}


void cls(void)
{
	printf("\x1b[H\x1b[2J");
}

static void printmonitor(acarsmsg_t * msg, int chn, time_t t)
{
	flight_t *fl;

	cls();

	printf("             Acarsdec monitor\n");
	printf(" Aircraft Flight  Nb Channels   Last     First\n");

	fl=flight_head;
	while(fl) {
		int i;

		printf("%8s %7s %3d ", fl->addr, fl->fid,fl->nbm);
		for(i=0;i<nbch;i++) printf("%c",(fl->chm&(1<<i))?'x':'.');
		for(;i<MAXNBCHANNELS;i++) printf(" ");
		printf(" ");printtime(fl->tl);
		printf(" ");printtime(fl->ts);
		printf("\n");

		fl=fl->next;
	}

	//printmsg(msg,chn,t);
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
	k++;

	msg.label[0] = blk->txt[k];
	k++;
	msg.label[1] = blk->txt[k];
	if(msg.label[1]==0x7f) msg.label[1]='d';
	k++;
	msg.label[2] = '\0';

	msg.bid = blk->txt[k];
	k++;

	/* txt start  */
	msg.bs = blk->txt[k];
	k++;

	msg.no[0] = '\0';
	msg.fid[0] = '\0';
	msg.txt[0] = '\0';

	if ((msg.bs == 0x03 || msg.mode > 'Z') && airflt)
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

			if(outtype==3)
				addFlight(&msg,blk->chn,blk->tm);
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
	case 3:
		printmonitor(&msg, blk->chn, blk->tm);
		break;
	case 4:
		printjson(&msg, blk->chn, blk->tm);
		break;
	}
}

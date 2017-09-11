#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include "acarsdec.h"



static int label_q1(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    memcpy(oooi->woff,&(txt[8]),4);
    memcpy(oooi->won,&(txt[12]),4);
    memcpy(oooi->gin,&(txt[16]),4);
    memcpy(oooi->da,&(txt[24]),4);
    return 1;
}
static int label_q2(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->eta,&(txt[4]),4);
    return 1;
}
static int label_qa(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    return 1;
}
static int label_qb(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->woff,&(txt[4]),4);
    return 1;
}
static int label_qc(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->won,&(txt[4]),4);
    return 1;
}
static int label_qd(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gin,&(txt[4]),4);
    return 1;
}
static int label_qe(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    memcpy(oooi->da,&(txt[8]),4);
    return 1;
}
static int label_qf(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->woff,&(txt[4]),4);
    memcpy(oooi->da,&(txt[8]),4);
    return 1;
}
static int label_qg(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    memcpy(oooi->gin,&(txt[8]),4);
    return 1;
}
static int label_qh(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    return 1;
}
static int label_qk(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->won,&(txt[4]),4);
    memcpy(oooi->da,&(txt[8]),4);
    return 1;
}
static int label_ql(char *txt,oooi_t *oooi)
{
    memcpy(oooi->da,txt,4);
    memcpy(oooi->gin,&(txt[8]),4);
    memcpy(oooi->sa,&(txt[13]),4);
    return 1;
}
static int label_qm(char *txt,oooi_t *oooi)
{
    memcpy(oooi->da,txt,4);
    memcpy(oooi->sa,&(txt[8]),4);
    return 1;
}
static int label_qn(char *txt,oooi_t *oooi)
{
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->eta,&(txt[8]),4);
    return 1;
}
static int label_qp(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->gout,&(txt[8]),4);
    return 1;
}
static int label_qq(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->woff,&(txt[8]),4);
    return 1;
}
static int label_qr(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->won,&(txt[8]),4);
    return 1;
}
static int label_qs(char *txt,oooi_t *oooi)
{
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->gin,&(txt[8]),4);
    return 1;
}
static int label_qt(char *txt,oooi_t *oooi)
{
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->gout,&(txt[8]),4);
    memcpy(oooi->gin,&(txt[12]),4);
    return 1;
}
static int label_2z(char *txt,oooi_t *oooi)
{
    memcpy(oooi->da,txt,4);
    return 1;
}
static int label_26(char *txt,oooi_t *oooi)
{
    char *p;
    if(memcmp(txt,"VER/077",7)) return 0;
    p=index(txt,'\n'); if(p==NULL) return 0;
    p++;
    if(memcmp(p,"SCH/",4)) return 0;
    p=index(p+4,'/'); if(p==NULL) return 0;
    memcpy(oooi->sa,p+1,4);
    memcpy(oooi->da,p+6,4);
    p=index(p,'\n'); if(p==NULL) return 0;
    p++;
    if(memcmp(p,"ETA/",4)) return 1;
    memcpy(oooi->eta,p+4,4);
    return 1;
}
static int label_10(char *txt,oooi_t *oooi)
{
    if(memcmp(txt,"ARR01",5)) return 0;
    memcpy(oooi->da,&(txt[12]),4);
    memcpy(oooi->eta,&(txt[16]),4);
    return 1;
}
static int label_15(char *txt,oooi_t *oooi)
{
    if(memcmp(txt,"FST01",5)) return 0;
    memcpy(oooi->sa,&(txt[5]),4);
    memcpy(oooi->da,&(txt[9]),4);
    return 1;
}
static int label_8e(char *txt,oooi_t *oooi)
{
    if(txt[4]!=',') return 0;
    memcpy(oooi->da,txt,4);
    memcpy(oooi->eta,&(txt[5]),4);
    return 1;
}
static int label_8s(char *txt,oooi_t *oooi)
{
    if(txt[4]!=',') return 0;
    memcpy(oooi->da,txt,4);
    memcpy(oooi->eta,&(txt[5]),4);
    return 1;
}
static int label_b9(char *txt,oooi_t *oooi)
{
    if(txt[0]!='/') return 0;
    memcpy(oooi->da,&(txt[1]),4);
    return 1;
}


int DecodeLabel(acarsmsg_t *msg,oooi_t *oooi)
{
  int ov=0;
 
  memset(oooi,0,sizeof(oooi_t));

  switch(msg->label[0]) {
	case '1' :
		if(msg->label[1]=='0') 
			ov=label_10(msg->txt,oooi);
		if(msg->label[1]=='5') 
			ov=label_15(msg->txt,oooi);
		break;
	case '2' :
		if(msg->label[1]=='Z') 
			ov=label_2z(msg->txt,oooi);
		if(msg->label[1]=='6') 
			ov=label_26(msg->txt,oooi);
		break;
	case '8' :
		if(msg->label[1]=='E') 
			ov=label_8e(msg->txt,oooi);
		if(msg->label[1]=='S') 
			ov=label_8s(msg->txt,oooi);
		break;
	case 'B' :
		if(msg->label[1]=='9') 
			ov=label_b9(msg->txt,oooi);
		break;
	case 'Q' :
  		switch(msg->label[1]) {
			case '1':ov=label_q1(msg->txt,oooi);break;
			case '2':ov=label_q2(msg->txt,oooi);break;
			case 'A':ov=label_qa(msg->txt,oooi);break;
			case 'B':ov=label_qb(msg->txt,oooi);break;
			case 'C':ov=label_qc(msg->txt,oooi);break;
			case 'D':ov=label_qd(msg->txt,oooi);break;
			case 'E':ov=label_qe(msg->txt,oooi);break;
			case 'F':ov=label_qf(msg->txt,oooi);break;
			case 'G':ov=label_qg(msg->txt,oooi);break;
			case 'H':ov=label_qh(msg->txt,oooi);break;
			case 'K':ov=label_qk(msg->txt,oooi);break;
			case 'L':ov=label_ql(msg->txt,oooi);break;
			case 'M':ov=label_qm(msg->txt,oooi);break;
			case 'N':ov=label_qn(msg->txt,oooi);break;
			case 'P':ov=label_qp(msg->txt,oooi);break;
			case 'Q':ov=label_qq(msg->txt,oooi);break;
			case 'R':ov=label_qr(msg->txt,oooi);break;
			case 'S':ov=label_qs(msg->txt,oooi);break;
			case 'T':ov=label_qt(msg->txt,oooi);break;
		}
		break;
  }

  return ov;
}

/*
 *  Copyright (c) 2014 Thierry Leconte (f4dwv)
 *
 *   
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <time.h>
#include <pthread.h>

#define MAXNBCHANNELS 4

typedef float sample_t;

typedef struct mskblk_s {
	struct mskblk_s *prev;
	int chn;
	time_t tm;
	int len;
	int lvl,err;
	unsigned char txt[250];
	unsigned char crc[2];
} msgblk_t;

typedef struct {
	int chn;
	int inmode;
	int Infs;

#ifdef WITH_RTL
	float Fr;
	float *swf;
	float *cwf;
#endif

	float MskPhi;
	float MskFreq,MskDf;
	float Mska,MskKa;
	float Mskdc,Mskdcf;
	float MskClk;
	unsigned int   MskS;

	sample_t  *I,*Q;
	float *h;
	int flen,idx;

	unsigned char outbits;
	int	nbits;

	enum { WSYN, SYN2, SOH1, TXT, CRC1,CRC2, END } Acarsstate;
	msgblk_t *blk;

} channel_t;

extern channel_t channel[MAXNBCHANNELS];
extern unsigned int  nbch;
extern unsigned long wrktot;
extern unsigned long wrkmask;
extern pthread_mutex_t datamtx;
extern pthread_cond_t datawcd;


extern char *idstation;
extern int inpmode;
extern int verbose;
extern int outtype;
extern int netout;
extern int airflt;
extern int gain;
extern int ppm;

extern int initOutput(char*,char *);

#ifdef WITH_ALSA
extern int initAlsa(char **argv,int optind);
extern int runAlsaSample(void);
#endif
#ifdef WITH_SNDFILE
extern int initSoundfile(char **argv,int optind);
extern int runSoundfileSample(void);
#endif
#ifdef WITH_RTL
extern int initRTL(char **argv,int optind);
extern int runRTLSample(void);
#endif
extern int  initMsk(channel_t *);
extern void demodMsk(float in,channel_t *);

extern int  initAcars(channel_t *);
extern void decodeAcars(channel_t *);

extern void outputmsg(const msgblk_t*);



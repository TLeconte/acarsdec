/*
 *  Copyright (c) 2015 Thierry Leconte
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
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <complex.h>
#ifdef HAVE_LIBACARS
#include <libacars/libacars.h>
#include <libacars/reassembly.h>
#endif

#define ACARSDEC_VERSION "3.7"

#define MAXNBCHANNELS 16
#define INTRATE 12500

#define NETLOG_NONE 0
#define NETLOG_PLANEPLOTTER 1
#define NETLOG_NATIVE 2
#define NETLOG_JSON 3
#define NETLOG_MQTT 4

#define OUTTYPE_NONE 0
#define OUTTYPE_ONELINE 1
#define OUTTYPE_STD 2
#define OUTTYPE_MONITOR 3
#define OUTTYPE_JSON 4
#define OUTTYPE_ROUTEJSON 5

typedef float sample_t;

typedef struct mskblk_s {
	struct mskblk_s *prev;
	int chn;
	struct timeval tv;
	int len;
	int err;
	float lvl;
	char txt[250];
	unsigned char crc[2];
} msgblk_t;

typedef struct {
	int chn;

#if defined(WITH_RTL) || defined(WITH_AIR)
	int Fr;
	float complex *wf;
#endif
#if defined(WITH_AIR)
	float complex D;
#endif
#if defined(WITH_SDRPLAY) || defined(WITH_SOAPY)
	float	Fr;
        float	complex *oscillator;
        float	complex D;
	int	counter;
#endif

	float *dm_buffer;
	double MskPhi;
	double MskDf;
	float MskClk;
	double MskLvlSum;
	int MskBitCount;
	unsigned int MskS,idx;
	float complex *inb;

	unsigned char outbits;
	int	nbits;

	enum { WSYN, SYN2, SOH1, TXT, CRC1,CRC2, END } Acarsstate;
	msgblk_t *blk;

	pthread_t th;
} channel_t;

typedef struct {
        char da[5];
        char sa[5];
        char eta[5];
        char gout[5];
        char gin[5];
        char woff[5];
        char won[5];
} oooi_t;

typedef struct {
        char mode;
        char addr[8];
        char ack;
        char label[3];
        char bid;
        char no[5];
        char fid[7];
        char sublabel[3];
        char mfi[3];
        char bs, be;
        char *txt;
        int err;
        float lvl;
#ifdef HAVE_LIBACARS
        char msn[4];
        char msn_seq;
        la_proto_node *decoded_tree;
        la_reasm_status reasm_status;
#endif
} acarsmsg_t;

extern channel_t channel[MAXNBCHANNELS];
extern unsigned int  nbch;
extern unsigned long wrktot;
extern unsigned long wrkmask;
extern pthread_mutex_t datamtx;
extern pthread_cond_t datawcd;

extern int signalExit;

extern int inpmode;
extern int verbose;
extern int outtype;
extern int netout;
extern int airflt;
extern int emptymsg;
extern int mdly;
extern int hourly, daily;

extern int ppm;
extern	int	lnaState;
extern	int	GRdB;
extern int initOutput(char*,char *);

#ifdef HAVE_LIBACARS
extern int skip_reassembly;
#endif
#ifdef WITH_ALSA
extern int initAlsa(char **argv,int optind);
extern int runAlsaSample(void);
#endif
#ifdef WITH_SNDFILE
extern int initSoundfile(char **argv,int optind);
extern int runSoundfileSample(void);
#endif
#ifdef WITH_RTL
extern int initRtl(char **argv,int optind);
extern int runRtlSample(void);
extern int runRtlCancel(void);
extern int runRtlClose(void);
extern int rtlMult;
#endif
#ifdef WITH_AIR
extern int initAirspy(char **argv,int optind);
extern int runAirspySample(void);
#endif
#ifdef WITH_SOAPY
extern int initSoapy(char **argv,int optind);
extern int soapySetAntenna(const char *antenna);
extern int runSoapySample(void);
extern int runSoapyClose(void);
extern int rateMult;
extern int freq;
extern double gain;
#else
extern int gain;
#endif
#ifdef WITH_MQTT
extern int MQTTinit(char **urls, char * client_id, char *topic, char *user,char *passwd);
extern int MQTTsend(char *msgtxt);
extern void MQTTend();
#endif

extern int initRaw(char **argv,int optind);
extern int runRawSample(void);
extern int  initMsk(channel_t *);
extern void demodMSK(channel_t *ch,int len);


extern int  initAcars(channel_t *);
extern void decodeAcars(channel_t *);
extern int  deinitAcars(void);

extern int DecodeLabel(acarsmsg_t *msg,oooi_t *oooi);

extern void outputmsg(const msgblk_t*);

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

#ifndef acarsdec_h
#define acarsdec_h

#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <complex.h>
#ifdef HAVE_LIBACARS
#include <libacars/libacars.h>
#include <libacars/reassembly.h>
#endif

#define ACARSDEC_VERSION "3.X"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define INTRATE 12500

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof(x[0]))

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
	float complex *wf;
#endif
#if defined(WITH_RTL) || defined(WITH_SDRPLAY) || defined(WITH_SOAPY) || defined(WITH_AIR)
	unsigned int Fr;
	float complex *oscillator;
	float complex D;
	int counter;
#endif

	float *dm_buffer;
	double MskPhi;
	double MskDf;
	float MskClk;
	double MskLvlSum;
	int MskBitCount;
	unsigned int MskS, idx;
	float complex *inb;

	unsigned char outbits;
	int nbits;

	enum { WSYN, SYN2, SOH1, TXT, CRC1, CRC2, END } Acarsstate;
	msgblk_t *blk;

	pthread_t th;
} channel_t;

typedef struct output_s {
	enum { FMT_ONELINE = 1, FMT_FULL, FMT_MONITOR, FMT_PP, FMT_NATIVE, FMT_JSON, FMT_ROUTEJSON } fmt;
	enum { DST_FILE = 1, DST_UDP, DST_MQTT } dst;
	void *params;
	void *priv;
	struct output_s *next;
} output_t;

typedef struct {
	channel_t *channels;
	unsigned int nbch;

	int inmode;
	int verbose;
	int airflt;
	int emptymsg;
	int mdly;

	float gain;
	int ppm;
	int bias;
	int rateMult;
	int lnaState;
	int GRdB;
	unsigned int freq;

	char *idstation;

#ifdef HAVE_LIBACARS
	int skip_reassembly;
#endif

#ifdef WITH_SOAPY
	char *antenna;
#endif

	output_t *outputs;
} runtime_t;

extern runtime_t R;

#ifdef WITH_ALSA
extern int initAlsa(char **argv, int optind);
extern int runAlsaSample(void);
#endif

#ifdef WITH_SNDFILE
extern int initSoundfile(char **argv, int optind);
extern int runSoundfileSample(void);
#endif

#ifdef WITH_RTL
extern int initRtl(char **argv, int optind);
extern int runRtlSample(void);
extern int runRtlCancel(void);
extern int runRtlClose(void);
#endif

#ifdef WITH_AIR
extern int initAirspy(char **argv, int optind);
extern int runAirspySample(void);
#endif

#ifdef WITH_SOAPY
extern int initSoapy(char **argv, int optind);
extern int soapySetAntenna(const char *antenna);
extern int runSoapySample(void);
extern int runSoapyClose(void);
#endif

extern int initRaw(char **argv, int optind);
extern int runRawSample(void);
extern int initMsk(channel_t *);
extern void demodMSK(channel_t *ch, int len);

extern int initAcars(channel_t *);
extern void decodeAcars(channel_t *);
extern int deinitAcars(void);

#endif /* acarsdec_h */

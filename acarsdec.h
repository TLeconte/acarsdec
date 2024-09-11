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

#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <complex.h>
#ifdef HAVE_LIBACARS
#include <libacars/libacars.h>
#include <libacars/reassembly.h>
#endif

#define ACARSDEC_VERSION "4.0"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define INTRATE 12500U	// 12.5kHz: ACARS is 2400Bd NRZI, AM 10kHz BW with a 1800Hz Fc, 1200Hz shift MSK.
#define DMBUFSZ	1024U

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof(x[0]))

#ifdef __GNUC__
 #define likely(x)	__builtin_expect(!!(x), 1)
 #define unlikely(x)	__builtin_expect(!!(x), 0)
#else
 #define likely(x)	(x)
 #define unlikely(x)	(x)
#endif

#define vprerr(fmt, ...)	do { if (unlikely(R.verbose)) fprintf(stderr, fmt, ## __VA_ARGS__); } while(0)

typedef struct mskblk_s {
	struct mskblk_s *prev;
	struct timeval tv;
	float lvl;
	uint8_t chn;	// there will never be 255 channels
	uint8_t txtlen;
	uint8_t err;
	uint8_t crc[2];
	union {
		struct txtdata_s {
			char mode;
			char addr[7];
			char ack;
			char label[2];
			char bid;
			char sot;
			char text[220+1+2+1];	// text + suffix + CRC + DEL (latter 2 needed for missing suffix recovery)
		} d;
		uint8_t raw[sizeof(struct txtdata_s)];
	} txt;
} msgblk_t;

#define blk_textlen(blkp)	(blkp->txtlen - offsetof(struct txtdata_s, text))

typedef struct {
	msgblk_t *blk;
	float complex *oscillator;

	float *dm_buffer;		// INTRATE-sampled signal buffer
	float complex *inb;
	double MskPhi;
	double MskDf;
	float MskLvl;
	float MskClk;
	unsigned int MskS, idx;

	unsigned int Fr;		// channel frequency (in Hz)

	enum { PREKEY, SYNC, SOH1, TXT, CRC1, CRC2, END } Acarsstate;
	uint8_t chn;
	int8_t count;
	uint8_t nbits;
	uint8_t outbits;
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
	volatile bool running;
	bool verbose;

	// used only in output
	bool airflt;
	bool emptymsg;
	bool statsd;
#ifdef HAVE_LIBACARS
	bool skip_reassembly;
#endif
	int mdly;
	output_t *outputs;
	char *idstation;

	enum { IN_NONE = 0, IN_ALSA, IN_SNDFILE, IN_RTL, IN_AIR, IN_SDRPLAY, IN_SOAPY } inmode;

	// used only during setup
	float gain;
	int ppm;
	int bias;
	unsigned int rateMult;
	int lnaState;
	int GRdB;
	unsigned int Fc, minFc, maxFc;

#ifdef WITH_SOAPY
	char *antenna;
#endif
} runtime_t;

extern runtime_t R;

#endif /* acarsdec_h */

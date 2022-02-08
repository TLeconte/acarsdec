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
#ifdef WITH_SNDFILE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sndfile.h>
#include "acarsdec.h"

#define MAXNBFRAMES 4096
static SNDFILE *insnd;

int initSoundfile(char **argv, int optind)
{
	SF_INFO infsnd;
	int n;

	infsnd.format = 0;
	insnd = sf_open(argv[optind], SFM_READ, &infsnd);
	if (insnd == NULL) {
		fprintf(stderr, "could not open %s\n", argv[optind]);
		return (1);
	}
	nbch = infsnd.channels;
	if (nbch > MAXNBCHANNELS) {
		fprintf(stderr, "Too much input channels : %d\n", nbch);
		return (1);
	}
	if(infsnd.samplerate!=INTRATE) {
		fprintf(stderr, "unsupported sample rate : %d (must be %d)\n",infsnd.samplerate,INTRATE);
		return (1);
	}
	
	for (n = 0; n < nbch; n++) {
		channel[n].dm_buffer=malloc(sizeof(float)*MAXNBFRAMES);
	}

	return (0);
}

int runSoundfileSample(void)
{
	int nbi, n, i;
	sample_t sndbuff[MAXNBFRAMES * MAXNBCHANNELS];

	do {

		nbi = sf_read_float(insnd, sndbuff, MAXNBFRAMES * nbch);

		if (nbi == 0) {
			return -1;
		}

		for (n = 0; n < nbch; n++) {
			int len = nbi / nbch;
			for (i = 0; i < len; i++)
				channel[n].dm_buffer[i]=sndbuff[n + i * nbch];

			demodMSK(&(channel[n]),len);
		}

	} while (1);
	return 0;
}

#ifdef DEBUG
static SNDFILE *outsnd;
void initSndWrite(void)
{
	SF_INFO infsnd;

	infsnd.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	infsnd.samplerate = INTRATE;
	infsnd.channels = 1;
	outsnd = sf_open("data.wav", SFM_WRITE, &infsnd);
	if (outsnd == NULL) {
		fprintf(stderr, "could not open data\n ");
		return;
	}

}

void SndWrite(float *in)
{
	sf_write_float(outsnd, in, 1);
}

void SndWriteClose(void)
{
	sf_close(outsnd);
}
#endif

#endif

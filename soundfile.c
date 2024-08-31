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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sndfile.h>
#include "acarsdec.h"
#include "msk.h"

#define MAXNBFRAMES 4096

#define ERRPFX	"ERROR: SNDFILE: "

static SNDFILE *insnd;

int initSoundfile(char *optarg)
{
	SF_INFO infsnd;
	unsigned int n;

	infsnd.format = 0;
	insnd = sf_open(optarg, SFM_READ, &infsnd);
	if (insnd == NULL) {
		fprintf(stderr, ERRPFX "could not open '%s'\n", optarg);
		return (1);
	}

	if (infsnd.samplerate != INTRATE) {
		fprintf(stderr, ERRPFX "unsupported sample rate : %d (must be %d)\n", infsnd.samplerate, INTRATE);
		return (1);
	}

	R.channels = calloc(infsnd.channels, sizeof(*R.channels));
	if (!R.channels) {
		perror(NULL);
		return -1;
	}
	R.nbch = infsnd.channels;

	for (n = 0; n < R.nbch; n++) {
		R.channels[n].dm_buffer = malloc(sizeof(*R.channels[n].dm_buffer) * MAXNBFRAMES);
		if (!R.channels[n].dm_buffer) {
			perror(NULL);
			return -1;
		}
	}

	return (0);
}

int runSoundfileSample(void)
{
	sf_count_t nbi;
	unsigned int i, n;
	const unsigned int nch = R.nbch;
	float sndbuff[MAXNBFRAMES * nch];

	do {
		nbi = sf_readf_float(insnd, sndbuff, MAXNBFRAMES);

		if (!nbi)
			goto out;

		for (n = 0; n < nch; n++) {
			for (i = 0; i < nbi; i++)
				R.channels[n].dm_buffer[i] = sndbuff[n + i * nch];
		}
		for (n = 0; n < nch; n++)
			demodMSK(&R.channels[n], nbi);
	} while (R.running);

out:
	sf_close(insnd);

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

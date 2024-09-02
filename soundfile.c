/*
 *  Copyright (c) 2014 Thierry Leconte (f4dwv)
 *  Copyright (c) 2024 Thibaut VARENE
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
#include <limits.h>
#include <sndfile.h>
#include "acarsdec.h"
#include "msk.h"

#define MAXNBFRAMES 4096

#define ERRPFX	"ERROR: SNDFILE: "

static SNDFILE *insnd;

static int usage(void)
{
	fprintf(stderr,
		"sndfile input accepts as parameter:\n"
		" - either a filename, for any file format supported by libsndfile\n"
		" - or to process raw audio, a filename (optionally prefixed by 'file=') followed by a coma,\n"
		"   and any of the following coma-separated extra arguments:\n"
		"   'subtype=', 'channels=', 'endian='; where\n"
		"   'subtype=' is followed by a number matching a supported libsndfile subtype (required);\n"
		"   'channels=' is followed by a number representing the number of channels in the stream (default: 1);\n"
		"   'endian=' is one of 'big', 'little', 'cpu' (default: cpu).\n"
		"examples:\n"
		"   'file=data.raw,subtype=0x6' to process a single-channel, 32-bit cpu-endian float data\n"
		"   'data.raw,subtype=2,channels=2,endian=little' to process a 2-channel, 16-bit little-endian PCM data\n"
		);

	return 1;
}

int initSoundfile(char *optarg)
{
	char *stype = NULL, *chans = NULL, *endian = NULL, *fname = NULL;
	struct params_s soundp[] = {
		{ .name = "subtype", .valp = &stype, },
		{ .name = "channels", .valp = &chans, },
		{ .name = "endian", .valp = &endian, },
		{ .name = "file", .valp = &fname, },
	};
	char *retp, *sep;
	SF_INFO infsnd = {0};
	unsigned int n;

	do {
		retp = parse_params(&optarg, soundp, ARRAY_SIZE(soundp));
		if (retp) {
			sep = strchr(retp, '=');
			// backward compat: if we find a lone token, assume it's the filename if not already set
			if (!sep && !fname)
				fname = retp;
			else {
				fprintf(stderr, ERRPFX "invalid parameter '%s'\n", retp);
				return -1;

			}
		}
	} while (retp);

	if (!fname) {
		fprintf(stderr, ERRPFX "missing filename\n");
		return -1;
	}

	if (!strcmp("help", fname))
		return usage();

	// if we have a subtype, assume we're dealing with raw input
	if (stype) {
		unsigned long sub = strtoul(stype, &stype, 0);
		if ('\0' != *stype) {
			fprintf(stderr, ERRPFX "invalid subtype value '%s'\n", stype);
			return -1;
		}

		unsigned long nchs = 1;
		if (chans) {
			nchs = strtoul(chans, &chans, 0);
			if ('\0' != *chans) {
				fprintf(stderr, ERRPFX "invalid channels value '%s'\n", chans);
				return -1;
			}
		}
		infsnd.channels = (int)nchs;
		infsnd.samplerate = INTRATE;

		unsigned long end = SF_ENDIAN_CPU;
		if (endian) {
			if (!strcmp("little", endian))
				end = SF_ENDIAN_LITTLE;
			else if (!strcmp("big", endian))
				end = SF_ENDIAN_BIG;
			else if (!strcmp("cpu", endian))
				end = SF_ENDIAN_CPU;
			else {
				fprintf(stderr, ERRPFX "invalid endian value '%s'\n", endian);
				return -1;
			}
		}
		infsnd.format = SF_FORMAT_RAW|(sub & SF_FORMAT_SUBMASK)|end;
	}
	else
		infsnd.format = 0;

	insnd = sf_open(fname, SFM_READ, &infsnd);
	if (insnd == NULL) {
		fprintf(stderr, ERRPFX "could not open '%s'\n", fname);
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

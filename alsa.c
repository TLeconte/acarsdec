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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include "acarsdec.h"
#include "msk.h"

#define MAXNBFRAMES 4096

#define ERRPFX	"ERROR: ALSA: "

static snd_pcm_t *capture_handle;
int initAlsa(char *optarg)
{
	snd_pcm_hw_params_t *hw_params;
	int err, n;
	unsigned int Fs;

	if ((err = snd_pcm_open(&capture_handle, optarg,
				SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, ERRPFX "cannot open audio device %s (%s)\n",
			optarg, snd_strerror(err));
		return 1;
	}

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		fprintf(stderr,
			ERRPFX "cannot allocate hardware parameter structure (%s)\n",
			snd_strerror(err));
		return 1;
	}

	if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
		fprintf(stderr,
			ERRPFX "cannot initialize hardware parameter structure (%s)\n",
			snd_strerror(err));
		return 1;
	}

	if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params,
						SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, ERRPFX "cannot set access type (%s)\n",
			snd_strerror(err));
		return 1;
	}

	if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_FLOAT)) < 0) {
		fprintf(stderr, ERRPFX "cannot set sample format (%s)\n",
			snd_strerror(err));
		return 1;
	}

	snd_pcm_hw_params_set_rate_resample(capture_handle, hw_params, 0);
	Fs = INTRATE;
	n = 1;
	if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &Fs, &n)) < 0) {
		fprintf(stderr, ERRPFX "cannot set sample rate (%s)\n",
			snd_strerror(err));
		return 1;
	}
	if (snd_pcm_hw_params_get_channels(hw_params, &R.nbch) != 0) {
		fprintf(stderr, ERRPFX "cannot get number of channels\n");
		return 1;
	}
	if (R.nbch > 1) {
		fprintf(stderr, ERRPFX "too many channels : %d\n", R.nbch);
		return 1;
	}
	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
		fprintf(stderr, ERRPFX "cannot set parameters (%s)\n",
			snd_strerror(err));
		return 1;
	}
	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(capture_handle)) < 0) {
		fprintf(stderr,
			ERRPFX "cannot prepare audio interface for use (%s)\n",
			snd_strerror(err));
		return 1;
	}

	R.channels = calloc(1, sizeof(*R.channels));
	if (!R.channels) {
		perror(NULL);
		return 1;
	}

	R.channels[0].chn = 0;
	R.channels[0].dm_buffer = malloc(MAXNBFRAMES * sizeof(*R.channels[0].dm_buffer));
	if (!R.channels[0].dm_buffer) {
		perror(NULL);
		return 1;
	}

	return (0);
}

int runAlsaSample(void)
{
	int r;

	do {
		r = snd_pcm_readi(capture_handle, R.channels[0].dm_buffer, MAXNBFRAMES);
		if (r <= 0) {
			fprintf(stderr,
				ERRPFX "cannot read from interface (%s)\n",
				snd_strerror(r));
			return -1;
		}

		demodMSK(&(R.channels[0]), r);

	} while (R.running);
	
	return 0;
}

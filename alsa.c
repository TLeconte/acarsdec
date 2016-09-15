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
#ifdef WITH_ALSA
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include "acarsdec.h"

#define MAXNBFRAMES 8192

static snd_pcm_t *capture_handle;
int initAlsa(char **argv, int optind)
{
	snd_pcm_hw_params_t *hw_params;
	int err, n;
	unsigned int Fs;

	if ((err = snd_pcm_open(&capture_handle, argv[optind],
				SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, "Alsa cannot open audio device %s (%s)\n",
			argv[optind], snd_strerror(err));
		return 1;
	}

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		fprintf(stderr,
			"Alsa cannot allocate hardware parameter structure (%s)\n",
			snd_strerror(err));
		return 1;
	}

	if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
		fprintf(stderr,
			"Alsa cannot initialize hardware parameter structure (%s)\n",
			snd_strerror(err));
		return 1;
	}

	if ((err =
	     snd_pcm_hw_params_set_access(capture_handle, hw_params,
					  SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "Alsa cannot set access type (%s)\n",
			snd_strerror(err));
		return 1;
	}

	if ((err =
	     snd_pcm_hw_params_set_format(capture_handle, hw_params,
					  SND_PCM_FORMAT_S16)) < 0) {
		fprintf(stderr, "Alsa cannot set sample format (%s)\n",
			snd_strerror(err));
		return 1;
	}

	snd_pcm_hw_params_set_rate_resample(capture_handle, hw_params, 0);
	Fs = 12500;
	n = 1;
	if ((err =
	     snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &Fs,
					     &n)) < 0) {
		fprintf(stderr, "Alsa cannot set sample rate (%s)\n",
			snd_strerror(err));
		return 1;
	}
	fprintf(stderr, "Alsa sample rate %d\n", Fs);

	if (snd_pcm_hw_params_get_channels(hw_params, &nbch) != 0) {
		fprintf(stderr, "Alsa cannot get number of channels\n");
		return 1;
	}
	if (nbch > MAXNBCHANNELS) {
		fprintf(stderr, "Alsa too much channels : %d\n", nbch);
		return 1;

	}
	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
		fprintf(stderr, "Alsa cannot set parameters (%s)\n",
			snd_strerror(err));
		return 1;
	}
	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(capture_handle)) < 0) {
		fprintf(stderr,
			"Alsa cannot prepare audio interface for use (%s)\n",
			snd_strerror(err));
		return 1;
	}

	for (n = 0; n < nbch; n++) {
		channel[n].Infs = Fs;
	}

	return (0);
}

int runAlsaSample(void)
{
	int r, n, i;
	signed short sndbuff[MAXNBFRAMES * MAXNBCHANNELS];

	do {
		r = snd_pcm_readi(capture_handle, sndbuff, MAXNBFRAMES);
		if (r <= 0) {
			fprintf(stderr,
				"Alsa cannot read from interface (%s)\n",
				snd_strerror(r));
			return -1;
		}

		for (n = 0; n < nbch; n++) {
			int len = r;
			for (i = 0; i < len; i++)
				demodMsk((float)(sndbuff[n + i * nbch]) /
					 32768.0, &(channel[n]));

		}

	} while (1);
	return 0;
}

#endif

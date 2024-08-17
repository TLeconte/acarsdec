/*
 *  Copyright (c) 2015 Thierry Leconte
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
#include <math.h>
#include <complex.h>

#include "acarsdec.h"
#include "lib.h"

int parse_freqs(char **argv, const int argind)
{
	unsigned int nb = 0;
	unsigned int freq, minF, maxF;
	int ind = argind;
	char *argF;

	// XXX TODO sanitization

	minF = 140000000U;
	maxF = 0;

	/* count frequency args */
	while ((argF = argv[ind])) {
		ind++;
		freq = (1000U * (unsigned int)atof(argF));
		if (freq < 118000U || freq > 138000U) {
			fprintf(stderr, "WARNING: Ignoring invalid frequency '%s'\n", argF);
			continue;
		}
		nb++;
	}

	if (!nb) {
		fprintf(stderr, "ERROR: Need a least one frequency\n");
		return 1;
	}

	/* allocate channels */
	R.channels = calloc(nb, sizeof(*R.channels));
	if (!R.channels) {
		perror(NULL);
		return -1;
	}

	R.nbch = nb;

	/* parse frequency args */
	nb = 0;
	ind = argind;
	while ((argF = argv[ind])) {
		ind++;
		freq = (((unsigned int)(1000000 * atof(argF)) + INTRATE / 2) / INTRATE) * INTRATE;
		if (freq < 118000000 || freq > 138000000)
			continue;

		R.channels[nb].chn = nb;
		R.channels[nb].Fr = freq;

		minF = freq < minF ? freq : minF;
		maxF = freq > maxF ? freq : maxF;
		nb++;
	};

	R.minFc = minF;
	R.maxFc = maxF;

	return 0;
}

unsigned int find_centerfreq(unsigned int minFc, unsigned int maxFc, unsigned int multiplier)
{
	if (R.Fc)
		return R.Fc;
	
	if ((maxFc - minFc) > multiplier * INTRATE - 4 * INTRATE) {
		fprintf(stderr, "Frequencies too far apart\n");
		return 0;
	}

	// the original tried to pin the center frequency to one of the provided ACARS freqs
	// there is no reason to do this, so keep this simple:
	return (maxFc + minFc) / 2;

#if 0
	for (Fc = Fd[nbch - 1] + 2 * INTRATE; Fc > Fd[0] - 2 * INTRATE; Fc--) {
		for (n = 0; n < nbch; n++) {
			if (abs(Fc - Fd[n]) > multiplier * INTRATE / 2 - 2 * INTRATE)
				break;
			if (abs(Fc - Fd[n]) < 2 * INTRATE)
				break;
			if (n > 0 && Fc - Fd[n - 1] == Fd[n] - Fc)
				break;
		}
		if (n == nbch)
			break;
	}

	return Fc;
#endif
}

int channels_init_sdr(unsigned int Fc, unsigned int multiplier, float scale)
{
	unsigned int n, ind;
	float correctionPhase;

	for (n = 0; n < R.nbch; n++) {
		channel_t *ch = &R.channels[n];

		ch->counter = 0;
		ch->D = 0;

		ch->oscillator = malloc(multiplier * sizeof(*ch->oscillator));
		ch->dm_buffer = malloc(DMBUFSZ * sizeof(*ch->dm_buffer));
		if (ch->oscillator == NULL || ch->dm_buffer == NULL) {
			perror(NULL);
			return 1;
		}

		/* precompute a scaled, oversampled local INTRATE oscillator per channel
		 mixing this oscillator with the received full-scale oversampled signal
		 will provide a normalized signal at the channel frequency */
		correctionPhase = (signed)(ch->Fr - Fc) / (float)(INTRATE * multiplier) * 2.0 * M_PI;
		if (R.verbose)
			fprintf(stderr, "#%d: Fc = %uHz, Fr = %uHz, phase = % f (%+dHz)\n", n+1, Fc, ch->Fr, correctionPhase, (signed)(ch->Fr - Fc));
		for (ind = 0; ind < multiplier; ind++)
			ch->oscillator[ind] = cexpf(correctionPhase * ind * -I) / multiplier / scale;
	}

	return 0;
}

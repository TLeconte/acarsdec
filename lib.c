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

#include "acarsdec.h"
#include "lib.h"

int parse_freqs(char **argv, const int argind, unsigned int *minFc, unsigned int *maxFc)
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
			fprintf(stderr, "WARNING: Ignoring invalid frequency %d\n", freq);
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

	if (minFc && maxFc) {
		*minFc = minF;
		*maxFc = maxF;
	}

	return 0;
}

unsigned int find_centerfreq(unsigned int minFc, unsigned int maxFc, unsigned int inrate)
{
	if (R.freq)
		return R.freq;
	
	if ((maxFc - minFc) > inrate - 4 * INTRATE) {
		fprintf(stderr, "Frequencies too far apart\n");
		return 0;
	}

	// the original tried to pin the center frequency to one of the provided ACARS freqs
	// there is no reason to do this, so keep this simple:
	return (maxFc + minFc) / 2;

#if 0
	for (Fc = Fd[nbch - 1] + 2 * INTRATE; Fc > Fd[0] - 2 * INTRATE; Fc--) {
		for (n = 0; n < nbch; n++) {
			if (abs(Fc - Fd[n]) > rtlInRate / 2 - 2 * INTRATE)
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

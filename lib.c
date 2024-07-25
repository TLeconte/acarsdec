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

#include "acarsdec.h"
#include "lib.h"

int parse_freqs(char **argv, int optind, unsigned int *minFc, unsigned int *maxFc)
{
	char *argF;
	int freq;

	// XXX TODO sanitization

	if (minFc && maxFc) {
		*minFc = 140000000;
		*maxFc = 0;
	}

	/* parse args */
	R.nbch = 0;
	while ((argF = argv[optind]) && R.nbch < MAXNBCHANNELS) {
		freq = ((int)(1000000 * atof(argF) + INTRATE / 2) / INTRATE) * INTRATE;
		optind++;
		if (freq < 118000000 || freq > 138000000) {
			fprintf(stderr, "WARNING: Invalid frequency %d\n", freq);
			continue;
		}

		R.channel[R.nbch].chn = R.nbch;
		R.channel[R.nbch].Fr = freq;
		R.nbch++;

		if (minFc && maxFc) {
			if (freq < *minFc)
				*minFc = freq;
			if (freq > *maxFc)
				*maxFc = freq;
		}
	};

	if (R.nbch > MAXNBCHANNELS)
		fprintf(stderr,
			"WARNING: too many frequencies, taking only the first %d\n",
			MAXNBCHANNELS);

	if (R.nbch == 0) {
		fprintf(stderr, "Need a least one frequency\n");
		return 1;
	}

	return 0;
}

unsigned int find_centerfreq(unsigned int minFc, unsigned int maxFc, unsigned int inrate)
{
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

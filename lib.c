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
#include <sysexits.h>
#include <err.h>

#include "acarsdec.h"
#include "lib.h"
#include "msk.h"

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
		vprerr("#%d: Fc = %uHz, Fr = %uHz, phase = % f (%+dHz)\n",
		       n+1, Fc, ch->Fr, correctionPhase, (signed)(ch->Fr - Fc));
		for (ind = 0; ind < multiplier; ind++)
			ch->oscillator[ind] = cexpf(correctionPhase * ind * -I) / multiplier / scale;
	}

	return 0;
}

/**
 * Compute the magnitude of the oversampled signal and update the channel buffer.
 * @param D an R.nbch-wide array of oversampled I/Q pairs
 * @note zeroes input
 */
static void channels_push_and_demod_sample(float complex *restrict D)
{
	static unsigned int counter = 0;
	const unsigned int nbch = R.nbch;
	unsigned int n;

	// each dm_buffer sample is made of a the magnitude of a rateMult-oversampled I/Q pair.
	// dm-buffer is rateMult-downsampled
	for (n = 0; n < nbch; n++) {
		R.channels[n].dm_buffer[counter] = cabsf(D[n]);
		D[n] = 0;
	}

	if (++counter >= DMBUFSZ) {
		for (n = 0; n < nbch; n++)
			demodMSK(&R.channels[n], DMBUFSZ);
		counter = 0;
	}
}

/**
 * Mix an array of oversampled phasors with each channel local oscillator to retrieve the signal at that channel frequency.
 * Invoke channels_push_and_demod_sample() when enough data has been accumulated.
 *
 * @param phasors an array of oversampled phasors
 * @param len the length of the phasors array (need not be a multiple of oversampling multiplier)
 * @param multiplier the oversampling multiplier
 * @note can process an arbitrary number of phasors, perf benefits are obtained if #len is always an exact multiple of #multiplier,
 * or if #len is larger than #multiplier *2
*
 * Theory of operation: together with channels_push_and_demod_sample():
 * For each channel, mix the oversampled full-scale phasor with channel downscaled oversampled
 * local oscillator and sum (integrate) the result rateMult times: this gives us a normalized complex signal
 * at the local oscillator freq. Then compute the magnitude of the resulting signal,
 * which is the magnitude of the signal received at that local oscillator freq
 */
void channels_mix_phasors(const float complex *restrict phasors, unsigned int len, const unsigned int multiplier)
{
	static float complex *restrict D = NULL;
	static unsigned int ind = 0;
	const unsigned int nbch = R.nbch;
	unsigned int n, k = 0;
	float complex d, *restrict oscillator;

	if (unlikely(!len))
		return;

	if (unlikely(!D)) {
		D = calloc(nbch, sizeof(*D));
		if (!D)
			err(EX_OSERR, NULL);
	}

	// realign to multiplier stride if necessary
	if (unlikely(ind)) {
		for (k = 0; ind < multiplier && k < len; k++, ind++) {
			for (n = 0; n < nbch; n++)
				D[n] += phasors[k] * R.channels[n].oscillator[ind];
		}
		if (likely(multiplier == ind)) {
			channels_push_and_demod_sample(D);
			ind = 0;
		}
	}

	len -= k;
	phasors += k;

	// here either (ind==0 and len>=0) or len==0

	// then use a vectorized loop for the remainder of the buffer
	while (len) {
		unsigned int lim = unlikely(len < multiplier) ? len : multiplier;	// process multiplier-sized chunks
		for (n = 0; n < nbch; n++) {
			oscillator = R.channels[n].oscillator;
			for (d = 0, k = 0; k < lim; k++)		// vectorizable
				d += phasors[k] * oscillator[k];
			D[n] = d;	// update static variable outside of loop
		}
		if (likely(multiplier == lim))
			channels_push_and_demod_sample(D);
		else	// partial write
			ind = lim;
		len -= lim;
		phasors += lim;
	}
}

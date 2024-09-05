/*
 *  Copyright (c) 2017 Thierry Leconte
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "acarsdec.h"
#include "acars.h"

#define FLEN ((INTRATE / 1200) + 1)
#define MFLTOVER 12U
#define FLENO (FLEN * MFLTOVER + 1)
static float h[FLENO];

int initMsk(channel_t *ch)
{
	unsigned int i;

	ch->MskPhi = ch->MskClk = 0;
	ch->MskS = 0;

	ch->MskDf = 0;

	ch->idx = 0;
	ch->inb = calloc(FLEN, sizeof(*ch->inb));
	if (ch->inb == NULL) {
		perror(NULL);
		return -1;
	}

	if (ch->chn == 0)
		for (i = 0; i < FLENO; i++) {
			h[i] = cosf(2.0 * M_PI * 600.0 / INTRATE / MFLTOVER * (signed)(i - (FLENO - 1) / 2));
			if (h[i] < 0)
				h[i] = 0;
		}

	return 0;
}

// ACARS is LSb first
static inline void putbit(float v, channel_t *ch)
{
	ch->outbits >>= 1;
	if (v > 0)
		ch->outbits |= 0x80;

	if (--ch->nbits == 0)
		decodeAcars(ch);
}

const float PLLG = 38e-4;
const float PLLC = 0.52;

void demodMSK(channel_t *ch, int len)
{
	/* MSK demod */
	int n;
	unsigned int idx = ch->idx;
	double p = ch->MskPhi;

	for (n = 0; n < len; n++) {
		float in;
		double s;
		float complex v;
		unsigned int j, o;

		/* VCO */
		s = 1800.0 / INTRATE * 2.0 * M_PI + ch->MskDf;
		p += s;
		if (p >= 2.0 * M_PI)
			p -= 2.0 * M_PI;

		/* mixer */
		in = ch->dm_buffer[n];
		ch->inb[idx] = in * cexp(-p * I);
		idx = (idx + 1) % FLEN;

		/* bit clock */
		ch->MskClk += s;
		if (ch->MskClk >= 3 * M_PI / 2.0 - s / 2) {
			double dphi;
			float vo, lvl;

			ch->MskClk -= 3 * M_PI / 2.0;

			/* matched filter */
			o = MFLTOVER * (ch->MskClk / s + 0.5);
			if (o > MFLTOVER)
				o = MFLTOVER;
			for (v = 0, j = 0; j < FLEN; j++, o += MFLTOVER)
				v += h[o] * ch->inb[(j + idx) % FLEN];

			/* normalize */
			lvl = cabsf(v) + 1e-8F;
			v /= lvl;

			/* update magnitude exp moving average. Average over last 8 bits */
			ch->MskMag = ch->MskMag - (1.0F/8.0F * (ch->MskMag - lvl));

			if (ch->MskS & 1) {
				vo = cimagf(v);
				dphi = (vo >= 0) ? -crealf(v) : crealf(v);
			} else {
				vo = crealf(v);
				dphi = (vo >= 0) ? cimagf(v) : -cimagf(v);
			}
			putbit((ch->MskS & 2) ? -vo : vo, ch);
			ch->MskS++;

			/* PLL filter */
			ch->MskDf = PLLC * ch->MskDf + (1.0 - PLLC) * PLLG * dphi;
		}
	}

	ch->idx = idx;
	ch->MskPhi = p;
}

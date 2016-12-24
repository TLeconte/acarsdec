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
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "acarsdec.h"

#define DCCF 0.02

#define PLLKa 1.8991680918e+02
#define PLLKb 9.8503292076e-01
#define PLLKc 0.9995

#ifdef _MSC_VER
static void sincosf(float x, float* sin, float* cos)
{
	float _sin = sinf(x);
	float _cos = cosf(x);
	if (sin)
		*sin = _sin;
	if (cos)
		*cos = _cos;
}
#endif // _MSC_VER

pthread_mutex_t chmtx;
pthread_cond_t chprcd,chcscd;
int chmsk,tmsk;

#define FLEN 11
static float h[FLEN];

int initMsk(channel_t * ch)
{
	int i;

	ch->MskFreq = (float)(1800.0 / INTRATE * 2.0 * M_PI);
	ch->MskPhi = ch->MskClk = 0;
	ch->MskS = 0;

	ch->MskKa = (float)(PLLKa / INTRATE);
	ch->MskDf = ch->Mska = 0;

	ch->Mskdc = 0;

	ch->idx = 0;
	ch->inb = calloc(FLEN, sizeof(float complex));

	for (i = 0; i < FLEN; i++) {
		if(ch->chn==0)  h[i] = cosf((float)(2.0*M_PI*600.0/INTRATE*(i-FLEN/2)));
		ch->inb[i] = 0;
	}

	return 0;
}

static inline float fst_atan2(float y, float x)
{
	float r, angle;
	float abs_y = (float)(fabs(y) + 1e-10);	// kludge to prevent 0/0 condition
	if (x >= 0) {
		r = (x - abs_y) / (x + abs_y);
		angle = (float)(M_PI_4 - M_PI_4 * r);
	} else {
		r = (x + abs_y) / (abs_y - x);
		angle = (float)(3 * M_PI_4 - M_PI_4 * r);
	}
	if (y < 0)
		return (-angle);	// negate if in quad III or IV
	else
		return (angle);
}

static inline void putbit(float v, channel_t * ch)
{
	ch->outbits >>= 1;
	if (v > 0) {
		ch->outbits |= 0x80;
	}
	ch->nbits--;
	if (ch->nbits <= 0)
		decodeAcars(ch);
}

void demodMSK(channel_t *ch,int len)
{
   /* MSK demod */
   float dphi;
   float p, s, in;
   int idx=ch->idx;
   int n;

   for(n=0;n<len;n++) {	
	/* oscilator */
	p = ch->MskFreq + ch->MskDf;
	ch->MskClk += p;
	p = ch->MskPhi + p;
	if (p >= 2.0*M_PI) p -= (float)(2.0*M_PI); 
	ch->MskPhi = p;

	if (ch->MskClk > 3*M_PI/2) {
		int j;
		float bit;
		float complex v;

		ch->MskClk -= (float)(3*M_PI/2);

		/* matched filter */
		for (j = 0, v = 0; j < FLEN; j++) {
			int k = (idx+j)%FLEN;
			v += h[j] * ch->inb[k];
		}

		if ((ch->MskS & 1) == 0) {
			if (crealf(v) >= 0) dphi = fst_atan2(-cimagf(v), crealf(v)); else dphi = fst_atan2(cimagf(v), -crealf(v));
			if (ch->MskS & 2) bit = crealf(v); else bit = -crealf(v);
			putbit(bit, ch);
		} else {
			if (cimagf(v) >= 0) dphi = fst_atan2(crealf(v), cimagf(v)); else dphi = fst_atan2(-crealf(v), -cimagf(v));
			if (ch->MskS & 2) bit = -cimagf(v);  else  bit = cimagf(v);
			putbit(bit, ch);
		}
		ch->MskS = (ch->MskS + 1) & 3;

		/* PLL */
		dphi *= ch->MskKa;
		ch->MskDf = (float)(PLLKc * ch->MskDf + dphi - PLLKb * ch->Mska);
		ch->Mska = dphi;
	}

	/* DC blocking */
	in = ch->dm_buffer[n];
	s = in - ch->Mskdc;
	ch->Mskdc = (float)((1.0 - DCCF) * ch->Mskdc + DCCF * in);

	/* mixer */
	ch->inb[idx] = s * cexpf(p*I);

	idx=(idx+1)%FLEN;
    }
    ch->idx=idx;

}


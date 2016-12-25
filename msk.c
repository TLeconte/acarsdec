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

const float PLLKa=0.0063;
const float PLLKb=0.9835;


pthread_mutex_t chmtx;
pthread_cond_t chprcd,chcscd;
int chmsk,tmsk;

#define FLEN 11
static float h[FLEN];

int initMsk(channel_t * ch)
{
	int i;

	ch->MskPhi = ch->MskClk = 0;
	ch->MskS = 0;

	ch->MskDf = ch->Mska = 0;

	ch->idx = 0;
	ch->inb = calloc(FLEN, sizeof(float complex));

	for (i = 0; i < FLEN; i++) {
		if(ch->chn==0)  h[i] = cosf(2.0*M_PI*600.0/INTRATE*(i-FLEN/2));
		ch->inb[i] = 0;
	}

	return 0;
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
	p = 1800.0 / INTRATE*2.0*M_PI + ch->MskDf;
	ch->MskClk += p;
	p = ch->MskPhi + p;
	if (p >= 2.0*M_PI) p -= 2.0*M_PI; 
	ch->MskPhi = p;

	if (ch->MskClk > 3*M_PI/2) {
		int j;
		float bit;
		int sI,sQ;
		float complex v;

		ch->MskClk -= 3*M_PI/2;

		/* matched filter */
		for (j = 0, v = 0; j < FLEN; j++) {
			int k = (idx+j)%FLEN;
			v += h[j] * ch->inb[k];
		}

		if(crealf(v)>0) sI=1; else sI=-1;
		if(cimagf(v)>0) sQ=1; else sQ=-1;
		dphi=(sI*cimag(v)-sQ*crealf(v))/(cabsf(v)+1e-5);

		if ((ch->MskS & 1) == 0) {
			if (ch->MskS & 2) bit = crealf(v); else bit = -crealf(v);
			putbit(bit, ch);
		} else {
			if (ch->MskS & 2) bit = -cimagf(v);  else  bit = cimagf(v);
			putbit(bit, ch);
		}


		ch->MskS = (ch->MskS + 1) & 3;

		/* PLL */
		dphi *=PLLKa;
		ch->MskDf -= dphi - PLLKb*ch->Mska;
		ch->Mska = dphi;
	}

	/* mixer */
	in = ch->dm_buffer[n];
	ch->inb[idx] = in * cexpf(p*I);

	ch->Msklvl = 0.99 * ch->Msklvl + 0.01*in*in;
	idx=(idx+1)%FLEN;
    }
    ch->idx=idx;

}


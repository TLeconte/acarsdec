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
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "acarsdec.h"

pthread_mutex_t chmtx;
pthread_cond_t chprcd,chcscd;
int chmsk,tmsk;

#define FLEN ((INTRATE/1200)+1)
#define MFLTOVER 12
#define FLENO (FLEN*MFLTOVER+1)
static float h[FLENO];

int initMsk(channel_t * ch)
{
	int i;

	ch->MskPhi = ch->MskClk = 0;
	ch->MskS = 0;

	ch->MskDf = 0;

	ch->idx = 0;
	ch->inb = calloc(FLEN, sizeof(float complex));
	if(ch->inb == NULL) 
		return -1;

	if(ch->chn==0) 
		for (i = 0; i < FLENO; i++) {
			h[i] = cosf(2.0*M_PI*600.0/INTRATE/MFLTOVER*(i-(FLENO-1)/2));
			if(h[i]<0) h[i]=0;
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

const float PLLG=38e-4;
const float PLLC=0.52;
void demodMSK(channel_t *ch,int len)
{
   /* MSK demod */
   int n;
   int idx=ch->idx;
   double p=ch->MskPhi;

   for(n=0;n<len;n++) {	
   	float in;
	double s;
	float complex v;
	int j,o;

	/* VCO */
	s = 1800.0/INTRATE*2.0*M_PI + ch->MskDf;
	p+=s;
	if (p >= 2.0*M_PI) p -= 2.0*M_PI; 

	/* mixer */
	in = ch->dm_buffer[n];
#ifdef DEBUG
	if(ch->chn==1) SndWrite(&in);
#endif
	ch->inb[idx] = in * cexp(-p*I);
	idx=(idx+1)%FLEN;


	/* bit clock */
	ch->MskClk+=s;
	if (ch->MskClk >=3*M_PI/2.0-s/2) {
		double dphi;
		float vo,lvl;

		ch->MskClk -= 3*M_PI/2.0;

		/* matched filter */
		o=MFLTOVER*(ch->MskClk/s+0.5);
		if(o>MFLTOVER) o=MFLTOVER;
		for (v = 0, j = 0; j < FLEN; j++,o+=MFLTOVER) {
			v += h[o]*ch->inb[(j+idx)%FLEN];
		}

		/* normalize */
		lvl=cabsf(v);
		v/=lvl+1e-8;
		ch->MskLvlSum += lvl * lvl / 4;
		ch->MskBitCount++;

		if(ch->MskS&1) {
			vo=cimagf(v);
			if(vo>=0) dphi=-crealf(v); else dphi=crealf(v);
		} else {
			vo=crealf(v);
			if(vo>=0) dphi=cimagf(v); else dphi=-cimagf(v);
		}
		if(ch->MskS&2) {
			putbit(-vo, ch);
		} else {
			putbit(vo, ch);
		}
		ch->MskS++;

		/* PLL filter */
		ch->MskDf=PLLC*ch->MskDf+(1.0-PLLC)*PLLG*dphi;
	}
    }

    ch->idx=idx;
    ch->MskPhi=p;

}


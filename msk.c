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
static float h[2*FLEN];

int initMsk(channel_t * ch)
{
	int i;

	ch->MskPhi = ch->MskClk = 0;
	ch->MskS = 0;

	ch->MskDf = ch->Mska = 0;

	ch->idx = 0;
	ch->inb = calloc(FLEN, sizeof(float complex));

	for (i = 0; i < FLEN; i++) {
		if(ch->chn==0)  {
			h[i] = h[i+FLEN]= cosf(2.0*M_PI*600.0/INTRATE*(i-FLEN/2));
		}
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

const float PLLC1=4e-8;
const float PLLC2=3.5e-3;

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
	float out[4];

	/* VCO */
	s = 1800.0/INTRATE*2.0*M_PI + ch->MskDf;
	p+=s;
	if (p >= 2.0*M_PI) p -= 2.0*M_PI; 

	/* mixer */
	in = ch->dm_buffer[n];
	ch->inb[idx] = in * cexp(-p*I);
	idx=(idx+1)%FLEN;


	/* bit clock */
	ch->MskClk+=s;
	if (ch->MskClk >=3*M_PI/2.0) {
		int sI,sQ;
		double dphi;
		float vo,lvl;

		ch->MskClk -= 3*M_PI/2.0;

		/* matched filter */
		o=FLEN-idx;
		v=0;
		for (j = 0; j < FLEN; j++,o++) {
			v += h[o]*ch->inb[j];
		}
		/* normalize */
		lvl=cabsf(v);
		v/=lvl+1e-6;
		ch->Msklvl = 0.99 * ch->Msklvl + 0.01*lvl/5.2;

		switch(ch->MskS&3) {
			case 0:
				vo=crealf(v);
				putbit(vo, ch);
				if(vo>=0) dphi=cimagf(v); else dphi=-cimagf(v);
				break;
			case 1:
				vo=cimagf(v);
				putbit(vo, ch);
				if(vo>=0) dphi=-crealf(v); else dphi=crealf(v);
				break;
			case 2:
				vo=crealf(v);
				putbit(-vo, ch);
				if(vo>=0) dphi=cimagf(v); else dphi=-cimagf(v);
				break;
			case 3:
				vo=cimagf(v);
				putbit(-vo, ch);
				if(vo>=0) dphi=-crealf(v); else dphi=crealf(v);
				break;
		}
		ch->MskS++;

		/* PLL filter */
		ch->MskDf=PLLC2*dphi+ch->Mska;
		ch->Mska+=PLLC1*dphi;

	}
    }

    ch->idx=idx;
    ch->MskPhi=p;

}


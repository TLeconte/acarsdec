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
#ifdef WITH_AIR

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <libairspy/airspy.h>
#include "acarsdec.h"

//#define AIRMULT 1600
#define AIRMULT 400
#define AIRINRATE (INTRATE*AIRMULT)

static struct airspy_device* device = NULL;


extern void *compute_thread(void *arg);

int initAirspy(char **argv, int optind)
{
	int r, n;
	char *argF;
	unsigned int F0,Fc,minFc=140000000,maxFc=0;
	unsigned int Fd[MAXNBCHANNELS];
	int result;
	uint32_t i,count;
	uint32_t * supported_samplerates;


	nbch = 0;
	while ((argF = argv[optind]) && nbch < MAXNBCHANNELS) {
		Fd[nbch] =
		    ((int)(1000000 * atof(argF) + INTRATE / 2) / INTRATE) *
		    INTRATE;
		optind++;
		if (Fd[nbch] < 118000000 || Fd[nbch] > 138000000) {
			fprintf(stderr, "WARNING: Invalid frequency %d\n",
				Fd[nbch]);
			continue;
		}
		channel[nbch].chn = nbch;
		channel[nbch].Fr = (float)Fd[nbch];
		if(Fd[nbch]<minFc) minFc= Fd[nbch];
		if(Fd[nbch]>maxFc) maxFc= Fd[nbch];
		nbch++;
	};
	if (nbch >= MAXNBCHANNELS)
		fprintf(stderr,
			"WARNING: too much frequencies, taking only the %d firsts\n",
			MAXNBCHANNELS);

	if (nbch == 0) {
		fprintf(stderr, "Need a least one  frequencies\n");
		return 1;
	}

	Fc=(minFc+maxFc)/2;
	F0=Fc+AIRINRATE/4;

	if (maxFc-minFc>AIRINRATE/2-20*INTRATE) {
		fprintf(stderr, "max Freq to far from min Freq\n");
		return 1;
	}


	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int ind;
		double AMFreq;
	        pthread_t th;

		ch->cwf = malloc(AIRMULT * sizeof(float));
		ch->swf = malloc(AIRMULT * sizeof(float));
		ch->dm_buffer = malloc(1000 * sizeof(float));
		ch->DI=ch->DQ=0;

		AMFreq = 2.0*M_PI*(double)(F0-ch->Fr)/(double)(AIRINRATE);
		for (ind = 0; ind < AIRMULT; ind++) {
			sincosf((float)(AMFreq*ind), &(ch->swf[ind]), &(ch->cwf[ind]));
			ch->swf[ind] /= AIRMULT/2;
			ch->cwf[ind] /= AIRMULT/2;
		}
	}

	result = airspy_init();
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_init() failed: %s (%d)\n", airspy_error_name(result), result);
		return -1;
	}

	result = airspy_open(&device);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_open() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_exit();
		return -1;
	}

	airspy_get_samplerates(device, &count, 0);
	supported_samplerates = (uint32_t *) malloc(count * sizeof(uint32_t));
	airspy_get_samplerates(device, supported_samplerates, count);
	for(i=0;i<count;i++)
		if(supported_samplerates[i]==AIRINRATE/2) break;
	if(i>=count) {
		fprintf(stderr,"did not find needed sampling rate\n");
		airspy_exit();
		return -1;
	}
	free(supported_samplerates);

	result = airspy_set_samplerate(device, i);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_samplerate() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}

	result = airspy_set_sample_type(device, AIRSPY_SAMPLE_FLOAT32_REAL);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_sample_type() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}

	result = airspy_set_vga_gain(device, gain);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_vga_gain() failed: %s (%d)\n", airspy_error_name(result), result);
	}

	result = airspy_set_mixer_agc(device, 1);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_mixer_agc() failed: %s (%d)\n", airspy_error_name(result), result);
	}

	result = airspy_set_lna_agc(device, 1);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_lna_agc() failed: %s (%d)\n", airspy_error_name(result), result);
	}

	if (verbose)
		fprintf(stderr, "Set freq. to %d hz\n", Fc);

	result = airspy_set_freq(device, Fc);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_freq() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}


	return 0;
}

int ind=0;
static int rx_callback(airspy_transfer_t* transfer)
{

	float* pt_rx_buffer;	
	int n,i;
        int bo,be,ben,nbk;

	pt_rx_buffer = (float *)(transfer->samples);

        bo=AIRMULT-ind;
        nbk=(transfer->sample_count-bo)/AIRMULT;
        be=nbk*AIRMULT+bo;
        ben=transfer->sample_count-be;

	for(n=0;n<nbch;n++) {
        	channel_t *ch = &(channel[n]);
                float S,in;
                int k,bn,m;
        	float DI,DQ;

        	DI=ch->DI;
        	DQ=ch->DQ;

                /* compute */
                m=0;k=0;
                for (i=ind; i < AIRMULT;i++,k++) {
                        S = pt_rx_buffer[k];
                        DI += ch->cwf[i] * S;
                        DQ += ch->swf[i] * S;
                 }
                 ch->dm_buffer[m++]=hypotf(DI,DQ);

                 for (bn=0; bn<nbk;bn++) {
                        DI=DQ=0;
                        for (i=0; i < AIRMULT;i++,k++) {
                                S = pt_rx_buffer[k];
                                DI += ch->cwf[i] * S;
                                DQ += ch->swf[i] * S;
                        }
                        ch->dm_buffer[m++]=hypotf(DI,DQ);
                 }

                 DI=DQ=0;
                 for (i=0; i<ben;i++,k++) {
                        S = pt_rx_buffer[k];
                        DI += ch->cwf[i] * S;
                        DQ += ch->swf[i] * S;
                 }

        	 ch->DI=DI;
        	 ch->DQ=DQ;

                 demodMSK(ch,m);
	}
        ind=i;

	return 0;
}


int runAirspy(void) 
{
 int result ;

 result = airspy_start_rx(device, rx_callback, NULL);
 if( result != AIRSPY_SUCCESS ) {
	fprintf(stderr,"airspy_start_rx() failed: %s (%d)\n", airspy_error_name(result), result);
	airspy_close(device);
	airspy_exit();
	return -1;
 }

 while(airspy_is_streaming(device) == AIRSPY_TRUE) {
 	sleep(2);
 }

 return 0;
}

#endif

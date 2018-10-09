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
 *	Taken the input variant from rtl.c, a variant for use with the
 *	sdrplay was created 
 *	J van Katwijk, Lazy Chair Computing (J.vanKatwijk@gmail.com)
 */
#ifdef WITH_SDRPLAY

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include	<stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include "acarsdec.h"
#include <mirsdrapi-rsp.h>

#define SDRPLAY_MULT 160
#define SDRPLAY_INRATE (INTRATE * SDRPLAY_MULT)

extern void *compute_thread (void *arg);

static	int	hwVersion;
static
unsigned int chooseFc (uint32_t *Fd, uint32_t nbch) {
int n;
int ne;
int Fc;
	do {
	   ne = 0;
	   for (n = 0; n < nbch - 1; n++) {
	      if (Fd [n] > Fd [n + 1]) {
	         uint32_t t;
	         t = Fd [n + 1];
	         Fd [n + 1] = Fd [n];
	         Fd [n] = t;
	         ne = 1;
	      }
	   }
	} while (ne != 0);

	if ((Fd [nbch - 1] - Fd [0]) > SDRPLAY_INRATE - 4 * INTRATE) {
	   fprintf(stderr, "Frequencies too far apart\n");
	   return -1;
	}

	for (Fc = Fd [nbch - 1] + 2 * INTRATE;
	     Fc > Fd [0] - 2 * INTRATE; Fc --) {
	   for (n = 0; n < nbch; n++) {
	      if (abs (Fc - Fd [n]) > SDRPLAY_INRATE / 2 - 2 * INTRATE)
	         break;
	      if (abs (Fc - Fd [n]) < 2 * INTRATE)
	         break;
	      if (n > 0 && Fc - Fd[n - 1] == Fd[n] - Fc)
	         break;
	   }
	   if (n == nbch)
	      break;
	}

	return Fc;
}

static
int     RSP1_Table [] = {0, 24, 19, 43};

static
int     RSP1A_Table [] = {0, 6, 12, 18, 20, 26, 32, 38, 57, 62};

static
int     RSP2_Table [] = {0, 10, 15, 21, 24, 34, 39, 45, 64};

static
int	RSPduo_Table [] = {0, 6, 12, 18, 20, 26, 32, 38, 57, 62};

static
int	get_lnaGRdB (int hwVersion, int lnaState) {
	switch (hwVersion) {
	   case 1:
	      return RSP1_Table [lnaState];

	   case 2:
	      return RSP2_Table [lnaState];

	   default:
	      return RSP1A_Table [lnaState];
	}
}
//
unsigned int Fc;
int initSdrplay (char **argv, int optind) {
int r, n;
char *argF;
unsigned int F0, minFc = 140000000, maxFc = 0;
unsigned int Fd [MAXNBCHANNELS];
int result;
uint32_t i;
uint	deviceIndex, numofDevs;
mir_sdr_DeviceT devDesc [4];
mir_sdr_ErrT err;

	nbch = 0;
	while ((argF = argv [optind]) && nbch < MAXNBCHANNELS) {
	   Fd [nbch] =
		    ((int)(1000000 * atof (argF) + INTRATE / 2) / INTRATE) *
		    INTRATE;
	   optind++;
	   if (Fd [nbch] < 118000000 || Fd [nbch] > 138000000) {
	      fprintf (stderr, "WARNING: Invalid frequency %d\n", Fd [nbch]);
	      continue;
	   }

	   channel [nbch]. chn = nbch;
	   channel [nbch]. Fr = (float)Fd [nbch];
	   if (Fd [nbch] < minFc)
	      minFc =  Fd[nbch];
	   if (Fd [nbch] > maxFc)
	      maxFc = Fd[nbch];
		nbch++;
	}

	if (nbch > MAXNBCHANNELS)
	   fprintf (stderr,
	        "WARNING: too much frequencies, taking only the %d firsts\n",
	        MAXNBCHANNELS);

	if (nbch == 0) {
	   fprintf(stderr, "Need a least one  frequencies\n");
	   return 1;
	}

	Fc	= chooseFc (Fd, nbch);

	for (n = 0; n < nbch; n++) {
	   channel_t *ch = &(channel[n]);
	   ch	-> counter	= 0;
	   int ind;
	   double correctionPhase;
	   ch -> D = 0;
	   ch -> oscillator = (float complex *)malloc (SDRPLAY_MULT * sizeof (float complex));
           ch -> dm_buffer = (float *)malloc (512 * sizeof (float));

           correctionPhase = (ch -> Fr - (float)Fc) / (float)(SDRPLAY_INRATE) * 2.0 * M_PI;
	   fprintf (stderr, "Fc = %d, phase = %f (%f)\n",
	                     Fc, correctionPhase, ch -> Fr - (float)Fc);
	   for (ind = 0; ind < SDRPLAY_MULT; ind++) 
	      ch -> oscillator [ind] = cexpf (correctionPhase * ind * -I) / SDRPLAY_MULT;
	}

	float	ver;
	result		= mir_sdr_ApiVersion (&ver);
	if (ver != MIR_SDR_API_VERSION) {
	   fprintf (stderr, "wrong api version %f %d\n", ver, result);
	   return -1;
	}

	mir_sdr_GetDevices (devDesc, &numofDevs, (uint32_t) 4);
        if (numofDevs == 0) {
           fprintf (stderr, "Sorry, no device found\n");
	   exit (2);
        }

	deviceIndex	= 0;
	hwVersion	= devDesc [deviceIndex]. hwVer;
	fprintf (stderr, "%s %s\n",
	           devDesc [deviceIndex]. DevNm, devDesc [deviceIndex]. SerNo);
        err = mir_sdr_SetDeviceIdx (deviceIndex);
	if (err != mir_sdr_Success) {
	   fprintf (stderr, "Cannot start with device\n");
	   return 1;
	}

	if (GRdB == -100)
	   fprintf (stderr, "SDRplay device selects freq %d and sets autogain\n", Fc);
	else
	   fprintf (stderr, "SDRplay device selects freq %d and sets %d as gain reduction\n",
	         Fc, get_lnaGRdB (hwVersion, lnaState) + GRdB);
	
	return 0;
}

static
int current_index = 0;
static
void myStreamCallback (int16_t		*xi,
	               int16_t		*xq,
	               uint32_t		firstSampleNum, 
	               int32_t		grChanged,
	               int32_t		rfChanged,
	               int32_t		fsChanged,
	               uint32_t		numSamples,
	               uint32_t		reset,
	               uint32_t		hwRemoved,
	               void		*cbContext) {
int n, i;
int	local_ind;

	for (n = 0; n < nbch; n ++) {
	   local_ind = current_index;
	   channel_t *ch = &(channel [n]);
	   float complex D	= ch -> D;
	   for (i = 0; i < numSamples; i ++) {
	      float r = ((float)(xi [i]));
	      float g = ((float)(xq [i]));
	      float complex v = r + g * I;
	      D  += v * ch -> oscillator [local_ind ++];
	      if (local_ind >= SDRPLAY_MULT) {
	         ch -> dm_buffer [ch -> counter ++] = cabsf (D) / 4;
	         local_ind = 0;
	         D = 0;
	         if (ch -> counter >= 512) {
	            demodMSK (ch, 512);
	            ch -> counter = 0;
	         }
	      }
	   }
	   ch -> D = D;
	}
	current_index	= (current_index + numSamples) % SDRPLAY_MULT;
}

static
void	myGainChangeCallback (uint32_t	gRdB,
	                      uint32_t	lnaGRdB,
	                      void	*cbContext) {
	(void)gRdB;
	(void)lnaGRdB;	
	(void)cbContext;
}

int runSdrplaySample (void) {
int result ;
int gRdBSystem	= 0;
int samplesPerPacket;
int MHz_1		= 1000000;
int	localGRdB	= (20 <= GRdB) && (GRdB <= 59) ? GRdB : 20;

	result	= mir_sdr_StreamInit (&localGRdB,
	                              ((double) (SDRPLAY_INRATE)) / MHz_1,
	                              ((double) Fc) / MHz_1,
	                              mir_sdr_BW_1_536,
	                              mir_sdr_IF_Zero,
	                              lnaState,
	                              &gRdBSystem,
	                              mir_sdr_USE_RSP_SET_GR,
	                              &samplesPerPacket,
	                              (mir_sdr_StreamCallback_t)myStreamCallback,
	                              (mir_sdr_GainChangeCallback_t)myGainChangeCallback,
	                              NULL);

	if (result != mir_sdr_Success) {
	   fprintf (stderr, "Error %d on streamInit\n", result);
	   return -1;
	}
	if (GRdB == -100) {
           result  = mir_sdr_AgcControl (mir_sdr_AGC_100HZ,
                                         -30, 0, 0, 0, 0, lnaState);
	   if (result != mir_sdr_Success) 
	      fprintf (stderr, "Error %d on AgcControl\n", result);
	}

	mir_sdr_SetPpm       ((float)ppm);
	mir_sdr_SetDcMode (4, 1);
	mir_sdr_SetDcTrackTime (63);
//
	mir_sdr_DCoffsetIQimbalanceControl (0, 1);
	while (1)
	   sleep(2);

	mir_sdr_ReleaseDeviceIdx ();
	return 0;
}

#endif

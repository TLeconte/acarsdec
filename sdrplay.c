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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "acarsdec.h"
#include "lib.h"
#include <mirsdrapi-rsp.h>

#define SDRPLAY_MULT 160
#define SDRPLAY_INRATE (INTRATE * SDRPLAY_MULT)

#define ERRPFX	"ERROR: SDRplay: "
#define WARNPFX	"WARNING: SDRplay: "

extern void *compute_thread(void *arg);

static int hwVersion;

static int RSP1_Table[] = { 0, 24, 19, 43 };

static int RSP1A_Table[] = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 };

static int RSP2_Table[] = { 0, 10, 15, 21, 24, 34, 39, 45, 64 };

static int RSPduo_Table[] = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 };

static int get_lnaGRdB(int hwVersion, int lnaState)
{
	switch (hwVersion) {
	case 1:
		return RSP1_Table[lnaState];

	case 2:
		return RSP2_Table[lnaState];

	default:
		return RSP1A_Table[lnaState];
	}
}

//
unsigned int Fc;
int initSdrplay(void)
{
	int r;
	char *argF;
	int result;
	uint32_t i;
	uint deviceIndex, numofDevs;
	mir_sdr_DeviceT devDesc[4];
	mir_sdr_ErrT err;

	Fc = find_centerfreq(R.minFc, R.maxFc, SDRPLAY_MULT);
	if (Fc == 0)
		return 1;

	r = channels_init_sdr(Fc, SDRPLAY_MULT, DMBUFSZ, 32768.0F);
	if (r)
		return r;

	float ver;
	result = mir_sdr_ApiVersion(&ver);
	if (ver != MIR_SDR_API_VERSION) {
		fprintf(stderr, ERRPFX "wrong api version %f %d\n", ver, result);
		return -1;
	}

	mir_sdr_GetDevices(devDesc, &numofDevs, (uint32_t)4);
	if (numofDevs == 0) {
		fprintf(stderr, ERRPFX "Sorry, no device found\n");
		return 2;
	}

	deviceIndex = 0;
	hwVersion = devDesc[deviceIndex].hwVer;
	fprintf(stderr, "%s %s\n", devDesc[deviceIndex].DevNm, devDesc[deviceIndex].SerNo);
	err = mir_sdr_SetDeviceIdx(deviceIndex);
	if (err != mir_sdr_Success) {
		fprintf(stderr, ERRPFX "Cannot start with device\n");
		return 1;
	}

	if (R.GRdB == -100)
		fprintf(stderr, "SDRplay device selects freq %d and sets autogain\n", Fc);
	else
		fprintf(stderr, "SDRplay device selects freq %d and sets %d as gain reduction\n",
			Fc, get_lnaGRdB(hwVersion, R.lnaState) + R.GRdB);

	return 0;
}

static unsigned int current_index = 0;
static void myStreamCallback(int16_t *xi,
			     int16_t *xq,
			     uint32_t firstSampleNum,
			     int32_t grChanged,
			     int32_t rfChanged,
			     int32_t fsChanged,
			     uint32_t numSamples,
			     uint32_t reset,
			     uint32_t hwRemoved,
			     void *cbContext)
{
	float complex phasors[SDRPLAY_MULT];
	unsigned int i, lim;

	while (numSamples) {
		lim = numSamples < SDRPLAY_MULT ? numSamples : SDRPLAY_MULT;	// mult-sized chunks
		for (i = 0; i < lim; i++) {
			float r = ((float)(xi[i]));
			float g = ((float)(xq[i]));
			phasors[i] = r + g * I;
			channels_mix_phasors(phasors, lim, SDRPLAY_MULT);
		}
		numSamples -= lim;
	}
}

static void myGainChangeCallback(uint32_t gRdB,
				 uint32_t lnaGRdB,
				 void *cbContext)
{
	(void)gRdB;
	(void)lnaGRdB;
	(void)cbContext;
}

int runSdrplaySample(void)
{
	int result;
	int gRdBSystem = 0;
	int samplesPerPacket;
	int MHz_1 = 1000000;
	int localGRdB = (20 <= R.GRdB) && (R.GRdB <= 59) ? R.GRdB : 20;

	result = mir_sdr_StreamInit(&localGRdB,
				    ((double)(SDRPLAY_INRATE)) / MHz_1,
				    ((double)Fc) / MHz_1,
				    mir_sdr_BW_1_536,
				    mir_sdr_IF_Zero,
				    R.lnaState,
				    &gRdBSystem,
				    mir_sdr_USE_RSP_SET_GR,
				    &samplesPerPacket,
				    (mir_sdr_StreamCallback_t)myStreamCallback,
				    (mir_sdr_GainChangeCallback_t)myGainChangeCallback,
				    NULL);

	if (result != mir_sdr_Success) {
		fprintf(stderr, ERRPFX "Error %d on streamInit\n", result);
		return -1;
	}
	if (R.GRdB == -100) {
		result = mir_sdr_AgcControl(mir_sdr_AGC_100HZ,
					    -30, 0, 0, 0, 0, R.lnaState);
		if (result != mir_sdr_Success)
			fprintf(stderr, ERRPFX "Error %d on AgcControl\n", result);
	}

	mir_sdr_SetPpm((float)R.ppm);
	mir_sdr_SetDcMode(4, 1);
	mir_sdr_SetDcTrackTime(63);
	//
	mir_sdr_DCoffsetIQimbalanceControl(0, 1);
	while (R.running)
		sleep(2);

	mir_sdr_ReleaseDeviceIdx();
	return 0;
}

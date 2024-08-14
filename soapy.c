// 2024 changes (C) 2024 Thibaut VARENE

#define _GNU_SOURCE

#include <complex.h>
#include <math.h>
#include <signal.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "acarsdec.h"
#include "lib.h"

static SoapySDRDevice *dev = NULL;
static SoapySDRStream *stream = NULL;
static int soapyExit = 0;

#define SOAPYOUTBUFSZ 1024U

int initSoapy(char *optarg)
{
	int r;
	unsigned int Fc;

	if (optarg == NULL) {
		fprintf(stderr, "Need device string (ex: driver=rtltcp,rtltcp=127.0.0.1) after -d\n");
		exit(1);
	}

	dev = SoapySDRDevice_makeStrArgs(optarg);
	if (dev == NULL) {
		fprintf(stderr, "ERROR: opening SoapySDR device using string \"%s\": %s\n", optarg, SoapySDRDevice_lastError());
		return -1;
	}

	if (R.gain <= -10.0) {
		if (R.verbose)
			fprintf(stderr, "Tuner gain: AGC\n");
		r = SoapySDRDevice_setGainMode(dev, SOAPY_SDR_RX, 0, 1);
		if (r != 0)
			fprintf(stderr, "WARNING: Failed to turn on AGC: %s\n", SoapySDRDevice_lastError());
	} else {
		r = SoapySDRDevice_setGainMode(dev, SOAPY_SDR_RX, 0, 0);
		if (r != 0)
			fprintf(stderr, "WARNING: Failed to turn off AGC: %s\n", SoapySDRDevice_lastError());
		if (R.verbose)
			fprintf(stderr, "Setting gain to: %f\n", R.gain);
		r = SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, 0, R.gain);
		if (r != 0)
			fprintf(stderr, "WARNING: Failed to set gain: %s\n", SoapySDRDevice_lastError());
	}

	if (R.ppm != 0) {
		r = SoapySDRDevice_setFrequencyCorrection(dev, SOAPY_SDR_RX, 0, R.ppm);
		if (r != 0)
			fprintf(stderr, "WARNING: Failed to set freq correction: %s\n", SoapySDRDevice_lastError());
	}

	Fc = find_centerfreq(R.minFc, R.maxFc, R.rateMult);

	if (Fc == 0)
		return 1;

	r = channels_init_sdr(Fc, R.rateMult, SOAPYOUTBUFSZ, 1.0F);
	if (r)
		return r;

	if (R.verbose)
		fprintf(stderr, "Setting center freq. to %uHz\n", Fc);
	r = SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, 0, Fc, NULL);
	if (r != 0) {
		fprintf(stderr, "ERROR: Failed to set frequency: %s\n", SoapySDRDevice_lastError());
		return r;
	}

	if (R.verbose)
		fprintf(stderr, "Setting sample rate: %.4f MS/s\n", INTRATE * R.rateMult / 1e6);
	r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, 0, INTRATE * R.rateMult);
	if (r != 0) {
		fprintf(stderr, "ERROR: Failed to set sample rate: %s\n", SoapySDRDevice_lastError());
		return r;
	}

	if (R.antenna) {
		if (SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, 0, R.antenna) != 0) {
			fprintf(stderr, "ERROR: SoapySDRDevice_setAntenna failed (check antenna validity)\n");
			return 1;
		}
	}

	return 0;
}

#define SOAPYINBUFSZ 4096U
int runSoapySample(void)
{
	float complex D[R.nbch];
	float complex soapyInBuf[SOAPYINBUFSZ];
	void *bufs[] = { soapyInBuf };

	unsigned int ind = 0;
	unsigned int n, counter = 0;
	int m, res, flags = 0;
	long long timens = 0;

	stream = SoapySDRDevice_setupStream(dev, SOAPY_SDR_RX, SOAPY_SDR_CF32, NULL, 0, NULL);
	if (!stream) {
		fprintf(stderr, "WARNING: Failed to setup SoapySDR stream: %s\n", SoapySDRDevice_lastError());
		return 1;
	}

	if (SoapySDRDevice_activateStream(dev, stream, 0, 0, 0)) {
		fprintf(stderr, "WARNING: Failed to activate SoapySDR stream: %s\n", SoapySDRDevice_lastError());
		return 1;
	}

	for (n = 0; n < R.nbch; n++)
		D[n] = 0;

	while (!soapyExit) {
		flags = 0;
		res = SoapySDRDevice_readStream(dev, stream, bufs, SOAPYINBUFSZ, &flags, &timens, 10000000);
		if (res == 0) {
			usleep(500);
			continue; // retry
		}
		if (res < 0) {
			if (res == SOAPY_SDR_OVERFLOW)
				continue;
			fprintf(stderr, "WARNING: Failed to read SoapySDR stream (%d): %s\n", res, SoapySDRDevice_lastError());
			return 1;
		}

		for (m = 0; m < res; m++) {
			float complex phasor = soapyInBuf[m];

			for (n = 0; n < R.nbch; n++) {
				channel_t *ch = &(R.channels[n]);

				if (!ind) {	// NB first dm_buffer at startup will be 0. Considered harmless.
					ch->dm_buffer[counter] = cabsf(D[n]);
					D[n] = 0;

					if (n == R.nbch-1)	// update counter after last channel is processed
						counter++;
				}
				D[n] += phasor * ch->oscillator[ind];
			}
			if (++ind >= R.rateMult)
				ind = 0;

			if (counter >= SOAPYOUTBUFSZ) {
				for (n = 0; n < R.nbch; n++)
					demodMSK(&R.channels[n], SOAPYOUTBUFSZ);
				counter = 0;
			}
		}
	}
	return 0;
}

int runSoapyClose(void)
{
	int res = 0;
	soapyExit = 1;

	if (dev) {
		if (stream) {
			res = SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
			if (res != 0)
				fprintf(stderr, "WARNING: Failed to deactivate SoapySDR stream: %s\n", SoapySDRDevice_lastError());

			res = SoapySDRDevice_closeStream(dev, stream);
			if (res != 0)
				fprintf(stderr, "WARNING: Failed to close SoapySDR stream: %s\n", SoapySDRDevice_lastError());
			stream = NULL;
		}
		res = SoapySDRDevice_unmake(dev);
		if (res != 0)
			fprintf(stderr, "WARNING: Failed to close SoapySDR device: %s\n", SoapySDRDevice_lastError());
		dev = NULL;
	}

	return res;
}

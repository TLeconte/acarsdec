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
static int16_t *soapyInBuf = NULL;
static unsigned int soapyInBufSize = 0;
static int soapyExit = 0;

#define SOAPYOUTBUFSZ 1024U

int initSoapy(char **argv, int optind)
{
	int r;
	unsigned int Fc, minFc, maxFc;

	if (argv[optind] == NULL) {
		fprintf(stderr, "Need device string (ex: driver=rtltcp,rtltcp=127.0.0.1) after -d\n");
		exit(1);
	}

	dev = SoapySDRDevice_makeStrArgs(argv[optind]);
	if (dev == NULL) {
		fprintf(stderr, "Error opening SoapySDR device using string \"%s\": %s\n", argv[optind], SoapySDRDevice_lastError());
		return -1;
	}
	optind++;

	soapyInBufSize = SOAPYOUTBUFSZ * R.rateMult * 2U;

	soapyInBuf = malloc(sizeof(*soapyInBuf) * soapyInBufSize);
	if (!soapyInBuf) {
		perror(NULL);
		return 1;
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

	r = parse_freqs(argv, optind, &minFc, &maxFc);
	if (r)
		return r;

	Fc = find_centerfreq(minFc, maxFc, R.rateMult);

	if (Fc == 0)
		return 1;

	r = channels_init_sdr(Fc, R.rateMult, SOAPYOUTBUFSZ, 32768.0F);
	if (r)
		return r;

	if (R.verbose)
		fprintf(stderr, "Set center freq. to %uHz\n", Fc);
	r = SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, 0, Fc, NULL);
	if (r != 0)
		fprintf(stderr, "WARNING: Failed to set frequency: %s\n", SoapySDRDevice_lastError());

	if (R.verbose)
		fprintf(stderr, "Setting sample rate: %.4f MS/s\n", INTRATE * R.rateMult / 1e6);
	r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, 0, INTRATE * R.rateMult);
	if (r != 0)
		fprintf(stderr, "WARNING: Failed to set sample rate: %s\n", SoapySDRDevice_lastError());

	stream = SoapySDRDevice_setupStream(dev, SOAPY_SDR_RX, SOAPY_SDR_CS16, NULL, 0, NULL);
	if (!stream) {
		fprintf(stderr, "WARNING: Failed to setup SoapySDR stream: %s\n", SoapySDRDevice_lastError());
		return 1;
	}

	return 0;
}

int soapySetAntenna(const char *antenna)
{
	if (dev == NULL) {
		fprintf(stderr, "soapySetAntenna: SoapySDR not init'd\n");
		return 1;
	}

	if (antenna == NULL) {
		fprintf(stderr, "soapySetAntenna: antenna is NULL\n");
		return 1;
	}

	if (SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, 0, antenna) != 0) {
		fprintf(stderr, "soapySetAntenna: SoapySDRDevice_setAntenna failed (check antenna validity)\n");
		return 1;
	}

	return 0;
}

int runSoapySample(void)
{
	float complex D[R.nbch];
	unsigned int ind = 0;
	unsigned int n, counter = 0;
	int m, res = 0;
	int flags = 0;
	long long timens = 0;
	void *bufs[] = { soapyInBuf };

	if (SoapySDRDevice_activateStream(dev, stream, 0, 0, 0)) {
		fprintf(stderr, "WARNING: Failed to activate SoapySDR stream: %s\n", SoapySDRDevice_lastError());
		return 1;
	}

	for (n = 0; n < R.nbch; n++)
		D[n] = 0;

	while (!soapyExit) {
		flags = 0;
		res = SoapySDRDevice_readStream(dev, stream, bufs, soapyInBufSize / 2U, &flags, &timens, 10000000);
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

		for (m = 0; m < res * 2; m += 2) {
			float i = (float)soapyInBuf[m];
			float q = (float)soapyInBuf[m+1];
			float complex phasor = i + q * I;

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

	if (soapyInBuf) {
		free(soapyInBuf);
		soapyInBuf = NULL;
	}

	return res;
}

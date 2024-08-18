// 2024 changes (C) 2024 Thibaut VARENE

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

#define ERRPFX	"ERROR: SOAPYSDR: "
#define WARNPFX	"WARNING: SOAPYSDR: "

static SoapySDRDevice *dev = NULL;

int initSoapy(char *optarg)
{
	int r;
	unsigned int Fc;

	if (!optarg)
		return 1;	// cannot happen after getopt()

	Fc = find_centerfreq(R.minFc, R.maxFc, R.rateMult);
	if (!Fc)
		return 1;

	dev = SoapySDRDevice_makeStrArgs(optarg);
	if (dev == NULL) {
		fprintf(stderr, ERRPFX "opening SoapySDR device using string \"%s\": %s\n", optarg, SoapySDRDevice_lastError());
		return -1;
	}

	if (R.gain <= -10.0) {
		vprerr("Tuner gain: AGC\n");
		r = SoapySDRDevice_setGainMode(dev, SOAPY_SDR_RX, 0, 1);
		if (r != 0)
			fprintf(stderr, WARNPFX "Failed to turn on AGC: %s\n", SoapySDRDevice_lastError());
	} else {
		r = SoapySDRDevice_setGainMode(dev, SOAPY_SDR_RX, 0, 0);
		if (r != 0)
			fprintf(stderr, WARNPFX "Failed to turn off AGC: %s\n", SoapySDRDevice_lastError());
		vprerr("Setting gain to: %f\n", R.gain);
		r = SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, 0, R.gain);
		if (r != 0)
			fprintf(stderr, WARNPFX "Failed to set gain: %s\n", SoapySDRDevice_lastError());
	}

	if (R.ppm != 0) {
		r = SoapySDRDevice_setFrequencyCorrection(dev, SOAPY_SDR_RX, 0, R.ppm);
		if (r != 0)
			fprintf(stderr, WARNPFX "Failed to set frequency correction: %s\n", SoapySDRDevice_lastError());
	}

	r = channels_init_sdr(Fc, R.rateMult, 1.0F);
	if (r)
		return r;

	vprerr("Setting center freq: %.4f MHz\n", Fc / 1e6);
	r = SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, 0, Fc, NULL);
	if (r != 0) {
		fprintf(stderr, ERRPFX "Failed to set center frequency: %s\n", SoapySDRDevice_lastError());
		return r;
	}

	vprerr("Setting sample rate: %.4f MS/s\n", INTRATE * R.rateMult / 1e6);
	r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, 0, INTRATE * R.rateMult);
	if (r != 0) {
		fprintf(stderr, ERRPFX "Failed to set sample rate: %s\n", SoapySDRDevice_lastError());
		return r;
	}

	if (R.antenna) {
		if (SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, 0, R.antenna) != 0) {
			fprintf(stderr, ERRPFX "Failed to set antenna (check antenna validity)\n");
			return 1;
		}
	}

	return 0;
}

#define SOAPYINBUFSZ 4096U
int runSoapySample(void)
{
	const unsigned int mult = R.rateMult;
	float complex soapyInBuf[SOAPYINBUFSZ];
	void *bufs[] = { soapyInBuf };
	SoapySDRStream *stream;

	int res = 0, flags = 0;
	long long timens = 0;

	if (!dev)
		return 1;	// cannot happen after initSoapy returns 0

	stream = SoapySDRDevice_setupStream(dev, SOAPY_SDR_RX, SOAPY_SDR_CF32, NULL, 0, NULL);
	if (!stream) {
		fprintf(stderr, ERRPFX "Failed to setup SoapySDR stream: %s\n", SoapySDRDevice_lastError());
		res = 1;
		goto faildev;
	}

	if (SoapySDRDevice_activateStream(dev, stream, 0, 0, 0)) {
		fprintf(stderr, ERRPFX "Failed to activate SoapySDR stream: %s\n", SoapySDRDevice_lastError());
		res = 1;
		goto failstream;
	}

	while (likely(R.running)) {
		flags = 0;
		res = SoapySDRDevice_readStream(dev, stream, bufs, SOAPYINBUFSZ, &flags, &timens, 10000000);
		if (unlikely(res == 0)) {
			usleep(500);
			continue; // retry
		}
		if (unlikely(res < 0)) {
			if (res == SOAPY_SDR_OVERFLOW)
				continue;
			fprintf(stderr, ERRPFX "Failed to read SoapySDR stream (%d): %s\n", res, SoapySDRDevice_lastError());
			break;
		}

		channels_mix_phasors(soapyInBuf, res, mult);
	}

	if (SoapySDRDevice_deactivateStream(dev, stream, 0, 0) != 0)
		fprintf(stderr, WARNPFX "Failed to deactivate SoapySDR stream: %s\n", SoapySDRDevice_lastError());
failstream:
	if (SoapySDRDevice_closeStream(dev, stream) != 0)
		fprintf(stderr, WARNPFX "Failed to close SoapySDR stream: %s\n", SoapySDRDevice_lastError());
faildev:
	if (SoapySDRDevice_unmake(dev) != 0)
		fprintf(stderr, WARNPFX "Failed to close SoapySDR device: %s\n", SoapySDRDevice_lastError());
	dev = NULL;

	return res;
}

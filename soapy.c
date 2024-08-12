
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
static unsigned int soapyInRate = 0;
static int soapyExit = 0;

#define SOAPYOUTBUFSZ 1024U

int initSoapy(char **argv, int optind)
{
	int r;
	char *argF;
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
	soapyInRate = INTRATE * R.rateMult;

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

	Fc = find_centerfreq(minFc, maxFc, soapyInRate);

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
		fprintf(stderr, "Setting sample rate: %.4f MS/s\n", soapyInRate / 1e6);
	r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, 0, soapyInRate);
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
	int current_index = 0;
	unsigned int n, local_ind;
	int i, res = 0;
	int flags = 0;
	long long timens = 0;
	void *bufs[] = { soapyInBuf };

	if (SoapySDRDevice_activateStream(dev, stream, 0, 0, 0)) {
		fprintf(stderr, "WARNING: Failed to activate SoapySDR stream: %s\n", SoapySDRDevice_lastError());
		return 1;
	}

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
			if (!soapyExit) {
				fprintf(stderr, "WARNING: Failed to read SoapySDR stream (%d): %s\n", res, SoapySDRDevice_lastError());
				return 1;
			}
			break;
		}

		for (n = 0; n < R.nbch; n++) {
			local_ind = current_index;
			channel_t *ch = &(R.channels[n]);
			float complex D = ch->D;

			for (i = 0; i < res * 2; i += 2) {
				float r = (float)soapyInBuf[i];
				float g = (float)soapyInBuf[i + 1];
				float complex v = r + g * I;
				D += v * ch->oscillator[local_ind++];
				if (local_ind >= R.rateMult) {
					ch->dm_buffer[ch->counter++] = cabsf(D);
					local_ind = 0;
					D = 0;
					if (ch->counter >= SOAPYOUTBUFSZ) {
						demodMSK(ch, SOAPYOUTBUFSZ);
						ch->counter = 0;
					}
				}
			}
			ch->D = D;
		}
		current_index = (current_index + res) % R.rateMult;
	}
	return 0;
}

int runSoapyClose(void)
{
	int res = 0;
	soapyExit = 1;

	if (stream) {
		res = SoapySDRDevice_closeStream(dev, stream);
		stream = NULL;
		if (res != 0)
			fprintf(stderr, "WARNING: Failed to close SoapySDR stream: %s\n", SoapySDRDevice_lastError());

		res = SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
		stream = NULL;
		if (res != 0)
			fprintf(stderr, "WARNING: Failed to deactivate SoapySDR stream: %s\n", SoapySDRDevice_lastError());
	}
	if (dev) {
		res = SoapySDRDevice_unmake(dev);
		dev = NULL;
		if (res != 0)
			fprintf(stderr, "WARNING: Failed to close SoapySDR device: %s\n", SoapySDRDevice_lastError());
	}

	if (soapyInBuf) {
		free(soapyInBuf);
		soapyInBuf = NULL;
	}

	return res;
}

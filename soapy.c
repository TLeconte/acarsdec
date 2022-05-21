#ifdef WITH_SOAPY

#define _GNU_SOURCE

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Types.h>
#include <unistd.h>

#include "acarsdec.h"

static SoapySDRDevice *dev = NULL;
static SoapySDRStream *stream = NULL;
static int soapyInBufSize = 0;
static int soapyOutBufSize = 0;
static int soapyInRate = 0;
static int soapyMTU = 0;

static unsigned int chooseFc(unsigned int *Fd, unsigned int nbch)
{
	int n;
	int ne;
	int Fc;
	do {
		ne = 0;
		for (n = 0; n < nbch - 1; n++) {
			if (Fd[n] > Fd[n + 1]) {
				unsigned int t;
				t = Fd[n + 1];
				Fd[n + 1] = Fd[n];
				Fd[n] = t;
				ne = 1;
			}
		}
	} while (ne);

	if ((Fd[nbch - 1] - Fd[0]) > soapyInRate - 4 * INTRATE) {
		fprintf(stderr, "Frequencies too far apart\n");
		return 0;
	}

	for (Fc = Fd[nbch - 1] + 2 * INTRATE; Fc > Fd[0] - 2 * INTRATE; Fc--) {
		for (n = 0; n < nbch; n++) {
			if (abs(Fc - Fd[n]) > soapyInRate / 2 - 2 * INTRATE)
				break;
			if (abs(Fc - Fd[n]) < 2 * INTRATE)
				break;
			if (n > 0 && Fc - Fd[n - 1] == Fd[n] - Fc)
				break;
		}
		if (n == nbch)
			break;
	}

	return Fc;
}

int initSoapy(char **argv, int optind)
{
	int r, n;
	char *argF;
	unsigned int Fc;
	unsigned int Fd[MAXNBCHANNELS];

	if (argv[optind] == NULL) {
		fprintf(stderr, "Need device string (ex: driver=rtltcp,rtltcp=127.0.0.1) after -d\n");
		exit(1);
	}

	if (verbose)
		fprintf(stderr, "Opening SoapySDR device %s\n", argv[optind]);
	dev = SoapySDRDevice_makeStrArgs(argv[optind]);
	if(dev == NULL) {
		fprintf(stderr, "Error opening SoapySDR device using string \"%s\": %s", argv[optind], SoapySDRDevice_lastError());
		return -1;
	}
	optind++;

    soapyOutBufSize = 1024;
    soapyInBufSize = soapyOutBufSize * rateMult * 2;
    soapyInRate = INTRATE * rateMult;

	if (gain == -10.0) {
		if (verbose)
			fprintf(stderr, "Enabling AGC\n");
		r = SoapySDRDevice_setGainMode(dev, SOAPY_SDR_RX, 0, 1);
		if (r != 0)
			fprintf(stderr, "WARNING: Failed to turn on AGC: %s\n", SoapySDRDevice_lastError());
	} else {
		r = SoapySDRDevice_setGainMode(dev, SOAPY_SDR_RX, 0, 0);
		if (r != 0)
			fprintf(stderr, "WARNING: Failed to turn off AGC: %s\n", SoapySDRDevice_lastError());
        if (verbose)
            fprintf(stderr, "Setting gain to: %f\n", gain);
		r = SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, 0, gain);
		if (r != 0)
			fprintf(stderr, "WARNING: Failed to set gain: %s\n", SoapySDRDevice_lastError());
	}

	if (ppm != 0) {
		r = SoapySDRDevice_setFrequencyCorrection(dev, SOAPY_SDR_RX, 0, ppm);
		if (r != 0)
			fprintf(stderr, "WARNING: Failed to set freq correction: %s\n", SoapySDRDevice_lastError());
	}

	nbch = 0;
	while ((argF = argv[optind]) && nbch < MAXNBCHANNELS) {
		Fd[nbch] =
		    ((int)(1000000 * atof(argF) + INTRATE / 2) / INTRATE) * INTRATE;
		if (verbose) {
			fprintf(stderr, "Found string frequency %s\n", argF);
			fprintf(stderr, "Found frequency %d\n", Fd[nbch]);
		}
		optind++;
		if (Fd[nbch] < 118000000 || Fd[nbch] > 138000000) {
			fprintf(stderr, "WARNING: frequency not in range 118-138 MHz: %d\n",
				Fd[nbch]);
			continue;
		}
		channel[nbch].chn = nbch;
		channel[nbch].Fr = (float)Fd[nbch];
		nbch++;
	};
	if (nbch > MAXNBCHANNELS)
		fprintf(stderr,
			"WARNING: too many frequencies, using only the first %d\n",
			MAXNBCHANNELS);

	if (nbch == 0) {
		fprintf(stderr, "Need a least one frequency\n");
		return 1;
	}

	if(freq == 0)
		freq = chooseFc(Fd, nbch);

	for (n = 0; n < nbch; n++) {
		if (Fd[n] < freq - soapyInRate/2 || Fd[n] > freq + soapyInRate/2) {
			fprintf(stderr, "WARNING: frequency not in tuned range %d-%d: %d\n",
				freq - soapyInRate/2, freq + soapyInRate/2, Fd[n]);
			continue;
		}
	}

	stream = SoapySDRDevice_setupStream(dev, SOAPY_SDR_RX, SOAPY_SDR_CS16, NULL, 0, NULL);

	soapyMTU = SoapySDRDevice_getStreamMTU(dev, stream);
	if (soapyMTU <= 0) {
		fprintf(stderr, "WARNING: Failed to get stream MTU: %s\n", SoapySDRDevice_lastError());
	}

	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int ind;
		float AMFreq;

		ch->wf = malloc(rateMult * sizeof(float complex));
		ch->dm_buffer=malloc(soapyOutBufSize*sizeof(float));

		AMFreq = (ch->Fr - (float)freq) / (float)(soapyInRate) * 2.0 * M_PI;
		for (ind = 0; ind < rateMult; ind++) {
			ch->wf[ind]=cexpf(AMFreq*ind*-I)/rateMult/127.5;
		}
	}

	if (verbose)
		fprintf(stderr, "Set center freq. to %dHz\n", (int)freq);
	r = SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, 0, freq, NULL);
	if (r != 0)
		fprintf(stderr, "WARNING: Failed to set frequency: %s\n", SoapySDRDevice_lastError());

	if (verbose)
		fprintf(stderr, "Setting sample rate: %.4f MS/s\n", soapyInRate / 1e6);
	r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, 0, soapyInRate);
	if (r != 0)
		fprintf(stderr, "WARNING: Failed to set sample rate: %s\n", SoapySDRDevice_lastError());

	return 0;
}

int runSoapySample(void)
{
	// from rtl

	SoapySDRDevice_closeStream(dev, stream);
	SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
	SoapySDRDevice_unmake(dev);
	return 0;
}

#endif

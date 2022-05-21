#ifdef WITH_SOAPY

#define _GNU_SOURCE

#include <math.h>
#include <pthread.h>
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
static int watchdogCounter = 50;
static pthread_mutex_t cbMutex = PTHREAD_MUTEX_INITIALIZER;

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

static void in_callback(unsigned char *soapyinbuff, uint32_t nread, void *ctx)
{
	int n;

	pthread_mutex_lock(&cbMutex);
	watchdogCounter = 50;
	pthread_mutex_unlock(&cbMutex);

	if (nread != soapyInBufSize) {
		fprintf(stderr, "warning: partial read\n");
		return;

	}
	status=0;

	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int i,m;
		float complex D,*wf;

		wf = ch->wf;
		m=0;
		for (i = 0; i < soapyInBufSize;) {
			int ind;

			D = 0;
			for (ind = 0; ind < rateMult; ind++) {
				float r, g;
				float complex v;

				r = (float)soapyinbuff[i] - (float)127.37; i++;
				g = (float)soapyinbuff[i] - (float)127.37; i++;

				v=r+g*I;
				D+=v*wf[ind];
			}
			ch->dm_buffer[m++]=cabsf(D);
		}
		demodMSK(ch,m);
	}
}

static void *readThreadEntryPoint(void *arg) {
	rtlsdr_read_async(dev, in_callback, NULL, 4, soapyInBufSize);
	pthread_mutex_lock(&cbMutex);
	signalExit = 1;
	pthread_mutex_unlock(&cbMutex);
	return NULL;
}

int runSoapySample(void)
{
	pthread_t readThread;
	pthread_create(&readThread, NULL, readThreadEntryPoint, NULL);

	pthread_mutex_lock(&cbMutex);

	while (!signalExit) {
		if (--watchdogCounter <= 0) {
			fprintf(stderr, "No data from SoapySDR for 5 seconds, exiting ...\n");
			runSoapyCancel(); // watchdog triggered after 5 seconds of no data from SoapySDR
			break;
		}
		pthread_mutex_unlock(&cbMutex);
		usleep(100 * 1000); // 0.1 seconds
		pthread_mutex_lock(&cbMutex);
	}

	pthread_mutex_unlock(&cbMutex);

	int count = 100; // 10 seconds
	int err = 0;
	// Wait on reader thread exit
	while (count-- > 0 && (err = pthread_tryjoin_np(readThread, NULL))) {
		usleep(100 * 1000); // 0.1 seconds
	}
	if (err) {
		fprintf(stderr, "Receive thread termination failed, will raise SIGKILL to ensure we die!\n");
		raise(SIGKILL);
		return 1;
	}
	return 0;
}

int runSoapyCancel(void) {
	if (dev && stream) {
		SoapySDRDevice_closeStream(dev, stream);
	}
	return 0;
}

int runSoapyClose(void) {
	int res = 0;
	if (stream) {
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
	if (res) {
		fprintf(stderr, "rtlsdr_close: %d\n", res);
	}

	return res;
}

#endif

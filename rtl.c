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
#ifdef WITH_RTL

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <rtl-sdr.h>
#include "acarsdec.h"

#define RTLMULT 200
#define RTLINRATE (INTRATE*RTLMULT)

static rtlsdr_dev_t *dev = NULL;
static int status = 0;

#define RTLOUTBUFSZ 1024
#define RTLINBUFSZ (RTLOUTBUFSZ*RTLMULT*2)

/* function verbose_device_search by Kyle Keen
 * from http://cgit.osmocom.org/rtl-sdr/tree/src/convenience/convenience.c
 */
int verbose_device_search(char *s)
{
	int i, device_count, device, offset;
	char *s2;
	char vendor[256], product[256], serial[256];
	device_count = rtlsdr_get_device_count();
	if (!device_count) {
		fprintf(stderr, "No supported devices found.\n");
		return -1;
	}
	if (verbose)
		fprintf(stderr, "Found %d device(s):\n", device_count);
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (verbose)
			fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor,
				product, serial);
	}
	if (verbose)
		fprintf(stderr, "\n");
	/* does string look like raw id number */
	device = (int)strtol(s, &s2, 0);
	if (s2[0] == '\0' && device >= 0 && device < device_count) {
		if (verbose)
			fprintf(stderr, "Using device %d: %s\n",
				device,
				rtlsdr_get_device_name((uint32_t) device));
		return device;
	}
	/* does string exact match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strcmp(s, serial) != 0) {
			continue;
		}
		device = i;
		if (verbose)
			fprintf(stderr, "Using device %d: %s\n",
				device,
				rtlsdr_get_device_name((uint32_t) device));
		return device;
	}
	/* does string prefix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strncmp(s, serial, strlen(s)) != 0) {
			continue;
		}
		device = i;
		if (verbose)
			fprintf(stderr, "Using device %d: %s\n",
				device,
				rtlsdr_get_device_name((uint32_t) device));
		return device;
	}
	/* does string suffix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		offset = strlen(serial) - strlen(s);
		if (offset < 0) {
			continue;
		}
		if (strncmp(s, serial + offset, strlen(s)) != 0) {
			continue;
		}
		device = i;
		if (verbose)
			fprintf(stderr, "Using device %d: %s\n",
				device,
				rtlsdr_get_device_name((uint32_t) device));
		return device;
	}
	fprintf(stderr, "No matching devices found.\n");
	return -1;
}

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

	if ((Fd[nbch - 1] - Fd[0]) > RTLINRATE - 4 * INTRATE) {
		fprintf(stderr, "Frequencies to far apart\n");
		return -1;
	}

	for (Fc = Fd[nbch - 1] + 2 * INTRATE; Fc > Fd[0] - 2 * INTRATE; Fc--) {
		for (n = 0; n < nbch; n++) {
			if (abs(Fc - Fd[n]) > RTLINRATE / 2 - 2 * INTRATE)
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

int nearest_gain(int target_gain)
{
	int i, err1, err2, count, close_gain;
	int *gains;
	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return 0;
	}
	gains = malloc(sizeof(int) * count);
	count = rtlsdr_get_tuner_gains(dev, gains);
	close_gain = gains[0];
	for (i = 0; i < count; i++) {
		err1 = abs(target_gain - close_gain);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1) {
			close_gain = gains[i];
		}
	}
	free(gains);
	if (verbose)
		fprintf(stderr, "Tuner gain : %f\n", (float)close_gain / 10.0);
	return close_gain;
}

int initRtl(char **argv, int optind)
{
	int r, n;
	int dev_index;
	char *argF;
	unsigned int Fc;
	unsigned int Fd[4];

	if (argv[optind] == NULL) {
		fprintf(stderr, "Need device name or index (ex: 0) after -r\n");
		exit(1);
	}
	dev_index = verbose_device_search(argv[optind]);
	optind++;

	r = rtlsdr_open(&dev, dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device\n");
		return r;
	}

	rtlsdr_set_tuner_gain_mode(dev, 1);
	r = rtlsdr_set_tuner_gain(dev, nearest_gain(gain));
	if (r < 0)
		fprintf(stderr, "WARNING: Failed to set gain.\n");

	if (ppm != 0) {
		r = rtlsdr_set_freq_correction(dev, ppm);
		if (r < 0)
			fprintf(stderr,
				"WARNING: Failed to set freq. correction\n");
	}

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

	Fc = chooseFc(Fd, nbch);
	if (Fc == 0)
		return 1;

	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int ind;
		float AMFreq;

		ch->cwf = malloc(RTLMULT * sizeof(float));
		ch->swf = malloc(RTLMULT * sizeof(float));
		ch->dm_buffer=malloc(RTLOUTBUFSZ*sizeof(float));

		AMFreq = (ch->Fr - (float)Fc) / (float)(RTLINRATE) * 2.0 * M_PI;
		for (ind = 0; ind < RTLMULT; ind++) {
			sincosf(AMFreq * ind, &(ch->swf[ind]), &(ch->cwf[ind]));
			ch->swf[ind] /= RTLMULT/2;
			ch->cwf[ind] /= RTLMULT/2;
		}
	}

	if (verbose)
		fprintf(stderr, "Set center freq. to %dHz\n", (int)Fc);

	r = rtlsdr_set_center_freq(dev, Fc);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set center freq.\n");
		return 1;
	}

	r = rtlsdr_set_sample_rate(dev, RTLINRATE);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");
		return 1;
	}

	r = rtlsdr_reset_buffer(dev);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to reset buffers.\n");
		return 1;
	}

	return 0;
}

static void in_callback(unsigned char *rtlinbuff, uint32_t nread, void *ctx)
{
	int r, n;

	if (nread != RTLINBUFSZ) {
		fprintf(stderr, "warning: partial read\n");
		return;

	}
	status=0;

	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int i,m;
		float DI, DQ;
		float *swf, *cwf;

		swf = ch->swf;
		cwf = ch->cwf;
		m=0;
		for (i = 0; i < RTLINBUFSZ;) {
			int ind;

			DI = DQ = 0;
			for (ind = 0; ind < RTLMULT; ind++) {
				float I, Q;
				float sp, cp;

				I = (float)rtlinbuff[i] - (float)127.5; i++;
				Q = (float)rtlinbuff[i] - (float)127.5; i++;

				sp = swf[ind]; cp = cwf[ind];
				DI += cp * I + sp * Q;
				DQ += -sp * I + cp * Q;
			}
			ch->dm_buffer[m++]=hypotf(DI,DQ);
		}
		demodMSK(ch,m);
	}
}

int runRtlSample(void)
{
	int r;

	status = 1;
	r = rtlsdr_read_async(dev, in_callback, NULL, 32, RTLINBUFSZ);
	if (r) {
		fprintf(stderr, "Read async %d\n", r);
	}
	return status;
}

#endif

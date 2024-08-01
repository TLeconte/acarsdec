/*
 *  Copyright (c) 2016 Thierry Leconte
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
#include <math.h>
#include <rtl-sdr.h>
#include "acarsdec.h"
#include "lib.h"

// set the sameple rate by changing RTMULT
// 2.5Ms/s is the best but could be over limit for some hardware
// 2.0Ms/s is safer
// rateMult 160	// 2.0000 Ms/s
// rateMult 192	// 2.4000 Ms/s
// rateMult 200   // 2.5000 Ms/s

#define RTLMULTMAX 320 // this is well beyond the rtl-sdr capabilities

static rtlsdr_dev_t *dev = NULL;
static int status = 0;
static int rtlInBufSize = 0;
static int rtlInRate = 0;

#define RTLOUTBUFSZ 1024

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
	if (R.verbose)
		fprintf(stderr, "Found %d device(s):\n", device_count);

	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (R.verbose)
			fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
	}
	if (R.verbose)
		fprintf(stderr, "\n");

	/* does string look like an exact serial number */
	if (strlen(s) == 8)
		goto find_serial;
	/* does string look like raw id number */
	device = (int)strtol(s, &s2, 0);
	if (s2[0] == '\0' && device >= 0 && device < device_count) {
		if (R.verbose)
			fprintf(stderr, "Using device %d: %s\n", device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
find_serial:
	/* does string exact match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strcmp(s, serial) != 0) {
			continue;
		}
		device = i;
		if (R.verbose)
			fprintf(stderr, "Using device %d: %s\n", device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string prefix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strncmp(s, serial, strlen(s)) != 0)
			continue;

		device = i;
		if (R.verbose)
			fprintf(stderr, "Using device %d: %s\n", device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string suffix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		offset = strlen(serial) - strlen(s);
		if (offset < 0)
			continue;

		if (strncmp(s, serial + offset, strlen(s)) != 0)
			continue;

		device = i;
		if (R.verbose)
			fprintf(stderr, "Using device %d: %s\n", device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	fprintf(stderr, "No matching devices found.\n");
	return -1;
}

int nearest_gain(int target_gain)
{
	int i, err1, err2, count, close_gain;
	int *gains;

	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0)
		return 0;

	gains = malloc(sizeof(*gains) * count);
	if (gains == NULL)
		return 0;

	count = rtlsdr_get_tuner_gains(dev, gains);
	close_gain = gains[0];

	for (i = 0; i < count; i++) {
		err1 = abs(target_gain - close_gain);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1)
			close_gain = gains[i];
	}

	free(gains);
	return close_gain;
}

int initRtl(char **argv, int optind)
{
	int r, n;
	int dev_index;
	unsigned int Fc, minFc, maxFc;

	if (argv[optind] == NULL) {
		fprintf(stderr, "Need device name or index (ex: 0) after -r\n");
		exit(1);
	}
	dev_index = verbose_device_search(argv[optind]);
	optind++;

	if (R.rateMult > RTLMULTMAX) {
		fprintf(stderr, "rateMult can't be larger than %d\n", RTLMULTMAX);
		return 1;
	}

	rtlInBufSize = RTLOUTBUFSZ * R.rateMult * 2;
	rtlInRate = INTRATE * R.rateMult;

	r = rtlsdr_open(&dev, dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device\n");
		return r;
	}

	if (R.gain > 52.0F || R.gain <= 10.0F) {
		if (R.verbose)
			fprintf(stderr, "Tuner gain: AGC\n");
		r = rtlsdr_set_tuner_gain_mode(dev, 0);
	} else {
		rtlsdr_set_tuner_gain_mode(dev, 1);
		R.gain = nearest_gain((int)(R.gain * 10.0F));
		R.gain /= 10.0F;
		if (R.verbose)
			fprintf(stderr, "Tuner gain: %.1f\n", R.gain);
		r = rtlsdr_set_tuner_gain(dev, (int)(R.gain * 10.0F));
	}
	if (r < 0)
		fprintf(stderr, "WARNING: Failed to set gain.\n");

	if (R.ppm != 0) {
		r = rtlsdr_set_freq_correction(dev, R.ppm);
		if (r < 0)
			fprintf(stderr, "WARNING: Failed to set freq. correction\n");
	}

	r = parse_freqs(argv, optind, &minFc, &maxFc);
	if (r)
		return r;

	Fc = find_centerfreq(minFc, maxFc, rtlInRate);
	if (Fc == 0)
		return 1;

	for (n = 0; n < R.nbch; n++) {
		channel_t *ch = &(R.channels[n]);
		int ind;
		float AMFreq;

		ch->wf = malloc(R.rateMult * sizeof(*ch->wf));
		ch->dm_buffer = malloc(RTLOUTBUFSZ * sizeof(*ch->dm_buffer));
		if (ch->wf == NULL || ch->dm_buffer == NULL) {
			fprintf(stderr, "ERROR : malloc\n");
			return 1;
		}
		AMFreq = (signed)(ch->Fr - Fc) / (float)(rtlInRate) * 2.0 * M_PI;
		for (ind = 0; ind < R.rateMult; ind++)
			ch->wf[ind] = cexpf(AMFreq * ind * -I) / R.rateMult / 127.5;
	}

	if (R.verbose)
		fprintf(stderr, "Set center freq. to %dHz\n", (int)Fc);

	r = rtlsdr_set_center_freq(dev, Fc);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set center freq.\n");
		return 1;
	}

	fprintf(stderr, "Setting sample rate: %.4f MS/s\n", rtlInRate / 1e6);
	r = rtlsdr_set_sample_rate(dev, (unsigned)rtlInRate);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");
		return 1;
	}

	r = rtlsdr_reset_buffer(dev);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to reset buffers.\n");
		return 1;
	}

	r = rtlsdr_set_bias_tee(dev, R.bias);
	if (R.verbose)
		fprintf(stderr, "Set Bias Tee to %d\n", R.bias);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set bias tee\n");
		return 1;
	}
	return 0;
}

static void in_callback(unsigned char *rtlinbuff, uint32_t nread, void *ctx)
{
	int n;

	if (nread != rtlInBufSize) {
		fprintf(stderr, "warning: partial read\n");
		return;
	}
	status = 0;

	// code requires this relationship set in initRtl:
	// rtlInBufSize = RTLOUTBUFSZ * rateMult * 2;

	float complex vb[RTLMULTMAX];
	int i = 0;
	for (int m = 0; m < RTLOUTBUFSZ; m++) {
		for (int ind = 0; ind < R.rateMult; ind++) {
			float r, g;

			r = (float)rtlinbuff[i] - 127.37f;
			i++;
			g = (float)rtlinbuff[i] - 127.37f;
			i++;

			vb[ind] = r + g * I;
		}

		for (n = 0; n < R.nbch; n++) {
			channel_t *ch = &(R.channels[n]);
			float complex D, *wf;

			wf = ch->wf;
			D = 0;
			for (int ind = 0; ind < R.rateMult; ind++) {
				D += vb[ind] * wf[ind];
			}
			ch->dm_buffer[m] = cabsf(D);
		}
	}

	for (n = 0; n < R.nbch; n++) {
		channel_t *ch = &(R.channels[n]);
		demodMSK(ch, RTLOUTBUFSZ);
	}
}

int runRtlSample(void)
{
	rtlsdr_read_async(dev, in_callback, NULL, 4, rtlInBufSize);
	return 0;
}

int runRtlCancel(void)
{
	if (dev)
		rtlsdr_cancel_async(dev); // interrupt read_async

	return 0;
}

int runRtlClose(void)
{
	int res = 0;

	if (dev) {
		res = rtlsdr_close(dev);
		dev = NULL;
	}
	if (res)
		fprintf(stderr, "rtlsdr_close: %d\n", res);

	return res;
}

#endif

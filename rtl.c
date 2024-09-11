/*
 *  Copyright (c) 2016 Thierry Leconte
 *  Copyright (c) 2024 Thibaut VARENE
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

#define RTLMULTMAX 320U // this is well beyond the rtl-sdr capabilities

#define ERRPFX	"ERROR: RTLSDR: "
#define WARNPFX	"WARNING: RTLSDR: "

static rtlsdr_dev_t *dev = NULL;

/* function verbose_device_search by Kyle Keen
 * from http://cgit.osmocom.org/rtl-sdr/tree/src/convenience/convenience.c
 */
static int verbose_device_search(char *s)
{
	int i, device_count, device, offset;
	char *s2;
	char vendor[256], product[256], serial[256];

	device_count = rtlsdr_get_device_count();
	if (!device_count) {
		fprintf(stderr, ERRPFX "No supported devices found.\n");
		return -1;
	}

	vprerr("Found %d device(s):\n", device_count);
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		vprerr("  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
	}
	vprerr("\n");

	/* does string look like an exact serial number */
	if (strlen(s) == 8)
		goto find_serial;
	/* does string look like raw id number */
	device = (int)strtol(s, &s2, 0);
	if (s2[0] == '\0' && device >= 0 && device < device_count) {
		vprerr("Using device %d: %s\n", device, rtlsdr_get_device_name((uint32_t)device));
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
		vprerr("Using device %d: %s\n", device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string prefix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strncmp(s, serial, strlen(s)) != 0)
			continue;

		device = i;
		vprerr("Using device %d: %s\n", device, rtlsdr_get_device_name((uint32_t)device));
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
		vprerr("Using device %d: %s\n", device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	fprintf(stderr, ERRPFX "No matching devices found.\n");
	return -1;
}

static int nearest_gain(int target_gain)
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

int initRtl(char *optarg)
{
	int r;
	int dev_index;
	unsigned int Fc;

	if (!optarg)
		return 1;	// cannot happen with getopt()

	if (R.rateMult > RTLMULTMAX) {
		fprintf(stderr, ERRPFX "rateMult can't be larger than %d\n", RTLMULTMAX);
		return 1;
	}

	Fc = find_centerfreq(R.minFc, R.maxFc, R.rateMult);
	if (!Fc)
		return 1;

	dev_index = verbose_device_search(optarg);
	if (dev_index < 0)
		return 1;

	r = rtlsdr_open(&dev, dev_index);
	if (r) {
		fprintf(stderr, ERRPFX "Failed to open device\n");
		return r;
	}

	if (R.gain > 52.0F || R.gain <= -10.0F) {
		vprerr("Tuner gain: AGC\n");
		r = rtlsdr_set_tuner_gain_mode(dev, 0);
	} else {
		int gain = nearest_gain((int)(R.gain * 10.0F));
		r = rtlsdr_set_tuner_gain_mode(dev, 1);
		if (!r) {
			vprerr("Tuner gain: %.1f\n", gain / 10.0F);
			r = rtlsdr_set_tuner_gain(dev, gain);
		}
	}
	if (r)
		fprintf(stderr, WARNPFX "Failed to set gain.\n");

	if (R.ppm != 0) {
		r = rtlsdr_set_freq_correction(dev, R.ppm);
		if (r)
			fprintf(stderr, WARNPFX "Failed to set frequency correction\n");
	}

	r = channels_init_sdr(Fc, R.rateMult, 127.5F);
	if (r)
		return r;

	vprerr("Setting center freq: %.4f MHz\n", Fc / 1e6);
	r = rtlsdr_set_center_freq(dev, Fc);
	if (r) {
		fprintf(stderr, ERRPFX "Failed to set center frequency.\n");
		return 1;
	}

	fprintf(stderr, "Setting sample rate: %.4f MS/s\n", INTRATE * R.rateMult / 1e6);
	r = rtlsdr_set_sample_rate(dev, INTRATE * R.rateMult);
	if (r) {
		fprintf(stderr, ERRPFX "Failed to set sample rate.\n");
		return 1;
	}

	uint32_t bw = (R.maxFc - R.minFc) + 2 * INTRATE;
	fprintf(stderr, "Setting bandwidth to: %.2f kHz\n", bw / 1e3);
	r = rtlsdr_set_tuner_bandwidth(dev, bw);
	if (r) {
		fprintf(stderr, WARNPFX "Failed to set bandwidth.\n");
	}

	r = rtlsdr_reset_buffer(dev);
	if (r) {
		fprintf(stderr, ERRPFX "Failed to reset buffers.\n");
		return 1;
	}

	vprerr("Setting bias tee to %d\n", R.bias);
	r = rtlsdr_set_bias_tee(dev, R.bias);
	if (r)
		fprintf(stderr, WARNPFX "Failed to set bias tee\n");

	return 0;
}

/*
 Regarding I/Q offset
 https://osmocom-sdr.osmocom.narkive.com/58eMkTE4/recording-iq-stream-with-rtlsdr-and-sdr-in-the-same-format#post16
 Quoted below:

 > rtl dongle output a 8bit unsigned interger for both I/Q signals.
 > To convert it to float man must remove the zero value.
 > A signed 8bit is normally between -128 and 127 so the convertion must
 > be something like :
 > I=(float)rtlinbuff[i++]-128.0; // with rtlinbuff the input 8bit unsigned buffer
 > Q=(float)rtlinbuff[i++]-128.0; // and I/Q float values
 >
 > But, I try to find that 0 level by a very simple low pass filter.
 > something like :
 > IM=0.99999*IM+0.00001*(double)rtlinbuff[i++];
 > QM=0.99999*QM+0.00001*(double)rtlinbuff[i++];
 >
 > and after a few seconds of running with antenna disconnected , I find a
 > value around 127.35 for a first dongle and 127.4 for a second.
 > I do some differents runs and these values seem consistant.

 Using 127.37 as a "work for all" average.
 */
static void in_callback(unsigned char *rtlinbuff, uint32_t nread, void *ctx)
{
	const unsigned int mult = R.rateMult;
	float complex phasors[mult];

	if (nread % 2) {
		fprintf(stderr, ERRPFX "incomplete read\n");
		return;
	}

	while (nread) {
		unsigned int lim = unlikely(nread/2 < mult) ? nread/2 : mult;	// mult-sized chunks
		// compute rateMult-oversampled, full-scale phasor
		// this loops consumes at most mult*2 bytes of rtlinbuff
		for (unsigned int ind = 0; ind < lim; ind++) {
			float i, q;

			// make I and Q signed floats, range c. -127.5 to 127.5 (see note above)
			i = (float)(*rtlinbuff++) - 127.37f;
			q = (float)(*rtlinbuff++) - 127.37f;

			phasors[ind] = i + q * I;
		}
		channels_mix_phasors(phasors, lim, mult);
		nread -= lim * 2;
	}
}

int runRtlSample(void)
{
	unsigned int rtlInBufSize = DMBUFSZ * R.rateMult * 2;
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
		fprintf(stderr, WARNPFX "rtlsdr_close: %d\n", res);

	return res;
}

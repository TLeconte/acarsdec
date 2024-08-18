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

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <libairspy/airspy.h>
#include "acarsdec.h"
#include "lib.h"
#include "msk.h"

static unsigned int AIRMULT;
static unsigned int AIRINRATE;

static struct airspy_device *device = NULL;
extern void *compute_thread(void *arg);

static const unsigned int r820t_hf[] = { 1953050, 1980748, 2001344, 2032592, 2060291, 2087988 };
static const unsigned int r820t_lf[] = { 525548, 656935, 795424, 898403, 1186034, 1502073, 1715133, 1853622 };
static float complex *chD;

static unsigned int chooseFc(unsigned int minF, unsigned int maxF, int filter)
{
	unsigned int bw = maxF - minF + 2 * INTRATE;
	unsigned int off = 0;
	int i, j;

	if (filter) {
		/* The reasoning and research for this is described here:
		 * https://tleconte.github.io/R820T/r820IF.html */

		for (i = 7; i >= 0; i--)
			if ((r820t_hf[5] - r820t_lf[i]) >= bw)
				break;

		if (i < 0) {
			fprintf(stderr, "Unable to use airspy R820T filter optimization, continuing.\n");
		} else {
			fprintf(stderr, "Enabling airspy R820T filter optimization.\n");

			for (j = 5; j >= 0; j--)
				if ((r820t_hf[j] - r820t_lf[i]) <= bw)
					break;
			j++;

			off = (r820t_hf[j] + r820t_lf[i]) / 2 - AIRINRATE / 4;

			airspy_r820t_write(device, 10, 0xB0 | (15 - j));
			airspy_r820t_write(device, 11, 0xE0 | (15 - i));
		}
	}

	return (((maxF + minF) / 2 + off + INTRATE / 2) / INTRATE * INTRATE);
}

int initAirspy(char *optarg)
{
	int n;
	char *argF;
	unsigned int Fc;
	int result;
	uint32_t i, count;
	uint32_t *supported_samplerates;
	uint64_t airspy_serial = 0;
	int airspy_device_count = 0;
	uint64_t *airspy_device_list = NULL;

	chD = calloc(R.nbch, sizeof(*chD));
	if (!chD) {
		perror(NULL);
		return -1;
	}

	// Request the total number of libairspy devices connected, allocate space, then request the list.
	result = airspy_device_count = airspy_list_devices(NULL, 0);
	if (result < 1) {
		if (result == 0) {
			fprintf(stderr, "No airspy devices found.\n");
		} else {
			fprintf(stderr, "airspy_list_devices() failed: %s (%d).\n", airspy_error_name(result), result);
		}
		airspy_exit();
		return -1;
	}

	airspy_device_list = malloc(sizeof(*airspy_device_list) * airspy_device_count);
	if (airspy_device_list == NULL) {
		perror(NULL);
		return -1;
	}
	result = airspy_list_devices(airspy_device_list, airspy_device_count);
	if (result != airspy_device_count) {
		fprintf(stderr, "airspy_list_devices() failed.\n");
		free(airspy_device_list);
		airspy_exit();
		return -1;
	}

	// clear errno to catch invalid input.
	errno = 0;
	// Attempt to interpret first argument as a specific device serial.
	airspy_serial = strtoull(optarg, &argF, 16);

	// If strtoull result is an integer from 0 to airspy_device_count:
	//  1. Attempt to open airspy serial indicated.
	//  2. If successful, consume argument and continue.
	// If still no device and strtoull successfully finds a 16bit hex value, then:
	//  1. Attempt to open a specific airspy device using value as a serialnumber.
	//  2. If succesful, consume argument and continue.
	// If still no device and strtoull result fails
	//  1. Iterate over list of airspy devices and attempt to open each one.
	//  2. If opened succesfully, do not consume argument and continue.
	// Else:
	//  1. Give up.

	if ((optarg != argF) && (errno == 0)) {
		if ((airspy_serial < airspy_device_count)) {
			vprerr("Attempting to open airspy device slot #%lu with serial %016lx.\n", airspy_serial, airspy_device_list[airspy_serial]);
			result = airspy_open_sn(&device, airspy_device_list[airspy_serial]);
		} else {
			vprerr("Attempting to open airspy serial 0x%016lx\n", airspy_serial);
			result = airspy_open_sn(&device, airspy_serial);
		}
	}

	if (device == NULL) {
		for (n = 0; n < airspy_device_count; n++) {
			vprerr("Attempting to open airspy device #%d.\n", n);
			result = airspy_open_sn(&device, airspy_device_list[n]);
			if (result == AIRSPY_SUCCESS)
				break;
		}
	}
	memset(airspy_device_list, 0, sizeof(*airspy_device_list) * airspy_device_count);
	free(airspy_device_list);
	airspy_device_list = NULL;

	if (device == NULL) {
		result = airspy_open(&device);
		if (result != AIRSPY_SUCCESS) {
			fprintf(stderr, "Failed to open any airspy device.\n");
			airspy_exit();
			return -1;
		}
	}

	/* init airspy */

	// XXX REVIEW: airspy can do AIRSPY_SAMPLE_FLOAT32_IQ, like everyone else. Is there a reason to use real?
	result = airspy_set_sample_type(device, AIRSPY_SAMPLE_FLOAT32_REAL);
	if (result != AIRSPY_SUCCESS) {
		fprintf(stderr, "airspy_set_sample_type() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}

	airspy_get_samplerates(device, &count, 0);
	supported_samplerates = malloc(count * sizeof(*supported_samplerates));
	if (supported_samplerates == NULL) {
		perror(NULL);
		airspy_close(device);
		airspy_exit();
		return -1;
	}
	airspy_get_samplerates(device, supported_samplerates, count);
	for (i = 0; i < count; i++) {
		if (supported_samplerates[i] > 10000000)
			continue;
		AIRINRATE = supported_samplerates[i];
		AIRMULT = AIRINRATE / INTRATE;
		if ((AIRMULT * INTRATE) == AIRINRATE)
			break;
	}

	if (i >= count) {
		fprintf(stderr, "did not find needed sampling rate\n");
		airspy_close(device);
		airspy_exit();
		return -1;
	}

	free(supported_samplerates);

	vprerr("Using %d sampling rate\n", AIRINRATE);

	result = airspy_set_samplerate(device, i);
	if (result != AIRSPY_SUCCESS) {
		fprintf(stderr, "airspy_set_samplerate() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}

	/* enable packed samples */
	airspy_set_packing(device, 1);

	result = airspy_set_linearity_gain(device, (int)R.gain);
	if (result != AIRSPY_SUCCESS) {
		fprintf(stderr, "airspy_set_vga_gain() failed: %s (%d)\n", airspy_error_name(result), result);
	}

	Fc = chooseFc(R.minFc, R.maxFc, AIRINRATE == 5000000);
	if (Fc == 0) {
		fprintf(stderr, "Frequencies too far apart\n");
		return 1;
	}

	result = airspy_set_freq(device, Fc);
	if (result != AIRSPY_SUCCESS) {
		fprintf(stderr, "airspy_set_freq() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}
	vprerr("Set freq. to %d hz\n", Fc);

	/* computes mixers osc. */
	for (n = 0; n < R.nbch; n++) {
		channel_t *ch = &(R.channels[n]);
		int i;
		double AMFreq, Ph;

		ch->oscillator = malloc(AIRMULT * sizeof(*ch->oscillator));
		ch->dm_buffer = malloc(DMBUFSZ * sizeof(*ch->dm_buffer));
		if (ch->oscillator == NULL || ch->dm_buffer == NULL) {
			perror(NULL);
			airspy_close(device);
			airspy_exit();
			return -1;
		}

		AMFreq = 2.0 * M_PI * (Fc - ch->Fr + (double)AIRINRATE / 4) / (double)(AIRINRATE);
		for (i = 0, Ph = 0; i < AIRMULT; i++) {
			ch->oscillator[i] = cexpf(Ph * -I) / AIRMULT;
			Ph += AMFreq;
			if (Ph > 2.0 * M_PI)
				Ph -= 2.0 * M_PI;
			if (Ph < -2.0 * M_PI)
				Ph += 2.0 * M_PI;
		}
	}

	return 0;
}

static int rx_callback(airspy_transfer_t *transfer)
{
	static int ind = 0;
	float *pt_rx_buffer;
	int n, i;
	int bo, be, ben, nbk;

	pt_rx_buffer = (float *)(transfer->samples);

	bo = AIRMULT - ind;
	nbk = (transfer->sample_count - bo) / AIRMULT;
	be = nbk * AIRMULT + bo;
	ben = transfer->sample_count - be;

	for (n = 0; n < R.nbch; n++) {
		channel_t *ch = &(R.channels[n]);
		float S;
		int k, bn, m;
		float complex D;

		D = chD[n];

		/* compute */
		m = 0;
		k = 0;
		for (i = ind; i < AIRMULT; i++, k++) {
			S = pt_rx_buffer[k];
			D += ch->oscillator[i] * S;
		}
		ch->dm_buffer[m++] = cabsf(D);

		for (bn = 0; bn < nbk; bn++) {
			D = 0;
			for (i = 0; i < AIRMULT; i++, k++) {
				S = pt_rx_buffer[k];
				D += ch->oscillator[i] * S;
			}
			ch->dm_buffer[m++] = cabsf(D);
		}

		D = 0;
		for (i = 0; i < ben; i++, k++) {
			S = pt_rx_buffer[k];
			D += ch->oscillator[i] * S;
		}
		chD[n] = D;

		demodMSK(ch, m);
	}
	ind = ben;

	return 0;
}

int runAirspySample(void)
{
	int result;

	result = airspy_start_rx(device, rx_callback, NULL);
	if (result != AIRSPY_SUCCESS) {
		fprintf(stderr, "airspy_start_rx() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}

	while (R.running && airspy_is_streaming(device) == AIRSPY_TRUE) {
		sleep(2);
	}

	airspy_stop_rx(device);

	free(chD);

	return 0;
}

/*
 *  Copyright (c) 2015 Thierry Leconte
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
#include <signal.h>
#include <getopt.h>
#include <sched.h>
#include <unistd.h>
#include <limits.h>
#include <err.h>
#ifdef HAVE_LIBACARS
#include <libacars/version.h>
#endif
#include "acarsdec.h"
#include "msk.h"
#include "output.h"
#include "label.h"
#include "acars.h"
#include "lib.h"

#ifdef WITH_AIR
 #include "air.h"
#endif
#ifdef WITH_ALSA
 #include "alsa.h"
#endif
#ifdef WITH_RTL
 #include "rtl.h"
#endif
#ifdef WITH_SOAPY
 #include "soapy.h"
#endif
#ifdef WITH_SDRPLAY
 #include "sdrplay.h"
#endif
#ifdef WITH_SNDFILE
 #include "soundfile.h"
#endif

runtime_t R = {
	.mdly = 600,
	.rateMult = 160U,
	.lnaState = 2,
	.GRdB = 20,
};

static void usage(void)
{
	fprintf(stderr,
		"Acarsdec %s Copyright (c) 2022 Thierry Leconte, (c) 2024 Thibaut VARENE\n", ACARSDEC_VERSION);
#ifdef HAVE_LIBACARS
	fprintf(stderr, "(libacars %s)\n", LA_VERSION);
#endif
	fprintf(stderr, "\nUsage: acarsdec  [-t time] [-A] [-b 'labels,..'] [-e] [-i station_id] --output FORMAT:DESTINATION:PARAMS [--output ...]");
#ifdef HAVE_LIBACARS
	fprintf(stderr, " [--skip-reassembly] ");
#endif
#ifdef WITH_ALSA
	fprintf(stderr, " [-a alsapcmdevice] |");
#endif
#ifdef WITH_SNDFILE
	fprintf(stderr, " [-f inputwavfile] |");
#endif
#ifdef WITH_RTL
	fprintf(stderr, " [rtlopts] |");
#endif
#ifdef WITH_AIR
	fprintf(stderr, " [airspyopts] |");
#endif
#ifdef WITH_SDRPLAY
	fprintf(stderr, " [sdrplayopts] |");
#endif
#ifdef WITH_SOAPY
	fprintf(stderr, " [soapyopts]");
#endif
	fprintf(stderr, " f1 [f2] .. [fN]\n\n");
#ifdef HAVE_LIBACARS
	fprintf(stderr, " --skip-reassembly\t: disable reassembling fragmented ACARS messages\n");
#endif
	fprintf(stderr,
		" -i stationid\t\t: station id used in acarsdec network format.\n"
		" -A\t\t\t: don't output uplink messages (ie : only aircraft messages)\n"
		" -e\t\t\t: don't output empty messages (ie : _d,Q0, etc ...)\n"
		" -b filter\t\t: filter output by label (ex: -b \"H1:Q0\" : only output messages  with label H1 or Q0)\n"
		" -t time\t\t: set forget time (TTL) in seconds for flight routes (affects monitor and routejson, default=600s)\n"
		"\n"
		" Use \"--output help\" for available output options\n"
		" f1 [f2] .. [fN] are given in decimal MHz, e.g. 131.525\n"
		"\n");

#ifdef WITH_ALSA
	fprintf(stderr, " -a alsapcmdevice\t: decode from soundcard input alsapcmdevice (ie: hw:0,0)\n");
#endif
#ifdef WITH_SNDFILE
	fprintf(stderr, " -f inputwavfile\t: decode from a wav file at %uHz sampling rate\n", INTRATE);
#endif
#ifdef WITH_RTL
	fprintf(stderr,
		"\n rtlopts:\n"
		" -r rtldevice\t\t: decode from rtl dongle number or S/N rtldevice\n"
		" -g gain\t\t: set rtl gain in db (0 to 49.6; >52 and -10 will result in AGC; default is AGC)\n"
		" -p ppm\t\t\t: set rtl ppm frequency correction\n"
		" -m rateMult\t\t: set rtl sample rate multiplier: 160 for 2 MS/s or 192 for 2.4 MS/s (default: 160)\n"
		" -B bias\t\t: Enable (1) or Disable (0) the bias tee (default is 0)\n"
		" -c freq\t\t: set center frequency to tune to in MHz, e.g. 131.800\n");
#endif
#ifdef WITH_AIR
	fprintf(stderr,
		"\n airspyopts:\n"
		" -s airspydevice\t: decode from airspy dongle number or hex serial number\n"
		" -g linearity_gain\t: set linearity gain [0-21] default : 18\n");
#endif
#ifdef WITH_SDRPLAY
	fprintf(stderr,
		"\n sdrplayopts:\n"
		" -s\t\t: decode from sdrplay\n"
		" -L lnaState\t: set the lnaState (depends on the device)\n"
		" -G GRdB\t\t: gain reduction in dB's, range 20 .. 59 (-100 is autogain)\n"
		" -c freq\t\t: set center frequency to tune to in MHz, e.g. 131.800\n");
#endif
#ifdef WITH_SOAPY
	fprintf(stderr,
		"\n soapyopts:\n"
		" -d devicestring\t: decode from a SoapySDR device located by devicestring\n"
		" -g gain\t\t: set gain in db (-10 will result in AGC; default is AGC)\n"
		" -p ppm\t\t\t: set ppm frequency correction\n"
		" -c freq\t\t: set center frequency to tune to in MHz\n"
		" -m rateMult\t\t: set sample rate multiplier: 160 for 2 MS/s or 192 for 2.4 MS/s (default: 160)\n"
		" --antenna antenna\t: set antenna port to use\n");
#endif
	exit(1);
}

static void sigintHandler(int signum)
{
	fprintf(stderr, "Received signal %s, exiting.\n", strsignal(signum));
#if defined(DEBUG) && defined(WITH_SNDFILE)
	SndWriteClose();
#endif
#ifdef WITH_RTL
	runRtlCancel();
#endif
#ifdef WITH_SOAPY
	runSoapyClose();
#endif
	exit(0);
}

int main(int argc, char **argv)
{
	int c;
	int res;
	unsigned int n;
	struct sigaction sigact;
	struct option long_opts[] = {
		{ "verbose", no_argument, NULL, 'v' },
		{ "skip-reassembly", no_argument, NULL, 1 },
		{ "antenna", required_argument, NULL, 2 },
		{ "output", required_argument, NULL, 3 },
		{ NULL, 0, NULL, 0 }
	};
	char sys_hostname[HOST_NAME_MAX + 1];
	char *lblf = NULL;
	char *inarg = NULL;

	gethostname(sys_hostname, HOST_NAME_MAX);
	sys_hostname[HOST_NAME_MAX] = 0;
	R.idstation = strdup(sys_hostname);

	res = 0;
	while ((c = getopt_long(argc, argv, "hva:r:f:d:s:St:g:m:Aep:c:i:L:G:b:B:", long_opts, NULL)) != EOF) {
		switch (c) {
		case 3:
			res = setup_output(optarg);
			if (res)
				exit(res);
			break;
		case 'v':
			R.verbose = 1;
			break;
		case 't':
			R.mdly = atoi(optarg);
			break;
		case 'b':
			lblf = optarg;
			break;
#ifdef HAVE_LIBACARS
		case 1:
			R.skip_reassembly = 1;
			break;
#endif
#ifdef WITH_ALSA
		case 'a':
			if (R.inmode)
				errx(-1, "Only 1 input allowed");
			R.inmode = IN_ALSA;
			inarg = optarg;
			break;
#endif
#ifdef WITH_SNDFILE
		case 'f':
			if (R.inmode)
				errx(-1, "Only 1 input allowed");
			R.inmode = IN_SNDFILE;
			inarg = optarg;
			break;
#endif
		case 'g':
			R.gain = atof(optarg);
			break;
		case 'p':
			R.ppm = atoi(optarg);
			break;
		case 'm':
			R.rateMult = (unsigned)atoi(optarg);
			break;
#ifdef WITH_RTL
		case 'r':
			if (R.inmode)
				errx(-1, "Only 1 input allowed");
			R.inmode = IN_RTL;
			inarg = optarg;
			break;
		case 'B':
			R.bias = atoi(optarg);
			break;
#endif
#ifdef WITH_SDRPLAY
		case 'S':
			if (R.inmode)
				errx(-1, "Only 1 input allowed");
			R.inmode = IN_SDRPLAY;
			break;
		case 'L':
			R.lnaState = atoi(optarg);
			break;
		case 'G':
			R.GRdB = atoi(optarg);
			break;
#endif
#ifdef WITH_SOAPY
		case 2:
			R.antenna = optarg;
			break;
		case 'd':
			if (R.inmode)
				errx(-1, "Only 1 input allowed");
			R.inmode = IN_SOAPY;
			inarg = optarg;
			break;
#endif
#ifdef WITH_AIR
		case 's':
			if (R.inmode)
				errx(-1, "Only 1 input allowed");
			R.inmode = IN_AIR;
			inarg = optarg;
			break;
#endif
		case 'c':
			R.Fc = (unsigned int)(1000000 * atof(optarg));
			break;
		case 'A':
			R.airflt = 1;
			break;
		case 'e':
			R.emptymsg = 1;
			break;
		case 'i':
			free(R.idstation);
			R.idstation = strdup(optarg);
			break;
		case 'h':
		default:
			usage();
		}
	}

	if (R.inmode == IN_NONE) {
		fprintf(stderr, "Missing input\n");
		usage();
	}

	if (optind < argc) {	// parse remaining non-option arguments as frequencies
		res = parse_freqs(argv, optind);
		if (res)
			return res;
	}

	/* init input  */
	switch (R.inmode) {
#ifdef WITH_ALSA
	case IN_ALSA:
		res = initAlsa(inarg);
		break;
#endif
#ifdef WITH_SNDFILE
	case IN_SNDFILE:
		res = initSoundfile(inarg);
		break;
#endif
#ifdef WITH_RTL
	case IN_RTL:
		if (!R.gain)
			R.gain = -10;
		res = initRtl(inarg);
		break;
#endif
#ifdef WITH_AIR
	case IN_AIR:
		if (!R.gain)
			R.gain = 18;
		res = initAirspy(inarg);
		break;
#endif
#ifdef WITH_SDRPLAY
	case IN_SDRPLAY:
		res = initSdrplay();
		break;
#endif
#ifdef WITH_SOAPY
	case IN_SOAPY:
		if (!R.gain)
			R.gain = -10;
		res = initSoapy(inarg);
		break;
#endif
	default:
		res = -1;
	}

	if (res)
		errx(res, "Unable to init input\n");

	build_label_filter(lblf);

	res = initOutputs();
	if (res)
		errx(res, "Unable to init outputs\n");

	if (!R.channels)
		errx(-1, "No channel initialized!");

	sigact.sa_handler = sigintHandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	for (n = 0; n < R.nbch; n++) {
		R.channels[n].chn = n;

		res = initMsk(&(R.channels[n]));
		if (res)
			break;
		res = initAcars(&(R.channels[n]));
		if (res)
			break;
	}

	if (res)
		errx(res, "Unable to init internal decoders\n");

#if defined(DEBUG) && defined(WITH_SNDFILE)
	if (R.inmode != IN_SNDFILE) {
		initSndWrite();
	}
#endif

	if (R.verbose)
		fprintf(stderr, "Decoding %d channels\n", R.nbch);

	/* main decoding  */
	switch (R.inmode) {
#ifdef WITH_ALSA
	case IN_ALSA:
		res = runAlsaSample();
		break;
#endif
#ifdef WITH_SNDFILE
	case IN_SNDFILE:
		res = runSoundfileSample();
		break;
#endif
#ifdef WITH_RTL
	case IN_RTL:
		runRtlSample();
		res = runRtlClose();
		break;
#endif
#ifdef WITH_AIR
	case IN_AIR:
		res = runAirspySample();
		break;
#endif
#ifdef WITH_SDRPLAY
	case IN_SDRPLAY:
		res = runSdrplaySample();
		break;
#endif
#ifdef WITH_SOAPY
	case IN_SOAPY:
		res = runSoapySample();
		break;
#endif
	default:
		res = -1;
	}

	fprintf(stderr, "exiting ...\n");

	deinitAcars();

	exitOutputs();

	exit(res);
}

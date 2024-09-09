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
#include "statsd.h"

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
	.lnaState = 2,
	.GRdB = 20,
};

static void print_available_ins(void)
{
	int i, end;

	const char *const inopts[] = {
#ifdef WITH_ALSA
		"[--alsa <dev>]",
#endif
#ifdef WITH_SNDFILE
		"[--sndfile <file.wav>]",
#endif
	};

	const char *const sdropts[] = {
#ifdef WITH_RTL
		"[rtlopts]",
#endif
#ifdef WITH_AIR
		"[airspyopts]",
#endif
#ifdef WITH_SDRPLAY
		"[sdrplayopts]",
#endif
#ifdef WITH_SOAPY
		"[soapyopts]",
#endif
	};

	// non-sdr inputs first
	end = ARRAY_SIZE(inopts);

	if (end) {
		fprintf(stderr, " ");
		for (i = 0; i < end; i++) {
			fprintf(stderr, "%s", inopts[i]);
			if (i < end-1)
				fprintf(stderr, " | ");
		}
	}

	// then sdrs
	end = ARRAY_SIZE(sdropts);

	if (end) {
		if (ARRAY_SIZE(inopts))
			fprintf(stderr, " |");
		fprintf(stderr, " [ ");
		for (i = 0; i < end; i++) {
			fprintf(stderr, "%s", sdropts[i]);
			if (i < end-1)
				fprintf(stderr, " | ");
		}
		fprintf(stderr, " <f1> [<f2> [...]] ]\n\n");
		fprintf(stderr, " <f1> [<f2> [...]] are given in decimal MHz, e.g. 131.525");
	}
}

static void usage(void)
{
	fprintf(stderr,
		"Acarsdec %s Copyright (c) 2022 Thierry Leconte, (c) 2024 Thibaut VARENE\n", ACARSDEC_VERSION);
#ifdef HAVE_LIBACARS
	fprintf(stderr, "(libacars %s)\n", LA_VERSION);
#endif
	fprintf(stderr, "\nUsage: acarsdec  [-t secs] [-A] [-b 'labels,..'] [-e] [-i station_id] [--statsd host=ip,port=1234] --output FORMAT:DESTINATION:PARAMS [--output ...]");
#ifdef HAVE_LIBACARS
	fprintf(stderr, " [--skip-reassembly]");
#endif
	print_available_ins();
	fprintf(stderr,
		"\n\n"
		" -i <stationid>\t\t: station id used in acarsdec network format (default: hostname)\n"
		" -A\t\t\t: don't output uplink messages (ie : only aircraft messages)\n"
		" -e\t\t\t: don't output empty messages (ie : _d,Q0, etc ...)\n"
		" -b <filter>\t\t: filter output by label (ex: -b \"H1:Q0\" : only output messages  with label H1 or Q0)\n"
		" -t <seconds>\t\t: set forget time (TTL) to <seconds> for flight routes (affects monitor and routejson, default: 600)\n"
#ifdef HAVE_LIBACARS
		" --skip-reassembly\t: disable reassembling fragmented ACARS messages\n"
#endif
		" --statsd host=<myhost>,port=<1234>\t: enable statsd reporting to host <myhost> on port <1234>\n"
		"\n Use \"--output help\" for available output options\n"
		"\n Available inputs:\n");

#ifdef WITH_ALSA
	fprintf(stderr, "\n --alsa <alsadevice>\t: decode from soundcard input <alsadevice> (ie: hw:0,0)\n");
#endif
#ifdef WITH_SNDFILE
	fprintf(stderr, "\n --sndfile <file>\t: decode from <file> sampled at a multiple of %u Hz\n", INTRATE);
	fprintf(stderr, " see \"--sndfile help\" for details\n");
#endif
#ifdef WITH_RTL
	fprintf(stderr,
		"\n rtlopts:\n"
		" --rtlsdr <device>\t: decode from rtl dongle number <device> or S/N <device>\n"
		" -g <gain>\t\t: set rtl gain in db (0 to 49.6; >52 and -10 will result in AGC; default is AGC)\n"
		" -p <ppm>\t\t: set rtl ppm frequency correction (default: 0)\n"
		" -m <rateMult>\t\t: set rtl sample rate multiplier: 160 for 2 MS/s or 192 for 2.4 MS/s (default: 160)\n"
		" -B <bias>\t\t: enable (1) or disable (0) the bias tee (default is 0)\n"
		" -c <freq>\t\t: set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)\n");
#endif
#ifdef WITH_AIR
	fprintf(stderr,
		"\n airspyopts:\n"
		" --airspy <device>\t: decode from airspy dongle number <device> or hex serial <device>\n"
		" -g <linearity_gain>\t: set linearity gain [0-21] (default: 18)\n");
#endif
#ifdef WITH_SDRPLAY
	fprintf(stderr,
		"\n sdrplayopts:\n"
		" --sdrplay\t\t: decode from sdrplay\n"
		" -L <lnaState>\t: set the lnaState (depends on the device)\n"
		" -G <GRdB>\t\t: gain reduction in dB's, range 20 .. 59 (default: -100 is autogain)\n"
		" -c <freq>\t\t: set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)\n");
#endif
#ifdef WITH_SOAPY
	fprintf(stderr,
		"\n soapyopts:\n"
		" --soapysdr <params>\t: decode from a SoapySDR designed by device_string <params>\n"
		" -g <gain>\t\t: set gain in db (-10 will result in AGC; default is AGC)\n"
		" -p <ppm>\t\t: set ppm frequency correction (default: 0)\n"
		" -c <freq>\t\t: set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)\n"
		" -m <rateMult>\t\t: set sample rate multiplier: 160 for 2 MS/s or 192 for 2.4 MS/s (default: 160)\n"
		" -a <antenna>\t\t: set antenna port to use (default: soapy default)\n");
#endif
	exit(1);
}

static void sigintHandler(int signum)
{
	fprintf(stderr, "Received signal %s, terminating process\n", strsignal(signum));
	R.running = 0;
#ifdef WITH_RTL
	runRtlCancel();
#endif
}

static int parse_freqs(char **argv, const int argind)
{
	unsigned int nb = 0;
	unsigned int freq, minF, maxF;
	int ind = argind;
	char *argF;

	minF = 140000000U;
	maxF = 0;

	/* count frequency args */
	while ((argF = argv[ind])) {
		ind++;
		freq = (1000U * (unsigned int)atof(argF));
		if (freq < 118000U || freq > 138000U) {
			fprintf(stderr, "WARNING: Ignoring invalid frequency '%s'\n", argF);
			continue;
		}
		nb++;
	}

	if (!nb) {
		fprintf(stderr, "ERROR: Need a least one frequency\n");
		return 1;
	}

	/* allocate channels */
	R.channels = calloc(nb, sizeof(*R.channels));
	if (!R.channels) {
		perror(NULL);
		return -1;
	}

	R.nbch = nb;

	/* parse frequency args */
	nb = 0;
	ind = argind;
	while ((argF = argv[ind])) {
		ind++;
		// round freq to nearest kHz value
		freq = (((unsigned int)(1000000 * atof(argF)) + 1000 / 2) / 1000) * 1000;
		if (freq < 118000000 || freq > 138000000)
			continue;

		R.channels[nb].chn = nb;
		R.channels[nb].Fr = freq;

		minF = freq < minF ? freq : minF;
		maxF = freq > maxF ? freq : maxF;
		nb++;
	};

	R.minFc = minF;
	R.maxFc = maxF;

	return 0;
}

int main(int argc, char **argv)
{
	int c;
	int res;
	unsigned int n;
	struct sigaction sigact;
	struct option long_opts[] = {
#ifdef WITH_ALSA
		{ "alsa", required_argument, NULL, IN_ALSA },
#endif
#ifdef WITH_SNDFILE
		{ "sndfile", required_argument, NULL, IN_SNDFILE },
#endif
#ifdef WITH_RTL
		{ "rtlsdr", required_argument, NULL, IN_RTL },
#endif
#ifdef WITH_AIR
		{ "airspy", required_argument, NULL, IN_AIR },
#endif
#ifdef WITH_SDRPLAY
		{ "sdrplay", no_argument, NULL, IN_SDRPLAY },
#endif
#ifdef WITH_SOAPY
		{ "soapysdr", required_argument, NULL, IN_SOAPY },
#endif
		{ "verbose", no_argument, NULL, 'v' },
		{ "skip-reassembly", no_argument, NULL, -1 },
		{ "output", required_argument, NULL, -2 },
		{ "statsd", required_argument, NULL, -3 },
		{ NULL, 0, NULL, 0 }
	};
	char sys_hostname[HOST_NAME_MAX + 1];
	char *lblf = NULL;
	char *inarg = NULL, *statsdarg = NULL;

	gethostname(sys_hostname, HOST_NAME_MAX);
	sys_hostname[HOST_NAME_MAX] = 0;
	R.idstation = strdup(sys_hostname);

	res = 0;
	while ((c = getopt_long(argc, argv, "hvt:g:m:a:Aep:c:i:L:G:b:B:", long_opts, NULL)) != EOF) {
		switch (c) {
#ifdef HAVE_LIBACARS
		case -1:
			R.skip_reassembly = 1;
			break;
#endif
		case -2:
			res = setup_output(optarg);
			if (res)
				exit(res);
			break;
		case -3:
			statsdarg = optarg;
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
#ifdef WITH_ALSA
		case IN_ALSA:
			if (R.inmode)
				errx(-1, "Only 1 input allowed");
			R.inmode = IN_ALSA;
			inarg = optarg;
			break;
#endif
#ifdef WITH_SNDFILE
		case IN_SNDFILE:
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
		case IN_RTL:
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
		case IN_SDRPLAY:
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
		case IN_SOAPY:
			if (R.inmode)
				errx(-1, "Only 1 input allowed");
			R.inmode = IN_SOAPY;
			inarg = optarg;
			break;
		case 'a':
			R.antenna = optarg;
			break;
#endif
#ifdef WITH_AIR
		case IN_AIR:
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
		if (!R.rateMult)
			R.rateMult = 1U;
		res = initSoundfile(inarg);
		break;
#endif
#ifdef WITH_RTL
	case IN_RTL:
		if (!R.rateMult)
			R.rateMult = 160U;
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
		R.rateMult = 160U;
		res = initSdrplay();
		break;
#endif
#ifdef WITH_SOAPY
	case IN_SOAPY:
		if (!R.rateMult)
			R.rateMult = 160U;
		if (!R.gain)
			R.gain = -10;
		res = initSoapy(inarg);
		break;
#endif
	default:
		res = -1;
	}

	if (res)
		errx(res, "Unable to init input");

	build_label_filter(lblf);

	res = initOutputs();
	if (res)
		errx(res, "Unable to init outputs");

	if (!R.channels)
		errx(-1, "No channel initialized!");

	if (statsdarg) {
		res = statsd_init(statsdarg, R.idstation);
		if (res < 0)
			errx(res, "Unable to init statsd");
		R.statsd = 1;
	}

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
		errx(res, "Unable to init internal decoders");

#if defined(DEBUG) && defined(WITH_SNDFILE)
	if (R.inmode != IN_SNDFILE) {
		initSndWrite();
	}
#endif

	fprintf(stderr, "Starting, decoding %d channels\n", R.nbch);

	R.running = 1;

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

	R.running = 0;

	deinitAcars();

	exitOutputs();

#if defined(DEBUG) && defined(WITH_SNDFILE)
	SndWriteClose();
#endif
	exit(res);
}

/**
 * Parse a parameter string.
 * Parameter string formatted like "param1=foo,param2=blah,param3=42"
 * @paramsp pointer to params string input
 * @sp pointer to struct containing expected parameters, the #valp member will be updated for each match
 * @np array size of #sp
 * @return NULL if the input has been succesfully exhausted (or was NULL), pointer to first unmatched token otherwise
 * @note Behavior matching that of strsep(): *paramsp is updated to point to next token group or NULL if EOL
 * If an unidentified parameter is found in the string, it is returned by the function, with the '=' separator restored
 */
char * parse_params(char **paramsp, struct params_s *sp, const int np)
{
	char *param, *sep;
	int i;

	while ((param = strsep(paramsp, ","))) {
		sep = strchr(param, '=');
		if (!sep)
			return param;

		*sep++ = '\0';

		for (i = 0; i < np; i++) {
			if (!strcmp(sp[i].name, param)) {
				*sp[i].valp = sep;
				break;
			}
		}

		if (np == i) {		// unknown param
			*--sep = '=';	// restore key-value separator for external processing
			return param;
		}
	}

	return NULL;
}

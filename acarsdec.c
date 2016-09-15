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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sched.h>
#include "acarsdec.h"

channel_t channel[MAXNBCHANNELS];
unsigned int nbch;

char *idstation;
int inmode = 0;
int verbose = 0;
int outtype = 2;
int netout;
int airflt = 0;
#ifdef WITH_RTL
int gain = 1000;
int ppm = 0;
#endif
#ifdef WITH_AIR
int gain = 10;
#endif

char *Rawaddr = NULL;
char *logfilename = NULL;

static void usage(void)
{
	fprintf(stderr,
		"Acarsdec/acarsserv 3.2 Copyright (c) 2015 Thierry Leconte \n\n");
	fprintf(stderr,
		"Usage: acarsdec  [-v] [-o lv] [-A] [-n ipaddr:port] [-l logfile]");
#ifdef WITH_ALSA
	fprintf(stderr, " -a alsapcmdevice  |");
#endif
#ifdef WITH_RTL
	fprintf(stderr,
		" [-g gain] [-p ppm] -r rtldevicenumber  f1 [f2] ... [f4]");
#endif
	fprintf(stderr, "\n\n");
	fprintf(stderr, " -v\t\t\t: verbose\n");
	fprintf(stderr,
		" -A\t\t\t: don't display uplink messages (ie : only aircraft messages)\n");
	fprintf(stderr,
		"\n -o lv\t\t\t: output format : 0: no log, 1 one line by msg., 2 full (default) \n");
	fprintf(stderr,
		" -l logfile\t\t: Append log messages to logfile (Default : stdout).\n");
	fprintf(stderr,
		" -n ipaddr:port\t\t: send acars messages to addr:port on UDP in planeplotter compatible format\n");
	fprintf(stderr,
		" -N ipaddr:port\t\t: send acars messages to addr:port on UDP in acarsdev nativ format\n");
	fprintf(stderr,
		" -i stationid\t\t: station id used in acarsdec network format.\n\n");
#ifdef WITH_ALSA
	fprintf(stderr,
		" -a alsapcmdevice\t: decode from soundcard input alsapcmdevice (ie: hw:0,0)\n");
#endif
#ifdef WITH_RTL
	fprintf(stderr,
		" -g gain\t\t: set rtl preamp gain in tenth of db (ie -g 90 for +9db). By default use AGC\n");
	fprintf(stderr, " -p ppm\t\t\t: set rtl ppm frequency correction\n");
	fprintf(stderr,
		" -r rtldevice f1 [f2]...[f4]\t: decode from rtl dongle number or S/N rtldevice receiving at VHF frequencies f1 and optionaly f2 to f4 in Mhz (ie : -r 0 131.525 131.725 131.825 )\n");
#endif
#ifdef WITH_AIR
	fprintf(stderr,
		" -s f1 [f2]...[f4]\t: decode from airspy receiving at VHF frequencies f1 and optionaly f2 to f4 in Mhz (ie : -r 0 131.525 131.725 131.825 )\n");
#endif
	fprintf(stderr,
		"\nFor any input source , up to 4 channels  could be simultanously decoded\n");
	exit(1);
}

static void sighandler(int signum)
{
	fprintf(stderr, "Signal caught, exiting!\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	int res, n;
	struct sigaction sigact;

	while ((c = getopt(argc, argv, "vafrso:g:Ap:n:N:l:c:i:")) != EOF) {

		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'o':
			outtype = atoi(optarg);
			break;
#ifdef WITH_ALSA
		case 'a':
			res = initAlsa(argv, optind);
			inmode = 1;
			break;
#endif
#ifdef WITH_RTL
		case 'r':
			res = initRtl(argv, optind);
			inmode = 3;
			break;
		case 'p':
			ppm = atoi(optarg);
			break;
#endif
#ifdef WITH_AIR
		case 's':
			res = initAirspy(argv, optind);
			inmode = 4;
			break;
#endif
		case 'g':
			gain = atoi(optarg);
			break;
		case 'n':
			Rawaddr = optarg;
			netout = 0;
			break;
		case 'N':
			Rawaddr = optarg;
			netout = 1;
			break;
		case 'A':
			airflt = 1;
			break;
		case 'l':
			logfilename = optarg;
			break;
		case 'i':
			idstation = strndup(optarg,8);
			break;

		default:
			usage();
		}
	}

	if (inmode == 0) {
		fprintf(stderr, "Need at least one of -a|-f|-r options\n");
		usage();
	}

	if (res) {
		fprintf(stderr, "Unable to init input\n");
		exit(res);
	}

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	for (n = 0; n < nbch; n++) {
		channel[n].chn = n;

		res = initMsk(&(channel[n]));
		if (res)
			break;
		res = initAcars(&(channel[n]));
		if (res)
			break;
	}

	if (res) {
		fprintf(stderr, "Unable to init internal decoders\n");
		exit(res);
	}

	res = initOutput(logfilename, Rawaddr);
	if (res) {
		fprintf(stderr, "Unable to init output\n");
		exit(res);
	}
	if (verbose)
		fprintf(stderr, "Decoding %d channels\n", nbch);

	/* main decoding  */
	switch (inmode) {
#ifdef WITH_ALSA
	case 1:
		res = runAlsaSample();
		break;
#endif
#ifdef WITH_RTL
	case 3:
		res = runRtlSample();
		break;
#endif
#ifdef WITH_AIR
	case 4:
		res = runAirspy();
		break;
#endif
	default:
		res = -1;
	}

	exit(res);

}

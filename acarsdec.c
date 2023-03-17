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
#include <unistd.h>
#include <limits.h>
#ifdef HAVE_LIBACARS
#include <libacars/version.h>
#endif
#include "acarsdec.h"
extern void build_label_filter(char *arg);

channel_t channel[MAXNBCHANNELS];
unsigned int nbch;

char *idstation = NULL;
int inmode = 0;
int verbose = 0;
int outtype = OUTTYPE_STD;
int netout = NETLOG_NONE;
int airflt = 0;
int emptymsg = 0;
int mdly=600;
int hourly = 0;
int daily = 0;

int signalExit = 0;

#ifdef HAVE_LIBACARS
int skip_reassembly = 0;
#endif

#ifdef WITH_RTL
int gain = -100;
int ppm = 0;
int rtlMult = 160;
#endif

#ifdef WITH_AIR
int gain = 18;
#endif

#ifdef	WITH_SDRPLAY
int	lnaState	= 2;
int	GRdB		= 20;
int	ppm		= 0;
#endif
#ifdef WITH_SOAPY
char *antenna=NULL;
double gain = -10.0;
int ppm = 0;
int rateMult = 160;
int freq = 0;
#endif

#ifdef WITH_MQTT
char *mqtt_urls[16];
int mqtt_nburls=0;
char *mqtt_topic=NULL;
char *mqtt_user=NULL;
char *mqtt_passwd=NULL;
#endif

char *Rawaddr = NULL;
char *logfilename = NULL;

static void usage(void)
{
	fprintf(stderr,
		"Acarsdec/acarsserv %s Copyright (c) 2022 Thierry Leconte\n", ACARSDEC_VERSION);
#ifdef HAVE_LIBACARS
	fprintf(stderr,	"(libacars %s)\n", LA_VERSION);
#endif
	fprintf(stderr,
		"\nUsage: acarsdec  [-o lv] [-t time] [-A] [-b 'labels,..'] [-e] [-i station_id] [-n|-j|-N ipaddr:port] [-l logfile [-H|-D]]");
#ifdef HAVE_LIBACARS
	fprintf(stderr, " [--skip-reassembly] ");
#endif
#ifdef WITH_MQTT
	fprintf(stderr, " [ -M mqtt_url");
	fprintf(stderr, " [-T mqtt_topic] |");
	fprintf(stderr, " [-U mqtt_user |");
	fprintf(stderr, " -P mqtt_passwd]]|");
#endif
#ifdef WITH_ALSA
	fprintf(stderr, " -a alsapcmdevice  |");
#endif
#ifdef WITH_SNDFILE
	fprintf(stderr, " -f inputwavfile  |");
#endif
#ifdef WITH_RTL
	fprintf(stderr,
		" [-g gain] [-p ppm] -r rtldevicenumber  f1 [f2] ... [fN]");
#endif
#ifdef WITH_AIR
	fprintf(stderr,
		"[-g linearity_gain] -s airspydevicenumber f1 [f2] ... [fN]");
#endif
#ifdef	WITH_SDRPLAY
	fprintf (stderr, " [-L lnaState] [-G GRdB] [-p ppm] -s f1 [f2] .. [fN]");
#endif
#ifdef	WITH_SOAPY
	fprintf (stderr, " [--antenna antenna] [-g gain] [-p ppm] [-c freq] -d devicestring f1 [f2] .. [fN]");
#endif
	fprintf(stderr, "\n\n");
#ifdef HAVE_LIBACARS
	fprintf(stderr, " --skip-reassembly\t: disable reassembling fragmented ACARS messages\n");
#endif
	fprintf(stderr,
		" -i stationid\t\t: station id used in acarsdec network format.\n");
	fprintf(stderr,
		" -A\t\t\t: don't output uplink messages (ie : only aircraft messages)\n");
	fprintf(stderr,
		" -e\t\t\t: don't output empty messages (ie : _d,Q0, etc ...)\n");
	fprintf(stderr,
		" -b filter\t\t: filter output by label (ex: -b \"H1:Q0\" : only output messages  with label H1 or Q0)\n");
	fprintf(stderr,
		" -o lv\t\t\t: output format : 0 : no log, 1 : one line by msg, 2 : full (default) , 3 : monitor , 4 : msg JSON, 5 : route JSON\n");
	fprintf(stderr,
		" -t time\t\t: set forget time (TTL) in seconds for monitor mode (default=600s)\n");
	fprintf(stderr,
		" -l logfile\t\t: append log messages to logfile (Default : stdout).\n");
	fprintf(stderr,
		" -H\t\t\t: rotate log file once every hour\n");
	fprintf(stderr,
		" -D\t\t\t: rotate log file once every day\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
		" -n ipaddr:port\t\t: send acars messages to addr:port on UDP in planeplotter compatible format\n");
	fprintf(stderr,
		" -N ipaddr:port\t\t: send acars messages to addr:port on UDP in acarsdec native format\n");
	fprintf(stderr,
		" -j ipaddr:port\t\t: send acars messages to addr:port on UDP in acarsdec json format\n");
#ifdef WITH_MQTT
	fprintf(stderr, " -M mqtt_url\t\t: Url of MQTT broker\n");
	fprintf(stderr, " -T mqtt_topic\t\t: Optionnal MQTT topic (default : acarsdec/${station_id})\n");
	fprintf(stderr, " -U mqtt_user\t\t: Optional MQTT username\n");
	fprintf(stderr, " -P mqtt_passwd\t\t: Optional MQTT password\n");
#endif
	fprintf(stderr, "\n");

#ifdef WITH_ALSA
	fprintf(stderr,
		" -a alsapcmdevice\t: decode from soundcard input alsapcmdevice (ie: hw:0,0)\n");
#endif
#ifdef WITH_SNDFILE
	fprintf(stderr,
		" -f inputwavfile\t: decode from a wav file at %d sampling rate\n",INTRATE);
#endif
#ifdef WITH_RTL
	fprintf(stderr,
		" -g gain\t\t: set rtl gain in db (0 to 49.6; >52 and -10 will result in AGC; default is AGC)\n");
	fprintf(stderr, " -p ppm\t\t\t: set rtl ppm frequency correction\n");
	fprintf(stderr, " -m rtlMult\t\t\t: set rtl sample rate multiplier: 160 for 2 MS/s or 192 for 2.4 MS/s (default: 160)\n");
	fprintf(stderr,
		" -r rtldevice f1 [f2]...[f%d]\t: decode from rtl dongle number or S/N rtldevice receiving at VHF frequencies f1 and optionally f2 to f%d in Mhz (ie : -r 0 131.525 131.725 131.825 )\n", MAXNBCHANNELS, MAXNBCHANNELS);
#endif
#ifdef WITH_AIR
	fprintf(stderr,
		" -g linearity_gain\t: set linearity gain [0-21] default : 18\n");
	fprintf(stderr,
		" -s airspydevice f1 [f2]...[f%d]\t: decode from airspy dongle number or hex serial number receiving at VHF frequencies f1 and optionally f2 to f%d in Mhz (ie : -s 131.525 131.725 131.825 )\n", MAXNBCHANNELS, MAXNBCHANNELS);
#endif
#ifdef	WITH_SDRPLAY
	fprintf (stderr,
	          "-L lnaState: set the lnaState (depends on the device)\n"\
	          "-G Gain reducction in dB's, range 20 .. 59 (-100 is autogain)\n"\
	          " -s f1 [f2]...[f%d]\t: decode from sdrplay receiving at VHF frequencies f1 and optionally f2 to f%d in Mhz (ie : -s 131.525 131.725 131.825 )\n", MAXNBCHANNELS, MAXNBCHANNELS);
#endif
#ifdef	WITH_SOAPY
	fprintf(stderr, 
		" --antenna antenna\t: set antenna port to use\n");
	fprintf(stderr,
		" -g gain\t\t: set gain in db (-10 will result in AGC; default is AGC)\n");
	fprintf(stderr, " -p ppm\t\t\t: set ppm frequency correction\n");
	fprintf(stderr, " c freq\t\t\t: set center frequency to tune to\n");
	fprintf(stderr, " -m rateMult\t\t\t: set sample rate multiplier: 160 for 2 MS/s or 192 for 2.4 MS/s (default: 160)\n");
	fprintf (stderr,
		" -d devicestring f1 [f2] .. [f%d]\t: decode from a SoapySDR device located by devicestring at VHF frequencies f1 and optionally f2 to f%d in Mhz (ie : -d driver=rtltcp 131.525 131.725 131.825 )\n", MAXNBCHANNELS, MAXNBCHANNELS);
#endif

	fprintf(stderr,
		" Up to %d channels may be simultaneously decoded\n", MAXNBCHANNELS);
	exit(1);
}

static void sigintHandler(int signum)
{
	fprintf(stderr, "Received signal %s, exiting.\n", strsignal(signum));
#ifdef DEBUG
	SndWriteClose();
#endif
#ifdef WITH_RTL
	signalExit = 1;
	runRtlCancel();
#elif WITH_SOAPY
	signalExit = 1;
#else
	exit(0);
#endif
}

int main(int argc, char **argv)
{
	int c;
	int res, n;
	struct sigaction sigact;
	struct option long_opts[] = {
		{ "verbose", no_argument, NULL, 'v' },
		{ "skip-reassembly", no_argument, NULL, 1 },
#ifdef WITH_SOAPY
		{ "antenna", required_argument, NULL, 2},
#endif
		{ NULL, 0, NULL, 0 }
	};
	char sys_hostname[HOST_NAME_MAX+1];
	char *lblf=NULL;

	gethostname(sys_hostname, HOST_NAME_MAX);
	sys_hostname[HOST_NAME_MAX]=0;
	idstation = strdup(sys_hostname);

	res = 0;
	while ((c = getopt_long(argc, argv, "HDvarfdsRo:t:g:m:Aep:n:N:j:l:c:i:L:G:b:M:P:U:T:", long_opts, NULL)) != EOF) {

		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'o':
			outtype = atoi(optarg);
			break;
		case 't':
			mdly = atoi(optarg);
			break;
		case 'b':
			lblf=optarg;
			break;
#ifdef HAVE_LIBACARS
		case 1:
			skip_reassembly = 1;
			break;
#endif
#ifdef WITH_ALSA
		case 'a':
			res = initAlsa(argv, optind);
			inmode = 1;
			break;
#endif
#ifdef WITH_SNDFILE
		case 'f':
			res = initSoundfile(argv, optind);
			inmode = 2;
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
		case 'g':
			gain = 10 * atof(optarg);
			break;
		case 'm':
			rtlMult = atoi(optarg);
			break;
#endif
#ifdef	WITH_SDRPLAY
		case 's':
			res = initSdrplay (argv, optind);
			inmode = 5;
			break;
		case 'p':
			ppm = atoi(optarg);
			break;
    case 'L':
			lnaState = atoi(optarg);
			break;
    		case 'G':
			GRdB = atoi(optarg);
			break;
#endif
#ifdef WITH_SOAPY
		case 2:
			antenna = optarg;
			break;
		case 'd':
			res = initSoapy(argv, optind);
			inmode = 6;
			break;
		case 'p':
			ppm = atoi(optarg);
			break;
		case 'c':
			freq = atoi(optarg);
			break;
		case 'g':
			gain = atof(optarg);
			break;
		case 'm':
			rateMult = atoi(optarg);
			break;
#endif
#ifdef WITH_AIR
		case 's':
			res = initAirspy(argv, optind);
			inmode = 4;
			break;
    		case 'g':
			gain = atoi(optarg);
			break;
#endif
#ifdef WITH_MQTT
		case 'M':
			if(mqtt_nburls<15) {
				mqtt_urls[mqtt_nburls]=strdup(optarg);
				mqtt_nburls++;
				mqtt_urls[mqtt_nburls]=NULL;
				netout = NETLOG_MQTT;
			}
			break;
    		case 'U':
			mqtt_user = strdup(optarg);
			break;
    		case 'P':
			mqtt_passwd = strdup(optarg);
			break;
    		case 'T':
			mqtt_topic = strdup(optarg);
			break;
#endif
		case 'n':
			Rawaddr = optarg;
			netout = NETLOG_PLANEPLOTTER;
			break;
		case 'N':
			Rawaddr = optarg;
			netout = NETLOG_NATIVE;
			break;
		case 'j':
			Rawaddr = optarg;
			netout = NETLOG_JSON;
			break;
		case 'A':
			airflt = 1;
			break;
		case 'e':
			emptymsg = 1;
			break;
		case 'l':
			logfilename = optarg;
			break;
		case 'H':
			hourly = 1;
			break;
		case 'D':
			daily = 1;
			break;
		case 'i':
			free(idstation);
			idstation = strdup(optarg);
			break;

		default:
			usage();
		}
	}

	if (inmode == 0) {
		fprintf(stderr, "Need at least one of -a|-f|-r|-R|-d options\n");
		usage();
	}


	if (res) {
		fprintf(stderr, "Unable to init input\n");
		exit(res);
	}

	if(hourly && daily) {
		fprintf(stderr, "Options: -H and -D are exclusive\n");
		exit(1);
	}

	build_label_filter(lblf);

	res = initOutput(logfilename, Rawaddr);
	if (res) {
		fprintf(stderr, "Unable to init output\n");
		exit(res);
	}

#ifdef WITH_MQTT
	if (netout == NETLOG_MQTT) {
		res = MQTTinit(mqtt_urls,idstation,mqtt_topic,mqtt_user,mqtt_passwd);
		if (res) {
			fprintf(stderr, "Unable to init MQTT\n");
			exit(res);
		}
	}
#endif

#ifdef WITH_SOAPY
	if (antenna) {
		if (verbose) fprintf(stderr, "Setting soapy antenna to %s\n", antenna);
		res = soapySetAntenna(antenna);
		if (res) {
			fprintf(stderr, "Unable to set antenna for SoapySDR\n");
			exit(res);
		}
	}
#endif
	
	sigact.sa_handler = sigintHandler;
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

#ifdef DEBUG
	if (inmode !=2 ) {
		initSndWrite();
	}
#endif

	if (verbose)
		fprintf(stderr, "Decoding %d channels\n", nbch);

	/* main decoding  */
	switch (inmode) {
#ifdef WITH_ALSA
	case 1:
		res = runAlsaSample();
		break;
#endif
#ifdef WITH_SNDFILE
	case 2:
		res = runSoundfileSample();
		break;
#endif
#ifdef WITH_RTL
	case 3:
		runRtlSample();
		res = runRtlClose();
		break;
#endif
#ifdef WITH_AIR
	case 4:
		res = runAirspySample();
		break;
#endif
#ifdef WITH_SDRPLAY
	case 5:
              res = runSdrplaySample ();
              break;
#endif
#ifdef WITH_SOAPY
	case 6:
		runSoapySample();
		res = runSoapyClose();
		break;
#endif
	default:
		res = -1;
	}

	fprintf(stderr, "exiting ...\n");

	deinitAcars();

#ifdef WITH_MQTT
	MQTTend();
#endif
	exit(res);

}

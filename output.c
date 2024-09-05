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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#ifdef HAVE_LIBACARS
#include <sys/time.h>
#include <libacars/libacars.h>
#include <libacars/acars.h>
#include <libacars/reassembly.h>
#include <libacars/vstring.h>
#endif
#ifdef HAVE_CJSON
#include <cJSON.h>
#endif
#include "acarsdec.h"
#include "label.h"
#include "output.h"
#include "fileout.h"
#include "netout.h"
#ifdef WITH_MQTT
#include "mqttout.h"
#endif

#define FMTBUFLEN 30000
static char fmtbuf[FMTBUFLEN+1];

#define IS_DOWNLINK_BLK(bid) ((bid) >= '0' && (bid) <= '9')

#ifdef HAVE_LIBACARS
#define LA_ACARS_REASM_TABLE_CLEANUP_INTERVAL 1000
static la_reasm_ctx *reasm_ctx = NULL;
// ACARS reassembly timeouts
static struct timeval const acars_reasm_timeout_downlink = { .tv_sec = 660, .tv_usec = 0 }; // VGT4
static struct timeval const acars_reasm_timeout_uplink = { .tv_sec = 90, .tv_usec = 0 }; // VAT4

typedef struct {
	char *addr, *label, *msn;
} acars_key;

static uint32_t acars_key_hash(void const *key)
{
	acars_key *k = (acars_key *)key;
	uint32_t h = la_hash_string(k->addr, LA_HASH_INIT);
	h = la_hash_string(k->label, h);
	h = la_hash_string(k->msn, h);
	return h;
}

static bool acars_key_compare(void const *key1, void const *key2)
{
	acars_key *k1 = (acars_key *)key1;
	acars_key *k2 = (acars_key *)key2;
	return (!strcmp(k1->addr, k2->addr) &&
		!strcmp(k1->label, k2->label) &&
		!strcmp(k1->msn, k2->msn));
}

static void acars_key_destroy(void *ptr)
{
	if (ptr == NULL) {
		return;
	}
	acars_key *key = (acars_key *)ptr;
	free(key->addr);
	free(key->label);
	free(key->msn);
	free(key);
}

static void *acars_tmp_key_get(void const *msg)
{
	acarsmsg_t *amsg = (acarsmsg_t *)msg;
	acars_key *key = calloc(1, sizeof(*key));
	if (key == NULL)
		return NULL;
	key->addr = amsg->addr;
	key->label = amsg->label;
	key->msn = amsg->msn;
	return (void *)key;
}

static void *acars_key_get(void const *msg)
{
	acarsmsg_t *amsg = (acarsmsg_t *)msg;
	acars_key *key = calloc(1, sizeof(*key));
	if (key == NULL)
		return NULL;
	key->addr = strdup(amsg->addr);
	key->label = strdup(amsg->label);
	key->msn = strdup(amsg->msn);
	return (void *)key;
}

static la_reasm_table_funcs acars_reasm_funcs = {
	.get_key = acars_key_get,
	.get_tmp_key = acars_tmp_key_get,
	.hash_key = acars_key_hash,
	.compare_keys = acars_key_compare,
	.destroy_key = acars_key_destroy
};

#endif // HAVE_LIBACARS

static const struct {
	int fmt;
	const char *name;
	const char *desc;
} out_fmts[] = {
	{ FMT_ONELINE,	"oneline",	"Single line summary" },
	{ FMT_FULL,	"full",		"Full text decoding" },
	{ FMT_MONITOR,	"monitor",	"Live monitoring" },
	{ FMT_PP,	"pp",		"PlanePlotter format" },
	{ FMT_NATIVE,	"native",	"Acarsdec native format" },
#ifdef HAVE_CJSON
	{ FMT_JSON,	"json",	 	"Acarsdec JSON format" },
	{ FMT_ROUTEJSON,"routejson",	"Acarsdec Route JSON format" },
#endif
};

static const struct {
	int dst;
	const char *name;
	const char *desc;
} out_dsts[] = {
	{ DST_FILE,	"file",		"File (including stdout) output. PARAMS: path,rotate" },
	{ DST_UDP,	"udp",	 	"UDP network output. PARAMS: host,port" },
#ifdef WITH_MQTT
	{ DST_MQTT,	"mqtt",	 	"MQTT output. PARAMS: uri,user,passwd,topic" },
#endif
};

static bool validate_output(output_t *output)
{
	if (!output->fmt || !output->dst)
		return false;

	switch (output->dst) {
		case DST_FILE:
			// all formats valid
			return true;
		case DST_UDP:
			// all but MONITOR valid
			if (FMT_MONITOR == output->fmt)
				return false;
			else
				return true;
		case DST_MQTT:
			// only JSON formats valid
			switch (output->fmt) {
			case FMT_JSON:
			case FMT_ROUTEJSON:
				return true;
			default:
				return false;
			}
		default:
			return false;
	}
}

static void output_help(void)
{
	unsigned int i;

	fprintf(stderr,
		"--output FORMAT:DESTINATION:PARAMS\n"
		"\nSupported FORMAT:\n");
	for (i = 0; i < ARRAY_SIZE(out_fmts); i++)
		fprintf(stderr, " \"%s\": %s\n", out_fmts[i].name, out_fmts[i].desc);

	fprintf(stderr, "\nSupported DESTINATION:\n");
	for (i = 0; i < ARRAY_SIZE(out_dsts); i++)
		fprintf(stderr, " \"%s\": %s\n", out_dsts[i].name, out_dsts[i].desc);

	fprintf(stderr, "\n");
}

// fmt:dst:param1=xxx,param2=yyy
int setup_output(char *outarg)
{
	// NB: outarg is taken from program global argv: it exists throughout the execution of the program
	char **ap, *argv[3] = {0};
	output_t *output;
	unsigned int i;

	// check if help is needed first
	if (outarg && !strcmp("help", outarg)) {
		output_help();
		return -1;
	}

	// parse first 2 separators, leave the rest of the string untouched as argv[2]
	for (ap = argv; ap < &argv[2] && (*ap = strsep(&outarg, ":")); ap++);
	argv[2] = outarg;

	if (!argv[0] || '\0' == *argv[0] || !argv[1] || '\0' == *argv[1]) {
		fprintf(stderr, "ERROR: Not enough output arguments\n");
		return -1;	// not enough arguments
	}

	output = calloc(1, sizeof(*output));
	if (!output) {
		perror(NULL);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(out_fmts); i++) {
		if (!strcmp(argv[0], out_fmts[i].name)) {
			output->fmt = out_fmts[i].fmt;
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(out_dsts); i++) {
		if (!strcmp(argv[1], out_dsts[i].name)) {
			output->dst = out_dsts[i].dst;
			break;
		}
	}

	if (!validate_output(output)) {
		fprintf(stderr, "ERROR: Invalid output configuration: %s:%s\n", argv[0], argv[1]);
		return -1;	// invalid output config
	}

	if (argv[2] && '\0' != *argv[2])
		output->params = argv[2];

	output->next = R.outputs;
	R.outputs = output;

	return 0;
}

int initOutputs(void)
{
	output_t *out;

	if (!R.outputs)
		return -1;

	for (out = R.outputs; out; out = out->next) {
		switch (out->dst) {
		case DST_FILE:
			out->priv = Fileoutinit(out->params);
			if (!out->priv)
				return -1;
			break;
		case DST_UDP:
			out->priv = Netoutinit(out->params);
			if (!out->priv)
				return -1;
			break;
#ifdef WITH_MQTT
		case DST_MQTT:
			out->priv = MQTTinit(out->params);
			if (!out->priv)
				return -1;
			break;
#endif
		default:
			return -1;
		}

		if (out->fmt == FMT_MONITOR)
			R.verbose = 0;
	}

#ifdef HAVE_LIBACARS
	reasm_ctx = R.skip_reassembly ? NULL : la_reasm_ctx_new();
#endif
	return 0;
}

void exitOutputs(void)
{
	output_t *out;

	if (!R.outputs)
		return;

	for (out = R.outputs; out; out = out->next) {
		switch (out->dst) {
		default:
		case DST_FILE:
			Fileoutexit(out->priv);
			break;
		case DST_UDP:
			Netexit(out->priv);
			break;
#ifdef WITH_MQTT
		case DST_MQTT:
			MQTTexit(out->priv);
			break;
#endif
		}
	}
}

static int fmt_sv(acarsmsg_t *msg, int chn, struct timeval tv, char *buf, size_t bufsz)
{
	struct tm tmp;

	if (!msg || !buf)
		return -1;

	gmtime_r(&(tv.tv_sec), &tmp);

	return snprintf(buf, bufsz,
		       "%8s %1d %02d/%02d/%04d %02d:%02d:%02d %1d %03d %1c %7s %1c %2s %1c %4s %6s %s",
		       R.idstation, chn + 1, tmp.tm_mday, tmp.tm_mon + 1,
		       tmp.tm_year + 1900, tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
		       msg->err, (int)(msg->lvl), msg->mode, msg->addr, msg->ack, msg->label,
			msg->bid ? msg->bid : '.', msg->no, msg->fid, msg->txt ? msg->txt : "");
}

static int fmt_pp(acarsmsg_t *msg, char *buf, size_t bufsz)
{
	char *pstr;
	int res;

	if (!msg || !buf)
		return -1;

	char *txt = strdup(msg->txt ? msg->txt : "");
	for (pstr = txt; *pstr != 0; pstr++)
		if (*pstr == '\n' || *pstr == '\r')
			*pstr = ' ';

	res = snprintf(buf, bufsz, "AC%1c %7s %1c %2s %1c %4s %6s %s",
		       msg->mode, msg->addr, msg->ack, msg->label, msg->bid ? msg->bid : '.', msg->no,
		       msg->fid, txt);

	free(txt);
	return res;
}

static int fmt_time(struct timeval tv, char *buf, size_t bufsz)
{
	struct tm tmp;

	gmtime_r(&(tv.tv_sec), &tmp);

	return snprintf(buf, bufsz, "%02d:%02d:%02d.%03ld",
			tmp.tm_hour, tmp.tm_min, tmp.tm_sec, tv.tv_usec / 1000);
}

static int fmt_date(struct timeval tv, char *buf, size_t bufsz)
{
	struct tm tmp;
	int len;

	if (tv.tv_sec + tv.tv_usec == 0)
		return 0;

	gmtime_r(&(tv.tv_sec), &tmp);

	len = snprintf(buf, bufsz, "%02d/%02d/%04d ",
		       tmp.tm_mday, tmp.tm_mon + 1, tmp.tm_year + 1900);
	return len + fmt_time(tv, buf + len, bufsz - len);
}

static int fmt_msg(acarsmsg_t *msg, int chn, struct timeval tv, char *buf, size_t bufsz)
{
	oooi_t oooi = {0};
	int len = 0;

	if (R.inmode >= IN_RTL)
		len += snprintf(buf + len, bufsz - len, "[#%1d (F:%3.3f L:%+5.1f/%.1f E:%1d) ", chn + 1,
			R.channels[chn].Fr / 1000000.0, msg->lvl, msg->nf, msg->err);
	else
		len += snprintf(buf + len, bufsz - len, "[#%1d (L:%+5.1f/%.1f E:%1d) ", chn + 1, msg->lvl, msg->nf, msg->err);

	if (R.inmode != IN_SNDFILE)
		len += fmt_date(tv, buf + len, bufsz - len);

	len += snprintf(buf + len, bufsz - len, " --------------------------------\n");
	len += snprintf(buf + len, bufsz - len, "Mode : %1c ", msg->mode);
	len += snprintf(buf + len, bufsz - len, "Label : %2s ", msg->label);

	if (msg->bid) {
		len += snprintf(buf + len, bufsz - len, "Id : %1c ", msg->bid);
		if (msg->ack == '!')
			len += snprintf(buf + len, bufsz - len, "Nak\n");
		else
			len += snprintf(buf + len, bufsz - len, "Ack : %1c\n", msg->ack);
		len += snprintf(buf + len, bufsz - len, "Aircraft reg: %s ", msg->addr);
		if (IS_DOWNLINK_BLK(msg->bid)) {
			len += snprintf(buf + len, bufsz - len, "Flight id: %s\n", msg->fid);
			len += snprintf(buf + len, bufsz - len, "No: %4s", msg->no);
		}
		if (msg->sublabel[0] != '\0') {
			len += snprintf(buf + len, bufsz - len, "\nSublabel: %s", msg->sublabel);
			if (msg->mfi[0] != '\0') {
				len += snprintf(buf + len, bufsz - len, " MFI: %s", msg->mfi);
			}
		}
#ifdef HAVE_LIBACARS
		if (!R.skip_reassembly) {
			len += snprintf(buf + len, bufsz - len, "\nReassembly: %s", la_reasm_status_name_get(msg->reasm_status));
		}
#endif
	}

	len += snprintf(buf + len, bufsz - len, "\n");
	if (msg->txt)
		len += snprintf(buf + len, bufsz - len, "%s\n", msg->txt);
	if (msg->be == 0x17)
		len += snprintf(buf + len, bufsz - len, "ETB\n");

	if (DecodeLabel(msg, &oooi)) {
		len += snprintf(buf + len, bufsz - len, "##########################\n");
		if (oooi.da[0])
			len += snprintf(buf + len, bufsz - len, "Destination Airport : %s\n", oooi.da);
		if (oooi.sa[0])
			len += snprintf(buf + len, bufsz - len, "Departure Airport : %s\n", oooi.sa);
		if (oooi.eta[0])
			len += snprintf(buf + len, bufsz - len, "Estimation Time of Arrival : %s\n", oooi.eta);
		if (oooi.gout[0])
			len += snprintf(buf + len, bufsz - len, "Gate out Time : %s\n", oooi.gout);
		if (oooi.gin[0])
			len += snprintf(buf + len, bufsz - len, "Gate in Time : %s\n", oooi.gin);
		if (oooi.woff[0])
			len += snprintf(buf + len, bufsz - len, "Wheels off Time : %s\n", oooi.woff);
		if (oooi.won[0])
			len += snprintf(buf + len, bufsz - len, "Wheels on Time : %s\n", oooi.won);
	}
#ifdef HAVE_LIBACARS
	if (msg->decoded_tree != NULL) {
		la_vstring *vstr = la_proto_tree_format_text(NULL, msg->decoded_tree);
		len += snprintf(buf + len, bufsz - len, "%s\n", vstr->str);
		la_vstring_destroy(vstr, true);
	}
#endif
	return len;
}

static int fmt_oneline(acarsmsg_t *msg, int chn, struct timeval tv, char *buf, size_t bufsz)
{
	char txt[60];
	char *pstr;
	int len;

	strncpy(txt, msg->txt ? msg->txt : "", 59);
	txt[59] = 0;
	for (pstr = txt; *pstr != 0; pstr++)
		if (*pstr == '\n' || *pstr == '\r')
			*pstr = ' ';

	len = snprintf(buf, bufsz, "#%1d (L:%+5.1f/%.1f E:%1d) ", chn + 1, msg->lvl, msg->nf, msg->err);

	if (R.inmode != IN_SNDFILE)
		len += fmt_date(tv, buf + len, bufsz - len);

	len += snprintf(buf + len, bufsz - len, " %7s %6s %1c %2s %4s %s", msg->addr, msg->fid, msg->mode, msg->label, msg->no, txt);

	return len;
}

typedef struct flight_s flight_t;
struct flight_s {
	flight_t *next;
	char addr[8];
	char fid[7];
	struct timeval ts, tl;
	int chm;
	int nbm;
	int rt;
	oooi_t oooi;
};
static flight_t *flight_head = NULL;

static flight_t *addFlight(acarsmsg_t *msg, int chn, struct timeval tv)
{
	flight_t *fl, *fld, *flp;

	fl = flight_head;
	flp = NULL;
	while (fl) {
		if (memcmp(msg->addr, fl->addr, sizeof(fl->addr)) == 0)
			break;
		flp = fl;
		fl = fl->next;
	}

	if (fl == NULL) {
		fl = calloc(1, sizeof(*fl));
		if (fl == NULL) {
			perror(NULL);
			return (NULL);
		}
		memcpy(fl->addr, msg->addr, sizeof(fl->addr));
		fl->ts = tv;
	}

	memcpy(fl->fid, msg->fid, sizeof(fl->fid));
	fl->tl = tv;
	fl->chm |= (1 << chn);
	fl->nbm += 1;

	DecodeLabel(msg, &fl->oooi);

	if (flp) {
		flp->next = fl->next;
		fl->next = flight_head;
	}
	flight_head = fl;

	flp = NULL;
	fld = fl;
	while (fld) {
		if (fld->tl.tv_sec < (tv.tv_sec - R.mdly)) {
			if (flp) {
				flp->next = fld->next;
				free(fld);
				fld = flp->next;
			} else {
				flight_head = fld->next;
				free(fld);
				fld = flight_head;
			}
		} else {
			flp = fld;
			fld = fld->next;
		}
	}

	return (fl);
}

#ifdef HAVE_CJSON
static int fmt_json(acarsmsg_t *msg, int chn, struct timeval tv, char *buf, size_t bufsz)
{
	oooi_t oooi = {0};
	float freq = R.channels[chn].Fr / 1000000.0;
	cJSON *json_obj;
	int ok = 0;
	char convert_tmp[8];

	json_obj = cJSON_CreateObject();
	if (json_obj == NULL)
		return ok;

	double t = (double)tv.tv_sec + ((double)tv.tv_usec) / 1e6;
	cJSON_AddNumberToObject(json_obj, "timestamp", t);
	if (R.idstation[0])
		cJSON_AddStringToObject(json_obj, "station_id", R.idstation);
	cJSON_AddNumberToObject(json_obj, "channel", chn);
	snprintf(convert_tmp, sizeof(convert_tmp), "%3.3f", freq);
	cJSON_AddRawToObject(json_obj, "freq", convert_tmp);
	snprintf(convert_tmp, sizeof(convert_tmp), "%2.1f", msg->lvl);
	cJSON_AddRawToObject(json_obj, "level", convert_tmp);
	snprintf(convert_tmp, sizeof(convert_tmp), "%.1f", msg->nf);
	cJSON_AddRawToObject(json_obj, "noise", convert_tmp);
	cJSON_AddNumberToObject(json_obj, "error", msg->err);
	snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->mode);
	cJSON_AddStringToObject(json_obj, "mode", convert_tmp);
	cJSON_AddStringToObject(json_obj, "label", msg->label);

	if (msg->bid) {
		snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->bid);
		cJSON_AddStringToObject(json_obj, "block_id", convert_tmp);

		if (msg->ack == '!') {
			cJSON_AddFalseToObject(json_obj, "ack");
		} else {
			snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->ack);
			cJSON_AddStringToObject(json_obj, "ack", convert_tmp);
		}

		cJSON_AddStringToObject(json_obj, "tail", msg->addr);
		if (IS_DOWNLINK_BLK(msg->bid)) {
			cJSON_AddStringToObject(json_obj, "flight", msg->fid);
			cJSON_AddStringToObject(json_obj, "msgno", msg->no);
		}
	}
	if (msg->txt)
		cJSON_AddStringToObject(json_obj, "text", msg->txt);

	if (msg->be == 0x17)
		cJSON_AddTrueToObject(json_obj, "end");

	if (DecodeLabel(msg, &oooi)) {
		if (oooi.sa[0])
			cJSON_AddStringToObject(json_obj, "depa", oooi.sa);
		if (oooi.da[0])
			cJSON_AddStringToObject(json_obj, "dsta", oooi.da);
		if (oooi.eta[0])
			cJSON_AddStringToObject(json_obj, "eta", oooi.eta);
		if (oooi.gout[0])
			cJSON_AddStringToObject(json_obj, "gtout", oooi.gout);
		if (oooi.gin[0])
			cJSON_AddStringToObject(json_obj, "gtin", oooi.gin);
		if (oooi.woff[0])
			cJSON_AddStringToObject(json_obj, "wloff", oooi.woff);
		if (oooi.won[0])
			cJSON_AddStringToObject(json_obj, "wlin", oooi.won);
	}

	if (msg->sublabel[0] != '\0') {
		cJSON_AddStringToObject(json_obj, "sublabel", msg->sublabel);
		if (msg->mfi[0] != '\0')
			cJSON_AddStringToObject(json_obj, "mfi", msg->mfi);
	}
#ifdef HAVE_LIBACARS
	if (!R.skip_reassembly)
		cJSON_AddStringToObject(json_obj, "assstat", la_reasm_status_name_get(msg->reasm_status));
	if (msg->decoded_tree != NULL) {
		la_vstring *vstr = la_proto_tree_format_json(NULL, msg->decoded_tree);
		cJSON_AddRawToObject(json_obj, "libacars", vstr->str);
		la_vstring_destroy(vstr, true);
	}
#endif

	cJSON *app_info = cJSON_AddObjectToObject(json_obj, "app");
	if (app_info) {
		cJSON_AddStringToObject(app_info, "name", "acarsdec");
		cJSON_AddStringToObject(app_info, "ver", ACARSDEC_VERSION);
	}

	ok = cJSON_PrintPreallocated(json_obj, buf, bufsz, 0);
	cJSON_Delete(json_obj);
	return ok ? strlen(buf) : -1;
}

static int fmt_routejson(flight_t *fl, struct timeval tv, char *buf, size_t bufsz)
{
	if (fl == NULL)
		return 0;

	if (fl->rt == 0 && fl->fid[0] && fl->oooi.sa[0] && fl->oooi.da[0]) {
		cJSON *json_obj;
		int ok;

		json_obj = cJSON_CreateObject();
		if (json_obj == NULL)
			return 0;

		double t = (double)tv.tv_sec + ((double)tv.tv_usec) / 1e6;
		cJSON_AddNumberToObject(json_obj, "timestamp", t);
		if (R.idstation[0])
			cJSON_AddStringToObject(json_obj, "station_id", R.idstation);
		cJSON_AddStringToObject(json_obj, "flight", fl->fid);
		cJSON_AddStringToObject(json_obj, "depa", fl->oooi.sa);
		cJSON_AddStringToObject(json_obj, "dsta", fl->oooi.da);

		ok = cJSON_PrintPreallocated(json_obj, buf, bufsz, 0);
		cJSON_Delete(json_obj);

		fl->rt = ok;
		return ok ? strlen(buf) : -1;
	} else
		return 0;
}
#endif /* HAVE_CJSON */

static int fmt_monitor(acarsmsg_t *msg, int chn, struct timeval tv, char *buf, size_t bufsz)
{
	flight_t *fl;
	int len = 0;

	len += snprintf(buf + len, bufsz - len, "\x1b[H\x1b[2J");
	len += snprintf(buf + len, bufsz - len, "             Acarsdec monitor ");
	len += fmt_time(tv, buf + len, bufsz - len);
	len += snprintf(buf + len, bufsz - len, "\n Aircraft Flight   Nb Channels     First    DEP   ARR   ETA\n");

	fl = flight_head;
	while (fl) {
		unsigned int i;

		len += snprintf(buf + len, bufsz - len, " %-8s %-7s %3d ", fl->addr, fl->fid, fl->nbm);
		for (i = 0; i < R.nbch; i++)
			len += snprintf(buf + len, bufsz - len, "%c", (fl->chm & (1 << i)) ? 'x' : '.');
		len += snprintf(buf + len, bufsz - len, " ");
		len += fmt_time(fl->ts, buf + len, bufsz - len);
		if (fl->oooi.sa[0])
			len += snprintf(buf + len, bufsz - len, " %4s ", fl->oooi.sa);
		else
			len += snprintf(buf + len, bufsz - len, "      ");
		if (fl->oooi.da[0])
			len += snprintf(buf + len, bufsz - len, " %4s ", fl->oooi.da);
		else
			len += snprintf(buf + len, bufsz - len, "      ");
		if (fl->oooi.eta[0])
			len += snprintf(buf + len, bufsz - len, " %4s ", fl->oooi.eta);
		else
			len += snprintf(buf + len, bufsz - len, "      ");
		len += snprintf(buf + len, bufsz - len, "\n");

		fl = fl->next;
	}

	return len;
}

void outputmsg(const msgblk_t *blk)
{
	uint8_t *reassembled_msg = NULL;
	acarsmsg_t msg;
	int i, outflg = 0;
	flight_t *fl = NULL;
	output_t *out;

	/* fill msg struct */
	memset(&msg, 0, sizeof(msg));
	msg.lvl = blk->lvl;
	msg.nf = blk->nf;
	msg.err = blk->err;

	msg.mode = blk->txt.d.mode;

	// XXX NB we wouldn't need these nul-terminated copies if cJSON_AddStringToObject could specify string length

	for (i = 0; i < sizeof(blk->txt.d.addr); i++) {
		if (blk->txt.d.addr[i] != '.')	// skip leading dots -- XXX libacars/dumpvdl2 doesn't
			break;
	}
	memcpy(msg.addr, blk->txt.d.addr+i, sizeof(blk->txt.d.addr)-i);

	/* ACK/NAK */
	msg.ack = blk->txt.d.ack;
	if (msg.ack == 0x15) // NAK is nonprintable
		msg.ack = '!';

	memcpy(&msg.label, blk->txt.d.label, sizeof(blk->txt.d.label));
	if (msg.label[1] == 0x7f)
		msg.label[1] = 'd';

	msg.bid = blk->txt.d.bid;

	bool down = IS_DOWNLINK_BLK(msg.bid);
#ifdef HAVE_LIBACARS
	char msn_seq = 0;	// init to silence GCC invalid warning
	la_msg_dir msg_dir = down ? LA_MSG_DIR_AIR2GND : LA_MSG_DIR_GND2AIR;
	msg.reasm_status = LA_REASM_SKIPPED; // default value (valid for message with empty text)
#endif

	if (R.airflt && !down)
		return;
	if (label_filter(msg.label) == 0)
		return;

	int text_len = blk_textlen(blk);

	if (text_len && blk->txt.d.sot != 0x03) {
		msg.txt = blk->txt.d.text;

		/* txt end */
		msg.be = msg.txt[text_len - 1];

		/* terminate text string before suffix (necessary for cJSON_AddStringToObject()
		 msg.txt could be const without this. XXX keeping the warning as a reminder to revisit */
		msg.txt[text_len - 1] = '\0';
		text_len--;

		if (down) {
			i = sizeof(msg.no) - 1;
			if (text_len < i)
				goto skip;

			/* message no */
			memcpy(msg.no, msg.txt, i);
			msg.txt += i;
			text_len -= i;
#ifdef HAVE_LIBACARS
			/* The 3-char prefix is used in reassembly hash table key, so we need */
			/* to store the MSN separately as prefix and seq character. */
			for (i = 0; i < sizeof(msg.msn)-1; i++)
				msg.msn[i] = msg.no[i];
			msn_seq = msg.no[3];
#endif
			i = sizeof(msg.fid) - 1;
			if (text_len < i)
				goto skip;

			/* Flight id */
			memcpy(msg.fid, msg.txt, i);
			outflg = 1;
			msg.txt += i;
			text_len -= i;
skip:
			(void)i;	// nothing, goto target for end of if() block
		}
#ifdef HAVE_LIBACARS

		// Extract sublabel and MFI if present
		int offset = la_acars_extract_sublabel_and_mfi(msg.label, msg_dir,
							       msg.txt, text_len, msg.sublabel, msg.mfi);
		if (offset > 0) {
			msg.txt += offset;
			text_len -= offset;
		}

		la_reasm_table *acars_rtable = NULL;
		if (msg.bid != 0 && reasm_ctx != NULL) { // not a squitter && reassembly engine is enabled
			acars_rtable = la_reasm_table_lookup(reasm_ctx, &la_DEF_acars_message);
			if (acars_rtable == NULL)
				acars_rtable = la_reasm_table_new(reasm_ctx, &la_DEF_acars_message,
								  acars_reasm_funcs, LA_ACARS_REASM_TABLE_CLEANUP_INTERVAL);

			// The sequence number at which block id wraps at.
			// - downlinks: none (MSN always goes from 'A' up to 'P')
			// - uplinks:
			//   - for VHF Category A (mode=2): wraps after block id 'Z'
			//   - for VHF Category B (mode!=2): wraps after block id 'W'
			//     (blocks 'X'-'Z' are reserved for empty ACKs)

			int seq_num_wrap = SEQ_WRAP_NONE;
			if (!down)
				seq_num_wrap = msg.mode == '2' ? 'Z' + 1 - 'A' : 'X' - 'A';

			msg.reasm_status = la_reasm_fragment_add(acars_rtable,
								 &(la_reasm_fragment_info){
									 .msg_info = &msg,
									 .msg_data = (uint8_t *)msg.txt,
									 .msg_data_len = text_len,
									 .total_pdu_len = 0, // not used
									 .seq_num = down ? msn_seq - 'A' : msg.bid - 'A',
									 .seq_num_first = down ? 0 : SEQ_FIRST_NONE,
									 .seq_num_wrap = seq_num_wrap,
									 .is_final_fragment = msg.be != 0x17, // ETB means "more fragments"
									 .rx_time = blk->tv,
									 .reasm_timeout = down ? acars_reasm_timeout_downlink : acars_reasm_timeout_uplink
								});
		}
		if (msg.reasm_status == LA_REASM_COMPLETE &&
		    la_reasm_payload_get(acars_rtable, &msg, &reassembled_msg) > 0) {
			// reassembled_msg is a newly allocated byte buffer, which is guaranteed to
			// be NULL-terminated, so we can cast it to char * directly.
			msg.txt = (char *)reassembled_msg;
		}
#endif // HAVE_LIBACARS
	}

#ifdef HAVE_LIBACARS
	if (msg.txt != NULL && msg.txt[0] != '\0') {
		bool decode_apps = true;
		// Inhibit higher layer application decoding if reassembly is enabled and
		// is now in progress (ie. the message is not yet complete)
		if (reasm_ctx != NULL && (msg.reasm_status == LA_REASM_IN_PROGRESS ||
					  msg.reasm_status == LA_REASM_DUPLICATE)) {
			decode_apps = false;
		}
		if (decode_apps) {
			msg.decoded_tree = la_acars_apps_parse_and_reassemble(msg.addr, msg.label,
									      msg.txt, msg_dir, reasm_ctx, blk->tv);
		}
	}
#endif

	if (outflg)
		fl = addFlight(&msg, blk->chn, blk->tv);

	if (R.emptymsg && (msg.txt == NULL || msg.txt[0] == '\0'))
		return;

	// for now and until we see contention, we don't bother using a separate thread per output. KISS.
	for (out = R.outputs; out; out = out->next) {
		int len = 0;
		switch (out->fmt) {
		case FMT_MONITOR:
			len = fmt_monitor(&msg, blk->chn, blk->tv, fmtbuf, FMTBUFLEN);
			break;
		case FMT_ONELINE:
			len = fmt_oneline(&msg, blk->chn, blk->tv, fmtbuf, FMTBUFLEN);
			break;
		case FMT_FULL:
			len = fmt_msg(&msg, blk->chn, blk->tv, fmtbuf, FMTBUFLEN);
			break;
		case FMT_NATIVE:
			len = fmt_sv(&msg, blk->chn, blk->tv, fmtbuf, FMTBUFLEN);
			break;
		case FMT_PP:
			len = fmt_pp(&msg, fmtbuf, FMTBUFLEN);
			break;
#ifdef HAVE_CJSON
		case FMT_ROUTEJSON:
			len = fl ? fmt_routejson(fl, blk->tv, fmtbuf, FMTBUFLEN) : -1;
			break;;
		case FMT_JSON:
			len = fmt_json(&msg, blk->chn, blk->tv, fmtbuf, FMTBUFLEN);
			break;
#endif /* HAVE_CJSON */
		default:
			continue;
		}

		// NB if the same format is used for multiple outputs, the buffer will be recomputed each time. Deemed acceptable

		if (len <= 0)
			continue;

		switch (out->dst) {
		case DST_FILE:
			Filewrite(fmtbuf, len, out->priv);
			break;
		case DST_UDP:
			Netwrite(fmtbuf, len, out->priv);
			break;
#ifdef WITH_MQTT
		case DST_MQTT:
			MQTTwrite(fmtbuf, len, out->priv);
			break;
#endif
		default:
			continue;
		}
	}

#ifdef HAVE_LIBACARS
	la_proto_tree_destroy(msg.decoded_tree);
	free(reassembled_msg);
#endif
}

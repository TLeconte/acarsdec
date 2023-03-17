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
#include "acarsdec.h"
#include "cJSON.h"
#include "output.h"

extern int label_filter(char *lbl);

extern int inmode;
extern char *idstation;

static FILE *fdout;

static char *jsonbuf=NULL;
#define JSONBUFLEN 30000

#define IS_DOWNLINK_BLK(bid) ((bid) >= '0' && (bid) <= '9')

#ifdef HAVE_LIBACARS
#define LA_ACARS_REASM_TABLE_CLEANUP_INTERVAL 1000
static la_reasm_ctx *reasm_ctx = NULL;
// ACARS reassembly timeouts
static struct timeval const acars_reasm_timeout_downlink = { .tv_sec = 660,  .tv_usec = 0 };    // VGT4
static struct timeval const acars_reasm_timeout_uplink   = { .tv_sec = 90,   .tv_usec = 0 };    // VAT4

typedef struct {
	char *addr, *label, *msn;
} acars_key;

static uint32_t acars_key_hash(void const *key) {
	acars_key *k = (acars_key *)key;
	uint32_t h = la_hash_string(k->addr, LA_HASH_INIT);
	h = la_hash_string(k->label, h);
	h = la_hash_string(k->msn, h);
	return h;
}

static bool acars_key_compare(void const *key1, void const *key2) {
	acars_key *k1 = (acars_key *)key1;
	acars_key *k2 = (acars_key *)key2;
	return (!strcmp(k1->addr, k2->addr) &&
			!strcmp(k1->label, k2->label) &&
			!strcmp(k1->msn, k2->msn));
}

static void acars_key_destroy(void *ptr) {
	if(ptr == NULL) {
		return;
	}
	acars_key *key = (acars_key *)ptr;
	free(key->addr);
	free(key->label);
	free(key->msn);
	free(key);
}

static void *acars_tmp_key_get(void const *msg) {
	acarsmsg_t *amsg = (acarsmsg_t *)msg;
	acars_key *key = calloc(1, sizeof(acars_key));
	if(key == NULL) 
		return NULL;
	key->addr = amsg->addr;
	key->label = amsg->label;
	key->msn = amsg->msn;
	return (void *)key;
}

static void *acars_key_get(void const *msg) {
	acarsmsg_t *amsg = (acarsmsg_t *)msg;
	acars_key *key = calloc(1, sizeof(acars_key));
	if(key == NULL) 
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

static inline void cls(void)
{
	printf("\x1b[H\x1b[2J");
}

int initOutput(char *logfilename, char *Rawaddr)
{
	if (outtype != OUTTYPE_NONE && logfilename) {
		if((fdout=Fileoutinit(logfilename)) == NULL)
			return -1;
	} else {
		fdout = stdout;
	}

	if (Rawaddr) 
		if(Netoutinit(Rawaddr))
			return -1;

	if (outtype == OUTTYPE_MONITOR ) {
		verbose=0;
		cls();
		fflush(stdout);
	}

	if (outtype == OUTTYPE_JSON || outtype == OUTTYPE_ROUTEJSON || netout==NETLOG_JSON || netout==NETLOG_MQTT ) {
		jsonbuf = malloc(JSONBUFLEN+1);
		if(jsonbuf == NULL) 
			return -1;
	}
#ifdef HAVE_LIBACARS
	reasm_ctx = skip_reassembly ? NULL : la_reasm_ctx_new();
#endif
	return 0;
}

static void printtime(struct timeval tv)
{
	struct tm tmp;

	gmtime_r(&(tv.tv_sec), &tmp);

	fprintf(fdout, "%02d:%02d:%02d.%03ld",
		tmp.tm_hour, tmp.tm_min, tmp.tm_sec, tv.tv_usec/1000);
}

static void printdate(struct timeval tv)
{
	struct tm tmp;

	if (tv.tv_sec + tv.tv_usec == 0)
		return;

	gmtime_r(&(tv.tv_sec), &tmp);

	fprintf(fdout, "%02d/%02d/%04d ",
		tmp.tm_mday, tmp.tm_mon + 1, tmp.tm_year + 1900);
	printtime(tv);
}

static void printmsg(acarsmsg_t * msg, int chn, struct timeval tv)
{
	oooi_t oooi;

#if defined (WITH_RTL) || defined (WITH_AIR) || defined (WITH_SOAPY)
	if (inmode >= 3)
		fprintf(fdout, "\n[#%1d (F:%3.3f L:%+5.1f E:%1d) ", chn + 1,
			channel[chn].Fr / 1000000.0, msg->lvl, msg->err);
	else
#endif
		fprintf(fdout, "\n[#%1d (L:%+5.1f E:%1d) ", chn + 1, msg->lvl, msg->err);

	if (inmode != 2)
		printdate(tv);

	fprintf(fdout, " --------------------------------\n");
	fprintf(fdout, "Mode : %1c ", msg->mode);
	fprintf(fdout, "Label : %2s ", msg->label);

	if(msg->bid) {
		fprintf(fdout, "Id : %1c ", msg->bid);
		if(msg->ack=='!') fprintf(fdout, "Nak\n"); else fprintf(fdout, "Ack : %1c\n", msg->ack);
		fprintf(fdout, "Aircraft reg: %s ", msg->addr);
		if(IS_DOWNLINK_BLK(msg->bid)) {
			fprintf(fdout, "Flight id: %s\n", msg->fid);
			fprintf(fdout, "No: %4s", msg->no);
		}
		if(msg->sublabel[0] != '\0') {
			fprintf(fdout, "\nSublabel: %s", msg->sublabel);
			if(msg->mfi[0] != '\0') {
				fprintf(fdout, " MFI: %s", msg->mfi);
			}
		}
#ifdef HAVE_LIBACARS
		if (!skip_reassembly) {
			fprintf(fdout, "\nReassembly: %s", la_reasm_status_name_get(msg->reasm_status));
		}
#endif
	}

	fprintf(fdout, "\n");
	if(msg->txt[0]) fprintf(fdout, "%s\n", msg->txt);
	if (msg->be == 0x17) fprintf(fdout, "ETB\n");

	if(DecodeLabel(msg,&oooi)) {
		fprintf(fdout, "##########################\n");
		if(oooi.da[0]) fprintf(fdout,"Destination Airport : %s\n",oooi.da);
        	if(oooi.sa[0]) fprintf(fdout,"Departure Airport : %s\n",oooi.sa);
        	if(oooi.eta[0]) fprintf(fdout,"Estimation Time of Arrival : %s\n",oooi.eta);
        	if(oooi.gout[0]) fprintf(fdout,"Gate out Time : %s\n",oooi.gout);
        	if(oooi.gin[0]) fprintf(fdout,"Gate in Time : %s\n",oooi.gin);
        	if(oooi.woff[0]) fprintf(fdout,"Wheels off Tme : %s\n",oooi.woff);
        	if(oooi.won[0]) fprintf(fdout,"Wheels on Time : %s\n",oooi.won);
	}
#ifdef HAVE_LIBACARS
	if(msg->decoded_tree != NULL) {
		la_vstring *vstr = la_proto_tree_format_text(NULL, msg->decoded_tree);
		fprintf(fdout, "%s\n", vstr->str);
		la_vstring_destroy(vstr, true);
	}
#endif
	fflush(fdout);
}


static int buildjson(acarsmsg_t * msg, int chn, struct timeval tv)
{

	oooi_t oooi;
#if defined (WITH_RTL) || defined (WITH_AIR) || defined (WITH_SOAPY)
	float freq = channel[chn].Fr / 1000000.0;
#else
	float freq = 0;
#endif
	cJSON *json_obj;
	int ok = 0;
	char convert_tmp[8];

	json_obj = cJSON_CreateObject();
	if (json_obj == NULL)
		return ok;

	double t = (double)tv.tv_sec + ((double)tv.tv_usec)/1e6;
	cJSON_AddNumberToObject(json_obj, "timestamp", t);
	if(idstation[0]) cJSON_AddStringToObject(json_obj, "station_id", idstation);
	cJSON_AddNumberToObject(json_obj, "channel", chn);
	snprintf(convert_tmp, sizeof(convert_tmp), "%3.3f", freq);
	cJSON_AddRawToObject(json_obj, "freq", convert_tmp);
	snprintf(convert_tmp, sizeof(convert_tmp), "%2.1f", msg->lvl);
	cJSON_AddRawToObject(json_obj, "level", convert_tmp);
	cJSON_AddNumberToObject(json_obj, "error", msg->err);
	snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->mode);
	cJSON_AddStringToObject(json_obj, "mode", convert_tmp);
	cJSON_AddStringToObject(json_obj, "label", msg->label);

	if(msg->bid) {
		snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->bid);
		cJSON_AddStringToObject(json_obj, "block_id", convert_tmp);

		if(msg->ack == '!') {
			cJSON_AddFalseToObject(json_obj, "ack");
		} else {
			snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->ack);
			cJSON_AddStringToObject(json_obj, "ack", convert_tmp);
		}

		cJSON_AddStringToObject(json_obj, "tail", msg->addr);
		if(IS_DOWNLINK_BLK(msg->bid)) {
			cJSON_AddStringToObject(json_obj, "flight", msg->fid);
			cJSON_AddStringToObject(json_obj, "msgno", msg->no);
		}
	}
	if(msg->txt[0])
		cJSON_AddStringToObject(json_obj, "text", msg->txt);

	if (msg->be == 0x17)
		cJSON_AddTrueToObject(json_obj, "end");

	if(DecodeLabel(msg, &oooi)) {
		if(oooi.sa[0])
			cJSON_AddStringToObject(json_obj, "depa", oooi.sa);
		if(oooi.da[0])
			cJSON_AddStringToObject(json_obj, "dsta", oooi.da);
		if(oooi.eta[0])
			cJSON_AddStringToObject(json_obj, "eta", oooi.eta);
		if(oooi.gout[0])
			cJSON_AddStringToObject(json_obj, "gtout", oooi.gout);
		if(oooi.gin[0])
			cJSON_AddStringToObject(json_obj, "gtin", oooi.gin);
		if(oooi.woff[0])
			cJSON_AddStringToObject(json_obj, "wloff", oooi.woff);
		if(oooi.won[0])
			cJSON_AddStringToObject(json_obj, "wlin", oooi.won);
	}

	if (msg->sublabel[0] != '\0') {
		cJSON_AddStringToObject(json_obj, "sublabel", msg->sublabel);
		if (msg->mfi[0] != '\0') {
			cJSON_AddStringToObject(json_obj, "mfi", msg->mfi);
		}
	}
#ifdef HAVE_LIBACARS
	if (!skip_reassembly) {
		cJSON_AddStringToObject(json_obj, "assstat", la_reasm_status_name_get(msg->reasm_status));
	}
	if(msg->decoded_tree != NULL) {
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


	ok = cJSON_PrintPreallocated(json_obj, jsonbuf, JSONBUFLEN, 0);
	cJSON_Delete(json_obj);
	return ok;
}


static void printoneline(acarsmsg_t * msg, int chn, struct timeval tv)
{
	char txt[60];
	char *pstr;

	strncpy(txt, msg->txt, 59);
	txt[59] = 0;
	for (pstr = txt; *pstr != 0; pstr++)
		if (*pstr == '\n' || *pstr == '\r')
			*pstr = ' ';

	fprintf(fdout, "#%1d (L:%+5.1f E:%1d) ", chn + 1, msg->lvl, msg->err);

	if (inmode != 2)
		printdate(tv);
	fprintf(fdout, " %7s %6s %1c %2s %4s ", msg->addr, msg->fid, msg->mode, msg->label, msg->no);
	fprintf(fdout, "%s", txt);
	fprintf(fdout, "\n");
	fflush(fdout);
}

typedef struct flight_s flight_t;
struct flight_s {
	flight_t *next;
	char addr[8];
	char fid[7];
	struct timeval ts,tl;
	int chm;
	int nbm;
	int rt;
	oooi_t oooi;
};
static flight_t  *flight_head=NULL;

static  flight_t *addFlight(acarsmsg_t * msg, int chn, struct timeval tv)
{
	flight_t *fl,*fld,*flp;
	oooi_t oooi;

	fl=flight_head;
	flp=NULL;
	while(fl) {
		if(strcmp(msg->addr,fl->addr)==0) break;
		flp=fl;
		fl=fl->next;
	}

	if(fl==NULL) {
		fl=calloc(1,sizeof(flight_t));
		if(fl==NULL) 
			return (NULL);
		strncpy(fl->addr,msg->addr,8);
		fl->nbm=0;
		fl->ts=tv;
		fl->chm=0;
		fl->rt=0;
		fl->next=NULL;
	}

	strncpy(fl->fid,msg->fid,7);
	fl->tl=tv;
	fl->chm|=(1<<chn);
	fl->nbm+=1;

	if(DecodeLabel(msg,&oooi)) {
		if(oooi.da[0]) memcpy(fl->oooi.da,oooi.da,5);
        	if(oooi.sa[0]) memcpy(fl->oooi.sa,oooi.sa,5);
        	if(oooi.eta[0]) memcpy(fl->oooi.eta,oooi.eta,5);
        	if(oooi.gout[0]) memcpy(fl->oooi.gout,oooi.gout,5);
        	if(oooi.gin[0]) memcpy(fl->oooi.gin,oooi.gin,5);
        	if(oooi.woff[0]) memcpy(fl->oooi.woff,oooi.woff,5);
        	if(oooi.won[0]) memcpy(fl->oooi.won,oooi.won,5);
	}

	if(flp) {
		flp->next=fl->next;
		fl->next=flight_head;
	}
	flight_head=fl;

	flp=NULL;fld=fl;
	while(fld) {
		if(fld->tl.tv_sec<(tv.tv_sec-mdly)) {
			if(flp) {
				flp->next=fld->next;
				free(fld);
				fld=flp->next;
			} else {
				flight_head=fld->next;
				free(fld);
				fld=flight_head;
			}
		} else {
			flp=fld;
			fld=fld->next;
		}
	}

	return(fl);
}

static int routejson(flight_t *fl,struct timeval tv)
{
  if(fl==NULL)
	return 0;

  if(fl->rt==0 && fl->fid[0] && fl->oooi.sa[0] && fl->oooi.da[0]) {

	cJSON *json_obj;
	int ok;

	json_obj = cJSON_CreateObject();
	if (json_obj == NULL)
		return 0;

	double t = (double)tv.tv_sec + ((double)tv.tv_usec)/1e6;
	cJSON_AddNumberToObject(json_obj, "timestamp", t);
	if(idstation[0]) cJSON_AddStringToObject(json_obj, "station_id", idstation);
	cJSON_AddStringToObject(json_obj, "flight", fl->fid);
	cJSON_AddStringToObject(json_obj, "depa", fl->oooi.sa);
	cJSON_AddStringToObject(json_obj, "dsta", fl->oooi.da);

	ok = cJSON_PrintPreallocated(json_obj, jsonbuf, JSONBUFLEN, 0);
	cJSON_Delete(json_obj);
	
	fl->rt=ok;
	return ok;
 } else
	return 0;
}

static void printmonitor(acarsmsg_t * msg, int chn, struct timeval tv)
{
	flight_t *fl;

	cls();

	printf("             Acarsdec monitor "); printtime(tv);
	printf("\n Aircraft Flight   Nb Channels     First    DEP   ARR   ETA\n");

	fl=flight_head;
	while(fl) {
		int i;

		printf(" %-8s %-7s %3d ", fl->addr, fl->fid,fl->nbm);
		for(i=0;i<nbch;i++) printf("%c",(fl->chm&(1<<i))?'x':'.');
		for(;i<MAXNBCHANNELS;i++) printf(" ");
		printf(" "); printtime(fl->ts);
        	if(fl->oooi.sa[0]) printf(" %4s ",fl->oooi.sa); else printf("      ");
		if(fl->oooi.da[0]) printf(" %4s ",fl->oooi.da); else printf("      ");
        	if(fl->oooi.eta[0]) printf(" %4s ",fl->oooi.eta); else printf("      ");
		printf("\n");

		fl=fl->next;
	}

	fflush(stdout);
}

void outputmsg(const msgblk_t * blk)
{
	acarsmsg_t msg;
	int i, j, k;
	int jok=0;
	int outflg=0;
	flight_t *fl;

	/* fill msg struct */
	memset(&msg, 0, sizeof(msg));
	msg.lvl = blk->lvl;
	msg.err = blk->err;

	k = 0;
	msg.mode = blk->txt[k];
	k++;

        for (i = 0, j = 0; i < 7; i++, k++) {
                if (blk->txt[k] != '.') {
                        msg.addr[j] = blk->txt[k];
                        j++;
                }
        }
        msg.addr[j] = '\0';

	/* ACK/NAK */
	msg.ack = blk->txt[k];
	if(msg.ack == 0x15)     // NAK is nonprintable
		msg.ack = '!';
	k++;

	msg.label[0] = blk->txt[k];
	k++;
	msg.label[1] = blk->txt[k];
	if(msg.label[1]==0x7f) msg.label[1]='d';
	k++;
	msg.label[2] = '\0';

	msg.bid = blk->txt[k];
	k++;

	bool down = IS_DOWNLINK_BLK(msg.bid);
#ifdef HAVE_LIBACARS
	la_msg_dir msg_dir = down ? LA_MSG_DIR_AIR2GND : LA_MSG_DIR_GND2AIR;
	msg.reasm_status = LA_REASM_SKIPPED;    // default value (valid for message with empty text)
#endif

	/* txt start  */
	msg.bs = blk->txt[k];
	k++;

	if (airflt && !down)
		return;
	if(label_filter(msg.label)==0)
		return;

	/* txt end */
	msg.be = blk->txt[blk->len - 1];

	if (msg.bs != 0x03) {
		if (down) {
			/* message no */
			for (i = 0; i < 4 && k < blk->len - 1; i++, k++) {
				msg.no[i] = blk->txt[k];
			}
			msg.no[i] = '\0';
#ifdef HAVE_LIBACARS
			/* The 3-char prefix is used in reassembly hash table key, so we need */
			/* to store the MSN separately as prefix and seq character. */
			for (i = 0; i < 3; i++)
				msg.msn[i] = msg.no[i];
			msg.msn[3] = '\0';
			msg.msn_seq = msg.no[3];
#endif
			/* Flight id */
			for (i = 0; i < 6 && k < blk->len - 1; i++, k++) {
				msg.fid[i] = blk->txt[k];
			}
			msg.fid[i] = '\0';

			outflg=1;
		}
		int txt_len = blk->len - k - 1;
#ifdef HAVE_LIBACARS

		// Extract sublabel and MFI if present
		int offset = la_acars_extract_sublabel_and_mfi(msg.label, msg_dir,
				blk->txt + k, txt_len, msg.sublabel, msg.mfi);
		if(offset > 0) {
			k += offset;
			txt_len -= offset;
		}

		la_reasm_table *acars_rtable = NULL;
		if(msg.bid != 0 && reasm_ctx != NULL) { // not a squitter && reassembly engine is enabled
			acars_rtable = la_reasm_table_lookup(reasm_ctx, &la_DEF_acars_message);
			if(acars_rtable == NULL) {
				acars_rtable = la_reasm_table_new(reasm_ctx, &la_DEF_acars_message,
						acars_reasm_funcs, LA_ACARS_REASM_TABLE_CLEANUP_INTERVAL);
			}

			// The sequence number at which block id wraps at.
			// - downlinks: none (MSN always goes from 'A' up to 'P')
			// - uplinks:
			//   - for VHF Category A (mode=2): wraps after block id 'Z'
			//   - for VHF Category B (mode!=2): wraps after block id 'W'
			//     (blocks 'X'-'Z' are reserved for empty ACKs)

			int seq_num_wrap = SEQ_WRAP_NONE;
			if(!down)
				seq_num_wrap = msg.mode == '2' ? 'Z' + 1 - 'A' : 'X' - 'A';

			msg.reasm_status = la_reasm_fragment_add(acars_rtable,
					&(la_reasm_fragment_info){
						.msg_info = &msg,
						.msg_data = (uint8_t *)(blk->txt + k),
						.msg_data_len = txt_len,
						.total_pdu_len = 0,        // not used
						.seq_num = down ? msg.msn_seq - 'A' : msg.bid - 'A',
						.seq_num_first = down ? 0 : SEQ_FIRST_NONE,
						.seq_num_wrap = seq_num_wrap,
						.is_final_fragment = msg.be != 0x17,    // ETB means "more fragments"
						.rx_time = blk->tv,
						.reasm_timeout = down ? acars_reasm_timeout_downlink : acars_reasm_timeout_uplink
					});
		}
		uint8_t *reassembled_msg = NULL;
		if(msg.reasm_status == LA_REASM_COMPLETE &&
				la_reasm_payload_get(acars_rtable, &msg, &reassembled_msg) > 0) {
			// reassembled_msg is a newly allocated byte buffer, which is guaranteed to
			// be NULL-terminated, so we can cast it to char * directly.
			msg.txt = (char *)reassembled_msg;
		} else {
#endif // HAVE_LIBACARS
			msg.txt = calloc(txt_len + 1, sizeof(char));
			if(msg.txt && txt_len > 0) {
				memcpy(msg.txt, blk->txt + k, txt_len);
			}
#ifdef HAVE_LIBACARS
		}
#endif
	} else { // empty message text
		msg.txt = calloc(1, sizeof(char));
	}

#ifdef HAVE_LIBACARS
	if(msg.txt != NULL && msg.txt[0] != '\0') {
		bool decode_apps = true;
		// Inhibit higher layer application decoding if reassembly is enabled and
		// is now in progress (ie. the message is not yet complete)
		if(reasm_ctx != NULL && (msg.reasm_status == LA_REASM_IN_PROGRESS ||
				msg.reasm_status == LA_REASM_DUPLICATE)) {
			decode_apps = false;
		}
		if(decode_apps) {
			msg.decoded_tree = la_acars_apps_parse_and_reassemble(msg.addr, msg.label,
					msg.txt, msg_dir, reasm_ctx, blk->tv);
		}
	}
#endif

	if(outflg)
		fl=addFlight(&msg,blk->chn,blk->tv);

	if(emptymsg && ( msg.txt == NULL || msg.txt[0] == '\0'))
			return;

	if(jsonbuf) {
		if(outtype == OUTTYPE_ROUTEJSON ) {
			if(fl)
			       	jok=routejson(fl,blk->tv);
		} else {
			jok=buildjson(&msg, blk->chn, blk->tv);
		}
	}

	if((hourly || daily) && outtype != OUTTYPE_NONE && (fdout=Fileoutrotate(fdout))==NULL) {
		_exit(1);
	}
	switch (outtype) {
	case OUTTYPE_NONE:
		break;
	case OUTTYPE_ONELINE:
		printoneline(&msg, blk->chn, blk->tv);
		break;
	case OUTTYPE_STD:
		printmsg(&msg, blk->chn, blk->tv);
		break;
	case OUTTYPE_MONITOR:
		printmonitor(&msg, blk->chn, blk->tv);
		break;
	case OUTTYPE_ROUTEJSON:
	case OUTTYPE_JSON:
		if(jok) {
			fprintf(fdout, "%s\n", jsonbuf);
			fflush(fdout);
		}
		break;
	}

	switch (netout) {
		case NETLOG_PLANEPLOTTER:
			Netoutpp(&msg);
			break;
		case NETLOG_NATIVE:
			Netoutsv(&msg, idstation, blk->chn, blk->tv);
			break;
		case NETLOG_JSON:
			if(jok) Netoutjson(jsonbuf);
			break;
#ifdef WITH_MQTT
		case NETLOG_MQTT:
			MQTTsend(jsonbuf);
			break;
#endif
	}
	free(msg.txt);
#ifdef HAVE_LIBACARS
	la_proto_tree_destroy(msg.decoded_tree);
#endif
}

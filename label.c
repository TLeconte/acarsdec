#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "label.h"

static char *lblfilter[1024];

void build_label_filter(char *arg)
{
	int i = 0;
	char *aptr;

	lblfilter[0] = NULL;
	if (arg == NULL)
		return;

	aptr = strtok(strdup(arg), ":");
	while (aptr) {
		lblfilter[i] = aptr;
		i++;
		aptr = strtok(NULL, ":");
	}
	lblfilter[i] = NULL;
}

int label_filter(char *lbl)
{
	int i;

	if (lblfilter[0] == NULL)
		return 1;

	i = 0;
	while (lblfilter[i]) {
		if (strcmp(lbl, lblfilter[i]) == 0)
			return 1;
		i++;
	}

	return 0;
}

static int label_q1(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->gout, &(txt[4]), 4);
	memcpy(oooi->woff, &(txt[8]), 4);
	memcpy(oooi->won, &(txt[12]), 4);
	memcpy(oooi->gin, &(txt[16]), 4);
	memcpy(oooi->da, &(txt[24]), 4);
	return 1;
}
static int label_q2(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->eta, &(txt[4]), 4);
	return 1;
}
static int label_qa(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->gout, &(txt[4]), 4);
	return 1;
}
static int label_qb(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->woff, &(txt[4]), 4);
	return 1;
}
static int label_qc(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->won, &(txt[4]), 4);
	return 1;
}
static int label_qd(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->gin, &(txt[4]), 4);
	return 1;
}
static int label_qe(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->gout, &(txt[4]), 4);
	memcpy(oooi->da, &(txt[8]), 4);
	return 1;
}
static int label_qf(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->woff, &(txt[4]), 4);
	memcpy(oooi->da, &(txt[8]), 4);
	return 1;
}
static int label_qg(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->gout, &(txt[4]), 4);
	memcpy(oooi->gin, &(txt[8]), 4);
	return 1;
}
static int label_qh(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->gout, &(txt[4]), 4);
	return 1;
}
static int label_qk(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->won, &(txt[4]), 4);
	memcpy(oooi->da, &(txt[8]), 4);
	return 1;
}
static int label_ql(char *txt, oooi_t *oooi)
{
	memcpy(oooi->da, txt, 4);
	memcpy(oooi->gin, &(txt[8]), 4);
	memcpy(oooi->sa, &(txt[13]), 4);
	return 1;
}
static int label_qm(char *txt, oooi_t *oooi)
{
	memcpy(oooi->da, txt, 4);
	memcpy(oooi->sa, &(txt[8]), 4);
	return 1;
}
static int label_qn(char *txt, oooi_t *oooi)
{
	memcpy(oooi->da, &(txt[4]), 4);
	memcpy(oooi->eta, &(txt[8]), 4);
	return 1;
}
static int label_qp(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->da, &(txt[4]), 4);
	memcpy(oooi->gout, &(txt[8]), 4);
	return 1;
}
static int label_qq(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->da, &(txt[4]), 4);
	memcpy(oooi->woff, &(txt[8]), 4);
	return 1;
}
static int label_qr(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->da, &(txt[4]), 4);
	memcpy(oooi->won, &(txt[8]), 4);
	return 1;
}
static int label_qs(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->da, &(txt[4]), 4);
	memcpy(oooi->gin, &(txt[8]), 4);
	return 1;
}
static int label_qt(char *txt, oooi_t *oooi)
{
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->da, &(txt[4]), 4);
	memcpy(oooi->gout, &(txt[8]), 4);
	memcpy(oooi->gin, &(txt[12]), 4);
	return 1;
}

static int label_20(char *txt, oooi_t *oooi)
{
	if (memcmp(txt, "RST", 3))
		return 0;
	memcpy(oooi->sa, &(txt[22]), 4);
	memcpy(oooi->da, &(txt[26]), 4);
	return 1;
}
static int label_21(char *txt, oooi_t *oooi)
{
	if (txt[6] != ',')
		return 0;
	memcpy(oooi->sa, &(txt[7]), 4);
	if (txt[11] != ',')
		return 0;
	memcpy(oooi->da, &(txt[12]), 4);
	return 1;
}
static int label_26(char *txt, oooi_t *oooi)
{
	char *p;
	if (memcmp(txt, "VER/077", 7))
		return 0;
	p = strchr(txt, '\n');
	if (p == NULL)
		return 0;
	p++;
	if (memcmp(p, "SCH/", 4))
		return 0;
	p = strchr(p + 4, '/');
	if (p == NULL)
		return 0;
	memcpy(oooi->sa, p + 1, 4);
	memcpy(oooi->da, p + 6, 4);
	p = strchr(p, '\n');
	if (p == NULL)
		return 1;
	p++;
	if (memcmp(p, "ETA/", 4))
		return 0;
	memcpy(oooi->eta, p + 4, 4);
	return 1;
}
static int label_2N(char *txt, oooi_t *oooi)
{
	if (memcmp(txt, "TKO01", 5))
		return 0;
	if (txt[11] != '/')
		return 0;
	memcpy(oooi->sa, &(txt[20]), 4);
	memcpy(oooi->da, &(txt[24]), 4);
	return 1;
}

static int label_2Z(char *txt, oooi_t *oooi)
{
	memcpy(oooi->da, txt, 4);
	return 1;
}
static int label_33(char *txt, oooi_t *oooi)
{
	if (txt[0] != ',')
		return 0;
	if (txt[20] != ',')
		return 0;
	memcpy(oooi->sa, &(txt[21]), 4);
	if (txt[25] != ',')
		return 0;
	memcpy(oooi->da, &(txt[26]), 4);
	return 1;
}
static int label_39(char *txt, oooi_t *oooi)
{
	if (memcmp(txt, "GTA01", 5))
		return 0;
	if (txt[15] != '/')
		return 0;
	memcpy(oooi->sa, &(txt[24]), 4);
	memcpy(oooi->da, &(txt[28]), 4);
	return 1;
}
static int label_44(char *txt, oooi_t *oooi)
{
	if (txt[0] == '0') {
		if (txt[1] != '0')
			return 0;
		txt += 2;
	}
	if (memcmp(txt, "POS0", 4) && memcmp(txt, "ETA0", 4))
		return 0;
	if (txt[4] != '2' && txt[4] != '3')
		return 0;
	if (txt[23] != ',')
		return 0;
	memcpy(oooi->da, &(txt[24]), 4);
	if (txt[28] != ',')
		return 0;
	memcpy(oooi->eta, &(txt[29]), 4);
	if (txt[33] != ',')
		return 0;
	if (txt[38] != ',')
		return 0;
	if (txt[43] != ',')
		return 0;
	memcpy(oooi->eta, &(txt[44]), 4);
	return 1;
}
static int label_45(char *txt, oooi_t *oooi)
{
	if (txt[0] != 'A')
		return 0;
	memcpy(oooi->da, &(txt[1]), 4);
	return 1;
}
static int label_10(char *txt, oooi_t *oooi)
{
	if (memcmp(txt, "ARR01", 5))
		return 0;
	memcpy(oooi->da, &(txt[12]), 4);
	memcpy(oooi->eta, &(txt[16]), 4);
	return 1;
}
static int label_11(char *txt, oooi_t *oooi)
{
	if (memcmp(&(txt[13]), "/DS ", 4))
		return 0;
	memcpy(oooi->da, &(txt[17]), 4);
	if (memcmp(&(txt[21]), "/ETA ", 5))
		return 0;
	memcpy(oooi->eta, &(txt[26]), 4);
	return 1;
}
static int label_12(char *txt, oooi_t *oooi)
{
	if (txt[4] != ',')
		return 0;
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->da, &(txt[5]), 4);
	return 1;
}

static int label_15(char *txt, oooi_t *oooi)
{
	if (memcmp(txt, "FST01", 5))
		return 0;
	memcpy(oooi->sa, &(txt[5]), 4);
	memcpy(oooi->da, &(txt[9]), 4);
	return 1;
}
static int label_17(char *txt, oooi_t *oooi)
{
	if (memcmp(txt, "ETA ", 4))
		return 0;
	memcpy(oooi->eta, &(txt[4]), 4);
	if (txt[8] != ',')
		return 0;
	memcpy(oooi->sa, &(txt[9]), 4);
	if (txt[13] != ',')
		return 0;
	memcpy(oooi->da, &(txt[14]), 4);
	return 1;
}
static int label_1G(char *txt, oooi_t *oooi)
{
	if (txt[4] != ',')
		return 0;
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->da, &(txt[5]), 4);
	return 1;
}
static int label_80(char *txt, oooi_t *oooi)
{
	if (memcmp(&(txt[6]), "/DEST/", 5))
		return 0;
	memcpy(oooi->da, &(txt[12]), 4);
	return 1;
}
static int label_83(char *txt, oooi_t *oooi)
{
	if (txt[4] != ',')
		return 0;
	memcpy(oooi->sa, txt, 4);
	memcpy(oooi->da, &(txt[5]), 4);
	return 1;
}
static int label_8D(char *txt, oooi_t *oooi)
{
	if (txt[4] != ',')
		return 0;
	if (txt[35] != ',')
		return 0;
	memcpy(oooi->sa, &(txt[36]), 4);
	if (txt[40] != ',')
		return 0;
	memcpy(oooi->da, &(txt[41]), 4);
	return 1;
}
static int label_8e(char *txt, oooi_t *oooi)
{
	if (txt[4] != ',')
		return 0;
	memcpy(oooi->da, txt, 4);
	memcpy(oooi->eta, &(txt[5]), 4);
	return 1;
}
static int label_8s(char *txt, oooi_t *oooi)
{
	if (txt[4] != ',')
		return 0;
	memcpy(oooi->da, txt, 4);
	memcpy(oooi->eta, &(txt[5]), 4);
	return 1;
}

int DecodeLabel(acarsmsg_t *msg, oooi_t *oooi)
{
	switch (msg->label[0]) {
	case '1':
		switch (msg->label[1]) {
		case '0':
			return label_10(msg->txt, oooi);
		case '1':
			return label_11(msg->txt, oooi);
		case '2':
			return label_12(msg->txt, oooi);
		case '5':
			return label_15(msg->txt, oooi);
		case '7':
			return label_17(msg->txt, oooi);
		case 'G':
			return label_1G(msg->txt, oooi);
		default:
			break;
		}
		break;
	case '2':
		switch (msg->label[1]) {
		case '0':
			return label_20(msg->txt, oooi);
		case '1':
			return label_21(msg->txt, oooi);
		case '6':
			return label_26(msg->txt, oooi);
		case 'N':
			return label_2N(msg->txt, oooi);
		case 'Z':
			return label_2Z(msg->txt, oooi);
		default:
			break;
		}
		break;
	case '3':
		switch (msg->label[1]) {
		case '3':
			return label_33(msg->txt, oooi);
		case '9':
			return label_39(msg->txt, oooi);
		default:
			break;
		}
		break;
	case '4':
		switch (msg->label[1]) {
		case '4':
			return label_44(msg->txt, oooi);
		case '5':
			return label_45(msg->txt, oooi);
		default:
			break;
		}
		break;
	case '8':
		switch (msg->label[1]) {
		case '0':
			return label_80(msg->txt, oooi);
		case '3':
			return label_83(msg->txt, oooi);
		case 'D':
			return label_8D(msg->txt, oooi);
		case 'E':
			return label_8e(msg->txt, oooi);
		case 'S':
			return label_8s(msg->txt, oooi);
		default:
			break;
		}
		break;
	case 'R':
		if (msg->label[1] == 'B')
			return label_26(msg->txt, oooi);
		break;
	case 'Q':
		switch (msg->label[1]) {
		case '1':
			return label_q1(msg->txt, oooi);
		case '2':
			return label_q2(msg->txt, oooi);
		case 'A':
			return label_qa(msg->txt, oooi);
		case 'B':
			return label_qb(msg->txt, oooi);
		case 'C':
			return label_qc(msg->txt, oooi);
		case 'D':
			return label_qd(msg->txt, oooi);
		case 'E':
			return label_qe(msg->txt, oooi);
		case 'F':
			return label_qf(msg->txt, oooi);
		case 'G':
			return label_qg(msg->txt, oooi);
		case 'H':
			return label_qh(msg->txt, oooi);
		case 'K':
			return label_qk(msg->txt, oooi);
		case 'L':
			return label_ql(msg->txt, oooi);
		case 'M':
			return label_qm(msg->txt, oooi);
		case 'N':
			return label_qn(msg->txt, oooi);
		case 'P':
			return label_qp(msg->txt, oooi);
		case 'Q':
			return label_qq(msg->txt, oooi);
		case 'R':
			return label_qr(msg->txt, oooi);
		case 'S':
			return label_qs(msg->txt, oooi);
		case 'T':
			return label_qt(msg->txt, oooi);
		default:
			break;
		}
		break;
	}

	return 0;
}

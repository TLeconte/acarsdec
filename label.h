#ifndef label_h
#define label_h

#include "acarsdec.h"

typedef struct {
	char da[5];
	char sa[5];
	char eta[5];
	char gout[5];
	char gin[5];
	char woff[5];
	char won[5];
} oooi_t;

void build_label_filter(char *arg);
int label_filter(char *lbl);
int DecodeLabel(acarsmsg_t *msg, oooi_t *oooi);

#endif /* label_h */

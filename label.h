#ifndef label_h
#define label_h

#ifdef HAVE_LIBACARS
#include <libacars/libacars.h>
#include <libacars/reassembly.h>
#endif

typedef struct {
	char mode;
	char addr[8];
	char ack;
	char label[3];
	char bid;
	char no[5];
	char fid[7];
	char sublabel[3];
	char mfi[3];
	char bs, be;
	char *txt;
	int err;
	float lvl;
#ifdef HAVE_LIBACARS
	char msn[4];
	char msn_seq;
	la_proto_node *decoded_tree;
	la_reasm_status reasm_status;
#endif
} acarsmsg_t;

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

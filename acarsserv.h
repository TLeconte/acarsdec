typedef struct {
	time_t tm;
        int chn;
        char idst[9];
        int err;
        int lvl;
        char mode;
        char reg[8];
        char ack;
        char label[3];
        char bid;
        char no[5];
        char fid[7];
        char txt[250];
} acarsmsg_t;

extern int initdb(char *);
extern int updatedb(acarsmsg_t* msg, int lm, char * ipaddr);
extern int posconv(const char*, const unsigned char *, double *, double *,  int *, int *);



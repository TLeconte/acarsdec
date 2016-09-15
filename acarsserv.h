typedef struct {
	time_t tm;
        unsigned int chn;
        unsigned char idst[9];
        unsigned int err;
        unsigned int lvl;
        unsigned char mode;
        unsigned char reg[8];
        unsigned char ack;
        unsigned char label[3];
        unsigned char bid;
        unsigned char no[5];
        unsigned char fid[7];
        unsigned char txt[250];
} acarsmsg_t;

extern int initdb(char *);
extern int updatedb(acarsmsg_t* msg, int lm, char * ipaddr);
extern int posconv(const char*, const unsigned char *, double *, double *,  int *, int *);



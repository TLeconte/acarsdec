extern int Netoutinit(char *Rawaddr);
extern void Netoutpp(acarsmsg_t * msg);
extern void Netoutsv(acarsmsg_t * msg, char * idstation, int chn, struct timeval tv);
extern void Netoutjson(char *jsonbuf);

extern FILE *Fileoutinit(char* logfilename);
extern FILE *Fileoutrotate(FILE *fd);


#ifndef fileout_h
#define fileout_h

#include <time.h>

typedef struct {
	FILE *F;
	const char *filename_prefix;
	const char *extension;
	size_t prefix_len;
	struct tm current_tm;
	enum { ROTATE_NONE, ROTATE_HOURLY, ROTATE_DAILY } rotate;
} fileout_t;

fileout_t *Fileoutinit(char *params);
void Filewrite(const char *buf, size_t buflen, fileout_t *fout);
void Fileoutexit(fileout_t *fout);

#endif /* fileout_h */

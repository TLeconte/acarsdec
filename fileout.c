#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

extern int hourly,daily;

static char *filename_prefix = NULL;
static char *extension = NULL;
static size_t prefix_len;
static struct tm current_tm;

static FILE *open_outfile() {
	char *filename = NULL;
	char *fmt = NULL;
	size_t tlen = 0;
	FILE *fd;

	if(hourly || daily) {
		time_t t = time(NULL);
		gmtime_r(&t, &current_tm);
		char suffix[16];
		if(hourly) {
			fmt = "_%Y%m%d_%H";
		} else {	// daily
			fmt = "_%Y%m%d";
		}
		tlen = strftime(suffix, sizeof(suffix), fmt, &current_tm);
		if(tlen == 0) {
			fprintf(stderr, "*open_outfile(): strfime returned 0\n");
			return NULL;
		}
		filename = calloc(prefix_len + tlen + 2, sizeof(char));
		if(filename == NULL) {
			fprintf(stderr, "open_outfile(): failed to allocate memory\n");
			return NULL;
		}
		sprintf(filename, "%s%s%s", filename_prefix, suffix, extension);
	} else {
		filename = strdup(filename_prefix);
	}

	if((fd = fopen(filename, "a+")) == NULL) {
		fprintf(stderr, "Could not open output file %s: %s\n", filename, strerror(errno));
		free(filename);
		return NULL;
	}
	free(filename);
	return fd;
}

FILE* Fileoutinit(char* logfilename)
{
	FILE *fd;

        filename_prefix = logfilename;
        prefix_len = strlen(filename_prefix);
        if(hourly || daily) {
              char *basename = strrchr(filename_prefix, '/');
              if(basename != NULL) {
                       basename++;
              } else {
                       basename = filename_prefix;
              }
              char *ext = strrchr(filename_prefix, '.');
              if(ext != NULL && (ext <= basename || ext[1] == '\0')) {
                     ext = NULL;
              }
              if(ext) {
                     extension = strdup(ext);
                     *ext = '\0';
              } else {
                      extension = strdup("");
              }
        }
        if((fd=open_outfile()) == NULL)
                return NULL;

	return fd;
}

FILE* Fileoutrotate(FILE *fd)
{
	struct tm new_tm;
	time_t t = time(NULL);
	gmtime_r(&t, &new_tm);
	if((hourly && new_tm.tm_hour != current_tm.tm_hour) ||
	   (daily && new_tm.tm_mday != current_tm.tm_mday)) {
		fclose(fd);
		return open_outfile();
	}
	return fd;
}


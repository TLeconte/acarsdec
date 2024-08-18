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
#include <err.h>
#include <libgen.h>

#include "acarsdec.h"
#include "fileout.h"

#define ERRPFX	"ERROR: FILE: "

static FILE *open_outfile(fileout_t *fout)
{
	char *filename = NULL;
	char *fmt = NULL;
	size_t tlen = 0;

	if (ROTATE_NONE != fout->rotate) {
		time_t t = time(NULL);
		gmtime_r(&t, &fout->current_tm);
		char suffix[16];

		if (ROTATE_HOURLY == fout->rotate)
			fmt = "_%Y%m%d_%H";
		else // daily
			fmt = "_%Y%m%d";

		tlen = strftime(suffix, sizeof(suffix), fmt, &fout->current_tm);
		if (tlen == 0) {
			vprerr(ERRPFX "open_outfile(): strfime returned 0\n");
			return NULL;
		}
		filename = malloc(fout->prefix_len + tlen + 2);
		if (filename == NULL) {
			perror(ERRPFX "open_outfile()");
			return NULL;
		}
		sprintf(filename, "%s%s%s", fout->filename_prefix, suffix, fout->extension ? fout->extension : "");
	} else {
		filename = strdup(fout->filename_prefix);
	}

	if ((fout->F = fopen(filename, "a+")) == NULL)
		fprintf(stderr, ERRPFX "could not open output file '%s': %s\n", filename, strerror(errno));

	free(filename);
	return fout->F;
}

// params: NULL (defaults to stdout) or "path=" followed by "-" for stdtout or full path to file
// optional "rotate=" parameter followed by "none" (default), "hourly", "daily"
fileout_t *Fileoutinit(char *params)
{
	char *param, *sep, *path = NULL, *rotate = NULL;
	fileout_t *fout;

	while ((param = strsep(&params, ","))) {
		sep = strchr(param, '=');
		if (!sep)
			continue;
		*sep++ = '\0';
		if (!strcmp("path", param))
			path = sep;
		if (!strcmp("rotate", param))
			rotate = sep;
	}

	fout = calloc(1, sizeof(*fout));
	if (!fout) {
		perror(NULL);
		return NULL;
	}

	// params is path or optional "-" for stdout
	if (!path || !strcmp("-", path)) {
		fout->F = stdout;
		return fout;
	}

	fout->rotate = ROTATE_NONE;
	if (rotate) {
		if (!strcmp("daily", rotate))
			fout->rotate = ROTATE_DAILY;
		else if (!strcmp("hourly", rotate))
			fout->rotate = ROTATE_HOURLY;
	}

	if (ROTATE_NONE != fout->rotate) {
		char *bname = basename(path);
		char *ext = strrchr(bname, '.');
		if (ext) {
			fout->extension = strdup(ext);
			path[strlen(path)-strlen(ext)] = '\0';
		}
	}
	fout->filename_prefix = path;
	fout->prefix_len = strlen(path);

	if ((open_outfile(fout)) == NULL) {
		free(fout);
		return NULL;
	}

	return fout;
}

static FILE *Fileoutrotate(fileout_t *fout)
{
	struct tm new_tm;
	time_t t = time(NULL);

	gmtime_r(&t, &new_tm);
	if ((ROTATE_HOURLY == fout->rotate && new_tm.tm_hour != fout->current_tm.tm_hour) ||
	    (ROTATE_DAILY == fout->rotate && new_tm.tm_mday != fout->current_tm.tm_mday)) {
		fclose(fout->F);
		return open_outfile(fout);
	}
	return fout->F;
}

void Filewrite(const char *buf, size_t buflen, fileout_t *fout)
{
	if ((ROTATE_NONE != fout->rotate) && !Fileoutrotate(fout))
		errx(1, ERRPFX "failed to rotate output file '%s'", fout->filename_prefix);

	fwrite(buf, buflen, 1, fout->F);
	fprintf(fout->F, "\n");
	fflush(fout->F);

}

void Fileoutexit(fileout_t *fout)
{
	if (stdout != fout->F)
		fclose(fout->F);

	free((void *)(uintptr_t)fout->extension);
	free(fout);
}

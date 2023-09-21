/*
 *  Copyright (c) 2023 Jakob Ketterl
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

#include "acarsdec.h"
#include <unistd.h>
#include <malloc.h>

#define MAX_SAMPLES 1024

int initStdIn(char **argv,int optind) {
    nbch = 1;
    channel[0].dm_buffer=malloc(sizeof(sample_t)*MAX_SAMPLES);
    return 0;
}

int runStdInSample(void) {
    ssize_t nbi;

    do {
        nbi = read(STDIN_FILENO, channel[0].dm_buffer, sizeof(sample_t) * MAX_SAMPLES);

        if (nbi <= 0) {
            return -1;
        }

        demodMSK(&(channel[0]), nbi / sizeof(sample_t));

    } while (1);

    return 0;
}

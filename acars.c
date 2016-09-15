/*
 *  Copyright (c) 2015 Thierry Leconte
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "acarsdec.h"

#define SYN 0x16
#define SOH 0x01
#define STX 0x02
#define ETX 0x83
#define ETB 0x97
#define DLE 0x7f

#include "syndrom.h"

static pthread_mutex_t blkmtx;
static pthread_cond_t blkwcd;
static msgblk_t *blkq_s = NULL;
static msgblk_t *blkq_e = NULL;
static unsigned char prev_crc[2] = { 0, 0 };

static time_t prev_t = 0;

static int fixerr(msgblk_t * blk, const unsigned short crc, int *pr, int pn)
{
	int i;

	if (pn > 0) {
		for (i = 0; i < 8; i++) {
			if (fixerr
			    (blk, crc ^ syndrom[i + 8 * (blk->len - *pr + 1)],
			     pr + 1, pn - 1)) {
				blk->txt[*pr] ^= (1 << i);
				return 1;
			}
		}
		return 0;
	} else {
		if (crc == 0)
			return 1;
		for (i = 0; i < 2 * 8; i++)
			if (syndrom[i] == crc) {
				return 1;
			}
		return 0;
	}
}

#define MAXPERR 2
static void *blk_thread(void *arg)
{
	do {
		msgblk_t *blk;
		int i, pn;
		unsigned short crc;
		int pr[MAXPERR];

		pthread_mutex_lock(&blkmtx);
		while (blkq_e == NULL)
			pthread_cond_wait(&blkwcd, &blkmtx);

		blk = blkq_e;
		blkq_e = blk->prev;
		if (blkq_e == NULL)
			blkq_s = NULL;
		pthread_mutex_unlock(&blkmtx);

		if (blk->len < 13) {
			if (verbose)
				fprintf(stderr, "#%d too short\n",
					blk->chn + 1);
			free(blk);
			continue;
		}

		/* force STX/ETX */
		blk->txt[12] &= (ETX | STX);
		blk->txt[12] |= (ETX & STX);

		/* parity check */
		pn = 0;
		for (i = 0; i < blk->len; i++) {
			if ((numbits[blk->txt[i]] & 1) == 0) {
				if (pn < MAXPERR) {
					pr[pn] = i;
				}
				pn++;
			}
		}
		if (pn > MAXPERR) {
			if (verbose)
				fprintf(stderr,
					"#%d too many parity errors: %d\n",
					blk->chn + 1, pn);
			free(blk);
			continue;
		}
		if (pn > 0 && verbose)
			fprintf(stderr, "#%d parity error(s): %d\n",
				blk->chn + 1, pn);
		blk->err = pn;

		/* crc check */
		crc = 0;
		for (i = 0; i < blk->len; i++) {
			update_crc(crc, blk->txt[i]);

		}
		update_crc(crc, blk->crc[0]);
		update_crc(crc, blk->crc[1]);
		if (crc && verbose)
			fprintf(stderr, "#%d crc error\n", blk->chn + 1);

		/* try to fix error */
		if (fixerr(blk, crc, pr, pn) == 0) {
			if (verbose)
				fprintf(stderr, "#%d not able to fix errors\n",
					blk->chn + 1);
			free(blk);
			continue;
		}

		/* redo parity checking and removing */
		pn = 0;
		for (i = 0; i < blk->len; i++) {
			if ((numbits[blk->txt[i]] & 1) == 0) {
				pn++;
			}
			blk->txt[i] &= 0x7f;
		}
		if (pn) {
			fprintf(stderr, "#%d parity check problem\n",
				blk->chn + 1);
			free(blk);
			continue;
		}
		if (prev_t == blk->tm &&
		    prev_crc[0] == blk->crc[0] && prev_crc[1] == blk->crc[1]) {
			if (verbose)
				fprintf(stderr, "#%d duplicate %d\n",
					blk->chn + 1, blk->lvl);
			free(blk);
			continue;
		}

		prev_t = blk->tm;
		prev_crc[0] = blk->crc[0];
		prev_crc[1] = blk->crc[1];

		outputmsg(blk);

		free(blk);

	} while (1);
	return NULL;
}

int initAcars(channel_t * ch)
{
	pthread_t th;

	ch->outbits = 0;
	ch->nbits = 8;
	ch->Acarsstate = WSYN;

	ch->blk = malloc(sizeof(msgblk_t));
	ch->blk->chn = ch->chn;

	pthread_mutex_init(&blkmtx, NULL);
	pthread_cond_init(&blkwcd, NULL);

	pthread_create(&th, NULL, blk_thread, NULL);

	return 0;
}

void decodeAcars(channel_t * ch)
{
	unsigned char r = ch->outbits;

	switch (ch->Acarsstate) {

	case WSYN:
		if (r == SYN) {
			ch->Acarsstate = SYN2;
			ch->nbits = 8;
			return;
		}
		if (r == (unsigned char)~SYN) {
			ch->MskS ^= 2;
			ch->Acarsstate = SYN2;
			ch->nbits = 8;
			return;
		}
		ch->nbits = 1;
		return;

	case SYN2:
		if (r == SYN) {
			ch->Acarsstate = SOH1;
			ch->nbits = 8;
			return;
		}
		if (r == (unsigned char)~SYN) {
			ch->MskS ^= 2;
			ch->nbits = 8;
			return;
		}
		ch->Acarsstate = WSYN;
		ch->nbits = 1;
		return;

	case SOH1:
		if (r == SOH) {
			time(&(ch->blk->tm));
			ch->Acarsstate = TXT;
			ch->blk->len = 0;
			ch->blk->err = 0;
			ch->nbits = 8;
			return;
		}
		ch->Acarsstate = WSYN;
		ch->nbits = 1;
		return;

	case TXT:
		ch->blk->txt[ch->blk->len] = r;
		ch->blk->len++;
		if ((numbits[r] & 1) == 0) {
			ch->blk->err++;

			if (ch->blk->err > MAXPERR + 1) {
				if (verbose)
					fprintf(stderr,
						"#%d too many parity errors\n",
						ch->chn + 1);
				ch->Acarsstate = WSYN;
				ch->nbits = 1;
				return;
			}
		}
		if (r == ETX || r == ETB) {
			ch->Acarsstate = CRC1;
			ch->nbits = 8;
			return;
		}
		if (ch->blk->len > 20 && r == DLE) {
			if (verbose)
				fprintf(stderr, "#%d miss txt end\n",
					ch->chn + 1);
			ch->blk->len -= 3;
			ch->blk->crc[0] = ch->blk->txt[ch->blk->len];
			ch->blk->crc[1] = ch->blk->txt[ch->blk->len + 1];
			ch->Acarsstate = CRC2;
			goto putmsg_lbl;
		}
		if (ch->blk->len > 240) {
			if (verbose)
				fprintf(stderr, "#%d too long\n", ch->chn + 1);
			ch->Acarsstate = WSYN;
			ch->nbits = 1;
			return;
		}
		ch->nbits = 8;
		return;

	case CRC1:
		ch->blk->crc[0] = r;
		ch->Acarsstate = CRC2;
		ch->nbits = 8;
		return;
	case CRC2:
		ch->blk->crc[1] = r;
 putmsg_lbl:
		ch->blk->lvl = 20*log10(ch->Mskdc)-48;

		pthread_mutex_lock(&blkmtx);
		ch->blk->prev = NULL;
		if (blkq_s)
			blkq_s->prev = ch->blk;
		blkq_s = ch->blk;
		if (blkq_e == NULL)
			blkq_e = blkq_s;
		pthread_cond_signal(&blkwcd);
		pthread_mutex_unlock(&blkmtx);

		ch->blk = malloc(sizeof(msgblk_t));
		ch->blk->chn = ch->chn;

		ch->Acarsstate = END;
		ch->nbits = 8;
		return;
	case END:
		ch->Acarsstate = WSYN;
		ch->MskDf = 0;
		ch->nbits = 8;
		return;
	}
}

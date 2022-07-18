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

/* message queue */
static pthread_mutex_t blkq_mtx;
static pthread_cond_t blkq_wcd;
static msgblk_t *blkq_s,*blkq_e;
static pthread_t blkth_id;

static int acars_shutdown;

#include "syndrom.h"

static int fixprerr(msgblk_t * blk, const unsigned short crc, int *pr, int pn)
{
	int i;

	if (pn > 0) {
		/* try to recursievly fix parity error */
		for (i = 0; i < 8; i++) {
			if (fixprerr(blk, crc ^ syndrom[i + 8 * (blk->len - *pr + 1)], pr + 1, pn - 1)) {
				blk->txt[*pr] ^= (1 << i);
				return 1;
			}
		}
		return 0;
	} else {
		/* end of recursion : no more parity error */
		if (crc == 0)
			return 1;

		/* test remainding error in crc */
		for (i = 0; i < 2 * 8; i++)
			if (syndrom[i] == crc) {
				return 1;
			}
		return 0;
	}
}

static int fixdberr(msgblk_t * blk, const unsigned short crc)
{
	int i,j,k;

	/* test remainding error in crc */
	for (i = 0; i < 2 * 8; i++)
		if (syndrom[i] == crc) {
			return 1;
		}

	/* test double error in bytes */
	for (k = 0; k < blk->len ; k++) {
	  int bo=8*(blk->len-k+1);
	  for (i = 0; i < 8; i++)
	   for (j = 0; j < 8; j++) {
		   if(i==j) continue;
		   if((crc^syndrom[i+bo]^syndrom[j+bo])==0) {
			   blk->txt[k] ^= (1 << i);
			   blk->txt[k] ^= (1 << j);
			   return 1;
		   }
	   }
	}
	return 0;
}

#define MAXPERR 3
static void *blk_thread(void *arg)
{
	do {
		msgblk_t *blk;
		int i, pn;
		unsigned short crc;
		int pr[MAXPERR];

		if (verbose)
			fprintf(stderr, "blk_starting\n");

		/* get a message */
		pthread_mutex_lock(&blkq_mtx);
		while ((blkq_e == NULL) && !acars_shutdown)
			pthread_cond_wait(&blkq_wcd, &blkq_mtx);

		if (acars_shutdown) {
			pthread_mutex_unlock(&blkq_mtx);
			break;
		}

		blk = blkq_e;
		blkq_e = blk->prev;
		if (blkq_e == NULL)
			blkq_s = NULL;
		pthread_mutex_unlock(&blkq_mtx);

		if (verbose)
			fprintf(stderr, "get message #%d\n", blk->chn + 1);

		/* handle message */
		if (blk->len < 13) {
			if (verbose)
				fprintf(stderr, "#%d too short\n", blk->chn + 1);
			free(blk);
			continue;
		}

		/* force STX/ETX */
		blk->txt[12] &= (ETX | STX);
		blk->txt[12] |= (ETX & STX);

		/* parity check */
		pn = 0;
		for (i = 0; i < blk->len; i++) {
			if ((numbits[(unsigned char)(blk->txt[i])] & 1) == 0) {
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
		if(pn) {
		  if (fixprerr(blk, crc, pr, pn) == 0) {
			if (verbose)
				fprintf(stderr, "#%d not able to fix errors\n", blk->chn + 1);
			free(blk);
			continue;
		  }
			if (verbose)
				fprintf(stderr, "#%d errors fixed\n", blk->chn + 1);
		} else {
		

		  if (crc) {
			 if(fixdberr(blk, crc) == 0) {
				if (verbose)
					fprintf(stderr, "#%d not able to fix errors\n", blk->chn + 1);
				free(blk);
				continue;
		  	}
		  	if (verbose)
				fprintf(stderr, "#%d errors fixed\n", blk->chn + 1);
		  }
		}

		/* redo parity checking and removing */
		pn = 0;
		for (i = 0; i < blk->len; i++) {
			if ((numbits[(unsigned char)(blk->txt[i])] & 1) == 0) {
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

		outputmsg(blk);

		free(blk);

	} while (1);
	return NULL;
}


int initAcars(channel_t * ch)
{
	if(ch->chn==0) {
        	/* init global message queue */
        	pthread_mutex_init(&blkq_mtx, NULL);
        	pthread_cond_init(&blkq_wcd, NULL);
        	blkq_e=blkq_s=NULL;
        	pthread_create(&blkth_id , NULL, blk_thread, NULL);

		acars_shutdown = 0;
	}

	ch->outbits = 0;
	ch->nbits = 8;
	ch->Acarsstate = WSYN;

	ch->blk = NULL;

	return 0;
}

static void resetAcars(channel_t * ch)
{
	ch->Acarsstate = WSYN;
	ch->MskDf = 0;
	ch->nbits = 1;
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
		resetAcars(ch);
		return;

	case SOH1:
		if (r == SOH) {
			if(ch->blk == NULL) {
				ch->blk = malloc(sizeof(msgblk_t));
				if(ch->blk == NULL) {
					resetAcars(ch);
					return;
				}
			}
			gettimeofday(&(ch->blk->tv), NULL);
			ch->Acarsstate = TXT;
			ch->blk->chn = ch->chn;
			ch->blk->len = 0;
			ch->blk->err = 0;
			ch->nbits = 8;
			ch->MskLvlSum = 0;
			ch->MskBitCount = 0;
			return;
		}
		resetAcars(ch);
		return;

	case TXT:

		ch->blk->txt[ch->blk->len] = r;
		ch->blk->len++;
		if ((numbits[(unsigned char)r] & 1) == 0) {
			ch->blk->err++;

			if (ch->blk->err > MAXPERR + 1) {
				if (verbose)
					fprintf(stderr,
						"#%d too many parity errors\n",
						ch->chn + 1);
				resetAcars(ch);
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
			resetAcars(ch);
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
		ch->blk->lvl = 10*log10(ch->MskLvlSum / ch->MskBitCount);

		if (verbose)
			fprintf(stderr, "put message #%d\n", ch->chn + 1);

		pthread_mutex_lock(&blkq_mtx);
		ch->blk->prev = NULL;
		if (blkq_s)
			blkq_s->prev = ch->blk;
		blkq_s = ch->blk;
		if (blkq_e == NULL)
			blkq_e = blkq_s;
		pthread_cond_signal(&blkq_wcd);
		pthread_mutex_unlock(&blkq_mtx);

		ch->blk=NULL;
		ch->Acarsstate = END;
		ch->nbits = 8;
		return;
	case END:
		resetAcars(ch);
		ch->nbits = 8;
		return;
	}
}


int deinitAcars(void)
{
	pthread_mutex_lock(&blkq_mtx);
	acars_shutdown = 1;
	pthread_mutex_unlock(&blkq_mtx);
	pthread_cond_signal(&blkq_wcd);

	pthread_join(blkth_id, NULL);

	return 0;
}

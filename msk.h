#ifndef msk_h
#define msk_h

#include "acarsdec.h"

int initMsk(channel_t *ch);
void demodMSK(channel_t *ch, int len);

#endif /* msk_h */

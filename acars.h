#ifndef acars_h
#define acars_h

#include "acarsdec.h"

int initAcars(channel_t *);
void decodeAcars(channel_t *);
int deinitAcars(void);

#endif /* acars_h */

#ifndef output_h
#define output_h

#include "acarsdec.h"

int setup_output(char *outarg);
int initOutputs(void);
void exitOutputs(void);
void outputmsg(const msgblk_t *blk);

#endif /* output_h */

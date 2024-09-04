#ifndef soundfile_h
#define soundfile_h

int initSoundfile(char *optarg);
int runSoundfileSample(void);

#ifdef DEBUG
void initSndWrite(void);
void SndWrite(int len);
void SndWriteClose(void);
#endif

#endif /* soundfile_h */

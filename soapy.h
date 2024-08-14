#ifndef soapy_h
#define soapy_h

int initSoapy(char *optarg);
int soapySetAntenna(const char *antenna);
int runSoapySample(void);
int runSoapyClose(void);

#endif /* soapy_h */

# See README for more information about choosing the right optimization
# flags for the target platform

OPT=-Ofast -msse2 -funroll-loops
#OPT= -Ofast -mfpu=vfpv4 -funroll-loops
#OPT= -Ofast -mfpu=neon-vfpv4 -funroll-loops
#OPT= -Ofast -mfpu=neon -ffast-math -funsafe-math-optimizations -fsingle-precision-constant

REQS="alsa libairspy librtlsdr libusb-1.0"
PCFLAGS!=pkg-config --cflags ${REQS}
PCLIBS!=pkg-config --libs ${REQS}
CFLAGS= ${OPT} -pthread -D WITH_RTL -D WITH_AIR -D WITH_ALSA  ${PCFLAGS}
LDLIBS= -lm -pthread ${PCLIBS}

acarsdec:	acarsdec.o acars.o msk.o rtl.o air.o output.o alsa.o
	$(CC) acarsdec.o acars.o msk.o rtl.o air.o output.o alsa.o -o $@ $(LDLIBS)

acarsserv:	acarsserv.o dbmgn.o
	$(CC) -Ofast acarsserv.o dbmgn.o -o $@ -lsqlite3

acarsdec.o:	acarsdec.c acarsdec.h
acars.o:	acars.c acarsdec.h syndrom.h
msk.o:	msk.c acarsdec.h
output.o:	output.c acarsdec.h
rtl.o:	rtl.c acarsdec.h
air.o:	air.c acarsdec.h
acarsserv.o:	acarsserv.h
dbmgm.o:	acarsserv.h

clean:
	@rm -f *.o acarsdec acarsserv

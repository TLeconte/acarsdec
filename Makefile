# See README if you don't want to compile with all libs
CFLAGS= -Ofast -pthread -D WITH_SNDFILE -D WITH_ALSA -D WITH_RTL
LDLIBS= -lm -pthread  -lrtlsdr -lsndfile -lasound


acarsdec:	acarsdec.o acars.o msk.o rtl.o output.o soundfile.o alsa.o
	$(CC) acarsdec.o acars.o msk.o rtl.o output.o soundfile.o alsa.o -o $@ $(LDLIBS)

acarsserv:	acarsserv.o dbmgn.o
	$(CC) acarsserv.o dbmgn.o -o $@ -lsqlite3

acarsdec.o:	acarsdec.c acarsdec.h
acars.o:	acars.c acarsdec.h syndrom.h
msk.o:	msk.c acarsdec.h
output.o:	output.c acarsdec.h
rtl.o:	rtl.c acarsdec.h
soundfile.o:	soundfile.c acarsdec.h
alsa.o:	alsa.c acarsdec.h
acarsserv.o:	acarsserv.h
dbmgm.o:	acarsserv.h

clean:
	@rm *.o acarsdec acarsserv

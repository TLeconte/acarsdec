Acarsdec is a multi-channels acars decoder with built-in rtl_sdr front end.
Since 3.0, It comes with a database backend : acarsserv to store receved acars messages.(See acarsserv chapter below).

Features :

 - up to four channels decoded simultaneously
 - error detection AND correction
 - input alsa sound card or software defined radio (SRD) via a rtl dongle (http://sdr.osmocom.org/trac/wiki/rtl-sdr) or airspy (http://airspy.com/)
 - could send packet over UDP in planeplotter format or in its own format to acarsserv to store ina sqlite database.

The multi channels decoding is particularly useful with the rtl dongle. It allows to directly listen simultaneously to 4 different frequencies , with a very low cost hardware.

Usage: acarsdec  [-v] [-o lv] [-A] [-n|N ipaddr:port] [i- stationid] [-l logfile]  -a alsapcmdevice  |   -r rtldevicenumber  f1 [f2] [f....] | -s f1 [f2] [f....]

 -v :			verbose
 -A :			don't display uplink messages (ie : only aircraft messages)
 -o lv :		output format : 0: no log, 1 one line by msg., 2 full (default) 
 -l logfile :		Append log messages to logfile (Default : stdout).

 -n ipaddr:port :	send acars messages to addr:port via UDP in planeplotter compatible format
 -N ipaddr:port :	send acars messages to addr:port via UDP in acarsdec format
 -i station id:		id use in acarsdec network format.

 -a alsapcmdevice :	decode from soundcard input alsapcmdevice (ie: hw:0,0)

 -r rtldevice f1 [f2] [f...] :		decode from rtl dongle number or S/N "rtldevice" receiving at VHF frequencies "f1" and optionaly "f2" to "f4" in Mhz (ie : -r 0 131.525 131.725 131.825 ). Frequencies must be in the same two Megahertz.
 -g gain :		set rtl preamp gain in tenth of db (ie -g 90 for +9db). By default use maximum gain
 -p ppm :		set rtl ppm frequency correction

 -s f1 [f2] [f...] :		decode from airspy receiving at VHF frequencies "f1" and optionaly "f2" to "f4" in Mhz (ie : -s  131.525 131.725 131.825 ). Frequencies must be in the same two Megahertz.


Examples :

Decoding from sound card with short output :
acarsdec -o1 -a hw:0,0

Decoding from rtl dongle number 0 on 3 frequencies , sending aircraft messages only to 192.168.1.1 on port 5555
and no other loging :
acarsdec -A -N 192.168.1.1:5555 -o0 -r 0 131.525 131.725 131.825

Decoding from airspy on 3 frequecies with verbose  logging
acarsdec -s 131.525 131.725 131.825


Compilation :
acarsdec must compile directly on any modern Linux distrib.
It has been tested on x86_64 with fedora 19-22 and on RaspberryPI (only rtl source tested)

It depends of some external libraries :
 - libasound  for sound card input (rpm package alsa-lib-devel on fedora)
 - librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 - libairspy for airspy software radio input 

If you don't have or don't need one of these libs, edit CFLAGS and LDFLAGS in Makefile.
Ie for rtl decoding only :
CFLAGS= -Ofast -ftree-vectorize -funroll-loops -D WITH_RTL -pthread
LDLIBS= -lm -pthread -lrtlsdr

Note : adapt compiler options to your hardware, particularly on arm platform -ffast-math -march -mfloat-abi -mtune -mfpu must be set correctly.
See exemples in Makefile


Acarsserv

acarsserv is a companion program for acarsdec. It listens to acars messages on UDP coming from one or more acarsdec processes and store them in a sqlite database.

To compile it, just type : 
make acarsserv

acarsserv need sqlite3 dev libraries (on Fedora : sqlite-devel rpm).

By default, it listens to any adresses on port 5555.
So running : 
acarserv &

must be sufficient for most configs.

Then run (possibly on an other computer) :
acarsdec -A -N 192.168.1.1:5555 -o0 -r 0 131.525 131.725 131.825
(where 192.168.1.1 is the ip address of your computer running acarsserv).

acarsserv will create by default an acarsserv.sqb database file where it will store received messages.
You could read its content with sqlite3 command (or more sophisticated graphical interfaces).

acarsserv have some messages filtering and network options (UTSL).



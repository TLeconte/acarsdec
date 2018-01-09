---
layout: default
title : Acarsdec
---

Acarsdec is a multi-channels acars decoder with built-in rtl_sdr front end.
Since 3.0, It comes with a database backend : acarsserv to store receved acars messages.(See acarsserv chapter below).

## Features :

 * up to 8 channels decoded simultaneously
 * error detection AND correction
 * input alsa sound card or software defined radio (SDR) via a [rtl dongle](http://sdr.osmocom.org/trac/wiki/rtl-sdr) or [airspy](http://airspy.com/)
 * could send packet over UDP in planeplotter format or in its own format to acarsserv to store in a sqlite database.

The multi channels decoding is particularly useful with the rtl dongle. It allows to directly listen simultaneously to 8 different frequencies , with a very low cost hardware.

## Usage
> acarsdec  [-v] [-o lv] [-t time] [-A] [-n|N ipaddr:port] [i- stationid] [-l logfile]  -a alsapcmdevice  |   -r rtldevicenumber  f1 [f2] [f....] | -s f1 [f2] [f....]

 -v :			verbose
 
 -A :			don't display uplink messages (ie : only aircraft messages)
 
 -o lv :		output format : 0: no log, 1 one line by msg., 2 full (default), 3 monitor mode
 
 -t time :		set forget time in secondes in monitor mode(default=600s)
 
 -l logfile :		Append log messages to logfile (Default : stdout)
 
 -n ipaddr:port :	send acars messages to addr:port via UDP in planeplotter compatible format
 
 -N ipaddr:port :	send acars messages to addr:port via UDP in acarsdec format
 
 -i station id:		id use in acarsdec network format.

 -a alsapcmdevice :	decode from soundcard input alsapcmdevice (ie: hw:0,0)

 -r rtldevice f1 [f2] ... [f8] :		decode from rtl dongle number or S/N "rtldevice" receiving at VHF frequencies "f1" and optionaly "f2" to "f8" in Mhz (ie : -r 0 131.525 131.725 131.825 ). Frequencies must be in the SAME TWO MHZ.
 
 -g gain :		set rtl preamp gain in tenth of db (ie -g 90 for +9db). By default use maximum gain
 
 -p ppm :		set rtl ppm frequency correction

 -s f1 [f2] ... [f8] :		decode from airspy receiving at VHF frequencies "f1" and optionaly "f2" to "f8" in Mhz (ie : -s  131.525 131.725 131.825 ). Frequencies must be in the same two Megahertz.


## Examples

Decoding from sound card with short output :
> acarsdec -o1 -a hw:0,0

Decoding from rtl dongle number 0 on 3 frequencies , sending aircraft messages only to 192.168.1.1 on port 5555
and no other loging :
> acarsdec -A -N 192.168.1.1:5555 -o0 -r 0 131.525 131.725 131.825

Decoding from airspy on 3 frequecies with verbose  logging
> acarsdec -s 131.525 131.725 131.825

Decoding from sound file test.wav (included) :
> acarsdec -f test.wav

### Output formats examples

#### One line by mesg format (-o 1)

    #2 (L:  -5 E:0) 25/12/2016 16:26:40 .EC-JBA IB3166 X B9 J80A /EGLL.TI2/000EGLLAABB2
    #3 (L:   8 E:0) 25/12/2016 16:26:44 .G-OZBF ZB494B 2 Q0 S12A 
    #3 (L:   0 E:0) 25/12/2016 16:26:44 .F-HZDP XK773C 2 16 M38A LAT N 47.176/LON E  2.943


#### Full message format (-o 2)

    [#1 (F:131.825 L:   4 E:0) 25/12/2016 16:27:45 --------------------------------
    Aircraft reg: .A6-EDY Flight id: EK0205
    Mode : 2 Label : SA Id : 4 Ack : !
    Message no: S31A :
    0EV162743VS/
    
    [#3 (F:131.825 L:   3 E:0) 25/12/2016 16:28:08 --------------------------------
    Aircraft reg: .F-GSPZ Flight id: AF0940
    Mode : 2 Label : B2 Id : 1 Ack : !
    Message no: L07A :
    /PIKCLYA.OC1/CLA 1627 161225 EGGX CLRNCE 606
    AFR940 CLRD TO MUHA VIA ETIKI
    RANDOM ROUTE
    46N020W 45N027W 44N030W 40N040W 36N050W
    34N055W 32N060W 27N070W
    FM ETIKI/1720 MNTN F340 M083
    ATC/LEVEL CHANGE
    END O

#### Monitoring mode (-o 3)

                 Acarsdec monitor
     Aircraft Flight  Nb Channels   Last     First
     .CN-RNV  AT852X   9 .x.      16:25:58 16:21:05
     .F-HBMI  ZI0321   1 .x.      16:25:52 16:25:52
     .F-GSPZ  AF0940   6 ..x      16:25:21 16:22:30
     .D-ABUF  DE0252   1 .x.      16:25:20 16:25:20
     .EC-MGS  V72422   1 .x.      16:25:07 16:25:07
     .G-EUUU  BA733C   2 .x.      16:24:38 16:24:33


## Compilation
acarsdec must compile directly on any modern Linux distrib.
It has been tested on x86_64 with fedora 19-25 and on RaspberryPI (only rtl source tested)

It depends of some external libraries :
 * libasound  for sound card input (rpm package alsa-lib-devel on fedora)
 * librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libairspy for airspy software radio input 

If you don't have or don't need one of these libs, edit CFLAGS and LDFLAGS in Makefile.
Ie for rtl decoding only :
> CFLAGS= -Ofast -D WITH_RTL -pthread
> LDLIBS= -lm -pthread -lrtlsdr

Note : change compiler options to your hardware, particularly on arm platform -ffast-math -march -mfloat-abi -mtune -mfpu must be set correctly.
See exemples in Makefile


# Acarsserv

acarsserv is a companion program for acarsdec. It listens to acars messages on UDP coming from one or more acarsdec processes and store them in a sqlite database.

To compile it, just type : 
> make acarsserv

acarsserv need sqlite3 dev libraries (on Fedora : sqlite-devel rpm).

By default, it listens to any adresses on port 5555.
So running : 
> acarserv &

must be sufficient for most configs.

Then run (possibly on an other computer) :
> acarsdec -A -N 192.168.1.1:5555 -o0 -r 0 131.525 131.725 131.825
> (where 192.168.1.1 is the ip address of your computer running acarsserv).

acarsserv will create by default an acarsserv.sqb database file where it will store received messages.
You could read its content with sqlite3 command (or more sophisticated graphical interfaces).

acarsserv have some messages filtering and network options (UTSL).



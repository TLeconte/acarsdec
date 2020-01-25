# ACARSDEC
Acarsdec is a multi-channels acars decoder with built-in rtl_sdr, airspy front end or sdrplay device.
Since 3.0, It comes with a database backend : acarsserv to store received acars messages. (See acarsserv chapter below).

## Features :

 * up to 8 channels decoded simultaneously
 * error detection AND correction
 * input via [rtl_sdr](https://sdr.osmocom.org/trac/wiki/rtl-sdr),
   or [airspy](https://airspy.com/) or [sdrplay](https://www.sdrplay.com) software defined radios (SDR)
 * logging data over UDP in planeplotter or acarsserv formats to store in an sqlite database, or JSON for custom processing.
 * decoding of ARINC-622 ATS applications (ADS-C, CPDLC) via [libacars](https://github.com/szpajder/libacars) library

Multi-channel decoding is particularly useful with broadband devices such
as the RTLSDR dongle, the AIRspy and the SDRplay device.
It allows the user to directly monitor to up to 8 different frequencies simultaneously with very low cost hardware.

## Usage
> acarsdec  [-v] [-o lv] [-t time] [-A] [-n|N|j ipaddr:port] [-i stationid] [-l logfile [-H|-D]] -r rtldevicenumber  f1 [f2] [... fN] | -s f1 [f2] [... fN]

 -v :			verbose
 
 -A :			don't display uplink messages (ie : only aircraft messages)
 
 -o lv :		output format : 0 : no log, 1 : one line by msg, 2 : full (default), 3 : monitor mode, 4 : msg JSON, 5 : route JSON
 
 -t time :		set forget time (TTL) in seconds in monitor mode(default=600s)
 
 -l logfile :		append log messages to logfile (Default : stdout)

 -H :			rotate log file once every hour

 -D :			rotate log file once every day
 
 -n ipaddr:port :	send acars messages to addr:port via UDP in planeplotter compatible format
 
 -N ipaddr:port :	send acars messages to addr:port via UDP in acarsdec format

 -j ipaddr:port :	send acars messages to addr:port via UDP in JSON format
 
 -i station id:		id use in acarsdec network format.

 -b filter:		filter output by label (ex: -b "H1:Q0" : only output messages  with label H1 or Q0"

for the RTLSDR device

 -r rtldevice f1 [f2] ... [fN] :		decode from rtl dongle number or S/N "rtldevice" receiving at VHF frequencies "f1" and optionally "f2" to "fN" in Mhz (ie : -r 0 131.525 131.725 131.825 ). Frequencies must be within the same 2MHz.
 
 -g gain :		set rtl preamp gain in tenth of db (ie -g 90 for +9db). By default use maximum gain
 
 -p ppm :		set rtl ppm frequency correction

for the AIRspy device

 -s f1 [f2] ... [fN] :		decode from airspy receiving at VHF frequencies "f1" and optionally "f2" to "fN" in Mhz (ie : -s  131.525 131.725 131.825 ). Frequencies must be within the same 2MHz.

for the SDRplay device

 -s f1 [f2] ... [fN] :		decode from SDRplay receiving at VHF frequencies "f1" and optionally "f2" to "fN" in Mhz (ie : -s  131.525 131.725 131.825 ). Frequencies must be within the same 2MHz.

 -L lnaState:	set the lnaState (depends on the selected SDRPlay hardware)

 -G GRdB:	set the Gain Reduction in dB's. -100 is used for agc.

## Examples

Decoding from rtl dongle number 0 on 3 frequencies , sending aircraft messages only to 192.168.1.1 on port 5555
and no other loging :
> acarsdec -A -N 192.168.1.1:5555 -o0 -r 0 131.525 131.725 131.825

Decoding from airspy on 3 frequencies with verbose logging
> acarsdec -s 131.525 131.725 131.825

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


#### JSON mode (-o 4)

    {"timestamp":1516206744.1849549,"channel":2,"freq":130.025,"level":-22,"error":0,"mode":"2","label":"H1","block_id":"6","ack":false,"tail":".N842UA","flight":"UA1412","msgno":"D04G","text":"#DFB9102,0043,188/9S101,0039,181/S0101,0043,188/0S100,0039,182/T1100,0043,188/1T099,0039,182/T2099,0043,189/2T098,0039,182/T3098,0043,189/3T097,0039,182/T4098,0043,189/4T097,0039,183/T5098,0043,189/5T097,0039,1","end":true,"station_id":"sigint"}
    {"timestamp":1516206745.249615,"channel":2,"freq":130.025,"level":-24,"error":2,"mode":"2","label":"RA","block_id":"R","ack":false,"tail":".N842UA","flight":"","msgno":"","text":"QUHDQWDUA?1HOWGOZIT\r\n ** PART 01 OF 01 **\r\nHOWGOZIT 1412-17 SJC\r\nCI: 17        RLS: 01 \r\nSJC 1615/1625     171A\r\nBMRNG    1630 37  159-\r\nTIPRE    1638 37  145\r\nINSLO    1701 37  125\r\nGAROT    1726 37  106\r\nEKR      1800 ","end":true,"station_id":"sigint"}

#### JSON route mode (-o 5) (wait for a while before an output)

    {"timestamp":1543677178.9600339,"flight":"BA750P","depa":"EGLL","dsta":"LFSB"}


#### with libacars and ARINC 622 decoding 

    [#2 (F:131.725 L:-33 E:0) 30/11/2018 19:45:46.645 --------------------------------
    Mode : 2 Label : H1 Id : 3 Nak
    Aircraft reg: G-OOBE Flight id: BY01WH
    No: F57A
    #M1B/B6 LPAFAYA.ADS.G-OOBE0720BD17DFD188CAEAE01F0C50F3715C88200D2344EFF62F08CA8883238E3FF7748768C00E0C88D9FFFC0F08A9847FFCFC16
    ADS-C message:
     Basic report:
      Lat: 46.0385513
      Lon: -5.6569290
      Alt: 36012 ft
      Time: 2744.000 sec past hour (:45:44.000)
      Position accuracy: <0.05 nm
      NAV unit redundancy: OK
      TCAS: OK
     Flight ID data:
      Flight ID: TOM1WH
     Predicted route:
      Next waypoint:
       Lat: 49.5972633
       Lon: -1.7255402
       Alt: 36008 ft
       ETA: 2179 sec
      Next+1 waypoint:
       Lat: 49.9999809
       Lon: -1.5020370
       Alt: 30348 ft
     Earth reference data:
      True track: 35.2 deg
      Ground speed: 435.5 kt
      Vertical speed: -16 ft/min
     Air reference data:
     True heading: 24.3 deg
     Mach speed: 0.7765
     Vertical speed: -16 ft/min


## Compilation
acarsdec must compile directly on any modern Linux distrib.

It needs cmake and a C compiler.

It depends on some external libraries :
 * libusb
 * librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libairspy for airspy software radio input 
 * libmirsdrapi-rsp for sdrplay software radio input 
 * optionaly libacars for decoding ATS applications (https://github.com/szpajder/libacars)

For rtl_sdr :
> mkdir build

> cd build

> cmake .. -Drtl=ON

> make

> sudo make install

For airspy :
> mkdir build

> cd build

> cmake .. -Dairspy=ON

> make

> sudo make install

For sdrplay :
> mkdir build

> cd build

> cmake .. -Dsdrplay=ON

> make

> sudo make install

Notes : 
 * For rtl_sdr, you could change the input sample rate by changing RTLMULT in rtl.c. Default is 2.0Ms/s which is a safe value. You could increase it for the better, but it could be over the limits of some hardware and will increase CPU usage too. 
 * Airspy version will set the R820T tuner bandwidth to suit given frequencies. See : (https://tleconte.github.io/R820T/r820IF.html)
 * libacars support is optional. If the library (version 2.0.0 or later) is installed and can be located with pkg-config, it will be enabled.
 * If you have call cmake .. -Dxxx one time, the option will be sticky . Remove build dir and redo to change sdr option.
 * For raspberry Pi and others ARM machines, the gcc compile option -march=native could not be working, so modify the add_compile_options in CMakeLists.txt to set the correct options for your platform.

# Acarsserv

acarsserv is a companion program for acarsdec. It listens to acars messages on UDP coming from one or more acarsdec processes and stores them in a sqlite database.

See : [acarsserv](https://github.com/TLeconte/acarsserv)

## Copyrights 
acarsdec and acarsserv are Copyright Thierry Leconte 2015-2018

These code are free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License version 2
published by the Free Software Foundation.

They include [cJSON](https://github.com/DaveGamble/cJSON) Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

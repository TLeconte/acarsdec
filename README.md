# ACARSDEC

acarsdec is a multi-channels ACARS decoder with built-in rtl_sdr, soapysdr, airspy or sdrplay device support.
Since 3.0, it can work with a database backend: [acarsserv](https://github.com/TLeconte/acarsserv) to store received ACARS messages.

## Features

 * arbitrary number of channels decoded simultaneously (limited by bandwidth and CPU power)
 * error detection AND correction
 * input via either WAV sound file, ALSA soundcard, [rtl_sdr](https://sdr.osmocom.org/trac/wiki/rtl-sdr),
   [SoapySDR](https://github.com/pothosware/SoapySDR/wiki), [airspy](https://airspy.com/)
   or [sdrplay](https://www.sdrplay.com) software defined radios (SDR)
 * multiple simultaneous outputs via file, UDP or MQTT
 * multiple output formats: one line, full text, planeplotter, acarsserv, JSON and live monitoring
 * decoding of ARINC-622 ATS applications (ADS-C, CPDLC) via [libacars](https://github.com/szpajder/libacars) library
 * statistics reporting via StatsD-compliant interface

## License

GPLv2-only - http://www.gnu.org/licenses/gpl-2.0.html

Copyright: (C) 2015-2022 Thierry Leconte 2015-2022, (C) 2024 Thibaut VARENE

> This program is free software; you can redistribute it and/or
> modify it under the terms of the GNU General Public License
> version 2, as published by the Free Software Foundation.
>
> This program is distributed in the hope that it will be useful,
> but WITHOUT ANY WARRANTY; without even the implied warranty of
> MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See LICENSE.md for details

## Compilation

### Dependencies

acarsdec requires CMake, pkg-config and a C compiler.

It depends on some optional external libraries :
 * librtlsdr for RTL-based SDR input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libsoapysdr for SoapySDR input (https://github.com/pothosware/SoapySDR)
 * libairspy for airspy SDR input (https://github.com/airspy/airspyone_host)
 * libmirsdrapi-rsp for sdrplay software radio input
 * libsndfile for WAV input (https://github.com/libsndfile/libsndfile)
 * libasound for ALSA input (https://github.com/alsa-project/alsa-lib)
 * libacars for decoding ATS applications (https://github.com/szpajder/libacars)
 * libcjson for JSON output support (https://github.com/DaveGamble/cJSON)
 * paho-mqtt3a (and libcjson) for MQTT output support (https://github.com/eclipse/paho.mqtt.c)

### Building

```
mkdir build
cd build
cmake .. -DCMAKE_C_FLAGS="-march=native"
make
```

All optional libraries will be autodected, a summary of what is enabled will be printed by cmake.

The `-DCMAKE_C_FLAGS="-march=native"` argument to cmake will produce an `acarsdec` executable
that is optimized for the machine it was built on (and may not run correctly on other devices).
It can be ommitted for a platform-generic build (with possibly reduced performance).
 
#### Raspberry Pi builds

It seems that the compile option `-march=native` may be problematic on Raspberry Pi.

In that case, one can use the following cmake parameter instead in the above procedure:

 * for PI 2B : `-DCMAKE_C_FLAGS="-mcpu=cortex-a7 -mfpu=neon-vfpv4"`
 * for PI 3B : `-DCMAKE_C_FLAGS="-mcpu=cortex-a53 -mfpu=neon-fp-armv8"`
 * for PI 4B : `-DCMAKE_C_FLAGS="-mcpu=cortex-a72 -mfpu=neon-fp-armv8"`

### Installing

```
sudo make install
```

## Usage

acarsdec operation can be controlled via multiple command line parameters
(the availability of which depends on which optional libraries have been enabled at build time),
they are detailed below:

### Common options

```
 -i <stationid>	station id used in statsd reports and network destinations (default: hostname)
 -A		don't output uplink messages (ie : only aircraft messages)
 -e		don't output empty messages (ie : _d,Q0, etc ...)
 -b <filter>	filter output by label (ex: -b "H1:Q0" : only output messages  with label H1 or Q0)
 -t <seconds>	set forget time (TTL) in seconds for monitor mode (default: 600)

 --statsd host=HOST,port=1234		enable statsd reporting to host "HOST" (name or IP) on port "1234"
 --output FORMAT:DESTINATION:DESTPARAMS (see below for supported output formats and destinations. DESTPARAMS are coma-separated: ',')
```

If libacars is enabled, the following extra option is available:

```
 --skip-reassembly	disable reassembling fragmented ACARS messages
```

Multiple instances of `--output` can be used. At least one is required.
See `--output help` for a list of supported formats and destinations.
Not all combinations of formats and destinations are valid, acarsdec will complain if an invalid combination is chosen.

One (and **only one**) input source must also be selected, see below.
Options (including frequencies) can be provided in any order.

#### Supported output formats

- `oneline` for single line text decoding
- `full` for full text decoding
- `monitor` for live decoding
- `pp` for PlanePlotter
- `native` for Acarsdec native format

With CJSON support enabled:
- `json` for JSON output
- `routejson` for flight route output in JSON format

#### Supported destinations

##### `file` output

Outputs to a file (including stdout, the default). Supports all output formats.

DESTPARAMS are optional (no param implies output to stdout) and can be:

- `path=` followed by either `-` for stdout or a path to a logfile
- `rotate=` followed by one of `none`, `hourly` or `daily` to dis/enable hourly/daily logfile rotation

##### `udp` output

Outputs to IPv4 or IPv6 network via UDP. Supports all but `monitor` output formats.

DESTPARAMS are:
- `host=` (required) followed by a hostname or an IP address
- `port=` (optional) followed by a port number (defaults to 5555)

##### `mqtt` output

With PAHO MQTT support enabled, pushes JSON to remote MQTT server. Supports only `json` and `routejson` output formats.

DESTPARAMS are:
- `uri=` (required) followed by a valid MQTT URI (e.g. `tcp://host:port`). Can be repeated up to 15 times.
- `user=` (optional) followed by a username
- `passwd=` (optional) followed by a password
- `topic=` (optional) followed by a topic (defaults to `acarsdec/<stationid>`) 

### Input sources

#### RTL-SDR

```
 --rtlsdr <device>	decode from rtl dongle number <device> or S/N <device>
 -g <gain>		set rtl gain in db (0 to 49.6; >52 and -10 will result in AGC; default is AGC)
 -p <ppm>		set rtl ppm frequency correction (default: 0)
 -m <rateMult>		set rtl sample rate multiplier: 160 for 2 MS/s or 192 for 2.4 MS/s (default: 160)
 -B <bias>		enable (1) or disable (0) the bias tee (default is 0)
 -c <freq>		set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)
```


#### Airspy R2 / Mini

```
 --airspy <device>	decode from airspy dongle number <device> or hex serial <device>
 -g <linearity_gain>	set linearity gain [0-21] (default: 18)
```
 
Note: acarsdec will try to set the R820T tuner bandwidth to suit given frequencies.
See https://tleconte.github.io/R820T/r820IF.html

#### SDRplay

```
 --sdrplay 		decode from sdrplay
 -L <lnaState>		set the lnaState (depends on the device)
 -G <GRdB>		gain reduction in dB's, range 20 .. 59 (default: -100 is autogain)
 -c <freq>		set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)

```

#### SoapySDR

```
 --soapysdr <params>	decode from a SoapySDR designed by device_string <params>\n"
 -g <gain>		set gain in db (-10 will result in AGC; default is AGC)\n"
 -p <ppm>		set ppm frequency correction (default: 0)\n"
 -c <freq>		set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)\n"
 -m <rateMult>		set sample rate multiplier: 160 for 2 MS/s or 192 for 2.4 MS/s (default: 160)\n"
 -a <antenna>		set antenna port to use (default: soapy default)\n");
```

All SDR sources described above expect a list of frequencies `<f1> [<f2> [...]]` to decode from, expressed in decimal MHz
e.g. `131.525 131.725 131.825`.

#### WAV sound file

```
 --sndfile <file.wav>	decode from <file.wav>. Must be sampled at 12.5kHz
```

#### ALSA device

```
 --alsa <alsadevice>	decode from ALSA PCM device <alsadevice>
```

## Examples

Decoding from rtl dongle number 0 on 3 frequencies, sending aircraft messages in native format to 192.168.1.1 on port 5555
and no other loging :

`acarsdec -A --output native:udp:host=192.168.1.1,port=5555 --rtlsdr 0 131.525 131.725 131.825`

Monitoring from rtl dongle with serial number `ACARS2` on 1 frequency with gain 34.0 :

`acarsdec --output monitor:file -g 34 --rtlsdr ACARS2 130.450`

Logging to file "airspy.log" rotated daily, from airspy mini with serial number `0xa74068c82f531693` on 11 frequencies with gain 18 :

`acarsdec --output full:file:path=airspy.log,rotate=daily -g 18 --airspy 0xa74068c82f531693 129.350 130.025 130.425 130.450 130.650 131.125 131.475 131.550 131.600 131.725 131.850`

Decoding with JSON output to stdout, an sdrplay device using Soapy driver, and specifying Antenna C :

`acarsdec --output json:file:path=- --soapysdr driver=sdrplay,agc_setpoint=-15 -a "Antenna C" 130.025 130.450 130.825 131.125 131.550 131.650 131.725`

Decoding 7 channels from rtl dongle with serial '86000034', filtering empty messages,
setting ppm correction to 36, gain to 48 dB, full text to stdout,
sending JSON to feed.acars.io:5550 with station ID "MY-STATION-ID",
sending statsd data to host "statsd.lan" port "8125":

`acarsdec -e -i MY-STATION-ID --output full:file --output json:udp:host=feed.acars.io,port=5550 -p 36 --rtlsdr 86000034 -g 48 131.550 130.825 130.850 131.525 131.725 131.825 131.850 --statsd host=statsd.lan,port=8125`

### Output formats examples

#### One line by mesg format (`--output oneline:file`)

```
    #2 (L:  -5 E:0) 25/12/2016 16:26:40 .EC-JBA IB3166 X B9 J80A /EGLL.TI2/000EGLLAABB2
    #3 (L:   8 E:0) 25/12/2016 16:26:44 .G-OZBF ZB494B 2 Q0 S12A 
    #3 (L:   0 E:0) 25/12/2016 16:26:44 .F-HZDP XK773C 2 16 M38A LAT N 47.176/LON E  2.943
```

#### Full message format (`--output full:file`)

```
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
```

#### Monitoring mode (`--output monitoring:file`) (wait for a while before output begins)

```
                 Acarsdec monitor
     Aircraft Flight  Nb Channels   Last     First
     .CN-RNV  AT852X   9 .x.      16:25:58 16:21:05
     .F-HBMI  ZI0321   1 .x.      16:25:52 16:25:52
     .F-GSPZ  AF0940   6 ..x      16:25:21 16:22:30
     .D-ABUF  DE0252   1 .x.      16:25:20 16:25:20
```

#### JSON mode (`--output json:file`)

```
    {"timestamp":1516206744.1849549,"channel":2,"freq":130.025,"level":-22,"error":0,"mode":"2","label":"H1","block_id":"6","ack":false,"tail":".N842UA","flight":"UA1412","msgno":"D04G","text":"#DFB9102,0043,188/9S101,0039,181/S0101,0043,188/0S100,0039,182/T1100,0043,188/1T099,0039,182/T2099,0043,189/2T098,0039,182/T3098,0043,189/3T097,0039,182/T4098,0043,189/4T097,0039,183/T5098,0043,189/5T097,0039,1","end":true,"station_id":"sigint"}
    {"timestamp":1516206745.249615,"channel":2,"freq":130.025,"level":-24,"error":2,"mode":"2","label":"RA","block_id":"R","ack":false,"tail":".N842UA","flight":"","msgno":"","text":"QUHDQWDUA?1HOWGOZIT\r\n ** PART 01 OF 01 **\r\nHOWGOZIT 1412-17 SJC\r\nCI: 17        RLS: 01 \r\nSJC 1615/1625     171A\r\nBMRNG    1630 37  159-\r\nTIPRE    1638 37  145\r\nINSLO    1701 37  125\r\nGAROT    1726 37  106\r\nEKR      1800 ","end":true,"station_id":"sigint"}
```

#### JSON route mode (`--output routejson:file`) (wait for a while before output begins)

```
    {"timestamp":1543677178.9600339,"flight":"BA750P","depa":"EGLL","dsta":"LFSB"}
```

#### with libacars and ARINC 622 decoding 

```
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
```

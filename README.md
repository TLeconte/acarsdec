# ACARSDEC

acarsdec is a multi-channels ACARS decoder with built-in rtl_sdr, soapysdr, airspy or sdrplay device support.
Since 3.0, it can work with a database backend: [acarsserv](https://github.com/TLeconte/acarsserv) to store received ACARS messages.

## Features

 * arbitrary number of channels decoded simultaneously (limited by bandwidth and CPU power)
 * error detection AND correction
 * input via either audio file (including raw audio), ALSA soundcard, [rtl_sdr](https://sdr.osmocom.org/trac/wiki/rtl-sdr),
   [SoapySDR](https://github.com/pothosware/SoapySDR/wiki), [airspy](https://airspy.com/)
   or [sdrplay](https://www.sdrplay.com) software defined radios (SDR)
 * multiple simultaneous outputs via file, UDP or MQTT
 * multiple output formats: one line, full text, planeplotter, acarsserv, JSON and live monitoring
 * decoding of ARINC-622 ATS applications (ADS-C, CPDLC) via [libacars](https://github.com/szpajder/libacars) library
 * statistics reporting via StatsD-compliant interface

## License

GPLv2-only - http://www.gnu.org/licenses/gpl-2.0.html

Copyright: (C) 2015-2022 Thierry Leconte, (C) 2024-2025 Thibaut VARENE

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
 * libsndfile for audio input (https://github.com/libsndfile/libsndfile)
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

In case the compile option `-march=native` doesn't work correctly on Raspberry Pi,
the following cmake parameter can be used instead in the above procedure:

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
 -m <rateMult>		set rtl sample rate multiplier: sample rate is <rateMult> * 12000 S/s (default: automatic)
 -B <bias>		enable (1) or disable (0) the bias tee (default is 0)
 -c <freq>		set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)
```

#### SoapySDR

```
 --soapysdr <params>	decode from a SoapySDR designed by device_string <params>
 -g <gain>		set gain in db (-10 will result in AGC; default is AGC)
 -p <ppm>		set ppm frequency correction (default: 0)
 -c <freq>		set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)
 -m <rateMult>		set sample rate multiplier: sample rate is <rateMult> * 12000 S/s (default: automatic)
 -a <antenna>		set antenna port to use (default: soapy default)
```

#### Airspy Mini (R2 is currently not supported)

```
 --airspy <device>	decode from airspy dongle number <device> or hex serial <device>
 -g <linearity_gain>	set linearity gain [0-21] (default: 18)
 -B <bias>		enable (1) or disable (0) the bias tee (default is 0)
```
 
Note: acarsdec will try to set the R820T tuner bandwidth to suit given frequencies.
See https://tleconte.github.io/R820T/r820IF.html

#### SDRplay (untested, uses legacy v2 API - help wanted)

```
 --sdrplay 		decode from sdrplay
 -L <lnaState>		set the lnaState (depends on the device)
 -G <GRdB>		gain reduction in dB's, range 20 .. 59 (default: -100 is autogain)
 -c <freq>		set center frequency to tune to in MHz, e.g. 131.800 (default: automatic)
```

All SDR sources described above expect a list of frequencies `<f1> [<f2> [...]]` to decode from, expressed in decimal MHz
e.g. `131.525 131.725 131.825`.

#### audio file

All formats supported by libsndfile can be processed.

```
 --sndfile <file>	decode from <file>, which must be a libsndfile supported format and sampled at a multiple of 12kHz
```

To decode raw audio, extra parameters must be provided. Example:

```
 --sndfile <file>,subtype=<N>	decode single-channel raw in libsndfile-supported <N> subtype from <file> sampled at a multiple of 12KHz.
```

For raw audio, the sample rate multiplier can be adjusted using `-m`. See `--sndfile help` for more details.

libsndfile-supported subtypes are listed here: http://libsndfile.github.io/libsndfile/api.html#open

NB: `<file>` can be `/dev/stdin` to process piped-in data.

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

Decoding raw CPU-endian single-channel PCM16 from stdin, providing one line per message on stdout:

`acarsdec --sndfile file=/dev/stdin,subtype=0x02 --output oneline:file`

### Output formats examples

#### One line by mesg format (`--output oneline:file`)

```
#1 (L:-11.7/-59.1 E:0) 10/07/2025 13:31:56.647  F-GSPN AF0650 2 _d S90A 
#3 (L:-30.5/-58.6 E:0) 10/07/2025 13:32:01.341  G-VIIS BA0127 2 H1 O49C 00011100111111100000001111  -350000000001011011111111101100
#3 (L:-33.5/-55.5 E:0) 10/07/2025 13:32:09.020  G-VIIS BA0127 2 H1 O49D 000001111  -32000000000101101111111110110001000001110011111
#3 (L:-35.7/-54.0 E:0) 10/07/2025 13:32:15.592  G-VIIS BA0127 2 H1 O49E 0000001011011111111101100010000011100111111100000001111  -2
#3 (L:-32.8/-52.4 E:0) 10/07/2025 13:32:25.149  G-VIIS BA0127 2 H1 O49F 11101100010000011100111111100000001111  -250000000001011011
#1 (L:-13.9/-49.6 E:0) 10/07/2025 13:32:26.087  F-GSPN AF0650 2 H1 D21A OP FUELPROBES  106110 94105 98 82 84 77 72 66 65 62 10 62 5
```

#### Full message format (`--output full:file`)

```
[#1 (F:131.525 L:-25.6/-46.6 E:0) 10/07/2025 13:28:57.434 --------------------------------
Mode : 2 Label : B2 Id : 3 Nak
Aircraft reg: F-GSPN Flight id: AF0650
No: L05A
Reassembly: in progress
/PIKCLYA.OC1/CLA 1328 250710 EGGX CLRNCE 320
AFR650 CLRD TO MMUN VIA BUNAV
RANDOM ROUTE
46N015W 44N020W 40N030W 35N040W 32N050W
30N060W 27N070W
FM BUNAV/1424 MNTN F340 M084
ATC/ENTRY POINT CHANGE ROUTE AM
ETB
```

##### with libacars and ARINC 622 decoding 

```
[#1 (F:131.525 L:-33.3/-46.4 E:0) 10/07/2025 13:24:08.751 --------------------------------
Mode : 2 Label : BA Id : 3 Nak
Aircraft reg: F-GSPK Flight id: AF0032
No: L08A
Reassembly: skipped
/SOUCAYA.AT1.F-GSPK618AD60600F470
FANS-1/A CPDLC Message:
 CPDLC Downlink Message:
  Header:
   Msg ID: 3
   Msg Ref: 5
   Timestamp: 13:24:06
  Message data:
   WILCO
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
{"timestamp":1752154097.8392439,"station_id":"MY-STATION-ID","channel":2,"freq":131.825,"level":-34.6,"noise":-48.7,"error":3,"mode":"2","label":"H1","block_id":"9","ack":false,"tail":"N827NW","flight":"NW0183","msgno":"D52A","text":"239182200601010098079\r\nR39/A33039,1,1\r\n239N827NW183 071025132804821 4783-  205339-19-49297 29AB0510111800\r\n 286 451 000303600030IH-LIRFKJFK","sublabel":"DF","assstat":"skipped","app":{"name":"acarsdec","ver":"4.1"}}
```

#### JSON route mode (`--output routejson:file`) (wait for a while before output begins)

```
{"timestamp":1752155500.3385351,"station_id":"MY-STATION-ID","flight":"BA0263","depa":"EGLL","dsta":"OERK"}
```

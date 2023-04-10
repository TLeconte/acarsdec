#!/bin/bash

(( fm=130 ))
while [ "$fm" -lt 137 ] ; do
	(( f0=0 )); (( f1=125 )); (( f2=250 )); (( f3=375 )); (( f4=500 )); (( f5=625 )); (( f6=750 )); (( f7=875 )); (( fx=0 ));
	echo -e "\tScanning $fm MHz..."
	while [ "$f0" -lt 125 ] ; do
		echo -e "\t Scanning $fm.$fx$f0 $fm.$f1 $fm.$f2 $fm.$f3 $fm.$f4 $fm.$f5 $fm.$f6 $fm.$f7"
		# If using with acarshub, add '-o 4 -j 127.0.0.1:5550', or the correct IP for your acarshub install. If also feeding to acars.io, add '-i <station_name>'. E.g.,
		# acarsdec -v -l scanlog -o 4 -j 127.0.0.1:5550 -r 0 "$fm"."$fx""$f0" "$fm"."$f1" "$fm"."$f2" "$fm"."$f3" "$fm"."$f4" "$fm"."$f5" "$fm"."$f6" "$fm"."$f7" &
		acarsdec -v -l scanlog -r 0 "$fm"."$fx""$f0" "$fm"."$f1" "$fm"."$f2" "$fm"."$f3" "$fm"."$f4" "$fm"."$f5" "$fm"."$f6" "$fm"."$f7" &
		sleep 300 # time to scan this freq set
		killall acarsdec
		(( f0=f0+25 )); (( f1=f1+25 )); (( f2=f2+25 )); (( f3=f3+25 )); (( f4=f4+25 )); (( f5=f5+25 )); (( f6=f6+25 )); (( f7=f7+25 ));
		if [ "$f0" -gt "76" ] ; then
	  	unset fx
		fi
		sleep 5 # give SDR time to become ready again
 	done
 	(( fm=fm+1 ));
done
grep --color=never -e "(F:" scanlog | cut -c 8-14 | sort | uniq -c
# If using with acarshub, the json file output will necesitate a different grep to pull the frequencies:
# grep --color=never -e freq scanlog | cut -d',' -f4 | cut -c 8-14 | sort | uniq -c

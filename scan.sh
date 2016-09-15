#/bin/bash

let fm=128
while [ "$fm" -lt 139 ] ; do
let f0=0; let f1=250 ; let f2=500 ; let f3=750
while [ "$f0" -lt 250 ] ; do
	echo $fm.$f0
	./acarsdec -l scan/$fm.$f0 -r 0 $fm.$f0 $fm.$f1 $fm.$f2 $fm.$f3 &
	sleep 800
	killall acarsdec
	sleep 5
	let f0=f0+25;let f1=f1+25;let f2=f2+25;let f3=f3+25;
 done
 let fm=fm+1;
done


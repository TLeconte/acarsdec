#/bin/bash

let fm=130
while [ "$fm" -lt 137 ] ; do
let f0=0; let f1=125 ; let f2=250 ; let f3=375 ; let f4=500 ; let f5=625 ; let f6=750 ; let f7=875 
echo $fm
while [ "$f0" -lt 125 ] ; do
	echo $f0
	./acarsdec  -l scanlog -s $fm.$f0 $fm.$f1 $fm.$f2 $fm.$f3 $fm.$f4 $fm.$f5 $fm.$f6 $fm.$f7 &
	sleep 300
	killall acarsdec
	let f0=f0+25;let f1=f1+25;let f2=f2+25;let f3=f3+25; f4=f4+25;let f5=f5+25;let f6=f6+25;let f7=f7+25;
 done
 let fm=fm+1;
done
grep -e "(F:" scanlog | cut -c 8-14 | sort | uniq -c

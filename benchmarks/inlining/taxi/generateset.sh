#!/bin/bash

##2 : final probability
#2 : end size
#3 : size steps
#4: instances per size

if [[ $# -lt 4 ]]; then
	echo "Error: Script expects 4 parameters"
	exit 1;
fi

if [[ $1 -le 1 ]];
then
	echo "Cannot start with size 1; must be >= 2"
	exit 1
fi

for (( size=$1; size <= $2; size+=$3 ))
do
	drivers=$size
	customers=$size
	regions=$(expr $size / 2)
	regGoesTo=$(expr $size / 2)
	eCustPercent=50

	./generate.sh $drivers $customers $regions $regGoesTo $eCustPercent $4
done

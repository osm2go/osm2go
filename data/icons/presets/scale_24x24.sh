#!/bin/bash
#
# All presets images are required to be of 24x24 pixels size
# This script makes sure they are ...
#
for i in *.png ; do 
    SIZE=`identify $i | cut -d' ' -f3`;
    WIDTH=`echo $SIZE | cut -dx -f1`;
    HEIGHT=`echo $SIZE | cut -dx -f2`;
    if ( [ $HEIGHT != 24 ] && [ $WIDTH != 24 ] ); then
	echo "Scaling $i since $WIDTH x $HEIGHT completely wrong size";
	convert $i -scale 24x24 $i;
    elif ( [ $HEIGHT == 24 ] && [ $WIDTH -gt 24 ] ); then
	echo "Scaling $i since $WIDTH x $HEIGHT is too wide";
	convert $i -scale 24x24 $i;
    elif ( [ $WIDTH == 24 ] && [ $HEIGHT -gt 24 ] ); then
	echo "Scaling $i since $WIDTH x $HEIGHT is too high";
	convert $i -scale 24x24 $i;
    fi;
done


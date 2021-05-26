#! /bin/sh

echo "No parents for:"

grep backup VolumeList | awk '{ print $8 }'  | sed 's/^W//' | while read -r i
do
    #echo Processing $i
    if ! grep -q "I$i" /vice/vol/VolumeList ; then
	grep "W$i" /vice/vol/VolumeList | awk '{ print $2 " " $1}' | sed 's/^I//' | sed 's/ B/ /'
    fi
done

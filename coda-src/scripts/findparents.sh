#! /bin/bash


grep  backup VolumeList | awk '{ print $8 }'  | sed 's/^W//' > /tmp/parents

echo "No parents for:"
for i in `cat /tmp/parents` ; do
#    echo Processing $i
    grep -q I$i /vice/vol/VolumeList
    if [ x$? != x0 ]; then 
	grep W$i /vice/vol/VolumeList | awk '{ print $2 " " $1}' | sed 's/^I//' | sed 's/ B/ /'
    fi

done

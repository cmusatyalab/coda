#!/bin/sh
# BLURB gpl
# 
#                            Coda File System
#                               Release 5
# 
#           Copyright (c) 1987-1999 Carnegie Mellon University
#                   Additional copyrights listed below
# 
# This  code  is  distributed "AS IS" without warranty of any kind under
# the terms of the GNU General Public Licence Version 2, as shown in the
# file  LICENSE.  The  technical and financial  contributors to Coda are
# listed in the file CREDITS.
# 
#                         Additional copyrights
#                            none currently
# 
#*/

PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH 

TAPE=/dev/nst0
BACKUPDIR=/backup
DATE=`date +%d%b%Y`
BACKUPLOG=/vice/backuplogs/backuplog.$DATE
DUMPLOG=/vice/backuplogs/dumplog.$DATE
ADDR=backuplogs@coda.cs.cmu.edu
DUMPLIST=/vice/db/dumplist


# send Henry and the robot the dumplist
mail -s dumplist weblist@coda.cs.cmu.edu < $DUMPLIST
mail -s dumplist hmpierce@cs.cmu.edu < $DUMPLIST

# run backup
backup -t 135 /vice/db/dumplist /backup > $BACKUPLOG 2>&1
if [ $? != 0 ]; then
     echo "Coda backup program failed" | mail -s '** backup failure!! **' $ADDR
fi

#dump to tape
tape.pl --tape $TAPE --dir $BACKUPDIR --size 4000000 >> $BACKUPLOG 2>&1
if [ $? != 0 ]; then
     echo "Coda tape.pl program failed" | mail -s '** dump failure!! **' $ADDR
fi

# send log to the list
cat $BACKUPLOG |  mail -s "backup for $DATE" $ADDR
exit 0


















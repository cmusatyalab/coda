#!/bin/csh -f
#ifndef _BLURB_
#define _BLURB_
#/*
#
#            Coda: an Experimental Distributed File System
#                             Release 4.0
#
#          Copyright (c) 1987-1996 Carnegie Mellon University
#                         All Rights Reserved
#
#Permission  to  use, copy, modify and distribute this software and its
#documentation is hereby granted,  provided  that  both  the  copyright
#notice  and  this  permission  notice  appear  in  all  copies  of the
#software, derivative works or  modified  versions,  and  any  portions
#thereof, and that both notices appear in supporting documentation, and
#that credit is given to Carnegie Mellon University  in  all  documents
#and publicity pertaining to direct or indirect use of this code or its
#derivatives.
#
#CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
#SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
#FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
#DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
#RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
#ANY DERIVATIVE WORK.
#
#Carnegie  Mellon  encourages  users  of  this  software  to return any
#improvements or extensions that  they  make,  and  to  grant  Carnegie
#Mellon the rights to redistribute these changes without encumbrance.
#*/
#
#static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/scripts/restore.sh,v 1.1 1996/11/22 19:06:58 braam Exp $";
#endif /*_BLURB_*/


#ifndef _BLURB_
#define _BLURB_
#/*
#
#            Coda: an Experimental Distributed File System
#                             Release 3.1
#
#          Copyright (c) 1987-1995 Carnegie Mellon University
#                         All Rights Reserved
#
#Permission  to  use, copy, modify and distribute this software and its
#documentation is hereby granted,  provided  that  both  the  copyright
#notice  and  this  permission  notice  appear  in  all  copies  of the
#software, derivative works or  modified  versions,  and  any  portions
#thereof, and that both notices appear in supporting documentation, and
#that credit is given to Carnegie Mellon University  in  all  documents
#and publicity pertaining to direct or indirect use of this code or its
#derivatives.
#
#CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
#SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
#FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
#DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
#RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
#ANY DERIVATIVE WORK.
#
#Carnegie  Mellon  encourages  users  of  this  software  to return any
#improvements or extensions that  they  make,  and  to  grant  Carnegie
#Mellon the rights to redistribute these changes without encumbrance.
#*/
#
#static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/scripts/restore.sh,v 1.1 1996/11/22 19:06:58 braam Exp $";
#endif /*_BLURB_*/


# A rough attempt at automating the restore process. It doesn't work, was
# never completed. Just here to aid someone else's attempt to do this.

echo 'I NEVER WORKED; WILL YOU FIX ME?'
exit


# This program should restore a volume from a given date, using
# dumpfiles from tapes and prompting operators to mount tapes.
# Usage:	restore.sh <tape> <date> [<volid> <name>]
#		where date has the form DD-MMM-YYYY

if ( ($#argv < 4) || ($#argv > 4)) then
	echo "Usage: restore.sh <tape> <date> [<volId> <volName>]"
	exit -1
endif

set TAPE = $1

# DATE should have form DD-MMM-YYYY
set tmp = `echo $2 | awk -F- '{printf "%s %s %s", $1, $2, $3}' -`

if ( $#tmp != 3) then
    echo Ackkk, the date must have the format DD-MMM-YYYY
    exit -1
endif

set requestDay = $tmp[1]
set requestMonth = $tmp[2]
set requestYear = $tmp[3]

set volid = $3
set volname = $4

# Determine the appropriate full backup tape. We assume that the entries in
# /vice/db/TAPELABELS are in ascending order, and have the date as the second
# field in the label. 

set date = awk '{print $2}' $TAPEDB | awk -F- '$2 == "'$requestMonth'" {print $3}'`

set FullDumpDay = Uninit

foreach i ( $date )
    if ($i <= $requestDay) then
	set FullDumpDay = $i
	break;
    endif
end
set FullDate = ${requestDay}${requestMonth}${requestYear}
set FullLabel = `awk $TAPEDB`

# if $FullDumpDate == "Uninit", full is from the previous month, year...
# So scan through the TAPEDB until we find the closest match, and take the
# previous entry.

# Problem is that alphanumeric ordering isn't sufficient -- Jan > Aug
# Need to figure out how to find the date just before the one requested.
#
# if ($FullDumpDate == "Uninit") then
#    set tmp = ${requestYear}-${requestMonth}-0
#    set FullDumpDate = `awk 'BEGIN {i = "Uninit"} $2 > "'$tmp'" {print i} { i = $2} END {print $2} $TAPEDB`
# endif


# Now we have the correct full dump date ($FullDate) and label $(FullLabel)
# get the full dump file.


# Sanity check this -- I copied it from extract.sh.

pushd /tmp
# Get tape label from tape
mt -f ${TAPE} rewind
tar xvfp ${TAPE} ${TAPELABEL}

# sanity check the tape label
set stamp = `egrep "Stamp:" ${TAPELABEL}`
if ("$stamp" != "Stamp: Coda File System Volume Dump Version 1.0") then
	echo "Tapelabel does not have correct version stamp!"
	exit -1
endif

set date = `egrep "Date:" ${TAPELABEL}`
set label = $date[4]$date[3]$date[7]

if ($label != $dumpdate) then
	echo "Tape label date" $label "does not match request" $dumpdate
	echo "Format the date in the form DDMMMYYYY"
	exit -1
endif

# Find the entry for this volume in the label
set info = `awk '$3 ~ /'$volname'/ ' ${TAPELABEL}`

if ($#info != 3) then
	echo "Error: Couldn't find entry in" ${TAPELABEL} "for" $volname
	echo "Format the volid like <nonrepid> or <groupid>.<repid>"
	exit -1
endif

set index = $info[2]
set name = $info[3]

# Scan forward on the tape to the correct spot

mt -f ${TAPE} rewind
mt -f ${TAPE} fsf $index

# Extract the file
echo copying tape file to $filename
tar xvfp ${TAPE} $name

mv /tmp/$name $filename
rm /tmp/$name

# clean up state and exit
rm ${TAPELABEL}
mt -f ${TAPE} rewoffl
popd

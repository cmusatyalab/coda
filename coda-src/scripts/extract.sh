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
#static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/scripts/Attic/extract.sh,v 4.1 1997/01/08 21:50:48 rvb Exp $";
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
#static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/scripts/Attic/extract.sh,v 4.1 1997/01/08 21:50:48 rvb Exp $";
#endif /*_BLURB_*/


# This program should extract a dump file from a tape.
# Usage:	extract tape date volid filename 

if ($#argv != 4) then
	echo "Usage: extract tape date volid filename"
	exit -1
endif

set TAPE = $1
set dumpdate = $2
set volname = $3
set filename = $4
set TAPELABEL = TAPELABEL

# Get tape label from tape
pushd /tmp
mt -f ${TAPE} rewind
tar xvfp ${TAPE} ${TAPELABEL}
if ($status != 0) then
	echo "Tar of TAPELABEL didn't work\!"
	exit -1
endif

# sanity check the tape label
set stamp = `egrep "Volume Dump Version" ${TAPELABEL}`
if ("$stamp" != "Stamp: Coda File System Volume Dump Version 1.0") then
	echo "Tapelabel does not have correct version stamp\!"
	goto EXIT
endif

set date = `egrep "Date:" ${TAPELABEL}`
set label = $date[4]$date[3]$date[7]

if ($label != $dumpdate) then
	echo "Tape label date" $label "does not match request" $dumpdate
	echo "Format the date in the form DDMMMYYYY"
	goto EXIT
endif

# Find the entry for this volume in the label
# set info = `awk '$3 ~ /'$volname'/ ' ${TAPELABEL}`
set info = `grep -i $volname ${TAPELABEL}`

if ($#info != 3) then
	echo "Error: Couldn't find entry in" ${TAPELABEL} "for" $volname
	echo "Format the volid like <nonrepid> or <groupid>.<repid>"
	goto EXIT
endif

set index = $info[2]
set name = $info[3]

# Scan forward on the tape to the correct spot

mt -f ${TAPE} rewind
mt -f ${TAPE} fsf $index

# Extract the file
echo copying tape file to $filename
tar xvfp ${TAPE} $name

popd
cp -p /tmp/$name $filename
rm /tmp/$name
rm -r /tmp/*.CODA*.EDU
echo "Please run 'mt -f ${TAPE} rewoffl' if you are done with this tape."

# clean up state and exit
EXIT:
rm /tmp/${TAPELABEL}
#mt -f ${TAPE} rewoffl
mt -f ${TAPE} rewind


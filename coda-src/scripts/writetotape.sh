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
#static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/scripts/writetotape.sh,v 1.1.1.1 1996/11/22 19:06:55 rvb Exp";
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
#static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/scripts/writetotape.sh,v 1.1.1.1 1996/11/22 19:06:55 rvb Exp";
#endif /*_BLURB_*/

# args: backupdir tape notify

if ($#argv < 2 || $#argv > 3) then
    echo  "Usage: writetotape <backupdir> <tapedrive> [<skip-notification>]"
    exit 1
endif

# Setup the appropriate environment variables
set backupdir = $1
set TAPE = $2
if ( $#argv == 3 ) set DO_NOT_NOTIFY = 1

set ADMINISTRATORS = "ern@cs raiff@cs dcs@cs"
set LABELS_DB = /vice/db/TAPELABELS
set NOTIFY_CUTOFF = 5
set BackupMachine = debussy.coda
set SLEEP_INTERVAL = 200
set MSG = "/tmp/message"
set LOGFILE = $backupdir/logfile
set NOTIFY = '/usr/misc/bin/zwrite -v -d -n -c backup -i coda '

# First, unmount whatever tape may be in the tape drive. This helps to
# avoid problems if someone left last-weeks full in the drive. This is
# not a perfect solution, since there maybe someone using the tape at the time.
# -- DCS 1/11/93
mt -f ${TAPE} rewoffl

# Now loop until we get the correct tape. Perhaps we should quit after a point?

@ count = 0
TOPOFTAPELOOP:
# First, if we've already done this a number of times, notify someone important
# that there is a problem here.

if ($count == $NOTIFY_CUTOFF) then
    if (! $?DO_NOT_NOTIFY ) then
	foreach i ( $ADMINISTRATORS )
	    cat <<EOF | mail -s "Coda Exception Message" $i
Attention: Coda backup waiting on tape drive $TAPE.
I've notified the operator $count times, is something wrong?
EOF
    	end
    endif
    @ count = 0
endif

# Remove the old message if it exists.
if ( -e $MSG ) rm -f $MSG

# Ask the operator to mount a tape. First, determine which tape to ask for.
set NOWTIME = `whenis -f '%3Month %day %0hour:%min:%sec' now`

if ( -e ${backupdir}/TAPELABEL ) then # someone already started a writetotape
    set TAPELABEL = `awk '/Label:/ {for (i = 2; i <= NF; i++) printf "%s ", $i}' ${backupdir}/TAPELABEL`

   echo $NOWTIME Please mount $TAPE with tape labeled '"'$TAPELABEL'"' >$MSG
else if ( -e ${backupdir}/FULLDUMP ) then
    @ label = `awk 'BEGIN {s = 0} {s++} END { print s }' ${LABELS_DB}`
    set tmp = `date`
    set backupdate = $tmp[6]-$tmp[2]-$tmp[3]

    set TAPELABEL  = "CODA.F.$label $backupdate"

    echo $NOWTIME Please mount $TAPE with NEW tape '"'$TAPELABEL'"' >$MSG
else	# Incrementals are only labeled with the day.
    set tmp = `date`
    set TAPELABEL = "CODA.I.$tmp[1]"

    echo $NOWTIME Please mount $TAPE with tape labeled '"'$TAPELABEL'"' >$MSG
endif



# Request that the a tape be mounted, use the message if it exists.
if (! -e $MSG ) then
    echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
    echo "ACKK. No message file $MSG to send to operators\!" >>${LOGFILE}
endif

if (! $?DO_NOT_NOTIFY ) then
    cat $MSG | $NOTIFY >& /dev/console
    cat $MSG >>$LOGFILE
endif
rm -f $MSG

@ count ++

# Wait for the tape to be mounted.
sleep $SLEEP_INTERVAL

# Check to see if the tape is mounted yet.
mt -f $TAPE rewind

# If an error occurs, report it unless a previous error exists.
if ($status != 0) then
    echo "Coda Failure(`whenis -f '%3Month %day %0hour:%min:%sec' now`)" >$MSG
    echo "Help\! Tape access failed on debussy drive $TAPE." >>$MSG
    mt -f $TAPE status >>& $MSG
    echo "Check the console for more details." >>$MSG
    echo Please mount the tape '"'$TAPELABEL'"' on $BackupMachine. >>$MSG

    if (! $?DO_NOT_NOTIFY ) then
        cat $MSG | $NOTIFY >& /dev/console
        cat $MSG >>${LOGFILE}
    endif
    rm -f $MSG

    goto TOPOFTAPELOOP
endif

# At this point the tape is mounted and rewound.
# Now make sure that the tape in the drive is either empty or today's.
# status != 0 might means the tape is unused or an error happened. Let later
# writes pick up the error if there is one. 
cd /tmp
set TAPELABEL_FILE = ${backupdir}/TAPELABEL

rm -f TAPELABEL ${MSG}
tar xf $TAPE TAPELABEL 

if ($status == 0) then
    if (-r $TAPELABEL_FILE) then
	set mydate = `awk '/Date:/ {print $2}' ${TAPELABEL_FILE}`
    else
	set tmp = `date`
	set mydate = $tmp[1]
    endif

    if (!(-r TAPELABEL)) then
	mt -f $TAPE rewoffl 
	set TIMENOW = `whenis -f '%3Month %day %0hour:%min:%sec' now`

	echo "$TIMENOW Help\! Non-Coda Tape in $TAPE\!" >$MSG

	if (! $?DO_NOT_NOTIFY ) then
	    cat $MSG | $NOTIFY >& /dev/console
	    cat $MSG >>${LOGFILE}
	endif
	rm -f $MSG

	goto TOPOFTAPELOOP
    endif

    # sanity check the tape label from the tape.
    set stamp = `egrep "Volume Dump Version" TAPELABEL`
    if ("$stamp" != "Stamp: Coda File System Volume Dump Version 1.0") then
	mt -f $TAPE rewoffl 
	set TIMENOW = `whenis -f '%3Month %day %0hour:%min:%sec' now`

	echo "$TIMENOW Help\! Tape in $TAPE has wrong version\!" >$MSG

	if (! $?DO_NOT_NOTIFY ) then
	    cat $MSG | $NOTIFY >& /dev/console
	    cat $MSG >>${LOGFILE}
	endif
	rm -f $MSG

	goto TOPOFTAPELOOP

    endif

    set tapedate = `awk '/Date:/ {print $2}' TAPELABEL`
    if ($mydate != $tapedate) then
	mt -f $TAPE rewoffl 
	set TIMENOW = `whenis -f '%3Month %day %0hour:%min:%sec' now`

	echo "$TIMENOW Help\! Tape in $TAPE has label for wrong day\!" >$MSG

	if (! $?DO_NOT_NOTIFY ) then
	    cat $MSG | $NOTIFY >& /dev/console
	    cat $MSG >>${LOGFILE}
	endif
	rm -f $MSG

	goto TOPOFTAPELOOP
    endif
endif
	
# Create a file to hold the tape label. This involves creating an index of
# all the files to be written to tape. The index will be stored in the
# tape label and will look like:
# 	size of file	offset of tar containing this file	filename
#
# The first awk selects the size and the filename from ls, the second awk
# breaks the files into groups of at least MINTARSIZE bytes (unless there are
# no more files) and outputs a line for each file in the above format.
# Skip to tar section if tape index already exists, this provides idempotency.

@ MINTARSIZE = 512000
set INDEX = ${backupdir}/index	

if (-r $INDEX) then
    if (-r $TAPELABEL_FILE) then
	goto WRITETOTAPE
    endif

    rm $INDEX
else
    # When writing stuff after the day it was created, you can create
    # a null TAPELABEL_FILE with the appropriate date so it passes the
    # above test. If the INDEX isn't present, it implies this was done
    # and that the TAPELABEL_FILE is garbage, so it should be removed here.
    if (-r $TAPELABEL_FILE) then
	rm $TAPELABEL_FILE
    endif
   
endif


cd ${backupdir} ; find . ! -type d -exec ls -ld {} \; | 	\
	awk '{printf "%10d %s\n", $4, $8}' | sort | 		\
	awk 'BEGIN {j = 1} i < '$MINTARSIZE' { i += $1; printf "%10d %5d %s\n", $1, j, $2} i >= '$MINTARSIZE' {i = 0; j++ }' >${INDEX}

echo "Stamp: Coda File System Volume Dump Version 1.0" >${TAPELABEL_FILE}
echo "Date: " `date` >>${TAPELABEL_FILE}
echo "Label: " $TAPELABEL >>${TAPELABEL_FILE}
cat ${INDEX} >>${TAPELABEL_FILE}

# Tar out the label file, tar out all the dump files and dbs, and then tar the 
# label file again. As a sanity check, read in two tape label files and 
# make sure they match, this will detect END-OF-TAPE or media failures
WRITETOTAPE:

set tryAgain = 1

TRYAGAIN:
echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
echo "Writing to tape" >> ${LOGFILE}
mt -f $TAPE rewind

tar chfS ${TAPE} ${backupdir}/ ${TAPELABEL_FILE} >>& ${LOGFILE}
if ($status != 0) then
    echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
    echo "AN ERROR OCCURRED WRITING TO TAPE" >> ${LOGFILE}
    if ($tryAgain == 1) then
	set tryAgain = 0
	sleep 300
	goto TRYAGAIN
    endif

    echo "EXITING" >>${LOGFILE}
    exit 1
endif

cd $backupdir
@ i = 1
set files = `awk '$2 == '$i' {printf "%s ", $3}' ${INDEX}`
while ($#files > 0)
	tar chf ${TAPE} $files >>& ${LOGFILE}
	if ($status != 0) then
	    echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
	    echo "AN ERROR OCCURRED WRITING TO TAPE, EXITING..." >> ${LOGFILE}
	    echo "Working on the "$i" tapefile, files = "$files >>${LOGFILE}
	    exit 1
	endif

	@ i ++
	set files = `awk '$2 == '$i' {printf "%s ", $3}' ${INDEX}`
end
	
tar chfS ${TAPE} ${backupdir}/ ${TAPELABEL_FILE} >>& ${LOGFILE}
if ($status != 0) then
    echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
    echo "AN ERROR OCCURRED WRITING TO TAPE" >> ${LOGFILE}
    echo "Failure writing the last ${TAPELABEL_FILE} to tape." >> ${LOGFILE}
    exit 1
endif

exit 0


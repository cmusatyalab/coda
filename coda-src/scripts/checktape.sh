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
#static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/scripts/checktape.sh,v 1.1.1.1 1996/11/22 19:06:43 rvb Exp";
#endif /*_BLURB_*/


# Usage: checktape.sh <backupdir> <tapedrive>

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
#static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/scripts/checktape.sh,v 1.1.1.1 1996/11/22 19:06:43 rvb Exp";
#endif /*_BLURB_*/



if ($#argv != 2) then
    echo  "Usage: checktape.sh <backupdir> <tapedrive>"
    exit 1
endif

set backupdir = $1
set TAPE = $2
set INDEX = $backupdir/index
set TAPELABEL = $backupdir/TAPELABEL
set LOGFILE = $backupdir/logfile
# special logfile to hold output of tar in case check fails.
set TARLOG = $backupdir/tarlog

mv $TARLOG ${TARLOG}.old
touch $TARLOG


echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
echo "Checking tape" >> ${LOGFILE}

cd /tmp
mt -f $TAPE rewind
tar xfp $TAPE TAPELABEL >>& ${TARLOG}

if ($status != 0) then
   echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
   echo "Tar of first TAPELABEL failed\!" >>${LOGFILE}
   echo "Tar of first TAPELABEL failed\!" >>${TARLOG}
   exit 1
endif

diff TAPELABEL ${TAPELABEL}
if ($status != 0) then
   echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
   echo "First TAPELABEL did not match\!"  >>${LOGFILE}
   exit 1
endif

@ i = 1
@ bogusity = 0

set files = `awk '$2 == '$i' {printf "%s ", $3}' ${INDEX}`
while ($#files > 0)
	mt -f $TAPE fsf 1
	set tfiles = `tar tf ${TAPE} |& tee -a ${TARLOG}`

	if ($status != 0) then
	    echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
	    echo "Tar $i failed, skipping..." >>${LOGFILE}
	    echo "Tar $i failed, skipping..." >>${TARLOG}
	    @ bogusity ++
	else if ($#files != $#tfiles) then
	    echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
	    echo "Number of files in tar file do not match for " $i >>${LOGFILE}
	    echo "Number of files in tar file do not match for " $i >>${TARLOG}
	    @ bogusity ++
	else

	    @ j = 0
	    while ($j < $#files)
		if ("$files[$j]" != "$tfiles[$j]") then
		    echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
		    echo "Tar file is wrong for tar number " $i >>${LOGFILE}
		    echo "Tar file is wrong for tar number " $i >>${TARLOG}
		    @ bogusity ++
		    break
		endif

		@ j ++
	    end
	endif

	# Prepare for next iteration
	@ i++
	set files = `awk '$2 == '$i' {printf "%s ", $3}' ${INDEX}`
end

# Skip over the remainder of the last tape file and check for the last label.
mt -f $TAPE fsf 1
tar xfp $TAPE TAPELABEL >>& ${TARLOG}
if ($status != 0) then
   echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
   echo "Tar of first TAPELABEL failed\!" >>${LOGFILE}
   echo "Tar of first TAPELABEL failed\!" >>${TARLOG}
   exit 1
endif

diff TAPELABEL ${TAPELABEL}
if ($status != 0) then
   echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >>${LOGFILE}
   echo "Final TAPELABEL did not match\!" >>${LOGFILE}
   echo "Final TAPELABEL did not match\!" >>${TARLOG}
   exit 1
endif

if ($bogusity > 0) then
   exit 1
endif

# Report successful check
echo "Tape validated at" `date` >>${LOGFILE}
exit 0

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
#static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/scripts/backup.sh,v 1.1.1.1 1996/11/22 19:06:40 rvb Exp";
#endif /*_BLURB_*/


# argv[1] == "directory in which to run backups."

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
#static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/scripts/backup.sh,v 1.1.1.1 1996/11/22 19:06:40 rvb Exp";
#endif /*_BLURB_*/

if ( ($#argv < 1) || (($#argv > 1) && ($#argv < 3))) then
    echo "Usage: backup.sh <backupdir> [<dumplist-file> <tape-drive>]"
    exit -1
endif

set backupdir = $1

if ($#argv > 1) then
   set DUMPLIST = $2
   set TAPE = $3
else
   set TAPE = /dev/nrmt0
   set DUMPLIST = /vice/db/dumplist
endif

if (! -d $backupdir) then
    echo "Usage: backup.sh <backupdir>"
    exit -1
endif

set before=`date`
set dir=$before[3]$before[2]$before[6]
set LOGFILE = ${backupdir}/${dir}/logfile

if (-d ${backupdir}/$dir) then
    echo $dir "already exists\! (exiting)" > ${backupdir}/BACKUPERROR
    exit -1
endif

mkdir ${backupdir}/$dir
touch ${LOGFILE}

set TAPELABEL = $backupdir/$dir/TAPELABEL

# Turn debugging on for the name server.
kill -USR1 `cat /etc/named.pid`
kill -USR1 `cat /etc/named.pid`
kill -USR1 `cat /etc/named.pid`
kill -USR1 `cat /etc/named.pid`
kill -USR1 `cat /etc/named.pid`
kill -USR1 `cat /etc/named.pid`

#
# Backup the appropriate volumes, using backup utility.
echo "backup" -t 135 ${DUMPLIST} ${backupdir}/${dir} >>$LOGFILE
date >>$LOGFILE
${backupdir}/bin/backup -t 135 $DUMPLIST ${backupdir}/${dir} >>& $LOGFILE
date >>${LOGFILE}


# Turn off debugging for name server
kill -USR2 `cat /etc/named.pid`

#
# Save copies of all important databases 
echo >>${LOGFILE}
echo -n `whenis -f '%3Month %day %0hour:%min:%sec' now` ": " >> ${LOGFILE}

echo "Copying databases to" ${dir} >> ${LOGFILE}

foreach i (VLDB VRDB servers hosts VSGDB TAPELABELS dumplist)
    cp -p /vice/db/${i} ${backupdir}/${dir}
end

#
#
# Write everything to tape
if ( "$TAPE" == "notape" ) then
    set DELETEFILES = 1
    goto CLEANUP
endif

${backupdir}/bin/writetotape.sh $backupdir/$dir $TAPE 
set mystatus = $status

if ($mystatus != 0) then
    echo " " >>${LOGFILE}
    echo "Writetotape failed\!" $mystatus >>${LOGFILE}
    goto POST
endif

# Check that write to tape worked correctly.
${backupdir}/bin/checktape.sh $backupdir/$dir $TAPE
set mystatus = $status

if ($mystatus != 0) then
    echo " " >>${LOGFILE}
    echo "CheckTape failed\!" $mystatus >>${LOGFILE}
    set DELETEFILES = 0
else
    set DELETEFILES = 1

    # If a fulldump was done, add today's physical tape label to the label db.
    if (-e $backupdir/$dir/FULLDUMP) then
	echo `awk '/Label*/ {print $2 " " $3}' $TAPELABEL` >> /vice/db/TAPELABELS
    endif
endif

echo "DEBUG: I will delete the files if $DELETEFILES == 1"
#
# Produce mail message
POST:

# Following awk commands looks like this with added white space
# awk ' /Successfully*/,/FAILED*/ { i += 1 }
#	/FAILED/,/NOT/ 		  { j += 1 }
#	/NOT/,/Copying/ 	  { k += 1 }
#	END { i-=3; j-=3; k-=3; printf "%d %d %d\n", i, j, k }' ${LOGFILE}

set good = ` awk ' BEGIN { i = 0 } /Successfully*/,/partially*/ { i += 1 } END { i -= 3 ; print i}' ${LOGFILE} `

set okay = ` awk ' BEGIN { i = 0 } /partially*/,/FAILED*/ { i += 1 } END { i -= 3 ; print i}' ${LOGFILE} `

set bad = ` awk ' BEGIN { i = 0 } /FAILED*/,/NOT*/ { i += 1 } END { i -= 3 ; print i}' ${LOGFILE} `

set notDone = ` awk ' BEGIN { i = 0 } /NOT*/,/Histogram*/ { i += 1 } END { i -= 3 ; print i}' ${LOGFILE} `

set nfiles = `egrep 'CODA.CS' $backupdir/$dir/index | wc -l`
set ntarfiles = `awk '{i = $2} END {print  i}' $backupdir/$dir/index`

set OUTFILE = ${backupdir}/${dir}/OUTFILE
echo " " >${OUTFILE}
echo "------------------- Backup Summary -------------------" >>${OUTFILE}
echo "Begin CODA Backup at:    " $before >>${OUTFILE}
echo "Volumes Successfully backed up:" $good >>${OUTFILE}
echo "Volumes Partially backed up   :" $okay >>${OUTFILE}
echo "Volumes that FAILED backup    :" $bad >>${OUTFILE}
echo "Volumes NOT backed up         :" $notDone >>${OUTFILE}
echo "Wrote " $nfiles " DumpFiles in " $ntarfiles "Tape files" >>${OUTFILE}
echo "Finished CODA Backup at: " `date` >>${OUTFILE}
echo "------------------- Backup Summary -------------------" >>${OUTFILE}
echo " " >>${OUTFILE}
echo "Tape label:" >>${OUTFILE}
cat ${TAPELABEL} >>${OUTFILE}
echo " " >>${OUTFILE}
echo " " >>${OUTFILE}
echo " " >>${OUTFILE}
cat ${LOGFILE} >>${OUTFILE}

mail -s "backup for $dir" raiff@cs dcs@cs <${OUTFILE}

post - -subject "backup for ${dir}" cmu.cs.proj.coda.backuplogs <${OUTFILE}

mt -f $TAPE rewoffl

#
# Cleanup all the files we created. Since the spooling area is limited (1.5Meg)
# it is essential that we cleanup everything that's made it to tape.
# Note, no use writing to LOGFILE since it is about to be deleted.
CLEANUP:
if ($DELETEFILES == 1) then
    rm -rf ${backupdir}/$dir
    # Cleanup the "spooling" areas (the ones mentioned in $DUMPLIST)
    set cleanupDirs = `awk '$1 ~ /\// {print}' $DUMPLIST`
    foreach i ($cleanupDirs)
	rm -rf ${i}/$dir
	# Debugging statment...
	echo "deleting ${i}/$dir" | /usr/misc/bin/zwrite -v -d -n dcs raiff
    end
endif
	

#!/bin/sh
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
#static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/scripts/backup.sh,v 4.3.2.2 1997/12/06 02:02:15 braam Exp $";
#endif /*_BLURB_*/

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
/vice/bin/backup -t 135 /vice/db/dumplist /backup > $BACKUPLOG 2>&1
if [ $? != 0 ]; then
      echo "Coda backup program failed" | mail -s "** backup failure!! **" $ADDR
fi

#dump to tape
/vice/bin/tape.pl --tape $TAPE --dir $BACKUPDIR --size 4000000 >> $BACKUPLOG 2>&1
if [ $? != 0 ]; then
      echo "Coda tape.pl program failed" | mail -s "** dump failure!! **" $ADDR
fi

# send log to the list
cat $BACKUPLOG |  mail -s "backup for $DATE" $ADDR
exit 0


















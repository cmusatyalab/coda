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
#static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/scripts/backup.sh,v 4.3 97/11/13 18:24:23 braam Exp $";
#endif /*_BLURB_*/

TAPE=/dev/nrst0
BACKUPDIR=/backup
DATE=`date +%d%b%Y`
BACKUPLOG=/vice/backuplogs/backuplog.$DATE
DUMPLOG=/vice/backuplogs/dumplog.$DATE
ADDR=backuplogs@coda.cs.cmu.edu

/vice/bin/backup -t 135 /vice/db/dumplist /backup > $BACKUPLOG 2>&1
if [ $? != 0 ]; then
      echo "Coda backup program failed" | mail -s "** backup failure!! **" $ADDR
fi

/vice/bin/tape.pl --tape /dev/nrst0 --dir /backup --size 4000000 >> $BACKUPLOG 2>&1
if [ $? != 0 ]; then
      echo "Coda backup program failed" | mail -s "** dump failure!! **" $ADDR
fi

cat $BACKUPLOG |  mail -s "backup for $dir" $ADDR
exit 0


















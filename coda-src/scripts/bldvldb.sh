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
#static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/scripts/bldvldb.sh,v 4.9 1998/05/27 20:29:14 braam Exp $";
#endif /*_BLURB_*/

THISHOST=`hostname | tr A-Z a-z`
REMOTE=/vice/vol/remote

PATH=/sbin:/usr/sbin:$PATH
export PATH
cd /vice/vol/remote
SERVERS=""

# Get the locally generated /vice/vol/VolumeList from 
#  - all servers (if argc = 1)
#  - the listed servers (if argc > 1) 

if [ $#  = 0 ]; then
	SERVERS=`awk '{ print $1 }' /vice/db/servers`
else
    for i in $* ; do
        NEWSERVER=`awk '{ print $1 }' /vice/db/servers | grep $i `
	SERVERS="$NEWSERVER $SERVERS"
    done
fi

echo "Fetching /vice/vol/Volumelist from $SERVERS"

for server in $SERVERS
do 

    updatefetch -h ${server} -r /vice/vol/VolumeList -l \
	${REMOTE}/${server}.list.new

    if [ -r ${REMOTE}/${server}.list.new ]; then
        mv ${REMOTE}/${server}.list.new ${REMOTE}/${server}.list
    else 
        echo "Trouble getting new list for server $server"
    fi

done

# Make on big list called composite
cd ${REMOTE}; cat *.list> ../BigVolumeList

# Make a new vldb from the list
volutil makevldb  /vice/vol/BigVolumeList

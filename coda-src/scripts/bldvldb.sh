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
#static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/scripts/Attic/bldvldb.sh,v 4.5 1997/10/15 15:50:20 braam Exp $";
#endif /*_BLURB_*/


KLIST=/usr/misc/bin/klist
KSRVTGT=/usr/local/bin/ksrvtgt
THISHOST=`hostname | tr A-Z a-z`
REMOTE=/vice/vol/remote

PATH=$PATH:/vice/bin
export PATH
cd /vice/vol/remote


# Get the locally generated /vice/vol/VolumeList from 
#  - all servers (if argc = 1)
#  - the listed servers (if argc > 1) 

if [ $#  = 0 ]; then
	SERVERS=`awk '{ print $2 }' /vice/db/hosts`
else
	SERVERS=$*
fi

for server in $SERVERS
do 
    # XXXX change this to use updfetch
    # Get a ticket-granting-ticket (which is good for 5 mins only) if needed
    $KLIST -t
    if [ $? != 0 ]; then
        $KSRVTGT rcmd $THISHOST
    fi

    echo ${server}

    # If your site does not have kerberos, use rcp instead.
    # For some reason I can't get a connection fom Mach to NetBSD machines
    # if I try to encrypt the data, so use -X for now.
    # Get rid of it once Mach machines go away or if you are at a site where
    # it's not needed.
    
    krcp -X ${server}:/vice/vol/VolumeList ${REMOTE}/${server}.list.new

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

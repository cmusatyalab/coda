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
#static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/scripts/Attic/bldvldb.sh,v 4.2 1997/06/18 12:24:18 raiff Exp $";
#endif /*_BLURB_*/


cd /vice/vol/remote

# Get the volume lists from the servers

# Get a temporary token.
/usr/local/bin/ksrvtgt rcmd `hostname | tr A-Z a-z`

foreach server (`awk '{ print $2 }' /vice/db/hosts`)
    echo ${server}

    # If your sight dies not have kerberos, use rcp instead.
    # For some reason I can't get a connection fom Mach to NetBSD machines
    # if I try to encrypt the data, so use -X for now.
    # Get rid of it once Mach machines go away or if you are at a site wher
    # its not needed.
    
    krcp -X ${server}:/vice/vol/VolumeList /vice/vol/remote/${server}.list.new

    #krcp ${server}:/vice/vol/VolumeList /vice/vol/remote/${server}.list.new

    if (! -r /vice/vol/remote/${server}.list.new) then
        echo "Trouble getting new list for server $server"
    endif

    if (-r /vice/vol/remote/${server}.list.new) then
        mv /vice/vol/remote/${server}.list.new /vice/vol/remote/${server}.list
    endif

end

# Make on big list called composite
cd /vice/vol/remote; cat *.list> ../BigVolumeList

# Make a new vldb from the list
/vice/bin/volutil makevldb  /vice/vol/BigVolumeList

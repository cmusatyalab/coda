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
#static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/scripts/installbetasrv.csh,v 4.1 1997/01/08 21:50:50 rvb Exp $";
#endif /*_BLURB_*/



if ($#argv != 1) then
    echo "Usage: $0  <Beta-base directory>"
    exit 1
endif 

set BaseDir=$1
set SRVPIDFILE = "/vice/srv/pid"

set SCMBINS=(purgevol purgevol_rep createvol createvol_rep bldvldb.sh)

set SRVBINS=(auth2 authmon printvrdb srv updateclnt updatemon updatesrv volutil)

set SPCLBINS=( rds_test rdsinit rvmutl)

#make sure server is officially down 
if (-f SRVPIDFILE) then
    echo "A server is running"
    echo "Make sure it is dead and remove the file $SRVPIDFILE"
    exit 1
endif

echo Getting binaries from  $BaseDir
cd /vice

echo "removing really old binaries in bin.old"
rm -rf /vice/bin.old

echo "moving existing binaries to bin.old"
mv bin bin.old
mkdir bin

echo -n "copying bin files: "
foreach f ($SRVBINS ) 
    echo -n $f " "
    copy $BaseDir/bin/$f bin
end
echo ""

if (`cat /.hostname` == `cat /.scm`) then
    echo -n "copying scm files: "
    foreach f ($SCMBINS)
        echo -n $f " "
        copy $BaseDir/bin/$f bin
    end
endif
echo ""

echo -n "copying special binaries(RVM): "
foreach f ($SPCLBINS ) 
    echo -n $f " "
    copy $BaseDir/bin/$f bin
end

echo ""

echo "copying old startserver script"
/bin/cp /vice/bin.old/startserver /vice/bin/startserver

echo "copying old restartserver script"
/bin/cp /vice/bin.old/restartserver /vice/bin

echo "copying old startfromboot script"
/bin/cp /vice/bin.old/startfromboot /vice/bin


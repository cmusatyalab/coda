#!/bin/csh -f -b
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
#static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/

#

set prog=advice_install.sh
set XFONTS=/usr/misc/.X11/lib/fonts/75dpi
set CODAETC=/usr/coda/etc
set TCLBIN=/usr/coda/etc
set TCLDIR=/usr/coda/tcl
set TCLLIB={$TCLDIR}/lib
set TCLFONT={$TCLDIR}/fonts
set SCRIPTS="discomiss hoardlist misslist pseudomiss reconnection reintegration_pending stoplight stoplight_statechange user_initiated weakmiss"

if ($#argv != 1) goto usage

switch ("$argv[1]")
    case "mre":
        set BINDIR=/coda/usr/mre/src/OBJS/@sys/advice
	set SCRIPTDIR=/coda/usr/mre/src/advice
        breaksw
    case "alpha":
        set BINDIR=/coda/project/coda/alpha/bin
	set SCRIPTDIR=/coda/project/coda/alpha/bin
        breaksw
    default:
        echo "${prog}: unknown switch ($argv[1])"
        goto usage
        breaksw
endsw


# Assume that CODAETC AND TCLBIN both exist!

# Copy advice server into the local Coda binary area.
test -f $BINDIR/advice_srv; if ($status == 0) copy $BINDIR/advice_srv $CODAETC
foreach f ($SCRIPTS) 
  test -f $SCRIPTDIR/{$f}; 
  if ($status == 0) copy $SCRIPTDIR/{$f} $CODAETC
  chmod 0755 $CODAETC/{$f}
end

# Copy tcl files into the local tcl library area.
copy /usr/misc/.tcl/bin/wish $CODAETC
test -d $TCLDIR; if ($status == 1) mkdir $TCLDIR
test -d $TCLLIB; if ($status == 1) mkdir $TCLLIB
test -d {$TCLLIB/tcl}; if ($status == 1) mkdir {$TCLLIB}/tcl
copy /usr/misc/.tcl/lib/tcl/*.tcl $TCLLIB/tcl
copy /usr/misc/.tcl/lib/tcl/tclIndex $TCLLIB/tcl
test -d {$TCLLIB/tk}; if ($status == 1) mkdir {$TCLLIB}/tk
copy /usr/misc/.tcl/lib/tk/*.tcl $TCLLIB/tk
copy /usr/misc/.tcl/lib/tk/tclIndex $TCLLIB/tk

# Copy the necessary font file into the tcl
test -d $TCLFONT; if ($status == 1) mkdir $TCLFONT
copy $XFONTS/courR12.* $TCLFONT
test -f $TCLFONT/fonts.dir; if ($status == 1) mkfontdir $TCLFONT
echo "You must add $TCLFONT to your XFILESEARCHPATH environment variable."
echo "You may need to add an 'xset +fp /usr/coda/tcl/fonts' to a dotfile."

# Assume the user has X11 and a window manager hoarded correctly!

# Make sure tcl link exists
echo "Make sure that /usr/misc/.tcl points into /coda"


# Exit normally
exit 0

# Exit with Usage statement
usage:
echo "Usage: ${prog} [mre] [alpha]"
exit 0


# #ifndef _BLURB_
# #define _BLURB_
# /*

# BLURB lgpl
# 
#                            Coda File System
#                               Release 5
# 
#           Copyright (c) 1987-1999 Carnegie Mellon University
#                   Additional copyrights listed below
# 
# This  code  is  distributed "AS IS" without warranty of any kind under
# the  terms of the  GNU  Library General Public Licence  Version 2,  as
# shown in the file LICENSE. The technical and financial contributors to
# Coda are listed in the file CREDITS.
# 
#                         Additional copyrights
#                            none currently
# 
#*/
#           Copyright (c) 1987-1996 Carnegie Mellon University
#                          All Rights Reserved

# Permission  to  use, copy, modify and distribute this software and its
# documentation is hereby granted,  provided  that  both  the  copyright
# notice  and  this  permission  notice  appear  in  all  copies  of the
# software, derivative works or  modified  versions,  and  any  portions
# thereof, and that both notices appear in supporting documentation, and
# that credit is given to Carnegie Mellon University  in  all  documents
# and publicity pertaining to direct or indirect use of this code or its
# derivatives.

# CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
# SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
# FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
# DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
# RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
# ANY DERIVATIVE WORK.

# Carnegie  Mellon  encourages  users  of  this  software  to return any
# improvements or extensions that  they  make,  and  to  grant  Carnegie
# Mellon the rights to redistribute these changes without encumbrance.
# */


# Coda error handling
# This perl script converts the standard input into a couple
# of header files as follows:

# standard input contains:
# Coda symbolic error constant, value, Unix symbolic constant, C-comment

# 
open( ERRH, ">errorsdefs.h");
# switchc2s is the Coda to System switch statement used in ntoherror
open( ERRC2S, ">switchc2s.h");
# switchs2c is the System to Coda switch statement used in htonerror
open( ERRS2C, ">switchs2c.h");

while (<STDIN>){
     chop;
     ($cname, $no, $sname, $text ) = split(/,/);
     $text =~ s/\/\*//;
      $text =~ s/\*\///;
 #    print " $cname, $no , $sname  $text\n";
     $cpy = "      ctxt = \"$text ($cname)\";\n";
     printf ERRH "\#define $cname $no\n";
     if ( ! ($sname =~ /VREADONLY|EWOULDBLOCK|VONLINE|VNOSERVICE|VDISKFULL|VOVERQUOTA|VBUSY/ )){
     print ERRS2C "\#ifdef $sname\n    case $sname:\n      cval = $cname;\n$cpy      break;\n\#endif\n";
 }
     if ( ! ($cname =~ /VREADONLY|CEWOULDBLOCK|VDISKFULL/ )){
	 print ERRC2S "\#ifdef $cname\n    case $cname:\n      sval = $sname;\n$cpy            break;\n\#endif\n";
     }
 }


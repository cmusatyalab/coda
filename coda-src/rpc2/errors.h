#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rpc2/Attic/errors.h,v 4.1 1997/09/23 15:13:27 braam Exp $";
#endif /*_BLURB_*/



/* This is a revamp of the Coda error handling. Aims and constraints:
   - errors returned by RPC's are platform independent. Ports to other
     platforms have made it mandatory to instate this without delay. 
   - there is room to accomodate Coda specific errors (like VSALVAGE)
     as well as system errors.
   - we want quick and easy translation of errors:
     a) from Coda to system errors (typically for clients)
     b) from system to Coda errors (typically for servers)
     c) we want "perror" like functions to save time in coding
   - Coda servers should only return errors which all of the client
     platforms can handle. 
   - If errors arrive on certain clients/servers and are not recognized
     a serious log message is printed and a default error is code
     is returned. 
*/

#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>
#include <sys/errno.h>
#include <errorsdefs.h>

/* exported definitions */
char *cerror(int cerrno); /* like perror */

#endif

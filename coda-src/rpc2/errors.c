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
#ifndef __MACH__
#include <unistd.h>
#include <stdlib.h>
#else
#include <libc.h>
#endif

#include <errors.h>
#include <rpc2.h>

/* translate RPC2 error to System Error */
int RPC2_R2SError(int cerr)
{
    int sval;
    char *ctxt;
    
    if ( cerr < 0 ) 
	return cerr;

    switch ( cerr ) {
    case 0:
	sval = 0;
	break;

        #include <switchc2s.h>

    default:
	sval=4711;
    }
    if ( sval == 4711 ) {
      fprintf(stderr, "Unknown R2S error translation sys %d, net %d\n", sval, cerr);
    }
    return sval;
}

/* translate System error to RPC2 error */
int RPC2_S2RError(int serr)
{
    int cval;
    char *ctxt;

    if ( serr < 0 )
	return serr;

    switch ( serr ) {
    case 0:
	cval = 0 ;
	break; 

        #include <switchs2c.h>
	
    default:
	cval = 4711;
	ctxt =  "Unknown error!";
    }
    if ( cval == 4711 ) {
      fprintf(stderr, "Unknown S2R error translation sys %d, net %d\n", serr, cval);
    }
    return cval;
}


char *cerror(int cerr)
{
    char *ctxt;
    int sval;

    switch ( cerr ) {
        #include <switchc2s.h>
	
    default:
	ctxt= "Unknown error!";
    }
    return ctxt;
}

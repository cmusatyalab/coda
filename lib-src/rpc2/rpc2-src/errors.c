/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <unistd.h>
#include <stdlib.h>
#include <rpc2/errors.h>
#include <rpc2/rpc2.h>

/* translate RPC2 error to System Error */
int RPC2_R2SError(int cerr)
{
    int sval;
    char *ctxt;
    
    if ( cerr <= 0 ) 
	return cerr;

    switch ( cerr ) {

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

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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/norton/norton.cc,v 4.4 1998/09/29 16:37:47 braam Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "norton.h"
#include "parser.h"

void usage(char * name) {
    fprintf(stderr, "Usage: %s <log_device> <data_device> <length>\n",
	    name);
}


int main(int argc, char * argv[]) {
    rvm_return_t 	err;
    
    if (argc != 4) {
	usage(argv[0]);
	exit(1);
    }

    
    NortonInit(argv[1], argv[2], atoi(argv[3]));
    
    InitParsing();
    Parser_commands();

    err = rvm_terminate();
    printf("rvm_terminate returns %s\n", rvm_return(err));
    return 0;
}

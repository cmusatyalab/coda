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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rpc2/mkcall.c,v 4.1 97/01/08 21:50:23 rvb Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


/* By Richard Draves, to replace mkcall.s for portability;
   number of INOUT and OUT args in a call should not exceed max in case below.
*/

#include <assert.h>
#include <stdio.h>
#include "rpc2.h"

int
mkcall(ClientHandler, ArgCount, HowMany, ConnList, offset, rpcval, args)
	int (*ClientHandler)();
	int ArgCount, HowMany;
	RPC2_Handle ConnList[];
	long offset, rpcval;
	int *args;
{
	switch (ArgCount) {
	    case 0:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval);
	    case 1:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0]);
	    case 2:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1]);
	    case 3:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2]);
	    case 4:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3]);
	    case 5:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3],
					args[4]);
	    case 6:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3],
					args[4], args[5]);
	    case 7:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3],
					args[4], args[5], args[6]);
	    case 8:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7]);
	    case 9:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7],
					args[8]);
	    case 10:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7],
					args[8], args[9]);
	    case 11:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7],
					args[8], args[9], args[10]);

	    case 12:
		return (*ClientHandler)(HowMany, ConnList, offset, rpcval,
					args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7],
					args[8], args[9], args[10], args[11]);
	    default:
		assert(ArgCount <= 12);
	}
	/*NOTREACHED*/
        return -1;
}

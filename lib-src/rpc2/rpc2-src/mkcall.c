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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


/* By Richard Draves, to replace mkcall.s for portability;
   number of INOUT and OUT args in a call should not exceed max in case below.
*/

#include <stdio.h>
#include <assert.h>
#include <rpc2/rpc2.h>

int mkcall(RPC2_HandleResult_func *ClientHandler, int ArgCount, int HowMany,
	   RPC2_Handle ConnList[], long offset, long rpcval, int *args)
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

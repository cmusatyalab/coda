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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vice/Attic/srvlock.cc,v 4.1 1997/01/08 21:51:59 rvb Exp $";
#endif /*_BLURB_*/







/************************************************************************/
/*									*/
/*  filelock.c	- adapted from fileproc.c				*/
/*		  because hc couldn't manage the length								*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <libc.h>


#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <rvmlib.h>
#include <srv.h>
#include <vicelock.h>


/*  *****  These should be deprecated! -JJK  *****  */

long ViceSetLock(RPC2_Handle RPCid, ViceFid *fid, ViceLockType type, RPC2_Unsigned PrimaryHost)
{
    static	char	* locktype[2] = {"LockRead","LockWrite"};

    LogMsg(0, SrvDebugLevel, stdout, "ViceSetLock type = %s Fid = %u.%d.%d...why??",
	    locktype[(int)type], fid->Volume, fid->Vnode, fid->Unique);
    return(EOPNOTSUPP);
}


long ViceReleaseLock(RPC2_Handle RPCid, ViceFid *fid, RPC2_Unsigned PrimaryHost)
{
    LogMsg(0, SrvDebugLevel, stdout,"ViceReleaseLock Fid = %u.%d.%d...why??",
	    fid->Volume, fid->Vnode, fid->Unique);
    return(EOPNOTSUPP);
}


/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/







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
#include <util.h>
#include <rvmlib.h>


#ifdef __cplusplus
}
#endif __cplusplus

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


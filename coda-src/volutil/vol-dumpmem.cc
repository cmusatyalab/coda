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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/vol-dumpmem.cc,v 4.2 1997/02/26 16:04:07 rvb Exp $";
#endif /*_BLURB_*/






/* vol-dumpmem.c 
  * utility to dump out an image of a memory chunk
  */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <ctype.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <sys/types.h>
#include <lwp.h>
#include <lock.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>


/*
  BEGIN_HTML
  <a name="S_VolDumpMem"><strong>Dump memory, given address and size, to a
  specified file</strong></a> 
  END_HTML
*/
long S_VolDumpMem(RPC2_Handle rpcid, RPC2_String formal_dumpfile, RPC2_Unsigned addr, RPC2_Unsigned size)
{
    int rc = 0;
    int status = 0;
    ProgramType *pt;
    int DumpFd; 

    /* To keep C++ 2.0 happy */
    char *dumpfile = (char *)formal_dumpfile;

    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    LogMsg(9, VolDebugLevel, stdout, "Entering S_DumpMem: rpcid = %d, addr = 0x%x, size = %d", rpcid, addr, size);

    /* open the file for writing out the dump */
    DumpFd = open(dumpfile, O_CREAT | O_WRONLY | O_TRUNC , 0755);
    if (DumpFd < 0){
	LogMsg(0, VolDebugLevel, stdout, "Dump: Couldnt open file to dump volume");
	return -1;
    }
    write(DumpFd, (char *)addr, size);
    close(DumpFd);
    return  0;
}


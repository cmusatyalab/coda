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






/* vol-dumpmem.c 
  * utility to dump out an image of a memory chunk
  */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>

#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <volutil.h>
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

    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

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


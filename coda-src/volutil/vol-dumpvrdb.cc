/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <vrdb.h>

/* S_VolDumpVRDB: Create a VRList file from the in-memory VRDB data. */
// This routine dumps a text file, VRList. The format should match whatever
// S_VolMakeVRDB expects.

long S_VolDumpVRDB(RPC2_Handle rpcid, RPC2_String formal_outfile)
{
    /* To keep C++ 2.0 happy */
    char *outfile = (char *)formal_outfile;
    int fd = -1, err = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolDumpVRDB; outfile %s",
	   outfile);
    
    fd = open(outfile, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd == -1) {
	LogMsg(0, VolDebugLevel, stdout, 
	       "S_VolDumpVRDB: unable to open file %s", outfile);
	err = VFAIL;
	goto Exit;
    }

    err = DumpVRDB(fd);

Exit:
    if (fd != -1) close(fd);
    return(err ? VFAIL : 0);
}

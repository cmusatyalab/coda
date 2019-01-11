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

#include <stdio.h>

#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <volume.h>
#include <util.h>
#include <srv.h>

/* GetVolumeList - Write out the list of volumes in RVM */
long S_GetVolumeList(RPC2_Handle rpcid, SE_Descriptor *formal_sed)
{
    SE_Descriptor sed;
    long rc = 0;
    LogMsg(1, VolDebugLevel, stdout, "Entering S_GetVolumeList\n");
    char *buf;
    unsigned int buflen;

    VListVolumes(&buf, &buflen);

    // ship the data back
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                            = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection          = SERVERTOCLIENT;
    sed.Value.SmartFTPD.Tag                            = FILEINVM;
    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_Byte *)buf;
    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen  = buflen;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT)
        LogMsg(0, VolDebugLevel, stdout,
               "GetVolumeList: InitSideEffect failed with %s",
               RPC2_ErrorMsg(rc));

    if (!rc && ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
                RPC2_ELIMIT))
        LogMsg(0, VolDebugLevel, stdout,
               "GetVolumeList: CheckSideEffect failed with %s",
               RPC2_ErrorMsg(rc));
    LogMsg(1, VolDebugLevel, stdout, "GetVolumeList returns %d\n", rc);
    free(buf);

    return (rc);
}

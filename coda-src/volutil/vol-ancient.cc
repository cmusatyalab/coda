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
#include <errno.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <util.h>
#include <rvmlib.h>

#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <camprivate.h>
#include <vutil.h>
#include <vrdb.h>
#include "vvlist.h"

/*
  VolMarkAsAncient - Mark the older dump file of a volume as ancient
*/
long S_NewVolMarkAsAncient(RPC2_Handle rpcid, VolumeId backupId)
{
    Volume *vp;
    vrent *vre;
    long volnum = 0, retcode;
    unsigned long error;

    vp = VGetVolume(&error, backupId);    
    if (error) {
	SLog(0, "Unable to get the volume %x",backupId);
	return (long)error;
    }

    /* Find the vrdb entry for the parent volume */
    vre = VRDB.ReverseFind(V_parentId(vp), NULL);
    if (vre) volnum = vre->volnum;

    retcode = S_VolMarkAsAncient(rpcid, volnum, V_parentId(vp));

    VPutVolume(vp);
    return retcode;
}

long S_VolMarkAsAncient(RPC2_Handle rpcid, VolumeId groupId, VolumeId repId)
{
    ProgramType *pt;
    int rc = 0;
    char *errstr;
    
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolMarkAsAncient: rpcid = %d, groupId = %x, repId = %x",
	rpcid, groupId, repId);

    rc = VInitVolUtil(volumeUtility);
    if (rc != 0)
	return rc;

    char listfile[PATH_MAX];
    getlistfilename(listfile, groupId, repId, "newlist");

    char newlistfile[PATH_MAX];
    getlistfilename(newlistfile, groupId, repId, "ancient");

    if (rename(listfile, newlistfile) < 0) {
	errstr = strerror(errno);
	LogMsg(0, VolDebugLevel, stdout,
	       "MarkAsAncient: rename %s->%s failed, %s",
	       listfile, newlistfile, errstr != NULL ? errstr: "Cannot rename");
	VDisconnectFS();
	return VFAIL;
    }

    VDisconnectFS();
    LogMsg(0, VolDebugLevel, stdout, "MarkAsAncient succeeded");

    return RPC2_SUCCESS;
}

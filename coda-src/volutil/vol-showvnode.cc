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

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>

#include <unistd.h>
#include <stdlib.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <util.h>
#include <rvmlib.h>

#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <srv.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <vrdb.h>
#include <codadir.h>

#include <ops.h>
#include <rsle.h>

#define INFOFILE "/tmp/vshowvnode.tmp"
static FILE *infofile;

/*
  S_VolShowVnode: Print out the specified vnode
*/
long S_VolShowVnode(RPC2_Handle rpcid, RPC2_Unsigned formal_volid, 
		    RPC2_Unsigned vnodeid, RPC2_Unsigned unique, 
		    SE_Descriptor *formal_sed)
{
    Volume *vp;
    Vnode *vnp = 0;
    Error error;
    SE_Descriptor sed;
    rvm_return_t status = RVM_SUCCESS;
    long rc = 0;
    ProgramType *pt;
    VolumeId tmpvolid;

    /* To keep C++ 2.0 happy */
    VolumeId volid = (VolumeId)formal_volid;

    VLog(9, "Checking lwp rock in S_VolShowVnode");
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    VLog(9, "Entering VolShowVnode(%d, 0x%x, 0x%x)", rpcid, volid, vnodeid);

    /* first check if it is replicated */
    tmpvolid = volid;
    if (!XlateVid(&volid))
	volid = tmpvolid;

    rvmlib_begin_transaction(restore);
    VInitVolUtil(volumeUtility);
/*    vp = VAttachVolume(&error, volid, V_READONLY); */
    vp = VGetVolume(&error, volid);
    if (error) {
	VLog(0, "S_VolInfo: failure attaching volume %d", volid);
	if (error != VNOVOL) {
	    VPutVolume(vp);
	}
        rvmlib_abort(error);
	goto exit;
    }
    /* VGetVnode moved from after VOffline to here 11/88 ***/
    vnp = VGetVnode(&error, vp, vnodeid, unique, READ_LOCK, 1, 1);
    if (error) {
	VLog(0, "S_VolShowVnode: VGetVnode failed with %d", error);
	VPutVolume(vp);
	rvmlib_abort(VFAIL);
	goto exit;
    }

    infofile = fopen(INFOFILE, "w");
    fprintf(infofile, "%lx.%lx(%lx), %s, cloned=%d, mode=%o, links=%d, length=%ld\n",
	vnodeid, vnp->disk.uniquifier, vnp->disk.dataVersion,
	vnp->disk.type == vFile? "file": vnp->disk.type == vDirectory? "directory":
	vnp->disk.type == vSymlink? "symlink" : "unknown type",
	vnp->disk.cloned, vnp->disk.modeBits, vnp->disk.linkCount,
	vnp->disk.length);
    fprintf(infofile, "inode=0x%lx, parent=%lx.%lx, serverTime=%s",
	vnp->disk.inodeNumber, vnp->disk.vparent, vnp->disk.uparent, ctime((long *)&vnp->disk.serverModifyTime));
    fprintf(infofile, "author=%lu, owner=%lu, modifyTime=%s, volumeindex = %d",
        vnp->disk.author, vnp->disk.owner, ctime((long *)&vnp->disk.unixModifyTime),
	vnp->disk.vol_index);
    PrintVV(infofile, &(vnp->disk.versionvector));

    if (vnp->disk.type == vDirectory) {
	PDCEntry pdce;
	pdce = DC_Get((PDirInode)vnp->disk.inodeNumber);
	if (pdce) {
	    DH_Print(DC_DC2DH(pdce), infofile);
	    DC_Put(pdce);
	}
    }
    
    if (AllowResolution && V_RVMResOn(vp) && vnp->disk.type == vDirectory) 
	PrintLog(vnp, infofile);

    VPutVolume(vp);

    fclose(infofile);
    
    /* set up SE_Descriptor for transfer */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, INFOFILE);
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) {
	VLog(0, "VolShowVnode: InitSideEffect failed with %s", RPC2_ErrorMsg(rc));
	rvmlib_abort(VFAIL);
	goto exit;
    }

    if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
		RPC2_ELIMIT) {
	VLog(0, "VolShowVnode: CheckSideEffect failed with %s", RPC2_ErrorMsg(rc));
	rvmlib_abort(VFAIL);
    }

    rvmlib_end_transaction(flush, &(status));
 exit:

    if (vnp){
	VPutVnode(&error, vnp);
	if (error) 
	    VLog(0, "S_VolShowVnode: Error occured while putting vnode ");
    }
    VDisconnectFS();
    if (status)
	VLog(0, "S_VolShowVnode failed with %d", status);
    return (status?status:rc);
}



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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-showvnode.cc,v 4.8 1998/11/25 19:23:39 braam Exp $";
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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>

#include <unistd.h>
#include <stdlib.h>
#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <rvmlib.h>

#include <volutil.h>
#ifdef __cplusplus
}
#endif __cplusplus
#include <srv.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <vrdb.h>
#include <reslog.h>

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
    int status;	    // transaction status variable
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

    RVMLIB_BEGIN_TRANSACTION(restore);
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
    fprintf(infofile, "%x.%x(%x), %s, cloned=%d, mode=%o, links=%d, length=%ld\n",
	vnodeid, vnp->disk.uniquifier, vnp->disk.dataVersion,
	vnp->disk.type == vFile? "file": vnp->disk.type == vDirectory? "directory":
	vnp->disk.type == vSymlink? "symlink" : "unknown type",
	vnp->disk.cloned, vnp->disk.modeBits, vnp->disk.linkCount,
	vnp->disk.length);
    fprintf(infofile, "inode=0x%x, parent=%x.%x, serverTime=%d",
	vnp->disk.inodeNumber, vnp->disk.vparent, vnp->disk.uparent, ctime((long *)&vnp->disk.serverModifyTime));
    fprintf(infofile, "author=%u, owner=%u, modifyTime=%s, volumeindex = %d",
        vnp->disk.author, vnp->disk.owner, (int)ctime((long *)&vnp->disk.unixModifyTime),
	vnp->disk.vol_index);
    PrintVV(infofile, &(vnp->disk.versionvector));
    
    if (AllowResolution && V_VMResOn(vp) && vnp->disk.type == vDirectory)
	/* print the resolution log */
	PrintResLog(vnp->disk.vol_index, vnodeid, unique, infofile);

    if (AllowResolution && V_RVMResOn(vp) && vnp->disk.type == vDirectory) 
	PrintLog(vnp, infofile);

    VPutVolume(vp);

    fclose(infofile);
    
    /* set up SE_Descriptor for transfer */
    bzero((void *)&sed, sizeof(sed));
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

    RVMLIB_END_TRANSACTION(flush, &(status));
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



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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-info.cc,v 4.2 1997/02/26 16:04:08 rvb Exp $";
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

#include <netinet/in.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <lwp.h>
#include <lock.h>
#include <timer.h>
#include <rpc2.h>
#include <se.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <rvmlib.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <index.h>
#include <vldb.h>
#include <volhash.h>
#include <resstats.h>

#define INFOFILE    "/tmp/volinfo.tmp"
static FILE * infofile;    // descriptor for info file

void PrintVnode(FILE *outfile, VnodeDiskObject *vnode, VnodeId vnodeNumber);

PRIVATE void PrintHeader(Volume *);
PRIVATE void printvns(Volume *, VnodeClass);
PRIVATE void date(unsigned long, char *);

/*
  BEGIN_HTML
  <a name="S_VolInfo"><strong>Dump out information (in ascii) about a volume </strong></a> 
  END_HTML
*/
S_VolInfo(RPC2_Handle rpcid, RPC2_String formal_volkey, RPC2_Integer dumpall, SE_Descriptor *formal_sed) {
    Volume *vp;
    Error error = 0;
    int status = 0;	    // transaction status variable
    long rc = 0;
    SE_Descriptor sed;
    ProgramType *pt;
    VolumeId volid;

    /* To keep C++ 2.0 happy */
    char *volkey = (char *)formal_volkey;
    
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolInfo(%u, %s, %d)", rpcid, volkey, dumpall);

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    VInitVolUtil(volumeUtility);

    /* If user entered volume name, use it to find volid. */
    struct vldb *vldbp = VLDBLookup(volkey);
    if (vldbp)
	volid = ntohl(vldbp->volumeId[vldbp->volumeType]);
    else {
	sscanf(volkey, "%X", &volid);
	long index = HashLookup(volid);
	if (index == -1) {
	    LogMsg(0, VolDebugLevel, stdout, "Info: Invalid name or volid %s!", volkey);
	    CAMLIB_ABORT(-1);
	}
    }

    vp = VGetVolume(&error, volid);
    if (vp == NULL) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolInfo: failure attaching volume %x", volid);
        CAMLIB_ABORT(error);
    }

    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "Vol-Info: VGetVolume returned error %d for %x", error, volid);
    }

    infofile = fopen(INFOFILE, "w");
    PrintHeader(vp);
    if (AllowResolution && V_RVMResOn(vp)) {
	V_VolLog(vp)->print(infofile);
	V_VolLog(vp)->vmrstats->precollect();
	V_VolLog(vp)->vmrstats->print(infofile);
	V_VolLog(vp)->vmrstats->postcollect();
    }
    if (dumpall) {
	fprintf(infofile, "\nLarge vnodes (directories)\n");
	printvns(vp, vLarge);
	fprintf(infofile, "\nSmall vnodes(files, symbolic links)\n");
	fflush(infofile);
	printvns(vp, vSmall);
    }
    
    fclose(infofile);
    VPutVolume(vp);

    /* set up SE_Descriptor for transfer */
    bzero(&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, INFOFILE);
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) {
	LogMsg(0, VolDebugLevel, stdout, "VolInfo: InitSideEffect failed with %s", RPC2_ErrorMsg(rc));
	CAMLIB_ABORT(VFAIL);
    }

    if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
		RPC2_ELIMIT) {
	LogMsg(0, VolDebugLevel, stdout, "VolInfo: CheckSideEffect failed with %s", RPC2_ErrorMsg(rc));
	CAMLIB_ABORT(VFAIL);
    }

    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
    VDisconnectFS();

    if (status)
	LogMsg(0, VolDebugLevel, stdout, "SVolInfo failed with %d", status);
    else
	LogMsg(9, VolDebugLevel, stdout, "SVolInfo returns %s", RPC2_ErrorMsg(rc));

    return (status?status:rc);
}

#define typestring(type) (type == RWVOL? "read/write": type == ROVOL? "readonly": type == BACKVOL? "backup": "unknown")

PRIVATE void PrintHeader(register Volume *vp)
{
    char d1[100];
    char d2[100];

    fprintf(infofile, "Volume header for volume %x (%s)\n", V_id(vp), V_name(vp));
    fprintf(infofile, "stamp.magic = %x, stamp.version = %u\n", V_stamp(vp).magic,
	V_stamp(vp).version);
    fprintf(infofile, "partition = (%s)\n", V_partname(vp));
    fprintf(infofile, "inUse = %d, inService = %d, blessed = %d, needsSalvaged = %d, dontSalvage = %d\n",
	V_inUse(vp), V_inService(vp), V_blessed(vp), V_needsSalvaged(vp), V_dontSalvage(vp));
    fprintf(infofile, "type = %d (%s), uniquifier = %u, needsCallback = %d, destroyMe = %x\n",
	V_type(vp), typestring(V_type(vp)), V_uniquifier(vp), V_needsCallback(vp), 
	V_destroyMe(vp));
    fprintf(infofile, "id = %x, parentId = %x, cloneId = %x, backupId = %x, restoredFromId = %x\n",
	V_id(vp), V_parentId(vp), V_cloneId(vp), V_backupId(vp), V_restoredFromId(vp));
    fprintf(infofile, "maxquota = %d, minquota = %d, maxfiles = %d, filecount = %d, diskused = %d\n",
	V_maxquota(vp), V_minquota(vp), V_maxfiles(vp), V_filecount(vp), V_diskused(vp));
    date(V_creationDate(vp), d1); date(V_copyDate(vp), d2);
    fprintf(infofile, "creationDate = %s, copyDate = %s\n", d1, d2);
    date(V_backupDate(vp), d1); date(V_expirationDate(vp), d2);
    fprintf(infofile, "backupDate = %s, expirationDate = %s\n", d1, d2);
    date(V_accessDate(vp), d1); date(V_updateDate(vp), d2);
    fprintf(infofile, "accessDate = %s, updateDate = %s\n", d1, d2);
    fprintf(infofile, "owner = %u, accountNumber = %u\n", V_owner(vp), V_accountNumber(vp));
    date(V_dayUseDate(vp), d1);
    fprintf(infofile, "dayUse = %u; week = (%u, %u, %u, %u, %u, %u, %u), dayUseDate = %s\n",
	V_dayUse(vp), V_weekUse(vp)[0], V_weekUse(vp)[1], V_weekUse(vp)[2],
	V_weekUse(vp)[3],V_weekUse(vp)[4],V_weekUse(vp)[5],V_weekUse(vp)[6], d1);
    if (V_groupId(vp) != 0) {
	fprintf(infofile, "replicated groupId = %x\n", V_groupId(vp));
	PrintVV(infofile, &(V_versionvector(vp)));
    }
}

PRIVATE void printvns(Volume *vp, VnodeClass vclass)
{
    register struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    register int vnodeIndex;

    vindex v_index(V_id(vp), vclass, vp->device, vcp->diskSize);
    vindex_iterator vnext(v_index);

    while ((vnodeIndex = vnext(vnode)) != -1) {
	if (vnode->type != vNull)
	    PrintVnode(infofile, vnode, bitNumberToVnodeNumber(vnodeIndex, vclass));
    }
}

void PrintVnode(FILE *outfile, VnodeDiskObject *vnode, VnodeId vnodeNumber)
{
    fprintf(outfile, "Vnode %u.%u.%u, cloned = %u, length = %u, inode = %u\n",
        vnodeNumber, vnode->uniquifier, vnode->dataVersion, vnode->cloned,
	vnode->length, vnode->inodeNumber);
    fprintf(outfile, "link count = %u, type = %u, volume index = %ld\n", vnode->linkCount, vnode->type, vnode->vol_index);
    PrintVV(outfile, &(vnode->versionvector));
}

PRIVATE void date(unsigned long date, char *result)
{
    struct tm *tm = localtime((long *)&date);
    sprintf(result, "%u (%02d/%02d/%02d.%02d:%02d:%02d)", date,
	tm->tm_year, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

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

#define RCSVERSION $Revision: 4.33 $

/* vol-dump.c */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

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

#include "coda_assert.h"
#include <struct.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <rpc2/errors.h>
#include <inodeops.h>
#include <util.h>
#include <rvmlib.h>
#include <codadir.h>
#include <partition.h>
#include <volutil.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <viceinode.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <vrdb.h>
#include <srv.h>
#include <voldump.h>

#include "vvlist.h"
#include "dump.h"

extern void PollAndYield();
static int VnodePollPeriod = 32;     /* How many vnodes to dump before polling */

#if (LISTLINESIZE >= SIZEOF_LARGEDISKVNODE)	/* Compile should fail.*/
#error "LISTLINESIZE >= SIZEOF_LARGEDISKVNODE)!"
#endif

/* Some sizes of dumped structures for the dump estimates */
#define ESTNAMESIZE V_MAXVOLNAMELEN /* just prepare for the worst case */
#define DUMPDUMPHEADERSIZE (44+ESTNAMESIZE)
#define DUMPVOLUMEDISKDATASIZE (229+4*ESTNAMESIZE)
#define DUMPENDSIZE 5

#define DUMPBUFSIZE 512000

/* We start off with the guts of the dump code */

static int DumpDumpHeader(DumpBuffer_t *dbuf, Volume *vp,
			  RPC2_Unsigned Incremental, long unique,
			  FILE *Ancient)
{
    int oldUnique;

    DumpDouble(dbuf, D_DUMPHEADER, DUMPBEGINMAGIC, DUMPVERSION);
    DumpInt32(dbuf, 'v', V_id(vp));
    DumpInt32(dbuf, 'p', V_parentId(vp));
    DumpString(dbuf, 'n',V_name(vp));
    DumpInt32(dbuf, 'b', V_copyDate(vp)); /* Date the backup clone was made */
    DumpInt32(dbuf, 'i', Incremental);

    /* Full dumps are w.r.t themselves */
    oldUnique = unique;

    if (Ancient) {
	/* Read in the header, hope it doesn't break */
	if (!ValidListVVHeader(Ancient, vp, &oldUnique)) {
	    SLog(0, "Dump: Ancient list file has invalid header");
	    return -1;
	}
    }

    return DumpDouble(dbuf, 'I', oldUnique, unique);
}

static int DumpVolumeDiskData(DumpBuffer_t *dbuf, VolumeDiskData *vol)
{
    DumpTag(dbuf, D_VOLUMEDISKDATA);
    DumpInt32(dbuf, 'i',vol->id);
    DumpInt32(dbuf, 'v',vol->stamp.version);
    DumpString(dbuf, 'n',vol->name);
    DumpString(dbuf, 'P',vol->partition);
    DumpBool(dbuf, 's',vol->inService);
    DumpBool(dbuf, '+',vol->blessed);
    DumpInt32(dbuf, 'u',vol->uniquifier);
    DumpByte(dbuf, 't',vol->type);
    DumpInt32(dbuf, 'p',vol->parentId);
    DumpInt32(dbuf, 'g',vol->groupId);
    DumpInt32(dbuf, 'c',vol->cloneId);
    DumpInt32(dbuf, 'b',vol->backupId);
    DumpInt32(dbuf, 'q',vol->maxquota);
    DumpInt32(dbuf, 'm',vol->minquota);
    DumpInt32(dbuf, 'x',vol->maxfiles);
    DumpInt32(dbuf, 'd',vol->diskused);
    DumpInt32(dbuf, 'f',vol->filecount);
    DumpShort(dbuf, 'l',(int)(vol->linkcount));
    DumpInt32(dbuf, 'a', vol->accountNumber);
    DumpInt32(dbuf, 'o', vol->owner);
    DumpInt32(dbuf, 'C',vol->creationDate);	/* Rw volume creation date */
    DumpInt32(dbuf, 'A',vol->accessDate);
    DumpInt32(dbuf, 'U',vol->updateDate);
    DumpInt32(dbuf, 'E',vol->expirationDate);
    DumpInt32(dbuf, 'B',vol->backupDate);		/* Rw volume backup clone date */
    DumpString(dbuf, 'O',vol->offlineMessage);
    DumpString(dbuf, 'M',vol->motd);
    DumpArrayInt32(dbuf, 'W', (unsigned int *)vol->weekUse, sizeof(vol->weekUse)/sizeof(vol->weekUse[0]));
    DumpInt32(dbuf, 'D', vol->dayUseDate);
    DumpInt32(dbuf, 'Z', vol->dayUse);
    return DumpVV(dbuf, 'V', &(vol->versionvector));
}

static int DumpVnodeDiskObject(DumpBuffer_t *dbuf, VnodeDiskObject *v,
			       int vnodeNumber, Device device)
{
    int fd;
    int i;
    SLog(9, "Dumping vnode number %x", vnodeNumber);
    if (!v || v->type == vNull) {
	return DumpTag(dbuf, D_NULLVNODE);
    }
    DumpDouble(dbuf, D_VNODE, vnodeNumber, v->uniquifier);
    DumpByte(dbuf, 't', v->type);
    DumpShort(dbuf, 'b', v->modeBits);
    DumpShort(dbuf, 'l', v->linkCount); /* May not need this */
    DumpInt32(dbuf, 'L', v->length);
    DumpInt32(dbuf, 'v', v->dataVersion);
    DumpVV(dbuf, 'V', (ViceVersionVector *)(&(v->versionvector)));
    DumpInt32(dbuf, 'm', v->unixModifyTime);
    DumpInt32(dbuf, 'a', v->author);
    DumpInt32(dbuf, 'o', v->owner);
    DumpInt32(dbuf, 'p', v->vparent);
    if (DumpInt32(dbuf, 'q', v->uparent) == -1)
	return -1;
    
    if (v->type != vDirectory) {
	if (v->inodeNumber) {
	    fd = iopen((int)device, (int)v->inodeNumber, O_RDONLY);
	    if (fd < 0) {

		/* Instead of causing the dump to fail, I should just stick in
		   a null marker, which will cause an empty inode to be created
		   upon restore.
		 */
		
		SLog(0, 0, stdout,
		       "dump: Unable to open inode %d for vnode 0x%x; not dumped",
		       v->inodeNumber, vnodeNumber);
		DumpTag(dbuf, D_BADINODE);
	    } else {
		if (DumpFile(dbuf, D_FILEDATA, fd, vnodeNumber) == -1) {
		    SLog(0, "Dump: DumpFile failed.");
		    return -1;
		}
		close(fd);
	    }
	} else {
	    SLog(0, 0, stdout,
		   "dump: ACKK! Found barren inode in RO volume. (%x.%x)\n",
		   vnodeNumber, v->uniquifier);
	    DumpTag(dbuf, D_BADINODE);
	}
    } else {
            DirInode *dip;
	int size;

	CODA_ASSERT(v->inodeNumber != 0);
	dip = (DirInode *)(v->inodeNumber);

	/* Dump the Access Control List */
	if (DumpByteString(dbuf, 'A', (char *)VVnodeDiskACL(v), VAclDiskSize(v)) == -1) {
	    SLog(0, "DumpVnode: BumpByteString failed.");
	    return -1;
	}

	/* Count number of pages in DirInode */
        size = DI_Pages(dip);
	if (DumpInt32(dbuf, D_DIRPAGES, size) == -1)
		return -1;

	for ( i = 0; i < size; i++) {
		if (DumpByteString(dbuf, 'P', (char *)DI_Page(dip, i), DIR_PAGESIZE) == -1)
		return -1;
	}

	SLog(9, "DumpVnode finished dumping directory");
    }
    return 0;
}

static int DumpVnodeIndex(DumpBuffer_t *dbuf, Volume *vp, 
			  VnodeClass vclass, RPC2_Unsigned Incremental,
			  int VVListFd, FILE *Ancient)
{
    struct VnodeClassInfo *vcp;
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode;
    bit32 nLists, nVnodes;
    
    SLog(9, "Entering DumpVnodeIndex()");

    vcp = &VnodeClassInfo_Array[vclass];

    if (vclass == vLarge) {
	DumpTag(dbuf, D_LARGEINDEX);
	nVnodes = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.nlargevnodes;
	nLists = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.nlargeLists;
    } else {
	DumpTag(dbuf, D_SMALLINDEX);
	nVnodes = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.nsmallvnodes;
	nLists = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.nsmallLists;
    }
    DumpInt32(dbuf, 'v', nVnodes);
    if (DumpInt32(dbuf, 's', nLists) == -1)
	return -1;
    
    sprintf(buf, "Start of %s list, %u vnodes, %u lists.\n",
	    ((vclass == vLarge)? "Large" : "Small"), nVnodes, nLists);
    if (write(VVListFd, buf, strlen(buf)) != (int)strlen(buf)) {
	SLog(0, "Write %s Index header didn't succeed.", ((vclass == vLarge)? "Large" : "Small"));
	VPutVolume(vp);
	return -1;
    }

   /* Currently we have two counts of vnodes: nVnodes and nvnodes; the # of vnodes
    * in the real volume and the # of vnodes in the vvlist. However, we are not
    * exploiting this info as a sanity check. Should we be doing this? It's hard
    * because we currently can't tell the difference between creation and
    * modification of vnodes.
    */

    vindex v_index(V_id(vp), vclass, V_device(vp), vcp->diskSize);
    vindex_iterator vnext(v_index);
	
    if (Incremental) {
	SLog(9, "Beginning Incremental dump of vnodes.");

	/* Determine how many entries in the list... */
	if (fgets(buf, LISTLINESIZE, Ancient) == NULL) {
	    SLog(10, "Dump: fgets indicates error."); /* Abort? */
	}

	/* Read in enough info from vvlist file to start a vvlist class object. */
	long nvnodes;
	int nlists;
	char Class[7];
	
	if (sscanf(buf, "Start of %s list, %ld vnodes, %d lists.\n",
		   Class, &nvnodes, &nlists)!=3) { 
	    SLog(0, "Couldn't scan head of %s Index.", 
		(vclass==vLarge?"Large":"Small"));
	    VPutVolume(vp);
	    return -1;
	}

	CODA_ASSERT(strcmp(Class,((vclass == vLarge)? "Large" : "Small")) == 0);
	SLog(9, "Ancient clone had %d vnodes, %d lists.", nvnodes, nlists);

	rec_smolist *vnList;
	vvtable vvlist(Ancient, vclass, nlists);

	if (vclass == vLarge) 
	    vnList = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.largeVnodeLists;
	else
	    vnList = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.smallVnodeLists;
	/* Foreach list, check to see if vnodes on the list were created,
	 * modified, or deleted.
	 */
	
	for (int vnodeIndex = 0; vnodeIndex < (int)nLists; vnodeIndex++) {
	    rec_smolist_iterator nextVnode(vnList[vnodeIndex]);
	    rec_smolink *vptr;
	    
	    while ( (vptr = nextVnode()) ) {	/* While more vnodes */
		vnode = strbase(VnodeDiskObject, vptr, nextvn);
		int VnodeNumber = bitNumberToVnodeNumber(vnodeIndex, vclass);
		unsigned int dumplevel;
		nVnodes--;

		/* If the vnode was modified or created, add it to dump. */
		if (vvlist.IsModified(vnodeIndex, vnode->uniquifier,
				      &(vnode->versionvector.StoreId),
				      Incremental, &dumplevel))
		{
		    if (DumpVnodeDiskObject(dbuf, vnode, VnodeNumber, V_device(vp)) == -1) {
			SLog(0, 0, stdout, "DumpVnodeDiskObject (%s) failed.",
			       (vclass == vLarge) ? "large" : "small");
			return -1;
		    }
		    SLog(9,
			   "Dump: Incremental found %x.%x modified.",
			   VnodeNumber, vnode->uniquifier);
		    if ((vnodeIndex % VnodePollPeriod) == 0)
			PollAndYield();
		}
		/* No matter what, add the vnode to our new list. */
		ListVV(VVListFd, VnodeNumber, vnode, dumplevel);
	    }
	    
	    /* Check for deleted files. If there exists an entry which wasn't
	     * marked by isModified(), put a delete record for it in the dump.
	     */
	    vvent_iterator vvnext(vvlist, vnodeIndex);
	    vvent *vventry;
		
	    while ( (vventry = vvnext()) ) { 
		if (!vventry->isThere) {
		    /* Note: I could define VnodeNumber in the outer loop and
		     * seemingly save effort. However, it would be done for every
		     * list and since only 1 in (repfactor) is non-zero, seems
		     * like even more of a waste to me.  --- DCS
		     */
		    
		    int VnodeNumber = bitNumberToVnodeNumber(vnodeIndex, vclass);
		    if (DumpDouble(dbuf, D_RMVNODE, VnodeNumber, vventry->unique) == -1) {
			SLog(0, 0, stdout, "Dump RMVNODE failed, aborting.");
			return -1;
		    }
		    SLog(9,
			   "Dump: Incremental found %x.%x deleted.",
			   VnodeNumber, vventry->unique);
		}
	    }
	}
    } else {
	SLog(9, "Beginning Full dump of vnodes.");
	int count = 0;

	vnode = (VnodeDiskObject *) buf;
	for( int vnodeIndex = 0;
	     nVnodes && ((vnodeIndex = vnext(vnode)) != -1);
	     nVnodes--, count++) {
	    int VnodeNumber = bitNumberToVnodeNumber(vnodeIndex, vclass);
	    if (DumpVnodeDiskObject(dbuf, vnode, VnodeNumber, V_device(vp)) == -1) {
		SLog(0, "DumpVnodeDiskObject (%s) failed.",
		    (vclass == vLarge) ? "large" : "small");
		return -1;
	    }
	    
	    ListVV(VVListFd, VnodeNumber, vnode, 0);
	    if ((count % VnodePollPeriod) == 0)
		PollAndYield();
	}
	CODA_ASSERT(vnext(vnode) == -1);
    }

    if (vclass == vLarge) { 	/* Output End of Large Vnode list */
	    sprintf(buf, "%s", ENDLARGEINDEX);	/* Macro contains a new-line */
	    if (write(VVListFd, buf, strlen(buf)) != (int)strlen(buf))
		    SLog(0, "EndLargeIndex write didn't succeed.");
    }
    SLog(9, "Leaving DumpVnodeIndex()");
    return 0;
}

static unsigned int DumpVnodeDiskObject_estimate(VnodeDiskObject *v)
{
    unsigned int size;

    if (!v || v->type == vNull) return 1;

    /* size of the diskobject header */
    size = 109;
    
    if (v->type != vDirectory) {
	size += 5 + v->length;
    } else {
	DirInode *dip;

	CODA_ASSERT(v->inodeNumber != 0);
	dip = (DirInode *)(v->inodeNumber);

	size += 6 + VAclDiskSize(v) + ((1 + DIR_PAGESIZE) * DI_Pages(dip));
    }
    return size;
}

static int DumpVnodeIndex_estimate(Volume *vp, VnodeClass vclass,
				   RPC2_Unsigned *sizes, FILE *Ancient)
{
    struct VnodeClassInfo *vcp;
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode;
    bit32 nLists, nVnodes;
    
    SLog(9, "Entering DumpVnodeIndex_estimate()");

    vcp = &VnodeClassInfo_Array[vclass];

    if (vclass == vLarge) {
	nVnodes = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.nlargevnodes;
	nLists = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.nlargeLists;
    } else {
	nVnodes = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.nsmallvnodes;
	nLists = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.nsmallLists;
    }

    /* TAG + 2 * INT */
    sizes[9] += 11;
    
    vindex v_index(V_id(vp), vclass, V_device(vp), vcp->diskSize);
    vindex_iterator vnext(v_index);
    vvtable *vvl = NULL;
	
    if (Ancient) {
	/* Determine how many entries in the list... */
	if (fgets(buf, LISTLINESIZE, Ancient) == NULL) {
	    SLog(10, "DumpVnodeIndex_estimate: fgets indicates error.");
	}

	/* Read in enough info from vvlist file to start a vvlist class object. */
	long nvnodes;
	int nlists;
	char Class[7];

	if (sscanf(buf, "Start of %s list, %ld vnodes, %d lists.\n",
		   Class, &nvnodes, &nlists)!=3) { 
	    SLog(0, "Couldn't scan head of %s Index.", 
		 (vclass==vLarge?"Large":"Small"));
	    VPutVolume(vp);
	    return -1;
	}

	CODA_ASSERT(strcmp(Class,((vclass == vLarge)? "Large" : "Small")) == 0);

	vvl = new vvtable(Ancient, vclass, nlists);
    }

    rec_smolist *vnList;

    if (vclass == vLarge) 
	vnList = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.largeVnodeLists;
    else
	vnList = SRV_RVM(VolumeList[V_volumeindex(vp)]).data.smallVnodeLists;

    /* Foreach list, check to see if vnodes on the list were created,
     * modified, or deleted.
     */
    for (int vnodeIndex = 0; vnodeIndex < (int)nLists; vnodeIndex++) {
	rec_smolist_iterator nextVnode(vnList[vnodeIndex]);
	rec_smolink *vptr;

	while ( (vptr = nextVnode()) ) {	/* While more vnodes */
	    vnode = strbase(VnodeDiskObject, vptr, nextvn);
	    unsigned int dumplevel = 9;
	    nVnodes--;

	    /* set dumplevel to the one this vnode was previously dumped at */
	    if (Ancient)
		vvl->IsModified(vnodeIndex, vnode->uniquifier,
				&(vnode->versionvector.StoreId),
				9, &dumplevel);

	    sizes[dumplevel] += DumpVnodeDiskObject_estimate(vnode);
	}

	/* Check for deleted files. If there exists an entry which wasn't
	 * marked by isModified(), put a delete record for it in the dump.
	 */
	if (Ancient) {
	    vvent_iterator vvnext(*vvl, vnodeIndex);
	    vvent *vventry;

	    while ( (vventry = vvnext()) ) { 
		if (!vventry->isThere) {
		    sizes[9] += 9; /* DOUBLE INT */
		}
	    }
	}
	PollAndYield();
    }

    if (vvl) delete vvl;

    SLog(9, "Leaving DumpVnodeIndex_estimate()");
    return 0;
}


/**************************************************************************/
/*
  S_VolNewDump: Dump the contents of the requested volume into a file in a 
  host independent manner
*/
long S_VolNewDump(RPC2_Handle rpcid, RPC2_Unsigned formal_volumeNumber, 
		  RPC2_Unsigned *Incremental)
{
    Volume *vp = 0;
    long rc = 0, retcode = 0;
    Error error;
    ProgramType *pt;
    DumpBuffer_t *dbuf = NULL;
    char *DumpBuf = 0;
    RPC2_HostIdent hid;
    RPC2_PortIdent pid;
    RPC2_SubsysIdent sid;
    RPC2_BindParms bparms;
    RPC2_Handle cid;
    RPC2_PeerInfo peerinfo;
    int VVListFd = -1;
    FILE *Ancient = NULL;

    /* To keep C++ 2.0 happy */
    VolumeId volumeNumber = (VolumeId)formal_volumeNumber;
    char *rock;

    CODA_ASSERT(LWP_GetRock(FSTAG, &rock) == LWP_SUCCESS);
    pt = (ProgramType *)rock;

    SLog(9, "S_VolNewDump: conn: %d, volume:  %#x, Inc?: %u", 
	 rpcid, volumeNumber, *Incremental);
    rc = VInitVolUtil(volumeUtility);
    if (rc) return rc;

    vp = VGetVolume(&error, volumeNumber);    
    if (error) {
	SLog(0, "Unable to get the volume %x, not dumped", volumeNumber);
	VDisconnectFS();
	return (int)error;
    }

    if (V_type(vp) == RWVOL) {
	SLog(0, "Volume is read-write.  Dump not allowed");
	retcode = VFAIL;
	goto failure;
    }

    DumpBuf = (char *)malloc(DUMPBUFSIZE);
    if (!DumpBuf) {
	SLog(0, "S_VolDumpHeader: Can't malloc buffer!");
	retcode = VFAIL;
	goto failure;
    }
    
    VolumeId volnum;
    long unique;
    int ix;

    volnum = V_parentId(vp);
    if (ReverseXlateVid(&volnum, &ix)) {
	unique = (&V_versionvector(vp).Versions.Site0)[ix]; 
    } else {
	volnum = 0; /* parent volume, nonexistent in the case... */
	/* Uniquely identify incrementals of non-rep volumes by updateDate */
	unique = V_updateDate(vp);
    }

    char VVlistfile[PATH_MAX];
    getlistfilename(VVlistfile, volnum, V_parentId(vp), "newlist");
    SLog(0, "NewDump: file %s volnum %x id %x parent %x",
	 VVlistfile, volnum, volumeNumber, V_parentId(vp));
    VVListFd = open(VVlistfile, O_CREAT | O_WRONLY | O_TRUNC, 0755);
    if (VVListFd < 0) {
	SLog(0, "S_VolDumpHeader: Couldn't open VVlistfile.");
	retcode = VFAIL;
	goto failure;
    }

    if (*Incremental) {
	char listfile[PATH_MAX];
	getlistfilename(listfile, volnum, V_parentId(vp), "ancient");

	Ancient = fopen(listfile, "r");
	if (Ancient == NULL) {
	    SLog(0, "S_VolDump: Couldn't open Ancient vvlist %s, will do a full backup instead.", listfile);
	    *Incremental = 0;
	}
    }

    /* Set up a connection with the client. */
    if ((rc = RPC2_GetPeerInfo(rpcid, &peerinfo)) != RPC2_SUCCESS) {
	SLog(0,"VolDump: GetPeerInfo failed with %s", RPC2_ErrorMsg((int)rc));
	retcode = rc;
	goto failure;
    }

    hid = peerinfo.RemoteHost;
    pid = peerinfo.RemotePort;
    sid.Tag = RPC2_SUBSYSBYID;
    sid.Value.SubsysId = VOLDUMP_SUBSYSTEMID;
    bparms.SecurityLevel = RPC2_OPENKIMONO;
    bparms.SideEffectType = SMARTFTP;
    bparms.EncryptionType = 0;
    bparms.ClientIdent = NULL;
    bparms.SharedSecret = NULL;
    
    if ((rc = RPC2_NewBinding(&hid, &pid, &sid, &bparms, &cid))!=RPC2_SUCCESS) {
	SLog(0, "VolDump: Bind to client failed, %s!", RPC2_ErrorMsg((int)rc));
	retcode = rc;
	goto failure;
    }

    dbuf = InitDumpBuf(DumpBuf, (long)DUMPBUFSIZE, V_id(vp), cid);

    /* Dump the volume.*/
    DumpListVVHeader(VVListFd, vp, *Incremental, unique);
    if ((DumpDumpHeader(dbuf, vp, *Incremental, unique, Ancient) == -1) ||
	(DumpVolumeDiskData(dbuf, &V_disk(vp)) == -1)	      ||
	(DumpVnodeIndex(dbuf, vp, vLarge, *Incremental, VVListFd, Ancient) == -1) ||
	(DumpVnodeIndex(dbuf, vp, vSmall, *Incremental, VVListFd, Ancient) == -1) ||
	(DumpEnd(dbuf) == -1)) {
	SLog(0, "Dump failed due to FlushBuf failure.");
	retcode = VFAIL;
    }

failure:
    if (VVListFd >= 0)
	close(VVListFd);
    if (Ancient)
	fclose(Ancient);

    /* zero the pointer, so we won't re-close it */
    Ancient = NULL;
    
    VDisconnectFS();
    VPutVolume(vp);
    
    if (dbuf) {
	SLog(2, "Dump took %d seconds to dump %d bytes.",
	     dbuf->secs, dbuf->nbytes);
	free(dbuf);
    }
    if (DumpBuf) 
	    free(DumpBuf);

    if (RPC2_Unbind(cid) != RPC2_SUCCESS) {
	SLog(0, "S_VolNewDump: Can't close binding %s", 
	     RPC2_ErrorMsg((int)rc));
    }
    
    if (retcode == 0) {
	SLog(0, "S_VolNewDump: %s volume dump succeeded",
	    *Incremental?"Incremental":"");
	return 0;
    }

    unlink(VVlistfile);
    SLog(0, "S_VolNewDump: %s volume dump failed with status = %d",
	*Incremental?"Incremental":"", retcode);

    return retcode;
}

/*
 * S_VolDumpEstimate - estimate the size of a volume dump at various levels. 
 */
long S_VolDumpEstimate(RPC2_Handle rpcid, RPC2_Unsigned formal_volumeNumber,
		       VolDumpEstimates *sizes)
{
    Volume *vp = 0;
    long rc = 0, retcode = 0;
    Error error;
    FILE *Ancient = NULL;
    VolumeId volnum;
    RPC2_Unsigned estimates[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    /* To keep C++ 2.0 happy */
    VolumeId volumeNumber = (VolumeId)formal_volumeNumber;

    SLog(9, "S_VolDumpEstimate: conn: %d, volume: %#x", rpcid, volumeNumber);
    rc = VInitVolUtil(volumeUtility);
    if (rc) return rc;

    vp = VGetVolume(&error, volumeNumber);    
    if (error) {
	SLog(0, "Unable to get the volume %x, not dumped", volumeNumber);
	VDisconnectFS();
	return (int)error;
    }

    if (V_type(vp) == RWVOL) {
	SLog(0, "Volume is read-write.  Dump not allowed");
	retcode = VFAIL;
	goto failure;
    }

    /* Find the replicated id and index for the parent volume */
    volnum = V_parentId(vp);
    if (!ReverseXlateVid(&volnum))
	volnum = 0; /* non-replicated volume */

    char listfile[PATH_MAX];
    int oldUnique;
    getlistfilename(listfile, volnum, V_parentId(vp), "ancient");

    Ancient = fopen(listfile, "r");
    if (Ancient) {
	if (!ValidListVVHeader(Ancient, vp, &oldUnique)) {
	    SLog(0, "Dump: Ancient list file has invalid header");
	    retcode = VFAIL;
	    goto failure;
	}
    }

    /* Estimate the size of the volume dump.*/
    estimates[9]  = DUMPDUMPHEADERSIZE;
    estimates[9] += DUMPVOLUMEDISKDATASIZE;
    if (DumpVnodeIndex_estimate(vp, vLarge, estimates, Ancient) == -1 ||
	DumpVnodeIndex_estimate(vp, vSmall, estimates, Ancient) == -1) {
	retcode = VFAIL;
	goto failure;
    }
    estimates[9] += DUMPENDSIZE;

    /* propagate estimated size of higher incremental backups to the lower
     * orders. */
    sizes->Lvl9 = estimates[9];
    sizes->Lvl8 = estimates[8] + sizes->Lvl9;
    sizes->Lvl7 = estimates[7] + sizes->Lvl8;
    sizes->Lvl6 = estimates[6] + sizes->Lvl7;
    sizes->Lvl5 = estimates[5] + sizes->Lvl6;
    sizes->Lvl4 = estimates[4] + sizes->Lvl5;
    sizes->Lvl3 = estimates[3] + sizes->Lvl4;
    sizes->Lvl2 = estimates[2] + sizes->Lvl3;
    sizes->Lvl1 = estimates[1] + sizes->Lvl2;
    sizes->Lvl0 = estimates[0] + sizes->Lvl1;

failure:
    if (Ancient) fclose(Ancient);

    /* zero the pointer, so we won't re-close it */
    Ancient = NULL;
    
    VDisconnectFS();
    VPutVolume(vp);
    
    return retcode;
}


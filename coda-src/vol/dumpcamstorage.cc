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








/********************************
 * dumpcamstorage.c		*
 ********************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#ifdef	__BSD44__
#include <fstab.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <struct.h>

#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include "cvnode.h"
#include "volume.h"
#include "vldb.h"
#include "partition.h"
#include "srvsignal.h"
#include "vutil.h"
#include "fssync.h"
#include "index.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"

void print_VolumeDiskData(VolumeDiskData *ddata);
static void PrintVersionVector(vv_t vv, char *indent);
void print_VnodeDiskObject(VnodeDiskObject *vnode);
void print_VolData(struct VolumeData *data);
void print_VolHead(struct VolHead *VolHead, int volindex);

/* These routines should be called from within a transaction */

void dump_storage(int level, char *s)
{
    int i;

    if (level > VolDebugLevel)
	return;

    printf("dump_storage at %s\n", s);
    printf("{\n    already_initialized = %d;\n\n    VolumeList = {\n",
	   SRV_RVM(already_initialized));
    for (i = 0; i < 14; i++) {
	print_VolHead(&SRV_RVM(VolumeList[i]), i);
	print_VolData(&(SRV_RVM(VolumeList[i]).data));
    }

    printf("    }\n\tSmallVnodeFreeList = {\n");
    for (i = 0; i < 3; i++) {
	if (SRV_RVM(SmallVnodeFreeList[i]) != NULL) {
	    printf("SmallVnodeFreeList[%d]\n", i);
	    print_VnodeDiskObject(SRV_RVM(SmallVnodeFreeList[i]));
	}
    }
    printf("    }\n\tLargeVnodeFreeList = {\n");
    for (i = 0; i < 2; i++) {
	if (SRV_RVM(LargeVnodeFreeList[i]) != NULL) {
	    printf("LargeVnodeFreeList[%d]\n", i);
	    print_VnodeDiskObject(SRV_RVM(LargeVnodeFreeList[i]));
	}
    }
    printf("    }\n    SmallVnodeIndex = %d;\n", SRV_RVM(SmallVnodeIndex));
    printf("    LargeVnodeIndex = %d;\n", SRV_RVM(LargeVnodeIndex));
    printf("    MaxVolId = %x;\n}\n", SRV_RVM(MaxVolId));

}

void print_VolHead(struct VolHead *VolHead, int volindex)
{
    printf("    VolHead VolumeList[%d]:\n", volindex);
    if (VolHead->header.stamp.magic != 0 ) {
	printf("\t\tversion stamp = %x, %u\n", VolHead->header.stamp.magic,
					    VolHead->header.stamp.version);
    }
    printf("\t\tid = %x\n\t\t parentid = %x\n\t\ttype = %u\n", VolHead->header.id,
				VolHead->header.parent, VolHead->header.type);
}

void print_VolData(struct VolumeData *data)
{
    int i = 0;

    printf("    VolumeData:\n");
    if (data->volumeInfo != NULL) {
	printf("\t\tVolumeDiskData *volumeInfo = %x\n", data->volumeInfo);
	print_VolumeDiskData(data->volumeInfo);
    }

    printf("\t\tsmallvnodes = %u\n\t\tsmallListSize = %u\n", data->nsmallvnodes, data->nsmallLists);
    printf("\t\trec_smolist *smallVnodeList = %x\n", data->smallVnodeLists);
    if (data->smallVnodeLists != NULL) {
	rec_smolist *p;
	for (i = 0; i < data->nsmallLists; i++) {
	    p = &(data->smallVnodeLists[i]);
	    p->print();
	    if (!p->IsEmpty()) {
		rec_smolist_iterator next(*p);
		rec_smolink *l;
		while (l = next()) {
		    VnodeDiskObject *vdo;
		    vdo = strbase(VnodeDiskObject, l, nextvn);
		    printf("\n\t\t\tVNODE%d\n", i);
		    print_VnodeDiskObject(vdo);
		}
	    }
	}
    }
    printf("\n");

    printf("\t\tlargevnodes = %u\n\t\tlargeListSize = %u\n", data->nlargevnodes, data->nlargeLists);
    printf("\t\tVnodeDiskObject **largeVnodeList = %x\n", data->largeVnodeLists);
    if (data->largeVnodeLists != NULL) {
	rec_smolist *p;
	for(i = 0; i < data->nlargeLists; i++) {
	    p = &(data->largeVnodeLists[i]);
	    p->print();
	    if (!p->IsEmpty()){
		rec_smolist_iterator next(*p);
		rec_smolink *l;
		while(l = next()){
		    VnodeDiskObject *vdo;
		    vdo = strbase(VnodeDiskObject, l, nextvn);
		    printf("\n\t\t\tVNODE%d\n", i);
		    print_VnodeDiskObject(vdo);
		}
	    }
	}
    }
    printf("\n\n");

}

void print_VnodeDiskObject(VnodeDiskObject *vnode)
{
    if (vnode->type == vNull && vnode->linkCount == 0)
	return;
    printf("\t\t\ttype = %u\n\t\t\tcloned = %u\n\t\t\tmode = %o\n\t\t\tlinks = %u\n",
	vnode->type, vnode->cloned, vnode->modeBits, vnode->linkCount);
    printf("\t\t\tlength = %u\n\t\t\tunique = %u\n\t\t\tversion = %u\n\t\t\tinode = %u\n",
	vnode->length, vnode->uniquifier, vnode->dataVersion, vnode->inodeNumber);
    PrintVersionVector(vnode->versionvector, "\t\t\t");
    printf("\t\t\tvolindex = %d\n\t\t\tmodtime = %u\n\t\t\tauthor = %u\n\t\t\towner = %u\n\t\t\tparent = %x.%x\n",
	vnode->vol_index, vnode->unixModifyTime, vnode->author, vnode->owner, vnode->vparent, vnode->uparent);
    printf("\t\t\tmagic = %x\n\t\t\tservermodtime = %u\n",
	vnode->vnodeMagic, vnode->serverModifyTime);
}

static void PrintVersionVector(vv_t vv, char *indent) {
    int i = 0;

    fprintf(stdout, "%s", indent);
    PrintVV(stdout, &vv);
}

void print_VolumeDiskData(VolumeDiskData *ddata)
{
    int i = 0;
    printf("\t\t\tversion stamp = %x, %u\n", ddata->stamp.magic, ddata->stamp.version);
    printf("\t\t\tid = %x\n\t\t\tpartition = %s\n\t\t\tname = %s\n\t\t\tinUse = %u\n\t\t\tinService = %u\n",
	    ddata->id, ddata->partition, ddata->name, ddata->inUse, ddata->inService);

    if (ddata->stamp.magic != 0) {
	printf("\t\t\tblessed = %u\n\t\t\tneedsSalvaged = %u\n\t\t\tuniquifier= %u\n\t\t\ttype = %d\n",
	    ddata->blessed, ddata->needsSalvaged, ddata->uniquifier, ddata->type);
	printf("\t\t\tparentId = %x\n\t\t\tgrpId = %x\n\t\t\tcloneId = %x\n\t\t\tbackupId = %xn\t\t\trestoreFromId = %x\n",
	    ddata->parentId, ddata->groupId, ddata->cloneId, ddata->backupId, ddata->restoredFromId);
	printf("\t\t\tneedsCallback = %u\n\t\t\tdestroyMe = %u\n\t\t\tdontSalvage = %u\n\t\t\treserveb3 = %u\n",
	    ddata->needsCallback, ddata->destroyMe, ddata->dontSalvage, ddata->reserveb3);
	PrintVersionVector(ddata->versionvector, "\t\t\t");
    }
    printf("\t\t\t");
    for (i = 0; i < 3; i++) {
	printf("reserved1[%d] = %u, ", i, ddata->reserved1[i]);
    }
    printf("\n\t\t\t");
    for (i = 3; i < 6; i++) {
	printf("reserved1[%d] = %u, ", i, ddata->reserved1[i]);
    }
    printf("\n");

    printf("\t\t\tmaxquota = %d\n\t\t\tminquota = %d\n\t\t\tmaxfiles = %d\n\t\t\tacctNum = %u\n\t\t\towner = %u\n",
	ddata->maxquota, ddata->minquota, ddata->maxfiles, ddata->accountNumber, ddata->owner);
    printf("\t\t\t");
    for (i = 0; i < 3; i++) {
	printf("reserved2[%d] = %d, ", i, ddata->reserved2[i]);
    }
    printf("\n\t\t\t");
    for (i = 3; i < 6; i++) {
	printf("reserved2[%d] = %d, ", i, ddata->reserved2[i]);
    }
    printf("\n\t\t\t");
    for (i = 6; i < 8; i++) {
	printf("reserved2[%d] = %d, ", i, ddata->reserved2[i]);
    }
    printf("\n");

    printf("\t\t\tfilecount = %d\n\t\t\tlinkcount = %u\n\t\t\tdiskused = %d\n\t\t\tdayUse = %d\n\t\t\tdayUseDate = %u\n",
	ddata->filecount, ddata->linkcount, ddata->diskused, ddata->dayUse, ddata->dayUseDate);
    printf("\t\t\t");
    for (i = 0; i < 3; i++) {
	printf("weekUse[%d] = %d, ", i, ddata->weekUse[i]);
    }
    printf("\n\t\t\t");
    for (i = 3; i < 6; i++) {
	printf("weekUse[%d] = %d, ", i, ddata->weekUse[i]);
    }
    printf("\n\t\t\t");
    for (i = 6; i < 7; i++) {
	printf("weekUse[%d] = %d, ", i, ddata->weekUse[i]);
    }
    printf("\n");

    printf("\t\t\t");
    for(i = 0; i < 3; i++) {
	printf("reserved3[%d] = %d, ", i, ddata->reserved3[i]);
    }
    printf("\n\t\t\t");
    for(i = 3; i < 6; i++) {
	printf("reserved3[%d] = %d, ", i, ddata->reserved3[i]);
    }
    printf("\n");
    printf("\t\t\t");
    for(i = 6; i < 9; i++) {
	printf("reserved3[%d] = %d, ", i, ddata->reserved3[i]);
    }
    printf("\n\t\t\t");
    for(i = 9; i < 11; i++) {
	printf("reserved3[%d] = %d, ", i, ddata->reserved3[i]);
    }
    printf("\n");

    printf("\t\t\tcreationDate = %u\n\t\t\taccessDate = %u\n\t\t\tupdateDate = %u\n\t\t\texpirationDate = %u\n",
	ddata->creationDate, ddata->accessDate, ddata->updateDate, ddata->expirationDate);
    printf("\t\t\tbackupDate = %u\n\t\t\tcopyDate = %u\n", ddata->backupDate, ddata->copyDate);
    printf("\t\t\t");
    for (i = 0; i < 3; i++) {
	printf("reserved4[%d] = %d, ", i, ddata->reserved4[i]);
    }
    printf("\n\t\t\t");
    for (i = 3; i < 6; i++) {
	printf("reserved4[%d] = %d, ", i, ddata->reserved4[i]);
    }
    printf("\n\t\t\t");
    for (i = 6; i < 8; i++) {
	printf("reserved4[%d] = %d, ", i, ddata->reserved4[i]);
    }
    printf("\n");
    printf("\t\t\tofflineMessage = %s\n", ddata->offlineMessage);
    printf("\t\t\tmotd = %s\n\n", ddata->motd);
}

void PrintCamVnode(int level, int volindex, int vclass, VnodeId vnodeindex, 
		    Unique_t unq)
{
    char *buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    Error ec;
    int rc = 0;

    if (level > VolDebugLevel) return;

    
    rc = ExtractVnode(&ec, volindex, vclass, vnodeindex, unq, vnode);
    if (ec != 0) {
	printf("Error %d from ExtractVnode; aborting vnode dump\n", ec);
	return;
    }
    printf("Printing %s vnode %u, (index %d) from volume %u\n",
	    ((vclass == vLarge)?"Large":"Small"),
	    bitNumberToVnodeNumber(vnodeindex,vclass), vnodeindex, volindex);
    print_VnodeDiskObject(vnode);
}

void PrintCamDiskData(int level, int volindex, VolumeDiskData *vdisk) {

    if (level > VolDebugLevel) return;

    printf("Printing VolumeDiskObject for volume index %d\n", volindex);
    print_VolumeDiskData(vdisk);
}

void PrintCamVolume(int level, int volindex) {

    if (level > VolDebugLevel) return;
    printf("Printing volume at index %d:\n", volindex);
    print_VolHead(&(SRV_RVM(VolumeList[volindex])), volindex);
    print_VolData(&(SRV_RVM(VolumeList[volindex].data)));
}

/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * A program to help decipher dump files. The basic idea is to have a command
 * interpreter (ci?) which provides commands to help a user poke around
 * the dump file. basic commands are show header, show vol, show vnode,
 * set vnode index, etc.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include <stdio.h>
#include <parser.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>

#ifdef __cplusplus
}
#endif

#include <inconsist.h>
#include <util.h>
#include <voltypes.h>
#include <vcrcommon.h>
#include <cvnode.h>
#include <volume.h>
#ifdef PAGESIZE
#undef PAGESIZE
#endif
#define PAGESIZE 2048	/* This is a problem, but can't inherit dirvnode.h */
#include "dump.h"
#include "dumpstream.h"


void OpenDumpFile(int, char **);
void setIndex(int, char **);
void showHeader(int, char **);
void showVolumeDiskData(int, char **);
void showVnodeDiskObject(int, char **);
void skipVnodes(int, char **);

command_t list[] = {
  {"openDumpFile", OpenDumpFile, 0, ""},		/* FileName */
  {"setIndex", setIndex, 0, ""},			/* "large" or "small" */
  {"showHeader", showHeader, 0, ""},			/* no args */
  {"showVolumeDiskData", showVolumeDiskData, 0, ""},	/* no args */
  {"nextVnode", showVnodeDiskObject, 0, ""},		/* args? */
  {"skipVnodes", skipVnodes, 0, ""},			/* nvnodes to skip */
  {"quit", Parser_exit, 0, ""},
  { 0, 0, 0, NULL }
};

char DefaultDumpFile[MAXPATHLEN];
char DefaultSize[10];			/* "large" or "small" */
dumpstream *DumpStream;
int Open = 0;

void OpenDumpFile(int largc, char **largv) {		/* FileName */
    char filename[MAXPATHLEN];

    if (largc == 1)
	Parser_getstr("Name of DumpFile?", DefaultDumpFile, filename,
		      MAXPATHLEN);
    else {
      if (largc != 2) {
	printf("openDumpFile <num>\n");
	return;
      }
      strncpy(filename, largv[1], MAXPATHLEN);
    }

    strcpy(DefaultDumpFile, filename);

    if (DumpStream)
	delete DumpStream;

    DumpStream = new dumpstream(filename);
    Open = 1;
    return;
}

int Rewind(char *args) {
    if (!Open) {
	printf("No DumpFile open yet!\n");
	return(-1);
    }

    CODA_ASSERT(DumpStream);
    delete DumpStream;
    DumpStream = new dumpstream(DefaultDumpFile);
    return 0;
}
    
void PrintVersionVector(ViceVersionVector *v, const char *prefix) {
    printf("%s{", prefix);
    FPrintVV(stdout, v);
    printf("}\n");
}    

void showHeader(int largc, char **largv)
{
    time_t timestamp;

    if (!Open) {
	printf("No DumpFile open yet!\n");
	return;
    }

    Rewind(0);	/* Start from the beginning of the dump */

    struct DumpHeader head;
    if (DumpStream->getDumpHeader(&head) == 0) {
	printf("Error -- DumpHead is not valid\n");
        return;
    }

    printf("%s Dump Version = %d\n", (head.Incremental)?"Incremental":"Full",
	   head.version);
    printf("VolId = %08x, name = %s\n", head.volumeId, head.volumeName);
    timestamp = (time_t)head.backupDate;
    printf("Parent = %08x, backupDate = %s", head.parentId, ctime(&timestamp));
    printf("Ordering references: Oldest %d, Latest %d\n",
	   head.oldest, head.latest);
    return;
}

void showVolumeDiskData(int largc, char **largv) {
    if (!Open) {
	printf("No DumpFile open yet!\n");
	return;
    }

    int i = 0;
    VolumeDiskData data;

    /* Place ourselves in the correct point in the dump. */
    Rewind(0);
    struct DumpHeader head;
    if (DumpStream->getDumpHeader(&head) == 0) {
	printf("Error -- DumpHead is not valid\n");
        return;
    }
    
    CODA_ASSERT(DumpStream->getVolDiskData(&data) == 0);

    printf("\tversion stamp = %x, %u\n", data.stamp.magic, data.stamp.version);
    printf("\tid = %08x\n\tpartition = %s\n\tname = %s\n\tinUse = %u\n\tinService = %u\n",
	    data.id, data.partition, data.name, data.inUse, data.inService);

    if (data.stamp.magic != 0) {
	printf("\tblessed = %u\n\tneedsSalvaged = %u\n\tuniquifier= %u\n\ttype = %d\n",
	    data.blessed, data.needsSalvaged, data.uniquifier, data.type);
	printf("\tparentId = %08x\n\tgrpId = %08x\n\tcloneId = %08x\n\tbackupId = %08xn\trestoreFromId = %08x\n",
	    data.parentId, data.groupId, data.cloneId, data.backupId, data.restoredFromId);
	printf("\tneedsCallback = %u\n\tdestroyMe = %u\n\tdontSalvage = %u\n\treserveb3 = %u\n",
	    data.needsCallback, data.destroyMe, data.dontSalvage, data.reserveb3);
	PrintVersionVector(&data.versionvector, "\t");
    }
    printf("\t");
    for (i = 0; i < 3; i++) {
	printf("reserved1[%d] = %u, ", i, data.reserved1[i]);
    }
    printf("\n\t");
    for (i = 3; i < 5; i++) {
	printf("reserved1[%d] = %u, ", i, data.reserved1[i]);
    }
    printf("\n");

    printf("\tmaxquota = %d\n\tminquota = %d\n\tmaxfiles = %d\n\tacctNum = %u\n\towner = %u\n",
	data.maxquota, data.minquota, data.maxfiles, data.accountNumber, data.owner);
    printf("\t");
    for (i = 0; i < 3; i++) {
	printf("reserved2[%d] = %d, ", i, data.reserved2[i]);
    }
    printf("\n\t");
    for (i = 3; i < 6; i++) {
	printf("reserved2[%d] = %d, ", i, data.reserved2[i]);
    }
    printf("\n\t");
    for (i = 6; i < 8; i++) {
	printf("reserved2[%d] = %d, ", i, data.reserved2[i]);
    }
    printf("\n");

    printf("\tfilecount = %d\n\tlinkcount = %u\n\tdiskused = %d\n\tdayUse = %d\n\tdayUseDate = %u\n",
	data.filecount, data.linkcount, data.diskused, data.dayUse, data.dayUseDate);
    printf("\t");
    for (i = 0; i < 3; i++) {
	printf("weekUse[%d] = %d, ", i, data.weekUse[i]);
    }
    printf("\n\t");
    for (i = 3; i < 6; i++) {
	printf("weekUse[%d] = %d, ", i, data.weekUse[i]);
    }
    printf("\n\t");
    for (i = 6; i < 7; i++) {
	printf("weekUse[%d] = %d, ", i, data.weekUse[i]);
    }
    printf("\n");

    printf("\t");
    for(i = 0; i < 3; i++) {
	printf("reserved3[%d] = %d, ", i, data.reserved3[i]);
    }
    printf("\n\t");
    for(i = 3; i < 6; i++) {
	printf("reserved3[%d] = %d, ", i, data.reserved3[i]);
    }
    printf("\n");
    printf("\t");
    for(i = 6; i < 9; i++) {
	printf("reserved3[%d] = %d, ", i, data.reserved3[i]);
    }
    printf("\n\t");
    for(i = 9; i < 11; i++) {
	printf("reserved3[%d] = %d, ", i, data.reserved3[i]);
    }
    printf("\n");

    printf("\tcreationDate = %u\n\taccessDate = %u\n\tupdateDate = %u\n\texpirationDate = %u\n",
	data.creationDate, data.accessDate, data.updateDate, data.expirationDate);
    printf("\tbackupDate = %u\n\tcopyDate = %u\n", data.backupDate, data.copyDate);
    printf("\t");
    for (i = 0; i < 3; i++) {
	printf("reserved4[%d] = %u, ", i, data.reserved4[i]);
    }
    printf("\n\t");
    for (i = 3; i < 6; i++) {
	printf("reserved4[%d] = %u, ", i, data.reserved4[i]);
    }
    printf("\n\t");
    for (i = 6; i < 7; i++) {
	printf("reserved4[%d] = %u, ", i, data.reserved4[i]);
    }
    printf("\n");
    printf("\tofflineMessage = %s\n", data.offlineMessage);
    printf("\tmotd = %s\n\n", data.motd);
    return;
}

void setIndex(int largc, char **largv) {
    char vnodeType[10];

    if (largc == 1)
      Parser_getstr("Large or Small?", DefaultSize, vnodeType, 10);
    else {
      if (largc != 2) {
	printf("setIndex <size>\n");
	return;
      }
      strncpy(vnodeType, largv[1], 10);
    }

    /* Case insensitize the input. */
    for (int i = 0; i < 10; i++) 
	if ((vnodeType[i] >= 'A') && (vnodeType[i] <= 'Z'))
	    vnodeType[i] = (vnodeType[i] - 'A') + 'a';

    if ((strcmp(vnodeType, "large") != 0) && (strcmp(vnodeType, "small") != 0)) {
	printf("Index must be either large or small\n");
	return;
    }

    if (vnodeType[0] == 'l') { /* Probably will pick small next time. */
	strcpy(DefaultSize, "small");
    }
    
    Rewind(0);
    struct DumpHeader head;
    if (DumpStream->getDumpHeader(&head) == 0) {
	printf("DumpHeader is not valid.\n");
	return;
    }

    /* skip over the volumediskdata */
    VolumeDiskData vol;
    if (DumpStream->getVolDiskData(&vol) != 0) {
	printf("VolumeDiskData is not valid.\n");
	return;
    }

    /* Large index */
    long nvnodes, nslots;
    if (DumpStream->getVnodeIndex(vLarge, &nvnodes, &nslots) == -1) {
	printf("Large Index header is not valid.\n");
	return;
    }

    if (vnodeType[0] == 's') {
	/* Skip large vnodes to get to small vnode index. */
	char buf[SIZEOF_LARGEDISKVNODE];
	VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
	VnodeId vn;
	off_t offset;
	int del, count = 0;

	while (DumpStream->getNextVnode(vnode, &vn, &del, &offset) != -1) {
	    count++;
	}

	CODA_ASSERT(nvnodes >= count); /* May have less if dump is incremental */
	
	/* We should have read all the Large Vnodes now, get the small index */
	if (DumpStream->getVnodeIndex(vSmall, &nvnodes, &nslots) == -1) {
	    printf("Small Index header is not valid.\n");
	    return;
	}
    }

    printf("There are %ld vnodes in %ld slots.\n", nvnodes, nslots);
    return;
}

void skipVnodes(int largc, char **largv) {
    if (!Open) {
	printf("No DumpFile open yet!\n");
	return;
    }

    long n;

    if (largc == 1)
      n = Parser_intarg(NULL, "Number of Vnodes to skip?", 0, 0, INT_MAX, 10);
    else {
      if (largc != 2) {
	printf("skipVnodes <num>\n");
	return;
      }
      Parser_arg2int(largv[1], &n, 10);
    }

    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    VnodeId vnum;
    off_t offset;
    int del;
    
    for (long i = 0; i < n; i++) {
	if (DumpStream->getNextVnode(vnode, &vnum, &del, &offset)) {
	    printf("Not positioned at a vnode, perhaps no more vNodes\n");
	    return;
	}
    }

    if (del) {
	printf("Vnode %08x at offset %lld was deleted.\n", vnum, (long long)offset);
	return;
    }
    
    printf("Vnode %08x is at offset %lld in the dump.\n", vnum, (long long)offset);
    if (vnode->type == vNull && vnode->linkCount == 0)
	return;
    printf("\ttype = %u\n\tcloned = %u\n\tmode = %o\n\tlinks = %u\n",
	vnode->type, vnode->cloned, vnode->modeBits, vnode->linkCount);
    printf("\tlength = %u\n\tunique = %08x\n\tversion = %u\n\tinode = %p\n",
	vnode->length, vnode->uniquifier, vnode->dataVersion, vnode->node.dirNode);
    PrintVersionVector(&vnode->versionvector, "\t");
    printf("\tvolindex = %u\n\tmodtime = %u\n\tauthor = %u\n\towner = %u\n\tparent = %08x.%08x\n",
	vnode->vol_index, vnode->unixModifyTime, vnode->author, vnode->owner, vnode->vparent, vnode->uparent);
    printf("\tmagic = %x\n\tservermodtime = %u\n",
	vnode->vnodeMagic, vnode->serverModifyTime);
    return;
}    

    
void showVnodeDiskObject(int largc, char **largv)
{
    if (!Open) {
	printf("No DumpFile open yet!\n");
	return;
    }

    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    VnodeId vnum;
    off_t offset;
    int del;
    if (DumpStream->getNextVnode(vnode, &vnum, &del, &offset)) {
	printf("Not positioned at a vnode, perhaps no more vNodes\n");
	return;
    }

    if (del) {
	printf("Vnode %08x at offset %lld was deleted.\n", vnum, (long long)offset);
	return;
    }
    
    printf("Vnode %08x is at offset %lld in the dump.\n", vnum, (long long)offset);
    if (vnode->type == vNull && vnode->linkCount == 0)
	return;
    printf("\ttype = %u\n\tcloned = %u\n\tmode = %o\n\tlinks = %u\n",
	vnode->type, vnode->cloned, vnode->modeBits, vnode->linkCount);
    printf("\tlength = %u\n\tunique = %08x\n\tversion = %u\n\tinode = %p\n",
	vnode->length, vnode->uniquifier, vnode->dataVersion, vnode->node.dirNode);
    PrintVersionVector(&vnode->versionvector, "\t");
    printf("\tvolindex = %d\n\tmodtime = %u\n\tauthor = %u\n\towner = %u\n\tparent = %08x.%08x\n",
	vnode->vol_index, vnode->unixModifyTime, vnode->author, vnode->owner, vnode->vparent, vnode->uparent);
    printf("\tmagic = %x\n\tservermodtime = %u\n",
	vnode->vnodeMagic, vnode->serverModifyTime);
    return;
}


int main(int argc, char **argv) {
    if (argc > 2) {
	printf("Usage: %s <dumpfile>\n", argv[0]);
	exit(-1);
    }

    if (argc > 1)	/* Use any argument as the default dumpfile. */
	strncpy(DefaultDumpFile, argv[1], sizeof(DefaultDumpFile));

    strcpy(DefaultSize, "Large");
    Parser_init("dump> ", list);
    Parser_commands();
}
    

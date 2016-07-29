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

/*
 * This file contains the utility to merge an incremental dump onto a full dump.
 * Essentially it replaces vnodes in the full dump by vnodes in the incremental.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/file.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <voltypes.h>
#include <vcrcommon.h>
#include <cvnode.h>
#include <volume.h>
#include "dump.h"
#include "dumpstream.h"

typedef struct entry {
    long unique;
    off_t offset;
    dumpstream *dump;
    struct entry *next;
} ventry;

typedef struct {
    ventry **table;
    long nvnodes, nslots;
} vtable;

int DumpFd = -1;

static void BuildTable(dumpstream *, vtable *);
static void ModifyTable(dumpstream *, VnodeClass, vtable *);
static void WriteTable(DumpBuffer_t *, vtable *, VnodeClass);
static void WriteVnodeDiskObject(DumpBuffer_t *, VnodeDiskObject *, int);
static void DumpVolumeDiskData(DumpBuffer_t *, register VolumeDiskData *);
static void WriteDumpHeader(DumpBuffer_t *buf, struct DumpHeader *, struct DumpHeader *);

#define DUMPBUFSIZE 512000

int main(int argc, char **argv)
{
    
    if (argc < 4) {
	LogMsg(0, VolDebugLevel, stderr, "Usage: %s <outfile> <full dump> <incremental dump>", argv[0]);
	exit(-1);
    }

    if (strcmp(argv[1], "-d") == 0) {
	VolDebugLevel = atoi(argv[2]);
	argv+=2;
	argc-=2;
    }

    dumpstream fdump(argv[2]);
    dumpstream idump(argv[3]);
    
    /* NOTE: Argv[2] and argv[3] are no longer valid here for some reason... */
    struct DumpHeader fdumphead, idumphead;

    if (fdump.getDumpHeader(&fdumphead) == 0) {
	LogMsg(0, VolDebugLevel, stderr, "Error -- DumpHeader of %s is not valid.\n", argv[2]);
	exit(-1);
    }
    
    if (idump.getDumpHeader(&idumphead) == 0) {
	LogMsg(0, VolDebugLevel, stderr, "Error -- DumpHeader of %s is not valid.\n", argv[3]);
	exit(-1);
    }
    
    /* Need to do something about the version -- perhaps in getDumpHeader? */

    if ((fdumphead.parentId != idumphead.parentId) || 
	(strcmp(fdumphead.volumeName, idumphead.volumeName) != 0)) {
	LogMsg(0, VolDebugLevel, stderr, "Error -- volume Id's or name's don't match!");
	LogMsg(0, VolDebugLevel, stderr, "%s: Volume id = %x, Volume name = %s", argv[2], 
		fdumphead.volumeId, fdumphead.volumeName);
	LogMsg(0, VolDebugLevel, stderr, "%s: Volume id = %x, Volume name = %s", argv[3],
		idumphead.volumeId, idumphead.volumeName);
	exit(-1);
    }

    if (fdumphead.Incremental) {
	LogMsg(0, VolDebugLevel, stderr, "Error -- trying to merge onto incremental dump!");
	exit(-1);
    }

    if (!idumphead.Incremental) {
	LogMsg(0, VolDebugLevel, stderr, "Error -- trying to merge from a full dump!");
	exit(-1);
    }

#if 0
    /* The incremental was taken w.r.t the full if the next test succeeds: */ 
    if (idumphead.oldest != fdumphead.latest) {
	LogMsg(0, VolDebugLevel, stderr, "Error -- %s is not with respect to %s!", argv[3], argv[2]);
	exit(-1);
    }
#endif

    DumpFd = open(argv[1], O_WRONLY | O_CREAT | O_EXCL, 00644);
    if (DumpFd <= 0) {
	perror("output file");
	exit(-1);
    }

    char *DumpBuf = (char *)malloc(DUMPBUFSIZE);
    CODA_ASSERT(DumpBuf != NULL);
    DumpBuffer_t *dbuf = InitDumpBuf(DumpBuf, DUMPBUFSIZE, DumpFd);
    
    /* At this point both headers should have been read and checked. Write out
       the merged header to the dump file. */

    WriteDumpHeader(dbuf, &fdumphead, &idumphead);
    
    /* skip over the volumediskdata, is there any need to compare it? */
    VolumeDiskData vol;
    CODA_ASSERT(fdump.getVolDiskData(&vol) == 0);/* Throw this info away for now... */
    CODA_ASSERT(idump.getVolDiskData(&vol) == 0);
    DumpVolumeDiskData(dbuf, &vol);

    /* Read in the Large Vnodes into an array, only saving the fid and an offset
     * where the vnode can be found. */

    vtable LTable;
    if (fdump.getVnodeIndex(vLarge, &LTable.nvnodes, &LTable.nslots) == -1) 
	exit(-1); /* Already printed out an error */
    
    BuildTable(&fdump, &LTable);
    ModifyTable(&idump, vLarge, &LTable);

    vtable STable;
    if (fdump.getVnodeIndex(vSmall, &STable.nvnodes, &STable.nslots) == -1)
	exit(-1); /* Already printed out the error */

    BuildTable(&fdump, &STable);
    ModifyTable(&idump, vSmall, &STable);

    if (fdump.EndOfDump()) {
	LogMsg(0, VolDebugLevel, stderr, "Full dump %s has improper postamble.", argv[2]);
	exit(-1);
    }

    if (idump.EndOfDump()) {
	LogMsg(0, VolDebugLevel, stderr, "Incremental dump has improper postamble.", argv[3]);
    }

    /* At this point the tables should reflect the merged state. */
    DumpTag(dbuf, D_LARGEINDEX); 
    WriteTable(dbuf, &LTable, vLarge);     /* Output the list of vnodes */

    DumpTag(dbuf, D_SMALLINDEX);   
    WriteTable(dbuf, &STable, vSmall);	/* Output the list of vnodes */

    DumpEnd(dbuf);	/* Output an EndMarker on the output stream. */
    close(DumpFd);
    free(DumpBuf);
    free(dbuf);
}

/* Construct a table of vnodes. For each vnode, create an entry in it which
 * contains the fid of the vnode, plus the dump and offset where the vnode
 * is located. Don't need the StoreId since the presence of a vnode in the
 * incremental implies it has been changed.
 */

static void BuildTable(dumpstream *dump, vtable *table)
{
    VnodeId vnodeNumber;
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vdo = (VnodeDiskObject *)buf;

    table->table = (ventry **)malloc(sizeof(ventry*) * table->nslots);
    CODA_ASSERT(table->table != NULL);
    memset((void *)table->table, 0, sizeof(ventry*) * table->nslots);
    for (int i = 0; i < table->nvnodes; i++) {
	off_t offset;
	int deleted;
	if (dump->getNextVnode(vdo, &vnodeNumber, &deleted, &offset) == -1) {
	    LogMsg(0, VolDebugLevel, stderr, "ERROR -- Failed to get a vnode!");
	    exit(-1);
	}

	CODA_ASSERT(deleted == 0); /* Can't have deleted vnode in full dump */
	LogMsg(10, VolDebugLevel, stdout, "vnodeNum %d, offset %d", vnodeNumber, offset);
	if (vdo->type != vNull) { /* Insert the vnode into the table */
	    ventry *tmp = (ventry *)malloc(sizeof(ventry));
	    CODA_ASSERT(tmp != NULL);
	    LogMsg(60, VolDebugLevel, stdout, "tmp %x", tmp);
	    tmp->unique = vdo->uniquifier;
	    tmp->offset = offset;
	    tmp->dump = dump;
	    int vnum = vnodeIdToBitNumber(vnodeNumber);
	    tmp->next = table->table[vnum];
	    table->table[vnum] = tmp;
	}
    }
}

/* In the case that a vnode was deleted and reused between the full and
 * incremental dumps, we can only add the new vnode to the list. It is
 * not possible to differentiate between the case where the new fid replaced
 * the old one, or was added to list for that vnode in the original volume.
 */

static void ModifyTable(dumpstream *dump, VnodeClass vclass, vtable *Table)
{
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vdo = (VnodeDiskObject *)buf;
    long nvnodes, nslots;
    VnodeId vnodeNumber;

    if (dump->getVnodeIndex(vclass, &nvnodes, &nslots) == -1) 
	exit(-1); /* Already printed out an error */

    if (Table->nslots > nslots) {
	LogMsg(0, VolDebugLevel, stderr, "ERROR -- full has more vnode slots than incremental!");
	exit(-1);
    }

    if (nslots > Table->nslots) { /* "Grow" Vnode Array */
	ventry **tmp = (ventry **)malloc(sizeof(ventry*) * nslots);
	CODA_ASSERT(tmp != NULL);
	memcpy(tmp, Table->table, sizeof(ventry*) * Table->nslots);
	free(Table->table);
	Table->nslots = nslots;
	Table->table = tmp;
    }

    int deleted;
    off_t offset;
    while (dump->getNextVnode(vdo, &vnodeNumber, &deleted, &offset) != -1) {
	int vnum = vnodeIdToBitNumber(vnodeNumber);
	CODA_ASSERT(vnum >= 0);
	if (vnum > Table->nslots){
	    LogMsg(0, VolDebugLevel, stderr, "vnum %d > nslots %d!", vnum, nslots);
	    exit(-1);
	}
	
	if (deleted) {
	    /* Locate the vnode in LTable and remove it. */
	    ventry *optr = 0, *ptr = Table->table[vnum];
	    while (ptr && (ptr->unique != (int)vdo->uniquifier)) {
		optr = ptr;
		ptr = ptr->next;
	    }

	    if (ptr) {
		if (optr) optr->next = ptr->next;
		else Table->table[vnum] = ptr->next;
		free(ptr);
		Table->nvnodes--;
	    } else {
		/* This could happen if the array is grown, vnodes created,
		   then deleted between the two backups. */
		LogMsg(0, VolDebugLevel, stdout, "Removing a null Vnode for %x.%x!", vnodeNumber, vdo->uniquifier);
	    }
	} else if (vdo->type != vNull) {
	    /* Find the entry for the new vnode and update it */
	    ventry *ptr = Table->table[vnum];
	    while (ptr && (ptr->unique != (int)vdo->uniquifier))
		ptr = ptr->next;

	    if (ptr) {
		ptr->offset = offset;
		ptr->dump = dump;
	    } else {			/* Must be new, add the entry. */
		ventry *tmp = (ventry *)malloc(sizeof(ventry));
		CODA_ASSERT(tmp != NULL);
		tmp->unique = vdo->uniquifier;
		tmp->offset = offset;
		tmp->dump = dump;
		tmp->next = Table->table[vnum];
		Table->table[vnum] = tmp;
		Table->nvnodes++;
	    }		
	}
    }
}

/* Write the combined headers to the output dump file. */
static void WriteDumpHeader(DumpBuffer_t *buf, struct DumpHeader *head, struct DumpHeader *ihead)
{
    DumpDouble(buf, D_DUMPHEADER, DUMPBEGINMAGIC, DUMPVERSION);
    DumpInt32(buf, 'v', head->volumeId);
    DumpInt32(buf, 'p', head->parentId);
    DumpString(buf, 'n', head->volumeName);
    DumpInt32(buf, 'b', ihead->backupDate);    /* Date the backup clone was made */
    DumpInt32(buf, 'i', head->Incremental);
    DumpDouble(buf, 'I', head->oldest, ihead->latest);
}

static void WriteTable(DumpBuffer_t *buf, vtable *table, VnodeClass vclass)
{
    char vbuf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vdo = (VnodeDiskObject *)vbuf;

    DumpInt32(buf, 'v', table->nvnodes);
    DumpInt32(buf, 's', table->nslots);
    
    for (int i = 0; i < table->nslots; i++) {
	ventry *ptr = table->table[i];
	while (ptr) {
	    long vnum = bitNumberToVnodeNumber(i, vclass);
	    (ptr->dump)->setIndex(vclass);
	    if ((ptr->dump)->getVnode(vnum, ptr->unique, ptr->offset, vdo)== -1){
		LogMsg(0, VolDebugLevel, stderr, "Couldn't get Vnode %d 2nd time, offset %x.",
			vnum, ptr->offset);
		exit(-1);
	    }

	    WriteVnodeDiskObject(buf, vdo, vnum);	/* Write out the Vnode */
	    (ptr->dump)->copyVnodeData(buf);
	    ptr = ptr->next;
	}
    }
}

static void WriteVnodeDiskObject(DumpBuffer_t *buf, VnodeDiskObject *v, int vnodeNumber)
{
    DumpDouble(buf, D_VNODE, vnodeNumber, v->uniquifier);
    DumpByte(buf, 't', v->type);
    DumpShort(buf, 'b', v->modeBits);
    DumpShort(buf, 'l', v->linkCount); /* May not need this */
    DumpInt32(buf, 'L', v->length);
    DumpInt32(buf, 'v', v->dataVersion);
    DumpVV(buf, 'V', (ViceVersionVector *)(&(v->versionvector)));
    DumpInt32(buf, 'm', v->unixModifyTime);
    DumpInt32(buf, 'a', v->author);
    DumpInt32(buf, 'o', v->owner);
    DumpInt32(buf, 'p', v->vparent);
    DumpInt32(buf, 'q', v->uparent);

    if (v->type == vDirectory) {
	/* Dump the Access Control List */
	DumpByteString(buf, 'A', (char *)VVnodeDiskACL(v), VAclDiskSize(v));
    }
}


static void DumpVolumeDiskData(DumpBuffer_t *buf, register VolumeDiskData *vol)
{
    DumpTag(buf, D_VOLUMEDISKDATA);
    DumpInt32(buf, 'i',vol->id);
    DumpInt32(buf, 'v',vol->stamp.version);
    DumpString(buf, 'n',vol->name);
    DumpString(buf, 'P',vol->partition);
    DumpBool(buf, 's',vol->inService);
    DumpBool(buf, '+',vol->blessed);
    DumpInt32(buf, 'u',vol->uniquifier);
    DumpByte(buf, 't',vol->type);
    DumpInt32(buf, 'p',vol->parentId);
    DumpInt32(buf, 'g',vol->groupId);
    DumpInt32(buf, 'c',vol->cloneId);
    DumpInt32(buf, 'b',vol->backupId);
    DumpInt32(buf, 'q',vol->maxquota);
    DumpInt32(buf, 'm',vol->minquota);
    DumpInt32(buf, 'x',vol->maxfiles);
    DumpInt32(buf, 'd',vol->diskused);
    DumpInt32(buf, 'f',vol->filecount);
    DumpShort(buf, 'l',(int)(vol->linkcount));
    DumpInt32(buf, 'a', vol->accountNumber);
    DumpInt32(buf, 'o', vol->owner);
    DumpInt32(buf, 'C',vol->creationDate);	/* Rw volume creation date */
    DumpInt32(buf, 'A',vol->accessDate);
    DumpInt32(buf, 'U',vol->updateDate);
    DumpInt32(buf, 'E',vol->expirationDate);
    DumpInt32(buf, 'B',vol->backupDate);		/* Rw volume backup clone date */
    DumpString(buf, 'O',vol->offlineMessage);
    DumpString(buf, 'M',vol->motd);
    DumpArrayInt32(buf, 'W', (unsigned int *)vol->weekUse,
		  sizeof(vol->weekUse)/sizeof(vol->weekUse[0]));
    DumpInt32(buf, 'D', vol->dayUseDate);
    DumpInt32(buf, 'Z', vol->dayUse);
    DumpVV(buf, 'V', &(vol->versionvector));
}

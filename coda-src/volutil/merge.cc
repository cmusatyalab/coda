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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/volutil/RCS/merge.cc,v 4.1 1997/01/08 21:52:25 rvb Exp $";
#endif /*_BLURB_*/





/*
 * This file contains the utility to merge an incremental dump onto a full dump.
 * Essentially it replaces vnodes in the full dump by vnodes in the incremental.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/file.h>
#include <stdio.h>
#include <errno.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <nfs.h>
#include <vcrcommon.h>
#include <cvnode.h>
#include <volume.h>
#include "dump.h"
#include "dumpstream.h"

typedef struct entry {
    long unique, offset;
    dumpstream *dump;
    struct entry *next;
} ventry;

typedef struct {
    ventry **table;
    long nvnodes, nslots;
} vtable;

int DumpFd = -1;

PRIVATE void BuildTable(dumpstream *, vtable *);
PRIVATE void ModifyTable(dumpstream *, VnodeClass, vtable *);
PRIVATE void WriteTable(DumpBuffer_t *, vtable *, VnodeClass);
PRIVATE void WriteVnodeDiskObject(DumpBuffer_t *, VnodeDiskObject *, int);
PRIVATE void DumpVolumeDiskData(DumpBuffer_t *, register VolumeDiskData *);
PRIVATE void WriteDumpHeader(DumpBuffer_t *buf, struct DumpHeader *, struct DumpHeader *);

#define DUMPBUFSIZE 512000

void main(int argc, char **argv)
{
    if (strcmp(argv[1], "-d") == 0) {
	VolDebugLevel = atoi(argv[2]);
	argv+=2;
	argc-=2;
    }
    
    if (argc < 4) {
	LogMsg(0, VolDebugLevel, stderr, "Usage: %s <outfile> <full dump> <incremental dump>", argv[0]);
	exit(-1);
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

    /* The incremental was taken w.r.t the full if the next test succeeds: */ 
    if (idumphead.oldest != fdumphead.latest) {
	LogMsg(0, VolDebugLevel, stderr, "Error -- %s is not with respect to %s!", argv[3], argv[2]);
	exit(-1);
    }

    DumpFd = open(argv[1], O_WRONLY | O_CREAT | O_EXCL, 00644);
    if (DumpFd <= 0) {
	perror("output file");
	exit(-1);
    }

    char *DumpBuf = (char *)malloc(DUMPBUFSIZE);
    DumpBuffer_t *dbuf = InitDumpBuf((byte *)DumpBuf, (long)DUMPBUFSIZE, DumpFd);
    
    /* At this point both headers should have been read and checked. Write out
       the merged header to the dump file. */

    WriteDumpHeader(dbuf, &fdumphead, &idumphead);
    
    /* skip over the volumediskdata, is there any need to compare it? */
    VolumeDiskData vol;
    assert(fdump.getVolDiskData(&vol) == 0);/* Throw this info away for now... */
    assert(idump.getVolDiskData(&vol) == 0);
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
    DumpTag(dbuf, (byte) D_LARGEINDEX); 
    WriteTable(dbuf, &LTable, vLarge);     /* Output the list of vnodes */

    DumpTag(dbuf, (byte) D_SMALLINDEX);   
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

PRIVATE void BuildTable(dumpstream *dump, vtable *table)
{
    long vnodeNumber;
    char *buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vdo = (VnodeDiskObject *)buf;

    table->table = (ventry **)malloc(sizeof(ventry) * table->nslots);
    bzero(table->table, sizeof(ventry) * table->nslots);
    for (int i = 0; i < table->nvnodes; i++) {
	long offset;
	int deleted;
	if (dump->getNextVnode(vdo, &vnodeNumber, &deleted, &offset) == -1) {
	    LogMsg(0, VolDebugLevel, stderr, "ERROR -- Failed to get a vnode!");
	    exit(-1);
	}

	assert(deleted == 0); /* Can't have deleted vnode in full dump */
	LogMsg(10, VolDebugLevel, stdout, "vnodeNum %d, offset %d", vnodeNumber, offset);
	if (vdo->type != vNull) { /* Insert the vnode into the table */
	    ventry *tmp = (ventry *)malloc(sizeof(ventry));
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

PRIVATE void ModifyTable(dumpstream *dump, VnodeClass vclass, vtable *Table)
{
    char *buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vdo = (VnodeDiskObject *)buf;
    long nvnodes, nslots, vnodeNumber;

    if (dump->getVnodeIndex(vclass, &nvnodes, &nslots) == -1) 
	exit(-1); /* Already printed out an error */

    if (Table->nslots > nslots) {
	LogMsg(0, VolDebugLevel, stderr, "ERROR -- full has more vnode slots than incremental!");
	exit(-1);
    }

    if (nslots > Table->nslots) { /* "Grow" Vnode Array */
	ventry **tmp = (ventry **)malloc(sizeof(ventry) * nslots);
	bcopy(Table->table, tmp, sizeof(ventry) * Table->nslots);
	free(Table->table);
	Table->nslots = nslots;
	Table->table = tmp;
    }

    int deleted;
    long offset;
    while (dump->getNextVnode(vdo, &vnodeNumber, &deleted, &offset) != -1) {
	int vnum = vnodeIdToBitNumber(vnodeNumber);
	assert(vnum >= 0);
	if (vnum > Table->nslots){
	    LogMsg(0, VolDebugLevel, stderr, "vnum %d > nslots %d!", vnum, nslots);
	    exit(-1);
	}
	
	if (deleted) {
	    /* Locate the vnode in LTable and remove it. */
	    ventry *optr = 0, *ptr = Table->table[vnum];
	    while (ptr && (ptr->unique != vdo->uniquifier)) {
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
	    while (ptr && (ptr->unique != vdo->uniquifier))
		ptr = ptr->next;

	    if (ptr) {
		ptr->offset = offset;
		ptr->dump = dump;
	    } else {			/* Must be new, add the entry. */
		ventry *tmp = (ventry *)malloc(sizeof(ventry));
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
PRIVATE void WriteDumpHeader(DumpBuffer_t *buf, struct DumpHeader *head, struct DumpHeader *ihead)
{
    DumpDouble(buf, (byte) D_DUMPHEADER, DUMPBEGINMAGIC, DUMPVERSION);
    DumpLong(buf, 'v', head->volumeId);
    DumpLong(buf, 'p', head->parentId);
    DumpString(buf, 'n', head->volumeName);
    DumpLong(buf, 'b', ihead->backupDate);    /* Date the backup clone was made */
    DumpLong(buf, 'i', head->Incremental);
    DumpDouble(buf, 'I', head->oldest, ihead->latest);
}

PRIVATE void WriteTable(DumpBuffer_t *buf, vtable *table, VnodeClass vclass)
{
    char *vbuf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vdo = (VnodeDiskObject *)vbuf;

    DumpLong(buf, 'v', table->nvnodes);
    DumpLong(buf, 's', table->nslots);
    
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

PRIVATE void WriteVnodeDiskObject(DumpBuffer_t *buf, VnodeDiskObject *v, int vnodeNumber)
{
    DumpDouble(buf, (byte) D_VNODE, vnodeNumber, v->uniquifier);
    DumpByte(buf, 't', v->type);
    DumpShort(buf, 'b', v->modeBits);
    DumpShort(buf, 'l', v->linkCount); /* May not need this */
    DumpLong(buf, 'L', v->length);
    DumpLong(buf, 'v', v->dataVersion);
    DumpVV(buf, 'V', (ViceVersionVector *)(&(v->versionvector)));
    DumpLong(buf, 'm', v->unixModifyTime);
    DumpLong(buf, 'a', v->author);
    DumpLong(buf, 'o', v->owner);
    DumpLong(buf, 'p', v->vparent);
    DumpLong(buf, 'q', v->uparent);

    if (v->type == vDirectory) {
	/* Dump the Access Control List */
	DumpByteString(buf, 'A', (byte *) VVnodeDiskACL(v), VAclDiskSize(v));
    }
}


PRIVATE void DumpVolumeDiskData(DumpBuffer_t *buf, register VolumeDiskData *vol)
{
    DumpTag(buf, (byte) D_VOLUMEDISKDATA);
    DumpLong(buf, 'i',vol->id);
    DumpLong(buf, 'v',vol->stamp.version);
    DumpString(buf, 'n',vol->name);
    DumpString(buf, 'P',vol->partition);
    DumpBool(buf, 's',vol->inService);
    DumpBool(buf, '+',vol->blessed);
    DumpLong(buf, 'u',vol->uniquifier);
    DumpByte(buf, 't',vol->type);
    DumpLong(buf, 'p',vol->parentId);
    DumpLong(buf, 'g',vol->groupId);
    DumpLong(buf, 'c',vol->cloneId);
    DumpLong(buf, 'b',vol->backupId);
    DumpLong(buf, 'q',vol->maxquota);
    DumpLong(buf, 'm',vol->minquota);
    DumpLong(buf, 'x',vol->maxfiles);
    DumpLong(buf, 'd',vol->diskused);
    DumpLong(buf, 'f',vol->filecount);
    DumpShort(buf, 'l',(int)(vol->linkcount));
    DumpLong(buf, 'a', vol->accountNumber);
    DumpLong(buf, 'o', vol->owner);
    DumpLong(buf, 'C',vol->creationDate);	/* Rw volume creation date */
    DumpLong(buf, 'A',vol->accessDate);
    DumpLong(buf, 'U',vol->updateDate);
    DumpLong(buf, 'E',vol->expirationDate);
    DumpLong(buf, 'B',vol->backupDate);		/* Rw volume backup clone date */
    DumpString(buf, 'O',vol->offlineMessage);
    DumpString(buf, 'M',vol->motd);
    DumpArrayLong(buf, 'W', (unsigned long *)vol->weekUse, sizeof(vol->weekUse)/sizeof(vol->weekUse[0]));
    DumpLong(buf, 'D', vol->dayUseDate);
    DumpLong(buf, 'Z', vol->dayUse);
    DumpVV(buf, 'V', &(vol->versionvector));
}

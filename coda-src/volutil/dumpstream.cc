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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/dumpstream.cc,v 4.6 1998/10/30 18:30:01 braam Exp $";
#endif /*_BLURB_*/





/*
 * Module to define a dump file stream class, with dedicated functions
 * to get and put information into it.
 */
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
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

#include <voltypes.h>
#include <vcrcommon.h>
#include <cvnode.h>
#include <volume.h>
#define PAGESIZE 2048	/* This is a problem, but can't inherit dirvnode.h */
#include <dump.h>
#include "dumpstream.h"
#include <util.h>

/*************************** From readstuff.c */

int GetShort(FILE *stream, unsigned short *sp) { /* Assuming two byte words. */
    unsigned char a, b;
    a = fgetc(stream);
    b = fgetc(stream);
    if (feof(stream))
	return FALSE;

    unsigned short v = (a << 8) | b;
    *sp = v;
    return TRUE;
}

int GetLong(FILE *stream, unsigned long *lp) {	 /* Assuming four byte words. */
    unsigned char a, b, c, d;
    a = fgetc(stream);
    b = fgetc(stream);
    c = fgetc(stream);
    d = fgetc(stream);
    if (feof(stream))
	return FALSE;

    unsigned long v = (a << 24) | (b << 16) | (c << 8) | d;
    *lp = v;
    return TRUE;
}

int GetString(FILE *stream, register char *to, register int max)
{
    unsigned long len;
    if (!GetLong(stream, &len))
	return FALSE;

    if (len + 1 > max) {	/* Ensure we only use max room */
	len = max - 1;
	LogMsg(0, VolDebugLevel, stdout, "GetString: String longer than max (%d>%d) truncating.",len,max);
    }
    
    while (len--)
	*to++ = fgetc(stream);

    to[len] = 0;		/* Make it null terminated */

    if (feof(stream))
	return FALSE;

    return TRUE;
}

int GetByteString(FILE *stream, register byte *to, register int size)
{
    while (size--)
	*to++ = fgetc(stream);

    if (feof(stream))
	return FALSE;

    return TRUE;
}

int GetVV(FILE *stream, register vv_t *vv)
{
    register tag;
    while ((tag = fgetc(stream)) > D_MAX && tag) {
	switch (tag) {
	    case '0':
		if (!GetLong(stream, (unsigned long *)&vv->Versions.Site0))
		    return FALSE;
		break;
	    case '1':
		if (!GetLong(stream, (unsigned long *)&vv->Versions.Site1))
		    return FALSE;
		break;
	    case '2':
		if (!GetLong(stream, (unsigned long *)&vv->Versions.Site2))
		    return FALSE;
		break;
	    case '3':
		if (!GetLong(stream, (unsigned long *)&vv->Versions.Site3))
		    return FALSE;
		break;
	    case '4':
		if (!GetLong(stream, (unsigned long *)&vv->Versions.Site4))
		    return FALSE;
		break;
	    case '5':
		if (!GetLong(stream, (unsigned long *)&vv->Versions.Site5))
		    return FALSE;
		break;
	    case '6':
		if (!GetLong(stream, (unsigned long *)&vv->Versions.Site6))
		    return FALSE;
		break;
	    case '7':
		if (!GetLong(stream, (unsigned long *)&vv->Versions.Site7))
		    return FALSE;
		break;
	    case 's':
		if (!GetLong(stream, (unsigned long *)(unsigned long *)&vv->StoreId.Host))
		    return FALSE;
		break;
	    case 'u':
		if (!GetLong(stream, (unsigned long *)&vv->StoreId.Uniquifier))
		    return FALSE;
		break;
	    case 'f':
		if (!GetLong(stream, (unsigned long *)&vv->Flags))
		    return FALSE;
		break;
	}
    }
    if (tag != EOF && tag != (byte)D_ENDVV) {
	LogMsg(0, VolDebugLevel, stdout, "GetVV: Error at end of VV");
	return FALSE;
    }

    return TRUE;
}

/*************************** From readstuff.c */

/* We also need a bogus definition of WriteDump. This should never be called */
long WriteDump(RPC2_Handle _cid, RPC2_Unsigned offset, RPC2_Unsigned *nbytes, RPC2_Unsigned volid, SE_Descriptor *sed)
{
    printf("In WRITEDUMP. should NEVER get here!\n");
    CODA_ASSERT(0);
    return(0); /* to keep C++ happy */
}



/* NOTE: there's gobs of duplication here with readstuff.c. Main problem is
 * that here we need to do seeks() through the stream to find vnodes on the
 * second pass. That doesn't seem necessarily useful in readstuff.
 */

dumpstream::dumpstream(char *filename)
{
    stream = fopen(filename, "r");
    if (stream == NULL) {
	LogMsg(0, VolDebugLevel, stderr, "Can't open dump file %s", filename);
	exit(-1);
    }
    strncpy(name, filename, MAXSTRLEN);
    name[MAXSTRLEN - 1] = (char)0;	/* Ensure last char is null */
    IndexType = -1;
}       

dumpstream::getDumpHeader(struct DumpHeader *hp)
{
    register tag;
    unsigned long beginMagic;
    if (fgetc(stream) != D_DUMPHEADER
       || !GetLong(stream, &beginMagic)
       || !GetLong(stream, (unsigned long *)&hp->version)
       || beginMagic != DUMPBEGINMAGIC)
	return 0;
    hp->volumeId = 0;
    while ((tag = fgetc(stream)) > D_MAX) {
	switch(tag) {
	    case 'v':
	    	if (!GetLong(stream, &hp->volumeId))
		    return 0;
		break;
	    case 'p':
	    	if (!GetLong(stream, &hp->parentId))
		    return 0;
		break;
	    case 'n':
	        GetString(stream, hp->volumeName, (int) sizeof(hp->volumeName));
		break;
	    case 'b':
	        if (!GetLong(stream, &hp->backupDate))
		    return 0;
		break;
	    case 'i' :
		if (!GetLong(stream, (unsigned long *)&hp->Incremental))
		    return 0;
		break;
	    case 'I' :
		if (!GetLong(stream, (unsigned long *)&hp->oldest) || !GetLong(stream, (unsigned long *)&hp->latest))
		    return 0;
		break;
	}
    }
    if (!hp->volumeId) {
	return 0;
    }
    ungetc(tag,stream);
    return 1;
}

    
dumpstream::getVolDiskData(VolumeDiskData *vol)
{
    register tag;
    bzero((char *)vol, (int) sizeof(*vol));

    if (fgetc(stream) != D_VOLUMEDISKDATA) {
	LogMsg(0, VolDebugLevel, stdout, "Volume header missing from dump %s!\n", name);
	/* Return the appropriate error code. */
	return -5;
    }

    while ((tag = fgetc(stream)) > D_MAX && tag != EOF) {
	switch (tag) {
	    case 'i':
		GetLong(stream, &vol->id);
		break;
	    case 'v':
	        GetLong(stream, &(vol->stamp.version));
		break;
	    case 'n':
		GetString(stream, vol->name, (int) sizeof(vol->name));
		break;
	    case 'P':
		GetString(stream, vol->partition, (int) sizeof(vol->partition));
		break;
	    case 's':
		vol->inService = fgetc(stream);
		break;
	    case '+':
		vol->blessed = fgetc(stream);
		break;
	    case 'u':
		GetLong(stream, &vol->uniquifier);
		break;
	    case 't':
		vol->type = fgetc(stream);
		break;
	    case 'p':
	        GetLong(stream, &vol->parentId);
		break;
	    case 'g':
		GetLong(stream, &vol->groupId);
		break;
	    case 'c':
	        GetLong(stream, &vol->cloneId);
		break;
	    case 'b' :
		GetLong(stream, &vol->backupId);
		break;
	    case 'q':
	        GetLong(stream, (unsigned long *)&vol->maxquota);
		break;
	    case 'm':
		GetLong(stream, (unsigned long *)&vol->minquota);
		break;
	    case 'x':
		GetLong(stream, (unsigned long *)&vol->maxfiles);
		break;
	    case 'd':
	        GetLong(stream, (unsigned long *)&vol->diskused); /* Bogus:  should calculate this */
		break;
	    case 'f':
		GetLong(stream, (unsigned long *)&vol->filecount);
		break;
	    case 'l': 
		GetShort(stream, (unsigned short *)&vol->linkcount);
		break;
	    case 'a':
		GetLong(stream, &vol->accountNumber);
		break;
	    case 'o':
	  	GetLong(stream, &vol->owner);
		break;
	    case 'C':
		GetLong(stream, &vol->creationDate);
		break;
	    case 'A':
		GetLong(stream, &vol->accessDate);
		break;
	    case 'U':
	    	GetLong(stream, &vol->updateDate);
		break;
	    case 'E':
	    	GetLong(stream, &vol->expirationDate);
		break;
	    case 'B':
	    	GetLong(stream, &vol->backupDate);
		break;
	    case 'O':
	    	GetString(stream, vol->offlineMessage, (int) sizeof(vol->offlineMessage));
		break;
	    case 'M':
		GetString(stream, vol->motd, (int) sizeof(vol->motd));
		break;
	    case 'W': {
		unsigned long length;
		int i;
    		unsigned long data;
	  	GetLong(stream, &length);
		for (i = 0; i<length; i++) {
		    GetLong(stream, &data);
		    if (i < sizeof(vol->weekUse)/sizeof(vol->weekUse[0]))
			vol->weekUse[i] = data;
		}
		break;
	    }
	    case 'D':
		GetLong(stream, &vol->dayUseDate);
		break;
	    case 'Z':
		GetLong(stream, (unsigned long *)&vol->dayUse);
		break;
	    case 'V':
		GetVV(stream, &vol->versionvector);
		break;
	}
    }
    ungetc(tag, stream);
    return 0;
}

/* This routine checks that the file argument passed is positioned at the
   magic number marking the end of a dump */
int dumpstream::EndOfDump()
{
    unsigned long magic;

    /* Skip over whatever garbage exists on the stream (remains of last vnode) */
    skip_vnode_garbage();

    if (fgetc(stream) != D_DUMPEND) {
	LogMsg(0, VolDebugLevel, stderr, "End of dump not found for %s", name);
	return -1;
    }

    GetLong(stream, &magic);
    if (magic != DUMPENDMAGIC) {
	LogMsg(0, VolDebugLevel, stderr, "Dump Magic Value Incorrect for %s", name);
	return -1;
    }

    if (fgetc(stream) != EOF) {
	LogMsg(0, VolDebugLevel, stderr, "Unrecognized postamble in dump %s", name);
	return -1;
    }

    return 0;
}

int dumpstream::getVnodeIndex(VnodeClass Type, long *nVnodes, long *listsize)
{
    register signed char tag;
    /* Skip over whatever garbage exists on the stream (remains of last vnode) */
    skip_vnode_garbage();

    if (Type == vLarge) {
	IndexType = vLarge;
	if (fgetc(stream) != D_LARGEINDEX) {
	    LogMsg(0, VolDebugLevel, stderr, "ERROR: Large Index not found in %s", name);
	    return -1;
	}
    } else if (Type == vSmall) {
	IndexType = vSmall;
	if (fgetc(stream) != D_SMALLINDEX) {
	    LogMsg(0, VolDebugLevel, stderr, "ERROR: Small Index not found in %s", name);
	    return -1;
	}
    } else {
	LogMsg(0, VolDebugLevel, stderr, "Illegal type passed to GetVnodeIndex!");
	return -1;
    }

    while ((tag = fgetc(stream)) > D_MAX && tag != EOF) {
	switch(tag) {
	  case 'v':
	    if (!GetLong(stream, (unsigned long *)nVnodes))
		return -1;
	    break;
	  case 's':
	    if (!GetLong(stream, (unsigned long *)listsize))
		return -1;
	    break;
	  default:
	    LogMsg(0, VolDebugLevel, stderr, "Unexpected field of Vnode found in %s.", name);
	    exit(-1);
	}
    }
    CODA_ASSERT(*listsize > 0);
    ungetc(tag, stream);
    return 0;
}    

/* next byte is either a vnode tag, Index tag, D_FILEDATA or D_DIRPAGES. */
int dumpstream::skip_vnode_garbage()
{
    char buf[4096];
    int size; 
    
    register tag = fgetc(stream);

    if (tag == D_DIRPAGES) {
	long npages;
	int size = PAGESIZE;

	CODA_ASSERT (IndexType == vLarge);
	LogMsg(10, VolDebugLevel, stdout, "SkipVnodeData: Skipping dirpages for %s", name);
	if (!GetLong(stream, (unsigned long *)&npages))
	    return -1;
	
	for (int i = 0; i < npages; i++){ 	/* Skip directory pages */
	    tag = fgetc(stream);
	    if (tag != 'P'){
		LogMsg(0, VolDebugLevel, stderr, "Restore: Dir page does not have a P tag");
		return -1;
	    }
	    GetByteString(stream, (byte *)buf, size);
	} 
    } else if (tag == D_FILEDATA) {
	size = (int) sizeof(buf);
	long nbytes, filesize;

	CODA_ASSERT (IndexType == vSmall);
	LogMsg(10, VolDebugLevel, stdout, "SkipVnodeData: Skipping file data for %s", name);

	if (!GetLong(stream, (unsigned long *)&filesize))
	    return -1;

	for (nbytes = filesize; nbytes; nbytes -= size) { /* Skip the filedata */
	    if (nbytes < size)
		size = nbytes;
	    if (fread(buf, size, 1, stream) != 1) {
		LogMsg(0, VolDebugLevel, stderr, "Error reading file from dump %s", name);
		return -1; 
	    }
	}
    } else {
	ungetc(tag, stream); 
    }
    return 0;
}

/*
 * fseek to offset and read in the Vnode there. Assume IndexType is set correctly.
 */
int dumpstream::getVnode(int vnum, long unique, long Offset, VnodeDiskObject *vdo)
{
    LogMsg(10, VolDebugLevel, stdout, "getVnode: vnum %d unique %d Offset %x Stream %s", vnum, unique, Offset, name);
    fseek(stream, Offset, 0);	/* Should I calculate the relative? */

    int deleted;
    long vnodeNumber, offset;
    int result = getNextVnode(vdo, &vnodeNumber, &deleted, &offset);

    if (result)
	return result;
    
    LogMsg(10, VolDebugLevel, stdout, "getVnode after getNextVnode: (%d, %d, %x)", vnodeNumber, deleted, offset);
    CODA_ASSERT(Offset == offset);

    CODA_ASSERT(vnum == vnodeNumber);	/* They'd better match! */
    CODA_ASSERT(unique == vdo->uniquifier);
    return 0;
}

/*
 * The idea here is to make the dump look like a series of VnodeDiskObjects.
 * Handle the file or directory data associated with Vnodes transparently.
 */

dumpstream::getNextVnode(VnodeDiskObject *vdop, long *vnodeNumber, int *deleted, long *offset)
{
    *deleted = 0;
    /* Skip over whatever garbage exists on the stream (remains of last vnode) */
    skip_vnode_garbage();

    *offset = ftell(stream);
    /* Now we'd better either be reading a valid or a null Vnode */
    register tag = fgetc(stream); 
    if (tag == D_NULLVNODE){
	vdop->type = vNull;
	return 0;
    } else if (tag == D_RMVNODE) { /* Vnode was deleted */
	if (!GetLong(stream, (unsigned long *)vnodeNumber) ||
	    !GetLong(stream, &vdop->uniquifier)) 
	    return -1;
	LogMsg(10, VolDebugLevel, stdout, "Deleted Vnode (%x.%x) found ",
	       *vnodeNumber, vdop->uniquifier);
	*deleted = 1;
	return 0;
    } else if (tag != D_VNODE){	   /* Probably read past last vnode in list */
	LogMsg(10, VolDebugLevel, stdout, "Ackk, bogus tag found %c", tag);
	ungetc(tag, stream);
	return -1;
    }

    LogMsg(10, VolDebugLevel, stdout, "GetNextVnode: Real Vnode found! ");
    if (!GetLong(stream, (unsigned long *)vnodeNumber) ||
	!GetLong(stream, &vdop->uniquifier))
	return -1;
    LogMsg(10, VolDebugLevel, stdout, "%d", *vnodeNumber);

    while ((tag = fgetc(stream)) > D_MAX && tag != EOF) {
	switch (tag) {
	  case 't':
	    vdop->type = (VnodeType) fgetc(stream); 
	    break;
	  case 'b': 
	    short modeBits;
	    GetShort(stream, (unsigned short *)&modeBits);
	    vdop->modeBits = modeBits;
	    break;
	  case 'l':
	    GetShort(stream, &vdop->linkCount);
	    break;
	  case 'L':
	    GetLong(stream, &vdop->length);
	    break;
	  case 'v':
	    GetLong(stream, &vdop->dataVersion);
	    break;
	  case 'V':
	    GetVV(stream, &vdop->versionvector);
	    break;
	  case 'm':
	    GetLong(stream, &vdop->unixModifyTime);
	    break;
	  case 'a':
	    GetLong(stream, &vdop->author);
	    break;
	  case 'o':
	    GetLong(stream, &vdop->owner);
	    break;
	  case 'p':
	    GetLong(stream, &vdop->vparent);
	    break;
	  case 'q':
	    GetLong(stream, &vdop->uparent);
	    break;
	  case 'A':
	    CODA_ASSERT(vdop->type == vDirectory);
	    GetByteString(stream, (byte *)VVnodeDiskACL(vdop), VAclDiskSize(vdop));
	    break;

	  default:
	    break;
	}
    }

    ungetc(tag, stream);
    return 0;
}

/* Copy the file or directory data which should be next in the stream to out */
/* Actually use the bogus Dump mechanism in DumpFD, it should be rewritten soon.*/
extern byte *Reserve(DumpBuffer_t *, int);
int
dumpstream::copyVnodeData(DumpBuffer_t *dbuf)
{
    register tag = fgetc(stream);
    char buf[PAGESIZE];
    register nbytes;

    LogMsg(10, VolDebugLevel, stdout, "Copy:%s type %x", (IndexType == vLarge?"Large":"Small"), tag);
    if (IndexType == vLarge) {
	CODA_ASSERT(tag == D_DIRPAGES); /* Do something similar for dirpages. */

	/* We get a number of pages, pages are PAGESIZE bytes long. */
	long num_pages;
	
	if (!GetLong(stream, (unsigned long *)&num_pages))
	    return -1;

	DumpLong(dbuf, D_DIRPAGES, num_pages);

	for (int i = 0; i < num_pages; i++) { /* copy PAGESIZE bytes to output. */
	    tag = fgetc(stream);
	    if (tag != 'P'){
		LogMsg(0, VolDebugLevel, stderr, "Restore: Dir page does not have a P tag");
		return -1;
	    }
	    LogMsg(10, VolDebugLevel, stdout, "copying one page for %s", name);
	    if (fread(buf, PAGESIZE, 1, stream) != 1) {
		LogMsg(0, VolDebugLevel, stderr, "Error reading dump file %s.", name);
		return -1;
	    }
	    DumpByteString(dbuf, (byte)'P', (byte *)buf, PAGESIZE);
	}
    } else if (IndexType == vSmall) {
	/* May not have a file associated with this vnode? don't think so...*/
	CODA_ASSERT(tag == D_FILEDATA);

	/* First need to output the tag and the length */
	long filesize, size = PAGESIZE;
	if (!GetLong(stream, (unsigned long *)&filesize)) 
	    return -1;
	byte *p;
	DumpLong(dbuf, D_FILEDATA, filesize);
	for (nbytes = filesize; nbytes; nbytes -= size) {
	    if (nbytes < size)
		size = nbytes;
	    p = Reserve(dbuf, size);	/* Mark off size bytes in output */
	    if (fread((char *)p, size, 1, stream) != 1) {
		LogMsg(0, VolDebugLevel, stderr, "Error reading dump file %s.", name);
		return -1;
	    }
	}
	return filesize;
    } 
    return 0;	    
}

void dumpstream::setIndex(VnodeClass vclass)
{
    CODA_ASSERT((vclass == vLarge) || (vclass == vSmall));
    IndexType = vclass;
}

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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/readstuff.cc,v 4.2 1997/02/26 16:04:04 rvb Exp $";
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

#include <lock.h>
#include <lwp.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <vutil.h>
#include <partition.h>
#include "dump.h"
#include "voldump.h"

/* Facts: if (offset = 0), the buffer hasn't been filled yet.
 * If DumpBufPtr+size >= BufEnd, need to refill the buffer.
 * If we're forced to refill the buffer, but not all of it has been used
 * yet, reset the offset into the file by the amount of the buffer not yet used.
 * I assume that a get is never done between a get and a put of that value.
 * (this implies no multiple puts are ever done.)
 * if out of dumpfile -- how should errors be reported?
 */

#define VOLID DumpFd		/* Overload this field if using newstyle dump */


PRIVATE char *get(DumpBuffer_t *buf, int size, int *error)
{
    char *retptr;
    unsigned long nbytes;
    LogMsg(100, VolDebugLevel, stdout, "**get: buf at 0x%x, size %d", buf, size);

    if ((buf->offset == 0) || (buf->DumpBufPtr + size > buf->DumpBufEnd)) {
	if (buf->offset != 0) {		/* Only happens on refill */
	    /* Copy the unused portion of the buffer to its start */
	    nbytes = buf->DumpBufEnd - buf->DumpBufPtr;

	    if (buf->DumpBuf + nbytes > buf->DumpBufPtr) { /* Is this an error? */
		LogMsg(0, VolDebugLevel, stdout, "Refilling buffer overflow: buf:%x ptr:%x n:%x",
		    buf->DumpBuf, buf->DumpBufPtr, nbytes);
	    }

	    /* Save unused portion of buffer. */
	    bcopy(buf->DumpBufPtr, buf->DumpBuf, (int)nbytes);	

	    buf->DumpBufPtr = buf->DumpBuf + nbytes;
	}

	/* We need to refill the buffer */
	if (buf->rpcid > 0) {
	    SE_Descriptor sed;
	    sed.Tag = SMARTFTP;
	    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	    sed.Value.SmartFTPD.ByteQuota = -1;
	    sed.Value.SmartFTPD.SeekOffset = 0;
	    sed.Value.SmartFTPD.Tag = FILEINVM;
	    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = buf->DumpBufPtr;
	    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = nbytes = 
		buf->DumpBufEnd - buf->DumpBufPtr; /* # of bytes in the buf */

	    LogMsg(2, SrvDebugLevel, stdout, "ReadDump: Requesting %d bytes.", nbytes);
	
	    unsigned long before = time(0);
	    RPC2_Integer numBytes = (RPC2_Integer)nbytes;
	    
	    int rc = ReadDump(buf->rpcid, (RPC2_Unsigned)buf->offset, &numBytes, buf->VOLID, &sed);
	    unsigned long after = time(0);
	    if (rc != RPC2_SUCCESS) {
		LogMsg(0, VolDebugLevel, stdout, "ReadStuff: ReadDump failed %s.", RPC2_ErrorMsg(rc));
		*error = rc;
		buf->rpcid = -1;
		return NULL;
	    }

	    buf->secs += (after - before);
	    LogMsg(2, SrvDebugLevel, stdout,"ReadDump: got %d bytes.", sed.Value.SmartFTPD.BytesTransferred);
    
	    if (nbytes != (buf->DumpBufEnd - buf->DumpBufPtr)) {
		LogMsg(2, VolDebugLevel, stdout, "ReadStuff: ReadDump didn't fetch enough -- end of dump.");
	    }
	    if (nbytes == 0) *error = EOF;

	    /* For debugging rpc2 connection -- end to end sanity check. */
	    int debug = 0;
	    if (debug) {
		int fd = open("/tmp/restore",O_APPEND | O_CREAT | O_WRONLY, 0755);
		if (fd < 0) LogMsg(0, VolDebugLevel, stdout, "Open failed!");
		else {
		    int n = write(fd, buf->DumpBufPtr, (int)nbytes);
		    if (n != nbytes) {
			LogMsg(0, VolDebugLevel, stdout, "Couldn't write %d bytes!", nbytes);
		    }
		    close(fd);
		}
	    }
	} else {
	    if (read(buf->DumpFd, buf->DumpBuf, buf->DumpBufPtr-buf->DumpBuf) !=
		buf->DumpBufPtr-buf->DumpBuf) {
		LogMsg(0, VolDebugLevel, stdout, "Dump: error reading dump; aborted");
		*error = errno;
		return 0;
	    }
	    nbytes = buf->DumpBufPtr-buf->DumpBuf;
	}

	buf->offset += nbytes; /* Update number of bytes written */
	buf->nbytes += nbytes;
	buf->DumpBufPtr = buf->DumpBuf;       /* reset DumpBufPtr to beginning. */
    }
    
    /* Increment buffer pointer */
    retptr = (char *)buf->DumpBufPtr;
    buf->DumpBufPtr += size;
    return (char *)retptr;
}

PRIVATE char *put(DumpBuffer_t *buf, int size, int *error)
{
    *error = 0;
    assert(buf->DumpBufPtr - size >= buf->DumpBuf);
    buf->DumpBufPtr -= size;
    return (char *) buf->DumpBufPtr;
}

char ReadTag(DumpBuffer_t *buf)
{
    int error = 0;
    register byte *p = (byte *)get(buf, 1, &error);
    if (!p || (error == EOF)) return FALSE;
    return *p;
}

int PutTag(char tag, DumpBuffer_t *buf)
{
    int error = 0;
    register byte *p = (byte *)put(buf, 1, &error);
    if (!p || (error == EOF)) return FALSE;
    *p = tag;
    return TRUE;
}

int ReadShort(DumpBuffer_t *buf, unsigned short *sp)
{
    int error = 0;
    unsigned short v = 0;
    unsigned char *p = (unsigned char *)get(buf, sizeof(short), &error);
    if (!p || (error == EOF)) return FALSE;
    v = (p[0] << 8) | (p[1]);
    *sp = v;
    return TRUE;	/* Return TRUE if successful */
}

int ReadLong(DumpBuffer_t *buf, unsigned long *lp)
{
    int error = 0;
    unsigned long v = 0;
    unsigned char *p = (unsigned char *)get(buf, sizeof(long), &error);
    if (!p || (error == EOF)) return FALSE;
    v = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | (p[3]);
    *lp = v;
    return TRUE;
}

int ReadString(register DumpBuffer_t *buf, register char *to, register int max)
{
    int error = 0;
    unsigned long len;
    if (!ReadLong(buf, &len)) 
	return FALSE;

    register char *str = (char *)get(buf, (int)len, &error);
    if (!str || (error == EOF)) return FALSE;

    if (len + 1 > max) { 	/* Ensure we only use max room */
	len = max - 1;
	LogMsg(0, VolDebugLevel, stdout,"ReadString: String longer than max (%d>%d) truncating.",len,max);
    }

    strncpy(to, str, (int)len);
    to[len] = 0;		/* Make it null terminated */
    
#ifdef 	NOTDEF
  if (to[-1]) {			/* Scan for the end of the string. */
	while ((c = getc(buf)) && c != EOF);
	to[-1] = 0;
    }
#endif NOTDEF
    return TRUE;
}

int ReadByteString(DumpBuffer_t *buf, register byte *to, register int size)
{
    int error = 0;
    register byte *str = (byte *)get(buf, size, &error);
    if (!str || (error == EOF)) return FALSE;

    while (size--)
	*to++ = *str++;
    return TRUE;
}

int ReadVV(register DumpBuffer_t *buf, register vv_t *vv)
{
    register tag;
    while ((tag = ReadTag(buf)) > D_MAX && tag) {
	switch (tag) {
	    case '0':
		if (!ReadLong(buf, (unsigned long *)&vv->Versions.Site0))
		    return FALSE;
		break;
	    case '1':
		if (!ReadLong(buf, (unsigned long *)&vv->Versions.Site1))
		    return FALSE;
		break;
	    case '2':
		if (!ReadLong(buf, (unsigned long *)&vv->Versions.Site2))
		    return FALSE;
		break;
	    case '3':
		if (!ReadLong(buf, (unsigned long *)&vv->Versions.Site3))
		    return FALSE;
		break;
	    case '4':
		if (!ReadLong(buf, (unsigned long *)&vv->Versions.Site4))
		    return FALSE;
		break;
	    case '5':
		if (!ReadLong(buf, (unsigned long *)&vv->Versions.Site5))
		    return FALSE;
		break;
	    case '6':
		if (!ReadLong(buf, (unsigned long *)&vv->Versions.Site6))
		    return FALSE;
		break;
	    case '7':
		if (!ReadLong(buf, (unsigned long *)&vv->Versions.Site7))
		    return FALSE;
		break;
	    case 's':
		if (!ReadLong(buf, (unsigned long *)&vv->StoreId.Host))
		    return FALSE;
		break;
	    case 'u':
		if (!ReadLong(buf, (unsigned long *)&vv->StoreId.Uniquifier))
		    return FALSE;
		break;
	    case 'f':
		if (!ReadLong(buf, (unsigned long *)&vv->Flags))
		    return FALSE;
		break;
	}
    }
    if (tag != EOF && tag != (byte)D_ENDVV) {
	LogMsg(0, VolDebugLevel, stdout, "ReadVV: Error at end of VV");
	return FALSE;
    }

    return TRUE;
}


int ReadDumpHeader(DumpBuffer_t *buf, struct DumpHeader *hp)
{
    register tag;
    unsigned long beginMagic;
    if (ReadTag(buf) != D_DUMPHEADER
       || !ReadLong(buf, (unsigned long *)&beginMagic) || !ReadLong(buf, (unsigned long *)&hp->version)
       || beginMagic != DUMPBEGINMAGIC
       ) return FALSE;
    hp->volumeId = 0;
    while ((tag = ReadTag(buf)) > D_MAX && tag) {
	switch(tag) {
	    case 'v':
	    	if (!ReadLong(buf, (unsigned long *)&hp->volumeId))
		    return FALSE;
		break;
	    case 'p':
	    	if (!ReadLong(buf, (unsigned long *)&hp->parentId))
		    return FALSE;
		break;
	    case 'n':
	        if (!ReadString(buf, hp->volumeName, sizeof(hp->volumeName)))
		    return FALSE;
		break;
	    case 'b':
	        if (!ReadLong(buf, (unsigned long *)&hp->backupDate))
		    return FALSE;
		break;
	    case 'i' :
		if (!ReadLong(buf, (unsigned long *)&hp->Incremental))
		    return FALSE;
		break;
	    case 'I' :
		if (!ReadLong(buf, (unsigned long *)&hp->oldest) || !ReadLong(buf, (unsigned long *)&hp->latest))
		    return FALSE;
		break;
	}
    }
    if (!hp->volumeId) {
	return FALSE;
    }
    PutTag(tag,buf);
    return TRUE;
}

/* This routine checks that the file argument passed is positioned at the magic number marking the end
   of a dump */
int EndOfDump(DumpBuffer_t *buf)
{
    long magic;

    if (ReadTag(buf) != D_DUMPEND) {
	LogMsg(0, VolDebugLevel, stdout, "End of dump not	found; restore aborted");
	return FALSE;
    }

    if (!ReadLong(buf, (unsigned long *)&magic) || (magic != DUMPENDMAGIC)) {
	LogMsg(0, VolDebugLevel, stdout, "Dump Magic Value Incorrect; restore aborted");
	return FALSE;
    }
    
#ifdef UNDEF		/* No way to detect last byte in dump with RPC2 ... */
    if (ReadTag(buf) != EOF) {
	LogMsg(0, VolDebugLevel, stdout, "Unrecognized postamble in dump; restore aborted");
	return FALSE;
    }
#endif UNDEF
    return TRUE;
}

/* Let ReadTag catch any errors that may occur. If Read* operations happen
 * after an error, effectively they become noops.
 */
int ReadVolumeDiskData(DumpBuffer_t *buf, VolumeDiskData *vol)
{
    register tag;
    bzero((char *)vol, sizeof(*vol));
    while ((tag = ReadTag(buf)) > D_MAX && tag) {
	switch (tag) {
	    case 'i':
		ReadLong(buf, &vol->id);
		break;
	    case 'v':
	        ReadLong(buf, &(vol->stamp.version));
		break;
	    case 'n':
		ReadString(buf, vol->name, sizeof(vol->name));
		break;
	    case 'P':
		ReadString(buf, vol->partition, sizeof(vol->partition));
		break;
	    case 's':
		vol->inService = ReadTag(buf);
		break;
	    case '+':
		vol->blessed = ReadTag(buf);
		break;
	    case 'u':
		ReadLong(buf, &vol->uniquifier);
		break;
	    case 't':
		vol->type = ReadTag(buf);
		break;
	    case 'p':
	        ReadLong(buf, &vol->parentId);
		break;
	    case 'g':
		ReadLong(buf, &vol->groupId);
		break;
	    case 'c':
	        ReadLong(buf, &vol->cloneId);
		break;
	    case 'b' :
		ReadLong(buf, &vol->backupId);
		break;
	    case 'q':
	        ReadLong(buf, (unsigned long *)&vol->maxquota);
		break;
	    case 'm':
		ReadLong(buf, (unsigned long *)&vol->minquota);
		break;
	    case 'x':
		ReadLong(buf, (unsigned long *)&vol->maxfiles);
		break;
	    case 'd':
	        ReadLong(buf, (unsigned long *)&vol->diskused); /* Bogus:  should calculate this */
		break;
	    case 'f':
		ReadLong(buf, (unsigned long *)&vol->filecount);
		break;
	    case 'l': 
		ReadShort(buf, (unsigned short *)&vol->linkcount);
		break;
	    case 'a':
		ReadLong(buf, &vol->accountNumber);
		break;
	    case 'o':
	  	ReadLong(buf, &vol->owner);
		break;
	    case 'C':
		ReadLong(buf, &vol->creationDate);
		break;
	    case 'A':
		ReadLong(buf, &vol->accessDate);
		break;
	    case 'U':
	    	ReadLong(buf, &vol->updateDate);
		break;
	    case 'E':
	    	ReadLong(buf, &vol->expirationDate);
		break;
	    case 'B':
	    	ReadLong(buf, &vol->backupDate);
		break;
	    case 'O':
	    	ReadString(buf, vol->offlineMessage, sizeof(vol->offlineMessage));
		break;
	    case 'M':
		ReadString(buf, vol->motd, sizeof(vol->motd));
		break;
	    case 'W': {
		unsigned long length;
		int i;
    		unsigned long data;
	  	ReadLong(buf, &length);
		for (i = 0; i<length; i++) {
		    ReadLong(buf, &data);
		    if (i < sizeof(vol->weekUse)/sizeof(vol->weekUse[0]))
			vol->weekUse[i] = (int)data;
		}
		break;
	    }
	    case 'D':
		ReadLong(buf, &vol->dayUseDate);
		break;
	    case 'Z':
		ReadLong(buf, (unsigned long *)&vol->dayUse);
		break;
	    case 'V':
		ReadVV(buf, &vol->versionvector);
		break;
	    }
    }
    if (!tag) return FALSE;
    PutTag(tag, buf);
    return TRUE;

}

int ReadFile(DumpBuffer_t *buf, FILE *outfile)
{
    char *bptr;
    int error = 0;
    long filesize;
    register long size = 4096;		/* What's the best value for this? */
    register long nbytes;

    if (!ReadLong(buf, (unsigned long *)&filesize))
	return -1;
    for (nbytes = filesize; nbytes; nbytes -= size) {
	if (nbytes < size)
	    size = nbytes;
	bptr = get(buf, (int)size, &error);	/* Get size bytes from client. */
	if (!bptr || (error == EOF)) return -1;
	if (fwrite(bptr, (int)size, 1, outfile) != 1) {
	    LogMsg(0, VolDebugLevel, stdout, "Error creating file in volume; restore aborted");
	    return -1;
	}
    }
    return filesize;
}

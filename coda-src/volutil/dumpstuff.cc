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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/volutil/RCS/dumpstuff.cc,v 4.1 1997/01/08 21:52:24 rvb Exp $";
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

#include <lwp.h>
#include <lock.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include <nfs.h>
#include <errors.h>
#include <cvnode.h>
#include <volume.h>
#include <partition.h>
#include <vutil.h>
#include "dump.h"
#include "voldump.h"

/*
 * Initialize dumpstuff to use a buffer at bufp which has size bytes.
 * If the buffer overflows, write to file descriptor fd, or use an rpc
 * if doing newstyle dumps (if rpcid > 0)
 */
DumpBuffer_t *
    InitDumpBuf(byte *ptr, long size, VolumeId volId, RPC2_Handle rpcid)
{
    DumpBuffer_t *buf;
    LogMsg(20, VolDebugLevel, stdout, "InitDumpbuf: ptr %x size %d volId %x rpcid %d", ptr, size, volId, rpcid);

    buf = (DumpBuffer_t *)malloc(sizeof(DumpBuffer_t));
    buf->DumpBufPtr = buf->DumpBuf = ptr;
    buf->DumpBufEnd = (byte *)ptr + size;
    buf->VOLID = (int)volId;
    buf->rpcid = rpcid;
    buf->offset = 0;
    buf->nbytes = 0;
    buf->secs = 0;
    return buf;
}

DumpBuffer_t *
    InitDumpBuf(byte *ptr, long size, int fd)
{
    LogMsg(20, VolDebugLevel, stdout, "InitDumpbuf: ptr %x size %d fd %d", ptr, size, fd);

    return InitDumpBuf(ptr, size, fd, 0);
}

int FlushBuf(DumpBuffer_t *buf)
{
    LogMsg(2, VolDebugLevel, stdout, "Flushing dump buf: %d bytes", buf->DumpBufPtr - buf->DumpBuf);
    if (buf->rpcid == -1) {	/* Previous rpc2 error -- abort */
	LogMsg(0, VolDebugLevel, stdout, "DumpStuff: RPCID is invalid! %d", buf->rpcid);
	return -1;
    }

    int nbytes;
    if (buf->rpcid > 0) {
	/* Write the buffer over to the client via an rpc2 call. */
	SE_Descriptor sed;
	sed.Tag = SMARTFTP;
	sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sed.Value.SmartFTPD.ByteQuota = -1;
	sed.Value.SmartFTPD.SeekOffset = 0;
	sed.Value.SmartFTPD.Tag = FILEINVM;
	sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = buf->DumpBuf;
	sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = 
	sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = nbytes =
	    buf->DumpBufPtr - buf->DumpBuf;  /* Number of bytes in the buffer */

	unsigned long before = time(0);
	long rc = WriteDump(buf->rpcid, buf->offset, (RPC2_Unsigned *)&nbytes, buf->VOLID,&sed);
	unsigned long after = time(0);
	if (rc != RPC2_SUCCESS) {
	    LogMsg(0, VolDebugLevel, stdout, "FlushBuf: WriteDump failed %s.", RPC2_ErrorMsg((int)rc));
	    buf->rpcid = -1;
	    return -1;
	}
	buf->secs += after - before;
	
	if (nbytes != buf->DumpBufPtr - buf->DumpBuf) {
	    LogMsg(0, VolDebugLevel, stdout, "FlushBuf: WriteDump didn't write enough! %d != %d",
		nbytes, buf->DumpBufPtr - buf->DumpBuf);
	    return -1;
	}
    } else {
	if (write(buf->DumpFd, buf->DumpBuf, buf->DumpBufPtr-buf->DumpBuf) !=
	       buf->DumpBufPtr-buf->DumpBuf) {
	    LogMsg(0, VolDebugLevel, stdout, "Dump:  error writing dump; aborted");
	    return 0;
	}
	nbytes = buf->DumpBufPtr - buf->DumpBuf;
    }
    buf->DumpBufPtr = buf->DumpBuf;
    buf->offset += nbytes;	/* Update the number of bytes written */
    buf->nbytes += nbytes;
	
    return 0;
}    

byte *Reserve(DumpBuffer_t *buf, int n)
{
    byte *current = buf->DumpBufPtr;
    if (current+n > buf->DumpBufEnd) {
        if (FlushBuf(buf) == -1) return NULL;
	current = buf->DumpBufPtr;
    }
    buf->DumpBufPtr += n;
    assert(buf->DumpBufPtr <= buf->DumpBufEnd);
    return current;
}

/* Throughout this code are implicit assumptions as to the size of objects */
int DumpTag(DumpBuffer_t *buf, register byte tag)
{
    register byte *p = Reserve(buf, 1);
    if (p) {
	*p = tag;
	return 0;
    }
    return -1;
}

int DumpByte(DumpBuffer_t *buf, register byte tag, byte value)
{
    register byte *p = Reserve(buf, 2);
    if (p) {
	*p++ = tag;
	*p = value;
	return 0;
    }
    return -1;
}

#define putlong(p, v) \
 *p++ =(byte)(v>>24); *p++ = (byte)(v>>16); *p++ = (byte)(v>>8); *p++ = (byte)(v);

#define putshort(p, v) *p++ = (byte)(v>>8); *p++ = (byte)(v);

int DumpDouble(DumpBuffer_t *buf, byte tag, register unsigned long value1, register unsigned long value2)
{
    register byte *p = Reserve(buf, 1 + 2 * sizeof(long));
    if (p) {
	*p++ = tag;
	putlong(p, value1);
	putlong(p, value2);
	return 0;
    }
    return -1;
}

int DumpLong(DumpBuffer_t *buf, byte tag, register unsigned long value)
{
    register byte *p = Reserve(buf, 1 + sizeof(long));
    if (p) {
	*p++ = tag;
	putlong(p, value);
	return 0;
    }
    return -1;
}

int DumpArrayLong(DumpBuffer_t *buf, byte tag, register unsigned long *array, register int nelem)
{
    register byte *p = Reserve(buf, 1 + sizeof(long) + (nelem * sizeof(long)));
    if (p) {
	*p++ = tag;
	putlong(p, nelem);
	while (nelem--) {
	    register unsigned long v = *array++;
	    putlong(p, v);
	}
	return 0;
    }
    return -1;
}

int DumpShort(DumpBuffer_t *buf, byte tag, unsigned int value)
{
    register byte *p = Reserve(buf, 1 + sizeof(short));
    if (p) {
	*p++ = tag;
	*p++ = value>>8;
	*p = (byte) value;
	return 0;
    }
    return -1;
}

int DumpBool(DumpBuffer_t *buf, byte tag, unsigned int value)
{
    register byte *p = Reserve(buf, 2);	/* Assume bool is same as byte */
    if (p) {
	*p++ = tag;
	*p = (byte) value;
	return 0;
    }
    return -1;
}

int DumpString(DumpBuffer_t *buf, byte tag, register char *s)
{
    register byte *p;
    register n;
    n = strlen(s)+1;
    if (p = Reserve(buf, 1 + n + sizeof(long))) {
	*p++ = tag;
	putlong(p, n);
	bcopy(s, p, n);
	return 0;
    }
    return -1;
}

int DumpByteString(DumpBuffer_t *buf, byte tag, register byte *bs, register int nbytes)
{
    register byte *p = Reserve(buf, 1+nbytes);
    if (p) {
	*p++ = tag;
	bcopy(bs, p, nbytes);
	return 0;
    }
    return -1;
}

/*
 * If any of these operations fail, the others will be nullops.
 */
int DumpVV(DumpBuffer_t *buf, byte tag, struct ViceVersionVector *vv)
{
    DumpTag(buf, tag);
    DumpLong(buf, '0', vv->Versions.Site0);
    DumpLong(buf, '1', vv->Versions.Site1);
    DumpLong(buf, '2', vv->Versions.Site2);
    DumpLong(buf, '3', vv->Versions.Site3);
    DumpLong(buf, '4', vv->Versions.Site4);
    DumpLong(buf, '5', vv->Versions.Site5);
    DumpLong(buf, '6', vv->Versions.Site6);
    DumpLong(buf, '7', vv->Versions.Site7);
    DumpLong(buf, 's', vv->StoreId.Host);
    DumpLong(buf, 'u', vv->StoreId.Uniquifier);
    DumpLong(buf, 'f', vv->Flags);
    return DumpTag(buf, (byte)D_ENDVV);
}

int DumpFile(DumpBuffer_t *buf, byte tag, int fd, int vnode)
{
    register long n, nbytes, howMany;
    byte *p;
    struct stat status;
    fstat(fd, &status);

    /* We want to dump st_blksize bytes at a time, so buffer better
     * be big enough to hold at least that many bytes. */
    if (status.st_blksize > (buf->DumpBufEnd - buf->DumpBuf)) {
	LogMsg(0, VolDebugLevel, stdout, "Dump Buffer not big enough!");
	return -1;
    }
    DumpLong(buf, tag, status.st_size);
    howMany = status.st_blksize;
    for (nbytes = status.st_size;nbytes;nbytes -= n) {
	if (howMany > nbytes)
	    howMany = nbytes;
	p = Reserve(buf, (int)howMany);
	if (!p) return -1;
	n = read(fd, p, (int)howMany);
	if (n < howMany){
	    LogMsg(0, VolDebugLevel, stdout, "Error reading inode %d for vnode %d; dump aborted",
	    	status.st_ino, vnode);
	    assert(0);
	}
    }
    return 0;
}

int DumpEnd(DumpBuffer_t *buf) {
    DumpLong(buf, D_DUMPEND, DUMPENDMAGIC);
    return FlushBuf(buf);	 /* Force whatever is left in buffer to disk. */
}


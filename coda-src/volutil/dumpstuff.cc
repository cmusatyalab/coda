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


#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/errors.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <vice.h>
#include <voltypes.h>
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
DumpBuffer_t *InitDumpBuf(char *ptr, long size, VolumeId volId,
			  RPC2_Handle rpcid)
{
    DumpBuffer_t *buf;
    LogMsg(20, VolDebugLevel, stdout, "InitDumpbuf: ptr %x size %d volId %x rpcid %d", ptr, size, volId, rpcid);

    buf = (DumpBuffer_t *)malloc(sizeof(DumpBuffer_t));
    buf->DumpBufPtr = buf->DumpBuf = ptr;
    buf->DumpBufEnd = ptr + size;
    buf->VOLID = (int)volId;
    buf->rpcid = rpcid;
    buf->offset = 0;
    buf->nbytes = 0;
    buf->secs = 0;
    return buf;
}

DumpBuffer_t *InitDumpBuf(char *ptr, long size, int fd)
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
	memset(&sed, 0, sizeof(SE_Descriptor));
	sed.Tag = SMARTFTP;
	sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sed.Value.SmartFTPD.ByteQuota = -1;
	sed.Value.SmartFTPD.SeekOffset = 0;
	sed.Value.SmartFTPD.Tag = FILEINVM;
	sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_Byte *)buf->DumpBuf;
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

char *Reserve(DumpBuffer_t *buf, int n)
{
    char *current = buf->DumpBufPtr;
    if (current+n > buf->DumpBufEnd) {
        if (FlushBuf(buf) == -1) return NULL;
	current = buf->DumpBufPtr;
    }
    buf->DumpBufPtr += n;
    CODA_ASSERT(buf->DumpBufPtr <= buf->DumpBufEnd);
    return current;
}

/* Throughout this code are implicit assumptions as to the size of objects */
int DumpTag(DumpBuffer_t *buf, char tag)
{
    char *p = Reserve(buf, 1);
    if (p) {
	*p = tag;
	return 0;
    }
    return -1;
}

int DumpByte(DumpBuffer_t *buf, char tag, char value)
{
    char *p = Reserve(buf, 2);
    if (p) {
	*p++ = tag;
	*p = value;
	return 0;
    }
    return -1;
}

#define putlong(p, v) \
 *p++ =(unsigned char)(v>>24); *p++ = (unsigned char)(v>>16); *p++ = (unsigned char)(v>>8); *p++ = (unsigned char)(v);

#define putshort(p, v) *p++ = (unsigned char)(v>>8); *p++ = (unsigned char)(v);

int DumpDouble(DumpBuffer_t *buf, char tag, unsigned int value1, unsigned int value2)
{
    char *p = Reserve(buf, 1 + 2 * sizeof(unsigned int));
    if (p) {
	*p++ = tag;
	putlong(p, value1);
	putlong(p, value2);
	return 0;
    }
    return -1;
}

int DumpInt32(DumpBuffer_t *buf, char tag, unsigned int value)
{
    char *p = Reserve(buf, 1 + sizeof(unsigned int));
    if (p) {
	*p++ = tag;
	putlong(p, value);
	return 0;
    }
    return -1;
}

int DumpArrayInt32(DumpBuffer_t *buf, char tag, unsigned int *array, int nelem)
{
    char *p = Reserve(buf, 1 + sizeof(unsigned int) +
		      (nelem * sizeof(unsigned int)));
    if (p) {
	*p++ = tag;
	putlong(p, nelem);
	while (nelem--) {
	    unsigned int v = *array++;
	    putlong(p, v);
	}
	return 0;
    }
    return -1;
}

int DumpShort(DumpBuffer_t *buf, char tag, unsigned int value)
{
    char *p = Reserve(buf, 1 + sizeof(short));
    if (p) {
	*p++ = tag;
	putshort(p, value);
	return 0;
    }
    return -1;
}

int DumpBool(DumpBuffer_t *buf, char tag, unsigned int value)
{
    char *p = Reserve(buf, 2);	/* Assume bool is same as byte */
    if (p) {
	*p++ = tag;
	*p = (char) value;
	return 0;
    }
    return -1;
}

int DumpString(DumpBuffer_t *buf, char tag, char *s)
{
    char *p;
    int n;
    n = strlen(s)+1;
    if ((p = Reserve(buf, 1 + n + sizeof(unsigned int)))) {
	*p++ = tag;
	putlong(p, n);
	memcpy(p, s, n);
	return 0;
    }
    return -1;
}

int DumpByteString(DumpBuffer_t *buf, char tag, char *bs, int nbytes)
{
    char *p = Reserve(buf, 1+nbytes);
    if (p) {
	*p++ = tag;
	memcpy(p, bs, nbytes);
	return 0;
    }
    return -1;
}

/*
 * If any of these operations fail, the others will be nullops.
 */
int DumpVV(DumpBuffer_t *buf, char tag, struct ViceVersionVector *vv)
{
    DumpTag(buf, tag);
    DumpInt32(buf, '0', vv->Versions.Site0);
    DumpInt32(buf, '1', vv->Versions.Site1);
    DumpInt32(buf, '2', vv->Versions.Site2);
    DumpInt32(buf, '3', vv->Versions.Site3);
    DumpInt32(buf, '4', vv->Versions.Site4);
    DumpInt32(buf, '5', vv->Versions.Site5);
    DumpInt32(buf, '6', vv->Versions.Site6);
    DumpInt32(buf, '7', vv->Versions.Site7);
    DumpInt32(buf, 's', vv->StoreId.Host);
    DumpInt32(buf, 'u', vv->StoreId.Uniquifier);
    DumpInt32(buf, 'f', vv->Flags);
    return DumpTag(buf, (char)D_ENDVV);
}

int DumpFile(DumpBuffer_t *buf, char tag, int fd, int vnode)
{
    long n, nbytes, howMany;
    char *p;
    struct stat status;
    fstat(fd, &status);

    /* We want to dump st_blksize bytes at a time, so buffer better
     * be big enough to hold at least that many bytes. */
    if ((int)status.st_blksize > (buf->DumpBufEnd - buf->DumpBuf)) {
	LogMsg(0, VolDebugLevel, stdout, "Dump Buffer not big enough!");
	return -1;
    }
    DumpInt32(buf, tag, status.st_size);
    howMany = status.st_blksize;
    for (nbytes = status.st_size; nbytes; nbytes -= n) {
	if (howMany > nbytes)
	    howMany = nbytes;
	p = Reserve(buf, (int)howMany);
	if (!p) return -1;
	n = read(fd, p, (int)howMany);
	if (n < howMany) {
	    LogMsg(0, VolDebugLevel, stdout,
		   "Error reading inode %d for vnode %d; dump aborted",
		   status.st_ino, vnode);
	    CODA_ASSERT(0);
	}
    }
    return 0;
}

int DumpEnd(DumpBuffer_t *buf) {
    DumpInt32(buf, D_DUMPEND, DUMPENDMAGIC);
    return FlushBuf(buf);	 /* Force whatever is left in buffer to disk. */
}


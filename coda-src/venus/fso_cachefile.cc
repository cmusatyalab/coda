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





/*
 *    Cache file management
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from venus */
#include "fso.h"
#include "venus.private.h"


/*  *****  CacheFile Members  *****  */

/* Pre-allocation routine. */
/* MUST be called from within transaction! */
CacheFile::CacheFile(int i) {
    CODA_ASSERT(this != 0);

    /* Assume caller has done RVMLIB_REC_OBJECT! */
/*    RVMLIB_REC_OBJECT(*this);*/
    sprintf(name, "V%d", i);
    inode = (ino_t)-1;
    length = validdata = 0;
    fd = -1;
    refcnt = 1;
    /* Container reset will be done by eventually by FSOInit()! */
}


CacheFile::CacheFile() {
    CODA_ASSERT(inode != (ino_t)-1 && length == 0);
    fd = -1;
    refcnt = 1;
}


CacheFile::~CacheFile() {
    CODA_ASSERT(inode != (ino_t)-1 && length == 0);
}


/* MUST NOT be called from within transaction! */
void CacheFile::Validate() {
    if (!ValidContainer())
	ResetContainer();
}


/* MUST NOT be called from within transaction! */
void CacheFile::Reset() {
    if (inode == (ino_t)-1 || length != 0 || !ValidContainer())
	ResetContainer();
}


int CacheFile::ValidContainer() {
    int code = 0;
    struct stat tstat;
    int valid = (code = ::stat(name, &tstat)) == 0 &&
#if !defined(DJGPP) && !defined(__CYGWIN32__)
      tstat.st_uid == (uid_t)V_UID &&
      tstat.st_gid == (gid_t)V_GID &&
      (tstat.st_mode & ~S_IFMT) == V_MODE &&
      tstat.st_ino == inode &&
#endif
      tstat.st_size == (off_t)length;

    if (!valid && LogLevel >= 0) {
	dprint("CacheFile::ValidContainer: %s invalid\n", name);
	if (code == 0)
	    dprint("\t(%u, %u), (%u, %u), (%o, %o), (%d, %d), (%d, %d)\n",
		   tstat.st_uid, (uid_t)V_UID, tstat.st_gid, (gid_t)V_GID,
		   (tstat.st_mode & ~S_IFMT), V_MODE,
		   tstat.st_ino, inode, tstat.st_size, length);
	else
	    dprint("\tstat failed (%d)\n", errno);
    }

    return(valid);
}


/* MUST NOT be called from within transaction! */
void CacheFile::ResetContainer() {
    LOG(10, ("CacheFile::ResetContainer: %s, %d, %d/%d\n",
	      name, inode, validdata, length));

    int tfd;
    struct stat tstat;
    if ((tfd = ::open(name, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, V_MODE)) < 0)
	CHOKE("CacheFile::ResetContainer: open failed (%d)", errno);
#ifndef DJGPP
    if (::fchmod(tfd, V_MODE) < 0)
	CHOKE("CacheFile::ResetContainer: fchmod failed (%d)", errno);

#ifndef __CYGWIN32__
    if (::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID) < 0)
	CHOKE("CacheFile::ResetContainer: fchown failed (%d)", errno);
#else
    if (::chown(name, (uid_t)V_UID, (gid_t)V_GID) < 0)
	CHOKE("CacheFile::ResetContainer: fchown failed (%d)", errno);
#endif
#endif
    if (::fstat(tfd, &tstat) < 0)
	CHOKE("CacheFile::ResetContainer: fstat failed (%d)", errno);
    if (::close(tfd) < 0)
	CHOKE("CacheFile::ResetContainer: close failed (%d)", errno);

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(*this);
    inode = tstat.st_ino;
    length = validdata = 0;
    Recov_EndTrans(MAXFP);
    
    if (fd != -1) {
	LOG(0, ("CacheFile::ResetContainer: container active"));
        Close();
    }
}

/* 
 * copies a cache file, data and attributes, to a new one.  
 */
int CacheFile::Copy(CacheFile *destination)
{
    ino_t ino;

    LOG(10, ("CacheFile::Copy: from %s, %d, %d/%d, to %s, %d, %d/%d\n",
	      name, inode, validdata, length, destination->name,
	      destination->inode, destination->validdata, destination->length));

    Copy(destination->name, &ino);

    destination->inode  = ino;
    destination->length = length;
    destination->validdata = validdata;

    return 0;
}

int CacheFile::Copy(char *destname, ino_t *ino)
{
    LOG(10, ("CacheFile::Copy: from %s, %d, %d/%d, to %s\n",
	      name, inode, validdata, length, destname));

    int tfd, ffd, n;
    struct stat tstat;
    char buf[DIR_PAGESIZE];

    if ((tfd = ::open(destname, O_RDWR | O_CREAT | O_TRUNC| O_BINARY, V_MODE)) < 0) {
	LOG(0, ("CacheFile::Copy: open failed (%d)", errno));
	return -1;
    }
#ifndef DJGPP
    if (::fchmod(tfd, V_MODE) < 0)
	CHOKE("CacheFile::Copy: fchmod failed (%d)", errno);
#ifdef __CYGWIN32__
    if (::chown(destname->name, (uid_t)V_UID, (gid_t)V_GID) < 0)
	CHOKE("CacheFile::ResetCopy: fchown failed (%d)", errno);
#else
    if (::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID) < 0)
	CHOKE("CacheFile::Copy: fchown failed (%d)", errno);
#endif
#endif
    if ((ffd = ::open(name, O_RDONLY| O_BINARY, V_MODE)) < 0)
	CHOKE("CacheFile::Copy: source open failed (%d)", errno);

    for (;;) {
        n = ::read(ffd, buf, (int) sizeof(buf));
	LOG(100,("CacheFile::Copy: copying %d bytes", n));
        if (n == 0)
	    break;
        if (n < 0)
	    CHOKE("CacheFile::Copy: read failed! (%d)", errno);
	if (::write(tfd, buf, n) != n) {
	    LOG(0, ("CacheFile::Copy: write failed! (%d)", errno));
	    return -1;
	}
    }
    if (::fstat(tfd, &tstat) < 0)
	CHOKE("CacheFile::Copy: fstat failed (%d)", errno);
    if (::close(tfd) < 0)
	CHOKE("CacheFile::Copy: close failed (%d)", errno);
    if (::close(ffd) < 0)
	CHOKE("CacheFile::Copy: source close failed (%d)", errno);
    
    CODA_ASSERT((off_t)length == tstat.st_size);

    return 0;
}


int CacheFile::DecRef()
{
    if (--refcnt == 0)
    {
	length = validdata = 0;
	if (::unlink(name) < 0)
	    CHOKE("CacheFile::DecRef: unlink failed (%d)", errno);
    }
    return refcnt;
}


/* N.B. length member is NOT updated as side-effect! */
void CacheFile::Stat(struct stat *tstat) {
    CODA_ASSERT(inode != (ino_t)-1);

    CODA_ASSERT(::stat(name, tstat) == 0);
#if ! defined(DJGPP) && ! defined(__CYGWIN32__)
    CODA_ASSERT(tstat->st_ino == inode);
#endif
}


/* MUST be called from within transaction! */
void CacheFile::Truncate(long newlen) {
#ifdef __CYGWIN32__
    int fd;
#endif

    CODA_ASSERT(inode != (ino_t)-1);

    /*
    if (length < newlen) {
       eprint("Truncate: %d->%d:  -> ::truncate(name %s, length %d)\n",
		length, newlen, name, newlen);
    }
    */
    if (length != newlen) {
	RVMLIB_REC_OBJECT(*this);
	length = validdata = newlen;
    }
#ifndef __CYGWIN32__
    CODA_ASSERT(::truncate(name, length) == 0);
#else 
    fd = open(name, O_RDWR);
    if ( fd < 0 )
	    CODA_ASSERT(0);
    CODA_ASSERT(::ftruncate(fd, length) == 0);
    close(fd);
#endif
}


/* MUST be called from within transaction! */
void CacheFile::SetLength(long newlen) {
    CODA_ASSERT(inode != (ino_t)-1);

    LOG(0, ("Cachefile::SetLength %d\n", newlen));

    if (length != newlen) {
	RVMLIB_REC_OBJECT(*this);
	length = validdata = newlen;
    }
}

/* MUST be called from within transaction! */
void CacheFile::SetValidData(long newoffset) {
    CODA_ASSERT(inode != (ino_t)-1);

    LOG(0, ("Cachefile::SetValidData %d\n", newoffset));

    if (validdata != newoffset) {
	RVMLIB_REC_OBJECT(validdata);
	validdata = newoffset;
    }
}

void CacheFile::print(int fdes) {
    fdprint(fdes, "[ %s, %d, %d/%d ]\n", name, inode, validdata, length);
}

int CacheFile::Open(fsobj *fso, int flags)
{
    union outputArgs msg;

    fd = ::open(name, flags, V_MODE);
    if (fd == -1) return -1;
    
#if 0 /* experimental code for linux-2.3, probably not needed anymore */
    if (HAVE.coda_openfid) {
        msg.oh.opcode = CODA_MAKE_CINODE;
        msg.oh.unique = 0;
        fso->GetFid(&msg.coda_make_cinode.CodaFid);
        fso->GetVattr(&msg.coda_make_cinode.attr);
        msg.coda_make_cinode.fd = fd;
        
        /* Send the message. */
        if (write(global_kernfd, (char *)&msg, sizeof(msg)) != sizeof(msg)) {
            LOG(0, ("coda_openfid: message write fails: errno %d", errno));
            /* Possibly an inode collision happened, which means that we
             * cannot wrap this file. But, userspace cannot get this inode at
             * the moment either. So we optimistically ignore the error. */
        }
    }
#endif

    return fd;
}

int CacheFile::Close()
{
    int ret;

    if (fd == -1)
        return 0;

    ret = ::close(fd);
    fd = -1;
    return ret;
}


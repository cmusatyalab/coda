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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/fso_cachefile.cc,v 4.3 1997/02/27 13:59:23 rvb Exp $";
#endif /*_BLURB_*/





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
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

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
    ASSERT(this	!= 0);

    /* Assume caller has done RVMLIB_REC_OBJECT! */
/*    RVMLIB_REC_OBJECT(*this);*/
    sprintf(name, "V%d", i);
    inode = (ino_t)-1;
    length = 0;

    /* Container reset will be done by eventually by FSOInit()! */
}


CacheFile::CacheFile() {
    if (Simulating) return;

    ASSERT(inode != (ino_t)-1 && length == 0);
}


CacheFile::~CacheFile() {
    if (Simulating) return;

    ASSERT(inode != (ino_t)-1 && length == 0);
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
    if (Simulating) return(1);

    int code = 0;
    struct stat tstat;
    int valid = (code = ::stat(name, &tstat)) == 0 &&
      tstat.st_uid == (uid_t)V_UID &&
      tstat.st_gid == (gid_t)V_GID &&
      (tstat.st_mode & ~S_IFMT) == V_MODE &&
      tstat.st_ino == inode &&
      tstat.st_size == length;

    if (!valid && LogLevel >= 10) {
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
    if (Simulating) return;

    LOG(10, ("CacheFile::ResetContainer: %s, %d, %d\n",
	      name, inode, length));

    int tfd;
    struct stat tstat;
    if ((tfd = ::open(name, O_RDWR | O_CREAT | O_TRUNC, V_MODE)) < 0)
	Choke("CacheFile::ResetContainer: open failed (%d)", errno);
    if (::fchmod(tfd, V_MODE) < 0)
	Choke("CacheFile::ResetContainer: fchmod failed (%d)", errno);

#ifndef __CYGWIN32__
    if (::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID) < 0)
	Choke("CacheFile::ResetContainer: fchown failed (%d)", errno);
#else
    if (::chown(name, (uid_t)V_UID, (gid_t)V_GID) < 0)
	Choke("CacheFile::ResetContainer: fchown failed (%d)", errno);
#endif
    if (::fstat(tfd, &tstat) < 0)
	Choke("CacheFile::ResetContainer: fstat failed (%d)", errno);
    if (::close(tfd) < 0)
	Choke("CacheFile::ResetContainer: close failed (%d)", errno);

    ATOMIC(
	RVMLIB_REC_OBJECT(*this);
	inode = tstat.st_ino;
	length = 0;
    , MAXFP)
}


/*
 * moves a cache file to a new one.  destination's name already
 * there, created by cache file constructor.
 */  
void CacheFile::Move(CacheFile *destination) {
    if (Simulating) return;

    LOG(10, ("CacheFile::Move: source: %s, %d, %d, dest: %s\n",
	      name, inode, length, destination->name));

    destination->inode = inode;
    destination->length = length;
    if (::rename(name, destination->name) != 0)
        Choke("CacheFile::RenameContainer: rename failed (%d)", errno);
}


/* 
 * copies a cache file, data and attributes, to a new one.  
 */
void CacheFile::Copy(CacheFile *source) {
    if (Simulating) return;

    LOG(10, ("CacheFile::Copy: %s, %d, %d\n",
	      name, inode, length));

    int tfd, ffd, n;
    struct stat tstat;
#ifndef __BSD44__
    char buf[PAGESIZE];
#else
    char buf[MAXBSIZE];
#endif

    if ((tfd = ::open(name, O_RDWR | O_CREAT | O_TRUNC, V_MODE)) < 0)
	Choke("CacheFile::Copy: open failed (%d)", errno);
    if (::fchmod(tfd, V_MODE) < 0)
	Choke("CacheFile::Copy: fchmod failed (%d)", errno);
#ifndef __CYGWIN32__
    if (::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID) < 0)
	Choke("CacheFile::Copy: fchown failed (%d)", errno);
#else
    if (::chown(name, (uid_t)V_UID, (gid_t)V_GID) < 0)
	Choke("CacheFile::ResetCopy: fchown failed (%d)", errno);
#endif
    if ((ffd = ::open(source->name, O_RDONLY, V_MODE)) < 0)
	Choke("CacheFile::Copy: source open failed (%d)", errno);

    for (;;) {
        n = ::read(ffd, buf, (int) sizeof(buf));
        if (n == 0)
	    break;
        if (n < 0)
	    Choke("CacheFile::Copy: read failed! (%d)", errno);
	if (::write(tfd, buf, n) != n)
	    Choke("CacheFile::Copy: write failed! (%d)", errno);
    }
    if (::fstat(tfd, &tstat) < 0)
	Choke("CacheFile::Copy: fstat failed (%d)", errno);
    if (::close(tfd) < 0)
	Choke("CacheFile::Copy: close failed (%d)", errno);
    if (::close(ffd) < 0)
	Choke("CacheFile::Copy: source close failed (%d)", errno);
    
    ASSERT(source->length == tstat.st_size);

    inode = tstat.st_ino;
    length = source->length;
}


void CacheFile::Remove() {
    length = 0;
    if (::unlink(name) < 0)
        Choke("CacheFile::Remove: unlink failed (%d)", errno);
}


/* N.B. length member is NOT updated as side-effect! */
void CacheFile::Stat(struct stat *tstat) {
    if (Simulating) return;

    ASSERT(inode != (ino_t)-1);

    ASSERT(::stat(name, tstat) == 0);
    ASSERT(tstat->st_ino == inode);
}


/* MUST be called from within transaction! */
void CacheFile::Truncate(unsigned newlen) {
    int fd;
    if (Simulating) return;

    ASSERT(inode != (ino_t)-1);

    /*
    if (length < newlen) {
       eprint("Truncate: %d->%d:  -> ::truncate(name %s, length %d)\n",
		length, newlen, name, newlen);
    }
    */
    if (length != newlen) {
	RVMLIB_REC_OBJECT(*this);
	length = newlen;
    }
#ifndef __CYGWIN32__
    ASSERT(::truncate(name, length) == 0);
#else 
    fd = open(name, O_RDWR);
    if ( fd < 0 )
	    ASSERT(0);
    ASSERT(::ftruncate(fd, length) == 0);
    close(fd);
#endif
}


/* MUST be called from within transaction! */
void CacheFile::SetLength(unsigned newlen) {
    ASSERT(inode != (ino_t)-1);

    if (length != newlen) {
	 RVMLIB_REC_OBJECT(*this);
	length = newlen;
    }
}


void CacheFile::print(int fdes) {
    fdprint(fdes, "[ %s, %d, %d ]\n", name, inode, length);
}


/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
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
#endif

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
#endif

/* interfaces */
#include <vice.h>
#include <mkpath.h>
#include <copyfile.h>

/* from util */
#include <rvmlib.h>

/* from venus */
#include "fso_cachefile.h"
#include "venusrecov.h"
#include "venus.private.h"

#ifndef fdatasync
#define fdatasync(fd) fsync(fd)
#endif

/* always useful to have a page of zero's ready */
static char zeropage[4096];

/*  *****  CacheFile Members  *****  */

/* Pre-allocation routine. */
/* MUST be called from within transaction! */
CacheFile::CacheFile(int i)
{
    /* Assume caller has done RVMLIB_REC_OBJECT! */
    /* RVMLIB_REC_OBJECT(*this); */
    sprintf(name, "%02X/%02X/%02X/%02X",
	    (i>>24) & 0xff, (i>>16) & 0xff, (i>>8) & 0xff, i & 0xff);
    length = validdata = 0;
    refcnt = 1;
    numopens = 0;
    cached_chuncks = new bitmap(1, 0);
    /* Container reset will be done by eventually by FSOInit()! */
    LOG(100, ("CacheFile::CacheFile(%d): %s (this=0x%x)\n", i, name, this));
}


CacheFile::CacheFile()
{
    CODA_ASSERT(length == 0);
    refcnt = 1;
    numopens = 0;
    cached_chuncks = new bitmap(1, 0);
}


CacheFile::~CacheFile()
{
    LOG(10, ("CacheFile::~CacheFile: %s (this=0x%x)\n", name, this));
    CODA_ASSERT(length == 0);
    delete(cached_chuncks);
}


/* MUST NOT be called from within transaction! */
void CacheFile::Validate()
{
    if (!ValidContainer())
	Reset();
}


/* MUST NOT be called from within transaction! */
void CacheFile::Reset()
{
    if (access(name, F_OK) == 0 && length != 0) {
	Recov_BeginTrans();
	Truncate(0);
	Recov_EndTrans(MAXFP);
    }
}

int CacheFile::ValidContainer()
{
    struct stat tstat;
    int rc;
    
    rc = ::stat(name, &tstat);
    if (rc) return 0;

    int valid =
#ifndef __CYGWIN32__
      tstat.st_uid == (uid_t)V_UID &&
      tstat.st_gid == (gid_t)V_GID &&
      (tstat.st_mode & ~S_IFMT) == V_MODE &&
#endif
      tstat.st_size == (off_t)length;

    if (!valid && LogLevel >= 0) {
	dprint("CacheFile::ValidContainer: %s invalid\n", name);
	dprint("\t(%u, %u), (%u, %u), (%o, %o), (%d, %d)\n",
	       tstat.st_uid, (uid_t)V_UID, tstat.st_gid, (gid_t)V_GID,
	       (tstat.st_mode & ~S_IFMT), V_MODE,
	       tstat.st_size, length);
    }
    return(valid);
}

/* Must be called from within a transaction!  Assume caller has done
   RVMLIB_REC_OBJECT() */

void CacheFile::Create(int newlength)
{
    LOG(10, ("CacheFile::Create: %s, %d\n", name, newlength));

    int tfd;
    struct stat tstat;
    if (mkpath(name, V_MODE | 0100)<0)
	CHOKE("CacheFile::Create: could not make path for %s", name);
    if ((tfd = ::open(name, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, V_MODE)) < 0)
	CHOKE("CacheFile::Create: open failed (%d)", errno);

#ifdef __CYGWIN32__
    ::chown(name, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif
    if (::ftruncate(tfd, newlength) < 0)
      CHOKE("CacheFile::Create: ftruncate failed (%d)", errno);
    if (::fstat(tfd, &tstat) < 0)
	CHOKE("CacheFile::ResetContainer: fstat failed (%d)", errno);
    if (::close(tfd) < 0)
	CHOKE("CacheFile::ResetContainer: close failed (%d)", errno);

    validdata = 0;
    length = newlength;
    refcnt = 1;
    numopens = 0;
    cached_chuncks->Grow(bytes_to_blocks_ceil(length));
}


/*
 * copies a cache file, data and attributes, to a new one.
 */
int CacheFile::Copy(CacheFile *destination)
{
    Copy(destination->name);

    destination->length = length;
    destination->validdata = validdata;
    destination->cached_chuncks = cached_chuncks;
    return 0;
}

int CacheFile::Copy(char *destname, int recovering)
{
    LOG(10, ("CacheFile::Copy: from %s, %d/%d, to %s\n",
	     name, validdata, length, destname));

    int tfd, ffd;
    struct stat tstat;

    if (mkpath(destname, V_MODE | 0100) < 0) {
	LOG(0, ("CacheFile::Copy: could not make path for %s\n", name));
	return -1;
    }
    if ((tfd = ::open(destname, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, V_MODE)) < 0) {
	LOG(0, ("CacheFile::Copy: open failed (%d)\n", errno));
	return -1;
    }
    ::fchmod(tfd, V_MODE);
#ifdef __CYGWIN32__
    ::chown(destname, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif
    if ((ffd = ::open(name, O_RDONLY| O_BINARY, V_MODE)) < 0)
	CHOKE("CacheFile::Copy: source open failed (%d)\n", errno);

    if (copyfile(ffd, tfd) < 0) {
	LOG(0, ("CacheFile::Copy failed! (%d)\n", errno));
	::close(ffd);
	::close(tfd);
	return -1;
    }
    if (::close(ffd) < 0)
	CHOKE("CacheFile::Copy: source close failed (%d)\n", errno);

    if (::fstat(tfd, &tstat) < 0)
	CHOKE("CacheFile::Copy: fstat failed (%d)\n", errno);
    if (::close(tfd) < 0)
	CHOKE("CacheFile::Copy: close failed (%d)\n", errno);
    
    CODA_ASSERT(recovering || (off_t)length == tstat.st_size);

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
void CacheFile::Stat(struct stat *tstat)
{
    CODA_ASSERT(::stat(name, tstat) == 0);
}


void CacheFile::Utimes(const struct timeval times[2])
{
    CODA_ASSERT(::utimes(name, times) == 0);
}


/* MUST be called from within transaction! */
void CacheFile::Truncate(long newlen)
{
    int fd;

    fd = open(name, O_WRONLY | O_BINARY);
    CODA_ASSERT(fd >= 0 && "fatal error opening container file");

    /* ISR tweak, write zeros to data area before truncation */
    if (option_isr && newlen < length) {
	size_t len = sizeof(zeropage), n = length - newlen;

	lseek(fd, newlen, SEEK_SET);

	while (n) {
	    if (n < len) len = n;
	    write(fd, zeropage, len);
	    n -= len;
	}
	/* we have to force these writes to disk, otherwise the following
	 * truncate would simply drop any unwritten data */
	fdatasync(fd);
    }

    if (length != newlen) {
	RVMLIB_REC_OBJECT(*this);
	length = validdata = newlen;
    }

    CODA_ASSERT(::ftruncate(fd, length) == 0);
    cached_chuncks->Resize(bytes_to_blocks_ceil(length));

    close(fd);
}

/* MUST be called from within transaction! */
void CacheFile::SetLength(uint64_t newlen)
{
    uint64_t length_b = bytes_to_blocks_ceil(length); /* Floor length in blocks */
    
    if (length != newlen) {
        RVMLIB_REC_OBJECT(*this);

        validdata = cached_chuncks->Count() * BYTES_BLOCK_SIZE;

        /* In case the the last block is set */
        if (cached_chuncks->Value(length_b - 1)) {
            validdata -= (length_b << BITS_BLOCK_SIZE) - length;
        }
        
        cached_chuncks->Resize(bytes_to_blocks_ceil(newlen));
        length = newlen;
    }
    
    LOG(60, ("CacheFile::SetLength: New Length: %d, Validata %d\n", newlen, validdata));
}

/* MUST be called from within transaction! */
void CacheFile::SetValidData(uint64_t len)
{
    SetValidData(0, len);
}

/* MUST be called from within transaction! */
void CacheFile::SetValidData(uint64_t start, int64_t len)
{
    uint64_t start_b = bytes_to_blocks_floor(start);
    uint64_t end_b = bytes_to_blocks_ceil(start + len);
    uint64_t newvaliddata = 0;
    uint64_t length_b_f = bytes_to_blocks_floor(length); /* Floor length in blocks */
    uint64_t length_b = bytes_to_blocks_ceil(length); /* Ceil length in blocks */
    
    if (len < 0) {
        end_b = length_b;
    }
    
    if (end_b > length_b) {
        end_b = length_b;
    }

    RVMLIB_REC_OBJECT(validdata);

    for (uint64_t i = start_b; i < end_b; i++) {
        if (cached_chuncks->Value(i)) {
            continue;
        }

        cached_chuncks->SetIndex(i);
        
        /* Add a full block */
        newvaliddata += BYTES_BLOCK_SIZE;

        /* The last block might not be full */
        if (i + 1 == length_b) {
            newvaliddata -= (length_b << BITS_BLOCK_SIZE) - length;
            continue;
        }
    }

    validdata += newvaliddata;

    LOG(60, ("CacheFile::SetValidData: { validdata: %d }\n", validdata));
    LOG(60, ("CacheFile::SetValidData: { fetchedblocks: %d, totalblocks: %d }\n",
            cached_chuncks->Count(), length_b));
}

void CacheFile::print(int fdes)
{
    fdprint(fdes, "[ %s, %d/%d ]\n", name, validdata, length);
}

int CacheFile::Open(int flags)
{
    int fd = ::open(name, flags | O_BINARY, V_MODE);

    CODA_ASSERT (fd != -1);
    numopens++;
    
    return fd;
}

int CacheFile::Close(int fd)
{
    CODA_ASSERT(fd != -1 && numopens);
    numopens--;
    return ::close(fd);
}

FILE *CacheFile::FOpen(const char *mode)
{
    int flags = 0;

    /* just support a subset */
    if (strcmp(mode, "r") == 0)
        flags = O_RDONLY;
    else if (strcmp(mode, "w") == 0)
        flags = O_WRONLY|O_TRUNC;
    else
        CODA_ASSERT(0);

    return fdopen(Open(flags), mode);
}

int CacheFile::FClose(FILE *f)
{
    CODA_ASSERT(f != NULL && numopens);
    int rc = fclose(f);
    numopens--;
    return rc;
}

uint64_t CacheFile::ConsecutiveValidData(void)
{
    /* Use the start of the first hole */
    uint64_t start = 0;
    uint64_t length_ccb = bytes_to_ccblocks_ceil(length);  // Ceil length in blocks

    /* Find the first 0 in the bitmap */
    for (start = 0; start < length_ccb; start++) {
        if (!cached_chuncks->Value(start)) {
            break;
        }
    }

    if (start != 0)
        start--;

    return start;
}

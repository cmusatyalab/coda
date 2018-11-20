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

uint64_t CacheChunkBlockSize = 0;
uint64_t CacheChunkBlockSizeMax = 0;
uint64_t CacheChunkBlockSizeBits = 0;
uint64_t CacheChunkBlockBitmapSize = 0;

/*  *****  CacheFile Members  *****  */

/* Pre-allocation routine. */
/* MUST be called from within transaction! */
CacheFile::CacheFile(int i, int recoverable)
{
    /* Assume caller has done RVMLIB_REC_OBJECT! */
    /* RVMLIB_REC_OBJECT(*this); */
    sprintf(name, "%02X/%02X/%02X/%02X",
	    (i>>24) & 0xff, (i>>16) & 0xff, (i>>8) & 0xff, i & 0xff);
            
    length = validdata = 0;
    refcnt = 1;
    numopens = 0;
    this->recoverable = recoverable;
    cached_chunks = new(recoverable) bitmap(CacheChunkBlockBitmapSize, recoverable);
    Lock_Init(&rw_lock);
    /* Container reset will be done by eventually by FSOInit()! */
    LOG(100, ("CacheFile::CacheFile(%d): %s (this=0x%x)\n", i, name, this));
}


CacheFile::CacheFile()
{
    CODA_ASSERT(length == 0);
    refcnt = 1;
    numopens = 0;
    this->recoverable = 1;
    Lock_Init(&rw_lock);
    cached_chunks = new(recoverable) bitmap(CacheChunkBlockBitmapSize, recoverable);
}


CacheFile::~CacheFile()
{
    LOG(10, ("CacheFile::~CacheFile: %s (this=0x%x)\n", name, this));
    CODA_ASSERT(length == 0);
    ::unlink(name);
    delete cached_chunks;
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
}


/*
 * copies a cache file, data and attributes, to a new one.
 */
int CacheFile::Copy(CacheFile *destination)
{
    Copy(destination->name);

    destination->length = length;
    destination->validdata = validdata;
    
    /* Acquire the locks in ascending address order */
    if (((uint64_t) &rw_lock) < ((uint64_t) &destination->rw_lock)) {
        ObtainReadLock(&rw_lock);
        ObtainWriteLock(&destination->rw_lock);
    } else {
        ObtainWriteLock(&destination->rw_lock);
        ObtainReadLock(&rw_lock);
    }

    *(destination->cached_chunks) = *cached_chunks;
    ReleaseWriteLock(&destination->rw_lock);
    ReleaseReadLock(&rw_lock);
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
void CacheFile::Truncate(uint64_t newlen)
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
        if (recoverable) RVMLIB_REC_OBJECT(*this);
    
        

        if (newlen < length) {
            ObtainWriteLock(&rw_lock);
            
            cached_chunks->FreeRange(bytes_to_ccblocks_floor(newlen), 
                bytes_to_ccblocks_ceil(length - newlen));
                
            ReleaseWriteLock(&rw_lock);
        } 

        length = newlen;

        UpdateValidData();
    }

    CODA_ASSERT(::ftruncate(fd, length) == 0);

    close(fd);
}

/* Update the valid data*/
void CacheFile::UpdateValidData() {
    uint64_t length_cb = bytes_to_ccblocks_ceil(length); /* Floor length in blocks */
    
    ObtainReadLock(&rw_lock);

    validdata = ccblocks_to_bytes(cached_chunks->Count());

    /* In case the the last block is set */
    if (cached_chunks->Value(length_cb - 1)) {
        validdata -= ccblocks_to_bytes(length_cb) - length;
    }
    
    ReleaseReadLock(&rw_lock);
}

/* MUST be called from within transaction! */
void CacheFile::SetLength(uint64_t newlen)
{    
    if (length != newlen) {
        if (recoverable) RVMLIB_REC_OBJECT(*this);

        if (newlen < length) {
            ObtainWriteLock(&rw_lock);
            
            cached_chunks->FreeRange(bytes_to_ccblocks_floor(newlen), 
                bytes_to_ccblocks_ceil(length - newlen));
                
            ReleaseWriteLock(&rw_lock);
        }

        length = newlen;

        UpdateValidData();

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
    uint64_t start_cb = ccblock_start(start);
    uint64_t end_cb = ccblock_end(start, len);
    uint64_t newvaliddata = 0;
    uint64_t length_cb = bytes_to_ccblocks_ceil(length);

    if (len < 0) {
        end_cb = length_cb;
    }

    if (end_cb > length_cb) {
        end_cb = length_cb;
    }

    if (recoverable) RVMLIB_REC_OBJECT(validdata);
    
    ObtainWriteLock(&rw_lock);

    for (uint64_t i = start_cb; i < end_cb; i++) {
        if (cached_chunks->Value(i)) {
            continue;
        }

        cached_chunks->SetIndex(i);

        /* Add a full block */
        newvaliddata += CacheChunkBlockSize;

        /* The last block might not be full */
        if (i + 1 == length_cb) {
            newvaliddata -= ccblocks_to_bytes(length_cb) - length;
            continue;
        }
    }
    
    ReleaseWriteLock(&rw_lock);

    validdata += newvaliddata;

    LOG(60, ("CacheFile::SetValidData: { validdata: %d }\n", validdata));
    LOG(60, ("CacheFile::SetValidData: { fetchedblocks: %d, totalblocks: %d }\n",
            cached_chunks->Count(), length_cb));
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
        if (!cached_chunks->Value(start)) {
            break;
        }
    }

    if (start != 0)
        start--;

    return start;
}

CacheChunkList::CacheChunkList()
{
    Lock_Init(&rd_wr_lock);
}

CacheChunkList::~CacheChunkList()
{
    CacheChunk * curr = NULL;

    while ((curr = (CacheChunk *)this->first())) {
        this->remove((dlink *)curr);
    }
}


CacheChunk CacheFile::GetNextHole(uint64_t start_b, uint64_t end_b) {
    /* Floor length in blocks */
    uint64_t length_b_f = bytes_to_ccblocks_floor(length);
    /* Ceil length in blocks */
    uint64_t length_b = bytes_to_ccblocks_ceil(length);
    uint64_t holestart = start_b;
    int64_t holesize = 0;

    for (uint64_t i = start_b; i < end_b; i++) {
        if (cached_chunks->Value(i)) {
            holesize = 0;
            holestart = i + 1;
            continue;
        }

        /* The last block might not be full */
        if (i + 1 == length_b) {
            holesize += length - (length_b_f << CacheChunkBlockSizeBits);
            return (CacheChunk(holestart * CacheChunkBlockSize, holesize));

        }

        /* Add a full block */
        holesize += CacheChunkBlockSize;

        if ((i + 1 == end_b) || cached_chunks->Value(i + 1)) {
            return (CacheChunk(holestart * CacheChunkBlockSize, holesize));
        }
    }

    return (CacheChunk());
}

CacheChunkList * CacheFile::GetHoles(uint64_t start, int64_t len) {
    uint64_t start_b = ccblock_start(start);
    uint64_t end_b = ccblock_end(start, len);
    uint64_t length_b = bytes_to_ccblocks_ceil(length);  // Ceil length in blocks
    CacheChunkList * clist = new CacheChunkList();
    CacheChunk currc;

    if (len < 0) {
        end_b = length_b;
    }

    LOG(0, ("CacheFile::GetHoles Range [%d - %d]\n", start, start + len - 1));

    for (uint64_t i = start_b; i < end_b; i++) {
        currc = GetNextHole(i, end_b);

        if (!currc.isValid()) break;

        LOG(0, ("CacheFile::GetHoles Found [%d, %d]\n", currc.GetStart(),
                currc.GetLength()));

        clist->AddChunk(currc.GetStart(), currc.GetLength());
        i = currc.GetStart() + currc.GetLength() + 1;
    }

    return clist;
}

void CacheChunkList::AddChunk(uint64_t start, int64_t len)
{
    WriteLock();
    CacheChunk * new_chunck = new CacheChunk(start, len);
    this->insert((dlink *) new_chunck);
    WriteUnlock();
}

void CacheChunkList::ReadLock()
{
    ObtainReadLock(&rd_wr_lock);
}

void CacheChunkList::WriteLock()
{
    ObtainWriteLock(&rd_wr_lock);
}


void CacheChunkList::ReadUnlock()
{
    ReleaseReadLock(&rd_wr_lock);
}

void CacheChunkList::WriteUnlock()
{
    ReleaseWriteLock(&rd_wr_lock);
}

bool CacheChunkList::ReverseCheck(uint64_t start, int64_t len)
{
    dlink * curr = NULL;
    CacheChunk * curr_cc = NULL;

    ReadLock();

    dlist_iterator previous(*this, DlDescending);

    while (curr = previous()) {
        curr_cc = (CacheChunk *)curr;

        if (!curr_cc->isValid()) continue;

        if (start != curr_cc->GetStart()) continue;

        if (len != curr_cc->GetLength()) continue;

        ReadUnlock();

        return true;
    }

    ReadUnlock();

    return false;
}

void CacheChunkList::ReverseRemove(uint64_t start, int64_t len)
{
    dlink * curr = NULL;
    CacheChunk * curr_cc = NULL;

    WriteLock();

    dlist_iterator previous(*this, DlDescending);

    while (curr = previous()) {
        curr_cc = (CacheChunk *)curr;

        if (!curr_cc->isValid()) continue;

        if (start != curr_cc->GetStart()) continue;

        if (len != curr_cc->GetLength()) continue;

        this->remove(curr);
        break;
    }

    WriteUnlock();
}

void CacheChunkList::ForEach(void (*foreachcb)(uint64_t start, int64_t len, 
    void * usr_data_cb), void * usr_data)
{
    dlink * curr = NULL;
    CacheChunk * curr_cc = NULL;

    if (!foreachcb) return;

    ReadLock();

    dlist_iterator next(*this);

    while (curr = next()) {
        curr_cc = (CacheChunk *)curr;
        foreachcb(curr_cc->GetStart(), curr_cc->GetLength(), usr_data);
    }

    ReadUnlock();
}

CacheChunk CacheChunkList::pop()
{
    dlink * curr_first = NULL;
    CacheChunk * tmp = NULL;

    WriteLock();

    curr_first = this->first();
    tmp = (CacheChunk *)curr_first; 

    if (!curr_first) {
        WriteUnlock();
        return CacheChunk();
    }

    CacheChunk cp = CacheChunk(*tmp);
    this->remove(curr_first);
    delete tmp;

    WriteUnlock();

    return CacheChunk(cp);
}

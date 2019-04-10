/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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

#ifndef fdatasync
#define fdatasync(fd) fsync(fd)
#endif

/* always useful to have a page of zero's ready */
static char zeropage[4096];

uint64_t cachechunksutil::CacheChunkBlockSize       = 0;
uint64_t cachechunksutil::CacheChunkBlockSizeBits   = 0;
uint64_t cachechunksutil::CacheChunkBlockSizeMax    = 0;
uint64_t cachechunksutil::CacheChunkBlockBitmapSize = 0;

/*  *****  CacheFile Members  *****  */

/* Pre-allocation routine. */
/* MUST be called from within transaction! */
CacheFile::CacheFile(int i, int recoverable, int partial)
{
    /* Assume caller has done RVMLIB_REC_OBJECT! */
    /* RVMLIB_REC_OBJECT(*this); */
    sprintf(name, "%02X/%02X/%02X/%02X", (i >> 24) & 0xff, (i >> 16) & 0xff,
            (i >> 8) & 0xff, i & 0xff);

    length = validdata = 0;
    refcnt             = 1;
    numopens           = 0;
    isPartial          = partial;
    this->recoverable  = recoverable;
    if (isPartial) {
        cached_chunks = new (recoverable)
            bitmap7(cachechunksutil::get_ccblocks_bitmap_size(), recoverable);
        Lock_Init(&rw_lock);
    }

    /* Container reset will be done by eventually by FSOInit()! */

    LOG(100, ("CacheFile::CacheFile(%d): %s (this=0x%x)\n", i, name, this));
}

CacheFile::CacheFile()
{
    LOG(10, ("CacheFile::CacheFile: %s (this=0x%x)\n", name, this));

    length = validdata = 0;
    refcnt             = 1;
    numopens           = 0;
    isPartial          = 0;
    recoverable        = 1;
}

CacheFile::~CacheFile()
{
    LOG(10, ("CacheFile::~CacheFile: %s (this=0x%x)\n", name, this));
    CODA_ASSERT(length == 0);

    if (isPartial)
        delete cached_chunks;

    ::unlink(name);
}

/* MUST NOT be called from within transaction! */
void CacheFile::Validate()
{
    if (!ValidContainer())
        Reset();
}

void CacheFile::SetPartial(bool is_partial)
{
    if (is_partial == this->isPartial)
        return;

    this->isPartial = is_partial;

    if (recoverable)
        RVMLIB_REC_OBJECT(*this);

    if (is_partial) {
        cached_chunks = new (recoverable) bitmap7(
            cachechunksutil::get_ccblocks_bitmap_size(), this->recoverable);
        Lock_Init(&this->rw_lock);

        /* If there's valid data set it to the bitmap */
        SetValidData(validdata);
    } else {
        /* Set validdata to mimic disconnection while fetching */
        SetValidData(ConsecutiveValidData());

        if (cached_chunks)
            delete (cached_chunks);
        cached_chunks = NULL;
    }
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
    if (rc)
        return 0;

    int valid =
#ifndef __CYGWIN32__
        tstat.st_uid == (uid_t)V_UID && tstat.st_gid == (gid_t)V_GID &&
        (tstat.st_mode & ~S_IFMT) == V_MODE &&
#endif
        tstat.st_size == (off_t)length;

    if (!valid && GetLogLevel() >= 0) {
        dprint("CacheFile::ValidContainer: %s invalid\n", name);
        dprint("\t(%u, %u), (%u, %u), (%o, %o), (%d, %d)\n", tstat.st_uid,
               (uid_t)V_UID, tstat.st_gid, (gid_t)V_GID,
               (tstat.st_mode & ~S_IFMT), V_MODE, tstat.st_size, length);
    }
    return (valid);
}

/* Must be called from within a transaction!  Assume caller has done
   RVMLIB_REC_OBJECT() */

void CacheFile::Create(int newlength)
{
    LOG(10, ("CacheFile::Create: %s, %d\n", name, newlength));

    int tfd;
    struct stat tstat;
    if (mkpath(name, V_MODE | 0100) < 0)
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

    // clear bitmap
    if (isPartial)
        cached_chunks->FreeRange(0, -1);

    validdata = 0;
    length    = newlength;
    refcnt    = 1;
    numopens  = 0;
}

/*
 * copies a cache file, data and attributes, to a new one.
 */
int CacheFile::Copy(CacheFile *destination)
{
    Copy(destination->name);

    destination->length    = length;
    destination->validdata = validdata;

    if (isPartial) {
        ObtainDualLock(&rw_lock, READ_LOCK, &destination->rw_lock, WRITE_LOCK);

        *(destination->cached_chunks) = *cached_chunks;

        ReleaseDualLock(&rw_lock, READ_LOCK, &destination->rw_lock, WRITE_LOCK);
    }

    return 0;
}

int CacheFile::Copy(char *destname, int recovering)
{
    LOG(10, ("CacheFile::Copy: from %s, %d/%d, to %s\n", name, validdata,
             length, destname));

    int tfd, ffd;
    struct stat tstat;

    if (mkpath(destname, V_MODE | 0100) < 0) {
        LOG(0, ("CacheFile::Copy: could not make path for %s\n", name));
        return -1;
    }
    if ((tfd = ::open(destname, O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
                      V_MODE)) < 0) {
        LOG(0, ("CacheFile::Copy: open failed (%d)\n", errno));
        return -1;
    }
    ::fchmod(tfd, V_MODE);
#ifdef __CYGWIN32__
    ::chown(destname, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif
    if ((ffd = ::open(name, O_RDONLY | O_BINARY, V_MODE)) < 0)
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
    if (--refcnt == 0) {
        // clear bitmap
        if (isPartial)
            cached_chunks->FreeRange(0, -1);

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
    static bool option_isr = GetVenusConf().get_bool_value("isr");
    int fd;

    fd = open(name, O_WRONLY | O_BINARY);
    CODA_ASSERT(fd >= 0 && "fatal error opening container file");

    /* ISR tweak, write zeros to data area before truncation */
    if (option_isr && newlen < length) {
        size_t len = sizeof(zeropage), n = length - newlen;

        lseek(fd, newlen, SEEK_SET);

        while (n) {
            if (n < len)
                len = n;
            write(fd, zeropage, len);
            n -= len;
        }
        /* we have to force these writes to disk, otherwise the following
	 * truncate would simply drop any unwritten data */
        fdatasync(fd);
    }

    if (recoverable)
        RVMLIB_REC_OBJECT(*this);

    if (length != newlen) {
        if (isPartial) {
            if (newlen < length) {
                ObtainWriteLock(&rw_lock);

                cached_chunks->FreeRange(
                    cachechunksutil::bytes_to_ccblocks_ceil(newlen),
                    cachechunksutil::bytes_to_ccblocks_ceil(length) -
                        cachechunksutil::bytes_to_ccblocks_ceil(newlen));

                ReleaseWriteLock(&rw_lock);
            }

            length = newlen;
            UpdateValidData();
        } else {
            /* Keep what is still valid data while shrinking */
            if (newlen < length)
                validdata = newlen;
            length = newlen;
        }
    }

    CODA_ASSERT(::ftruncate(fd, length) == 0);

    close(fd);
}

/* Update the valid data*/
void CacheFile::UpdateValidData()
{
    uint64_t length_cb = cachechunksutil::bytes_to_ccblocks_ceil(length);

    CODA_ASSERT(isPartial);

    ObtainReadLock(&rw_lock);

    validdata = cachechunksutil::ccblocks_to_bytes(cached_chunks->Count());

    /* In case the the last block is set */
    if (length_cb && cached_chunks->Value(length_cb - 1)) {
        uint64_t empty_tail =
            cachechunksutil::ccblocks_to_bytes(length_cb) - length;
        validdata -= empty_tail;
    }

    ReleaseReadLock(&rw_lock);
}

/* MUST be called from within transaction! */
void CacheFile::SetLength(uint64_t newlen)
{
    if (length != newlen) {
        if (recoverable)
            RVMLIB_REC_OBJECT(*this);

        if (isPartial) {
            if (newlen < length) {
                ObtainWriteLock(&rw_lock);

                cached_chunks->FreeRange(
                    cachechunksutil::bytes_to_ccblocks_floor(newlen),
                    cachechunksutil::bytes_to_ccblocks_ceil(length - newlen));

                ReleaseWriteLock(&rw_lock);
            }

            length = newlen;

            UpdateValidData();

        } else {
            length = newlen;
        }
    }

    LOG(60, ("CacheFile::SetLength: New Length: %d, Validdata %d\n", newlen,
             validdata));
}

/* MUST be called from within transaction! */
void CacheFile::SetValidData(uint64_t len)
{
    SetValidData(0, len);
}

/* MUST be called from within transaction! */
void CacheFile::SetValidData(uint64_t start, int64_t len)
{
    CODA_ASSERT(start <= length);

    // Negative length indicates we wanted (or got) to end of file
    if (len < 0)
        len = length - start;

    CODA_ASSERT(start + len <= length);

    // Empty received range implies we won't need to mark anything as cached
    if (len == 0) // this implicitly covers 'length == 0' as well
        return;

    // Skip partial blocks at the start or end
    uint64_t start_cb  = cachechunksutil::bytes_to_ccblocks_ceil(start);
    uint64_t end_cb    = cachechunksutil::bytes_to_ccblocks_floor(start + len);
    uint64_t length_cb = cachechunksutil::bytes_to_ccblocks_ceil(length);
    uint64_t newvaliddata = 0;

    if (recoverable)
        RVMLIB_REC_OBJECT(validdata);

    if (isPartial) {
        ObtainWriteLock(&rw_lock);

        for (uint64_t i = start_cb; i < end_cb; i++) {
            if (cached_chunks->Value(i))
                continue;

            /* Account for a full block */
            cached_chunks->SetIndex(i);
            newvaliddata += cachechunksutil::get_ccblocks_size();
        }

        /* End of file? The last block is allowed to not be full */
        CODA_ASSERT(length_cb > 0);
        if (start + len == length && !cached_chunks->Value(length_cb - 1)) {
            uint64_t tail =
                length - cachechunksutil::ccblocks_to_bytes(length_cb - 1);

            /* Account for a last partial block */
            cached_chunks->SetIndex(length_cb - 1);
            newvaliddata += tail;
        }

        validdata += newvaliddata;

        LOG(60,
            ("CacheFile::SetValidData: { cachedblocks: %d, totalblocks: %d }\n",
             cached_chunks->Count(), length_cb));

        ReleaseWriteLock(&rw_lock);
    } else {
        validdata = len < 0 ? length : len;
    }

    LOG(60, ("CacheFile::SetValidData: { validdata: %d }\n", validdata));
}

void CacheFile::print(int fdes)
{
    fdprint(fdes, "[ %s, %d/%d ]\n", name, validdata, length);
}

int CacheFile::Open(int flags)
{
    int fd = ::open(name, flags | O_BINARY, V_MODE);

    CODA_ASSERT(fd != -1);
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
        flags = O_WRONLY | O_TRUNC;
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
    /* Find the start of the first hole */
    uint64_t index      = 0;
    uint64_t length_ccb = cachechunksutil::bytes_to_ccblocks_ceil(length);

    if (!isPartial)
        return validdata;

    /* Find the first 0 in the bitmap */
    for (index = 0; index < length_ccb; index++)
        if (!cached_chunks->Value(index))
            break;

    /* In case we reached the last cache block */
    if (index == length_ccb)
        return length;

    return cachechunksutil::ccblocks_to_bytes(index);
}

int64_t CacheFile::CopySegment(CacheFile *from, CacheFile *to, uint64_t pos,
                               int64_t count)
{
    uint32_t byte_start  = cachechunksutil::pos_align_to_ccblock(pos);
    uint32_t block_start = cachechunksutil::bytes_to_ccblocks(byte_start);
    uint32_t byte_len    = cachechunksutil::length_align_to_ccblock(pos, count);
    int tfd, ffd;
    struct stat tstat;
    CacheChunkList *c_list;
    CacheChunk chunk;

    CODA_ASSERT(from->IsPartial());
    CODA_ASSERT(to->IsPartial());

    LOG(300, ("CacheFile::CopySegment: from %s [%d, %d], to %s\n", from->name,
              byte_start, byte_len, to->name));

    if (mkpath(to->name, V_MODE | 0100) < 0) {
        LOG(0,
            ("CacheFile::CopySegment: could not make path for %s\n", to->name));
        return -1;
    }

    tfd = to->Open(O_RDWR | O_CREAT);

    ::fchmod(tfd, V_MODE);

#ifdef __CYGWIN32__
    ::chown(name, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif

    ffd = from->Open(O_RDONLY);

    /* Skip the holes */
    c_list = from->GetValidChunks(byte_start, byte_len);

    for (chunk = c_list->pop(); chunk.isValid(); chunk = c_list->pop()) {
        if (copyfile_seg(ffd, tfd, chunk.GetStart(), chunk.GetLength()) < 0) {
            LOG(0, ("CacheFile::CopySegment failed! (%d)\n", errno));
            from->Close(ffd);
            to->Close(tfd);
            return -1;
        }
    }

    delete c_list;

    if (from->Close(ffd) < 0) {
        CHOKE("CacheFile::CopySegment: source close failed (%d)\n", errno);
    }

    if (::fstat(tfd, &tstat) < 0) {
        CHOKE("CacheFile::CopySegment: fstat failed (%d)\n", errno);
    }

    if (to->Close(tfd) < 0) {
        CHOKE("CacheFile::CopySegment: close failed (%d)\n", errno);
    }

    CODA_ASSERT((off_t)from->length == tstat.st_size);

    ObtainDualLock(&from->rw_lock, READ_LOCK, &to->rw_lock, WRITE_LOCK);

    from->cached_chunks->CopyRange(block_start,
                                   cachechunksutil::bytes_to_ccblocks(byte_len),
                                   *(to->cached_chunks));

    ReleaseDualLock(&from->rw_lock, READ_LOCK, &to->rw_lock, WRITE_LOCK);

    to->UpdateValidData();

    return byte_len;
}

CacheChunkList::CacheChunkList()
{
    Lock_Init(&rd_wr_lock);
}

CacheChunkList::~CacheChunkList()
{
    CacheChunk *curr = NULL;

    while ((curr = (CacheChunk *)this->first())) {
        this->remove((dlink *)curr);
    }
}

CacheChunk CacheFile::GetNextHole(uint64_t start_b, uint64_t end_b)
{
    CODA_ASSERT(start_b <= end_b);
    /* Number of blocks of the cache file */
    uint64_t nblocks = cachechunksutil::bytes_to_ccblocks_ceil(length);
    /* Number of full blocks */
    uint64_t nblocks_full   = cachechunksutil::bytes_to_ccblocks_floor(length);
    uint64_t hole_start_idx = 0;
    uint64_t hole_end_idx   = 0;
    uint64_t hole_size      = 0;

    CODA_ASSERT(isPartial);
    CODA_ASSERT(start_b <= nblocks);

    if (end_b > nblocks) {
        end_b = nblocks;
    }

    /* Find the start of the hole */
    for (hole_start_idx = start_b; hole_start_idx < end_b; hole_start_idx++) {
        if (!cached_chunks->Value(hole_start_idx)) {
            break;
        }
    }

    /* No hole */
    if (hole_start_idx == end_b)
        return CacheChunk();

    /* Find the end of the hole */
    for (hole_end_idx = hole_start_idx; hole_end_idx < end_b; hole_end_idx++) {
        if (cached_chunks->Value(hole_end_idx)) {
            break;
        }
    }

    CODA_ASSERT(hole_end_idx > hole_start_idx);

    hole_size =
        cachechunksutil::ccblocks_to_bytes(hole_end_idx - hole_start_idx - 1);

    /* If the hole ends at EOF and it has a tail */
    if (hole_end_idx == nblocks && nblocks_full != nblocks) {
        /* Add the tail */
        uint64_t tail =
            length - cachechunksutil::ccblocks_to_bytes(nblocks - 1);
        hole_size += tail;
    } else {
        /* Add the last accounted block as a whole block*/
        hole_size += cachechunksutil::get_ccblocks_size();
    }

    return (CacheChunk(cachechunksutil::ccblocks_to_bytes(hole_start_idx),
                       hole_size));
}

CacheChunkList *CacheFile::GetHoles(uint64_t start, int64_t len)
{
    uint64_t start_b  = cachechunksutil::ccblock_start(start);
    uint64_t end_b    = cachechunksutil::ccblock_end(start, len);
    uint64_t length_b = cachechunksutil::bytes_to_ccblocks_ceil(
        length); // Ceil length in blocks
    CacheChunkList *clist = new CacheChunkList();
    CacheChunk currc;

    CODA_ASSERT(isPartial);

    if (len < 0 || end_b > length_b) {
        end_b = length_b;
    }

    LOG(100, ("CacheFile::GetHoles Range [%lu - %lu]\n",
              cachechunksutil::ccblocks_to_bytes(start_b),
              cachechunksutil::ccblocks_to_bytes(end_b) - 1));

    for (uint64_t i = start_b; i < end_b; i++) {
        currc = GetNextHole(i, end_b);

        if (!currc.isValid())
            break;

        LOG(100, ("CacheFile::GetHoles Found [%d, %d]\n", currc.GetStart(),
                  currc.GetLength()));

        clist->AddChunk(currc.GetStart(), currc.GetLength());
        i = cachechunksutil::bytes_to_ccblocks(currc.GetStart() +
                                               currc.GetLength() - 1);
    }

    return clist;
}

CacheChunkList *CacheFile::GetValidChunks(uint64_t start, int64_t len)
{
    uint64_t start_b     = cachechunksutil::ccblock_start(start);
    uint64_t start_bytes = 0;
    uint64_t end_b       = cachechunksutil::ccblock_end(start, len);
    uint64_t length_b    = cachechunksutil::bytes_to_ccblocks_ceil(
        length); // Ceil length in blocks
    CacheChunkList *clist = new CacheChunkList();
    CacheChunk currc;
    uint64_t i = start_b;

    CODA_ASSERT(isPartial);

    if (len < 0) {
        end_b = length_b;
    }

    LOG(100, ("CacheFile::GetValidChunks Range [%lu - %lu]\n",
              cachechunksutil::ccblocks_to_bytes(start_b),
              cachechunksutil::ccblocks_to_bytes(end_b) - 1));

    for (i = start_b; i < end_b; i++) {
        currc = GetNextHole(i, end_b);

        if (!currc.isValid())
            break;

        LOG(100, ("CacheFile::GetValidChunks Found [%d, %d]\n",
                  currc.GetStart(), currc.GetLength()));

        start_bytes = cachechunksutil::ccblocks_to_bytes(i);

        if (start_bytes != currc.GetStart()) {
            clist->AddChunk(start_bytes, currc.GetStart() - start_bytes + 1);
        }

        i = cachechunksutil::bytes_to_ccblocks(currc.GetStart() +
                                               currc.GetLength() - 1);
    }

    /* Is case de range ends with valid data */
    if (i < end_b) {
        start_bytes = cachechunksutil::ccblocks_to_bytes(i);
        clist->AddChunk(i, end_b - i + 1);
    }

    return clist;
}

void CacheChunkList::AddChunk(uint64_t start, int64_t len)
{
    WriteLock();
    CacheChunk *new_chunk = new CacheChunk(start, len);
    this->insert((dlink *)new_chunk);
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
    dlink *curr         = NULL;
    CacheChunk *curr_cc = NULL;

    ReadLock();

    dlist_iterator previous(*this, DlDescending);

    while ((curr = previous())) {
        curr_cc = (CacheChunk *)curr;

        if (!curr_cc->isValid())
            continue;

        if (start != curr_cc->GetStart())
            continue;

        if (len != curr_cc->GetLength())
            continue;

        ReadUnlock();

        return true;
    }

    ReadUnlock();

    return false;
}

void CacheChunkList::ReverseRemove(uint64_t start, int64_t len)
{
    dlink *curr         = NULL;
    CacheChunk *curr_cc = NULL;

    WriteLock();

    dlist_iterator previous(*this, DlDescending);

    while ((curr = previous())) {
        curr_cc = (CacheChunk *)curr;

        if (!curr_cc->isValid())
            continue;

        if (start != curr_cc->GetStart())
            continue;

        if (len != curr_cc->GetLength())
            continue;

        this->remove(curr);
        break;
    }

    WriteUnlock();
}

void CacheChunkList::ForEach(void (*foreachcb)(uint64_t start, int64_t len,
                                               void *usr_data_cb),
                             void *usr_data)
{
    dlink *curr         = NULL;
    CacheChunk *curr_cc = NULL;

    if (!foreachcb)
        return;

    ReadLock();

    dlist_iterator next(*this);

    while ((curr = next())) {
        curr_cc = (CacheChunk *)curr;
        foreachcb(curr_cc->GetStart(), curr_cc->GetLength(), usr_data);
    }

    ReadUnlock();
}

CacheChunk CacheChunkList::pop()
{
    dlink *curr_first = NULL;
    CacheChunk *tmp   = NULL;

    WriteLock();

    curr_first = this->first();
    tmp        = (CacheChunk *)curr_first;

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

/* MUST be called from within transaction! */
SegmentedCacheFile::SegmentedCacheFile(int i)
    : CacheFile(i, 0, 1)
{
    sprintf(name, "%02X/%02X/%02X/%02X.seg", (i >> 24) & 0xff, (i >> 16) & 0xff,
            (i >> 8) & 0xff, i & 0xff);
}

SegmentedCacheFile::~SegmentedCacheFile()
{
    this->Truncate(0);
    DecRef();
}

void SegmentedCacheFile::Associate(CacheFile *cf)
{
    CacheFile::Create(cf->length);
    this->cf = cf;
}

int64_t SegmentedCacheFile::ExtractSegment(uint64_t pos, int64_t count)
{
    return CopySegment(cf, this, pos, count);
}

int64_t SegmentedCacheFile::InjectSegment(uint64_t pos, int64_t count)
{
    return CopySegment(this, cf, pos, count);
}

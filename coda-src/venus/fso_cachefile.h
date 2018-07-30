/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2008 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation

#*/

/*
 *
 *    Specification of the Venus File-System Object (fso) abstraction.
 *
 *    ToDo:
 *
 */


#ifndef _VENUS_FSO_CACHEFILE_H_
#define _VENUS_FSO_CACHEFILE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
// #include <sys/stat.h>

// #include <sys/uio.h>

// #include <rpc2/rpc2.h>
// 
// #include <codadir.h>

extern int global_kernfd;
#ifdef __cplusplus
}
#endif

/* interfaces */
// #include <user.h>
// #include <vice.h>

/* from util */
#include <bitmap.h>

/* from lka */
// #include <lka.h>

/* from venus */


/* from coda include again, must appear AFTER venus.private.h */


/* Representation of UFS file in cache directory. */
/* Currently, CacheFiles and fsobjs are statically bound to each
   other, one-to-one, by embedding */
/* a single CacheFile in each fsobj.  An fsobj may use its CacheFile
   in several ways (or not at all). */
/* We guarantee that these uses are mutually exclusive (in time,
   per-fsobj), hence the static allocation */
/* policy works.  In the future we may choose not to make the uses
   mutually exclusive, and will then */
/* have to implement some sort of dynamic allocation/binding
   scheme. */
/* The different uses of CacheFile are: */
/*    1. Copy of plain file */
/*    2. Unix-format copy of directory */

#define CACHEFILENAMELEN 12

#define BYTES_BLOCK_SIZE 4096
#define BYTES_BLOCK_SIZE_MAX (BYTES_BLOCK_SIZE - 1)
#define BITS_BLOCK_SIZE 12 /* 4096 = 2^12 */

static inline uint64_t bytes_to_blocks_floor(uint64_t bytes) {
    return bytes >> BITS_BLOCK_SIZE;
}

static inline uint64_t bytes_to_blocks_ceil(uint64_t bytes) {
    return bytes_to_blocks_floor(bytes + BYTES_BLOCK_SIZE_MAX);
}

static inline uint64_t bytes_round_up_block_size(uint64_t bytes) {
    return bytes_to_blocks_ceil(bytes) << BITS_BLOCK_SIZE;
}

static inline uint64_t bytes_round_down_block_size(uint64_t bytes) {
    return bytes_to_blocks_floor(bytes) << BITS_BLOCK_SIZE;
}

class CacheFile {
    long length;
    long validdata; /* amount of successfully fetched data */
    int  refcnt;
    char name[CACHEFILENAMELEN];		/* "xx/xx/xx/xx" */
    int numopens;
    bitmap *cached_chuncks;

    int ValidContainer();

 public:
    CacheFile(int);
    CacheFile();
    ~CacheFile();

    /* for safely obtaining access to container files, USE THESE!!! */
    void Create(int newlength = 0);
    int Open(int flags);
    int Close(int fd);

    FILE *FOpen(const char *mode);
    int FClose(FILE *f);

    void Validate();
    void Reset();
    int  Copy(CacheFile *destination);
    int  Copy(char *destname, int recovering = 0);

    void IncRef() { refcnt++; } /* creation already does an implicit incref */
    int  DecRef();             /* returns refcnt, unlinks if refcnt becomes 0 */

    void Stat(struct stat *);
    void Utimes(const struct timeval times[2]);
    void Truncate(long);
    void SetLength(long);
    void SetValidData(uint64_t len);
    void SetValidData(uint64_t start, int64_t len);

    char *Name()         { return(name); }
    long Length()        { return(length); }
    uint64_t ValidData(void) { return(validdata); }
    uint64_t ConsecutiveValidData(void);
    int  IsPartial(void) { return(length != validdata); }

    void print() { print (stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};

#endif	/* _VENUS_FSO_CACHEFILE_H_ */

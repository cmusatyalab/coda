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
#include <lwp/lock.h>

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

extern uint64_t CacheChunkBlockSize;
extern uint64_t CacheChunkBlockSizeBits;
extern uint64_t CacheChunkBlockSizeMax;
extern uint64_t CacheChunkBlockBitmapSize;

/**
 * Convert cache chunk block amount to bytes
 *
 * @param ccblocks cache chunk block amount
 *
 * @return bytes amount
 */
static inline uint64_t ccblocks_to_bytes(uint64_t ccblocks) {
    return ccblocks << CacheChunkBlockSizeBits;
}

/**
 * Convert bytes amount to cache chunk block amount (rounded down)
 *
 * @param bytes bytes amount
 *
 * @return cache chunk block amount
 */
static inline uint64_t bytes_to_ccblocks(uint64_t bytes) {
    return bytes >> CacheChunkBlockSizeBits;
}

/**
 * Convert bytes amount to cache chunk block amount (rounded down)
 *
 * @param bytes bytes amount
 *
 * @return cache chunk block amount
 */
static inline uint64_t bytes_to_ccblocks_floor(uint64_t bytes) {
    return bytes_to_ccblocks(bytes);
}

/**
 * Convert bytes amount to cache chunk block amount (rounded up)
 *
 * @param bytes bytes amount
 *
 * @return cache chunk block amount
 */
static inline uint64_t bytes_to_ccblocks_ceil(uint64_t bytes) {
    return bytes_to_ccblocks(bytes + CacheChunkBlockSizeMax);
}

/**
 * Align a bytes amount to the cache chunk block's upper-bound
 *
 * @param bytes bytes amount
 *
 * @return align amount in bytes
 */
static inline uint64_t align_to_ccblock_ceil(uint64_t bytes)
{
    return (bytes + CacheChunkBlockSizeMax) & ~CacheChunkBlockSizeMax;
}

/**
 * Align a bytes amount to the cache chunk block's lower-bound
 *
 * @param bytes bytes amount
 *
 * @return align amount in bytes
 */
static inline uint64_t align_to_ccblock_floor(uint64_t bytes)
{
    return (bytes & ~CacheChunkBlockSizeMax);
}

/**
 * Align a file position in bytes to the corresponding cache chunk block
 *
 * @param b_pos file position in bytes
 *
 * @return cache chunk block in which the file position is
 */
static inline uint64_t ccblock_start(uint64_t b_pos)
{
    return bytes_to_ccblocks_floor(b_pos);
}

/**
 * Align the end of range in bytes to the corresponding cache chunk block
 *
 * @param b_pos   start of the range in bytes
 * @param b_count lenght of the range in bytes
 *
 * @return cache chunk block in which the range ends
 */
static inline uint64_t ccblock_end(uint64_t b_pos, int64_t b_count)
{
    return bytes_to_ccblocks_ceil(b_pos + b_count);
}

/**
 * Calculate the amount of cache chunk blocks of a range (aligned)
 *
 * @param b_pos   start of the range in bytes
 * @param b_count lenght of the range in bytes
 *
 * @return amount of the cache chunk blocks
 */
static inline uint64_t ccblock_length(uint64_t b_pos, int64_t b_count)
{
    return ccblock_end(b_pos, b_count) - ccblock_start(b_pos);
}

/**
 * Align a file position in bytes to the start of the corresponding cache 
 * chunk block
 *
 * @param b_pos file position in bytes
 *
 * @return start of the corresponding cache chunk block in bytes
 */
static inline uint64_t pos_align_to_ccblock(uint64_t b_pos)
{
    return (b_pos & ~CacheChunkBlockSizeMax);
}

/**
 * Align a the end of a range in bytes to the end of the corresponding cache 
 * chunk block
 *
 * @param b_pos   start of the range in bytes
 * @param b_count lenght of the range in bytes
 *
 * @return end of the corresponding cache chunk block in bytes
 */
static inline uint64_t length_align_to_ccblock(uint64_t b_pos, int64_t b_count)
{
    return ccblocks_to_bytes(ccblock_length(b_pos, b_count));
}

#define FS_BLOCKS_SIZE_MAX    (4095)
#define FS_BLOCKS_SIZE_MASK   (~FS_BLOCKS_SIZE_MAX)
#define FS_BLOCKS_ALIGN(size) ((size + FS_BLOCKS_SIZE_MAX) & \ 
                               FS_BLOCKS_SIZE_MASK)

class CacheFile {
    long length;  /**< Length of the container file */
    long validdata; /**< Amount of actual and valid data in the container file */
    int  refcnt; /**< Reference counter */
    char name[CACHEFILENAMELEN]; /**< Container file path ("xx/xx/xx/xx") */
    int numopens; /**< Number of openers */
    bitmap *cached_chunks; /**< Bitmap of actual cached data */
    int recoverable;  /**< Recoverable flag (RVM) */
    Lock rw_lock;  /**< Read/Write Lock */

    /**
     * Validate the container file
     *
     * @return zero if file is invalid and different than zero otherwise
     */
    int ValidContainer();
    
    /**
     * Calculate the actual valid data based on the caching bitmap
     */
    void UpdateValidData();

public:

    /**
     * Constructor
     *
     * @param i            CacheFile index
     * @param recoverable  set cachefile to be recoverable from RVM
     */
    CacheFile(int i, int recoverable = 1);

    /**
     * Constructor
     */
    CacheFile();
    
    /**
     * Destructor
     */
    ~CacheFile();

    
    /**
     * Create and initialize a new cachefile (container file will be also 
     * created)
     *
     * @param newlength length of the container file in bytes
     */
    void Create(int newlength = 0);

    /**
     * Open the container file (with open sys call)
     *
     * @param flags open flags
     *
     * @return file descriptor of the file if opened successfully (Venus fails
     *         if container file couldn't be opened)
     */
    int Open(int flags);

    /**
     * Close the container file
     *
     * @param fd file descriptor of the opened file
     *
     * @return zero on success or -1 on error
     */
    int Close(int fd);

    /**
     * Open the container file (with fopen sys call)
     *
     * @param mode opening mode (only "r" and "w" supported)
     *
     * @return FILE structure of successfully opened file or NULL otherwise
     */
    FILE *FOpen(const char *mode);

    /**
     * Close the container file
     *
     * @param f FILE structure fo the opened file
     *
     * @return zero on success or errno otherwise
     */
    int FClose(FILE *f);

    /**
     * Validate the container file (Resets the container file if invalid)
     */
    void Validate();
    
    /**
     * Reset the container file to zero length and no data
     */
    void Reset();
    
    /**
     * Copy the container file and metadata to another object
     *
     * @param destination destination cache file object
     *
     * @return zero on success or -1 on error
     */
    int Copy(CacheFile *destination);
    
    /**
     * Copy the container file (only) to a specified location
     *
     * @param destname   destination cache file location
     * @param recovering flag if the cache file is being recovered
     *
     * @return zero on success or -1 on error
     */
    int  Copy(char *destname, int recovering = 0);

    /**
     * Increment reference counter. Creation already does an implicit IncRef() 
     */
    void IncRef() { refcnt++; }
    
    /**
     * Decrements reference counter
     *
     * @return reference counter value (unlinks if it becomes 0)
     */
    int DecRef();

    /**
     * Get container file's stat
     *
     * @param stat output stat structure
     */
    void Stat(struct stat *tstat);
    
    /**
     * Change container file's last access and modification times
     *
     * @param times time array of last access time and modification time, 
     *        respectively.
     */
    void Utimes(const struct timeval times[2]);
    
    /**
     * Truncate file to a new size
     *
     * @param newlen size to which the file will be truncated to
     */
    void Truncate(uint64_t newlen);
    
    /**
     * Set the cache file length without truncating the container file
     *
     * @param newlen new cache file metadata size
     */
    void SetLength(uint64_t newlen);
    
    /**
     * Set cache file's valid data (consecutive from beginning of the file)
     *
     * @param len amount of valid data
     */
    void SetValidData(uint64_t len);
    
    /**
     * Set cache file's valid data (within a range)
     *
     * @param start start of the valid data's range
     * @param len   length of the valid data's range
     */
    void SetValidData(uint64_t start, int64_t len);

    /**
     * Get the name of the container file
     *
     * @return name of the container file
     */
    char *Name()         { return(name); }
    
    /**
     * Get the length of the cache file
     *
     * @return length of the cache file in bytes
     */
    uint64_t Length()        { return(length); }
    
    /**
     * Get the amount of valid data of the cache file
     *
     * @return amount of valid data of the cache file in bytes
     */
    uint64_t ValidData() { return(validdata); }
    
    /**
     * Get the amount of consecutive valid data starting from beginning of the 
     * file
     *
     * @return amount of valid data of the cache file in bytes
     */
    uint64_t ConsecutiveValidData();
    
    /**
     * Check if file is partially cached
     *
     * @return zero if file is fully cached or 1 otherwise
     */
    int IsPartial() { return(length != validdata); }

    /**
     * Print the metadata to the standard output
     */
    void print() { print (stdout); }
    
    /**
     * Print the metadata to the specified file
     *
     * @param fp FILE structure of the output file
     */
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    
    /**
     * Print the metadata to the specified file
     *
     * @param fdes file descriptor of the output file
     */
    void print(int fdes);
};

#endif	/* _VENUS_FSO_CACHEFILE_H_ */

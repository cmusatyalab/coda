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
 * logalloc.h
 * Specification of memory allocator for arrays of equal sized objects 
 * Used by the portable doubly linked list (pdlist.h)
 *
 */

#ifndef _RES_LOG_ALLOC_H_
#define _RES_LOG_ALLOC_H_ 1

/*
 * Storage is allocated from the chunk of memory starting at baseAddr.
 * Bitmap keeps track of allocated blocks.  1 is for a record that is allocated.
 * Everytime the Storage allocator runs out of storage, it calls GrowStorageArea
 * that grows the storage by STORAGEGROWSIZE records.
 *
 */

/* GROWSIZE and SIZE should be multiples of 8 */
#define STORAGEGROWSIZE	32
#define STORAGESIZE	32
#define MAXLOGSIZE	32
#define RESOPCOUNTARRAYSIZE	20

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vcrcommon.h>
#include <res.h>

typedef struct resOpEntry {
    RPC2_Integer AllocCount;
    RPC2_Integer DeallocCount;
} resOpEntry ;

class PMemMgr {
    friend void DumpVolResLog(PMemMgr *, int);
    friend void ReadVolResLog(PMemMgr **, int);
    friend long RS_LockAndFetch(RPC2_Handle, ViceFid*, ResFetchType, ViceVersionVector*,
				ResStatus*, RPC2_Integer *, RPC2_Integer,
				RPC2_Integer *, ResPathElem *);
    friend int GetIndexViaWrapAround(PMemMgr *, int);
    friend void ChooseWrapAroundVnode(PMemMgr *, int);
    friend long S_VolSetLogParms(RPC2_Handle, VolumeId, RPC2_Integer, RPC2_Integer);
    friend void GetResStatistics(PMemMgr *, int *, int *, int *);
    int	volindex;		/* index of volume */
    char *baseAddr;		/* address of the first record */
    int	classSize;		/* size of each record */
    int maxRecordsAllowed;	/* max number of records allowed */
    int maxEntries;		/* max number of records space in bitmap */
    int highWater;              /* records the high water mark */
    int	nEntries;		/* number of records used */
    int	nEntriesAllocated;		/* number of records allocated */
    int	nEntriesDeallocated;		/* number of records deallocated */
    unsigned char  *bitmap;	/* bitmap to indicate allocated records */
    int bitmapSize;		/* length of bitmap */
    long wrapvnode;		/* vnode number whose log is being eaten 
				   due to wraparound */
    long wrapunique;
    void GrowStorageArea();
    int GetFreeBitIndex();	/* gets a free slot */
    void FreeBitIndex(int);	/* frees up a bit entry */
    void SetBitIndex(int);	/* set a bit entry */
  public:
    PMemMgr(int, int, int, int =MAXLOGSIZE);/* gives initial size of storage area */
    PMemMgr(int, int);
    ~PMemMgr();
    int NewMem();		/* returns index of an unused record */
    void FreeMem(char *);	/* marks a record as unused */
    int	AddrToIndex(char *);	/* given an address, returns its index in array */
    void *IndexToAddr(int);	/* given an index, returns the address of block */
    resOpEntry opCountArray[RESOPCOUNTARRAYSIZE];	/* array of number of allocated op */
};
#endif not _RES_LOG_ALLOC_H_

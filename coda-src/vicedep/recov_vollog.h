/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _RECOV_VOLLOG_H
#define _RECOV_VOLLOG_H 1
/*
 * recov_vollog.h
 *	Created 2/12/92 -- Puneet  Kumar
 * Declaration of the volume log structure in RVM
 */

#include <bitmap.h>

#define LOGRECORD_BLOCKSIZE	32	/* # log records in each block allocated */
#define VOLLOG_GROWSIZE		32	/* # log records to grow by */
#define SEQNO_GROWSIZE		200	/* increase seq no on log records in rvm every 200 spools */
#define VERSION_NUMBER		1

#define MAXWRAPTRIES		32	// no of tries to wrap around

class recov_vol_log;
class recle;
class rec_dlist;
class resstats;
#include "res.h"
#include <cvnode.h>
#include <volume.h>

typedef struct VolumeDiskData VolumeDiskData;

class recov_vol_log {
    friend long RS_LockAndFetch(RPC2_Handle, ViceFid *, ResFetchType, 
				ViceVersionVector *, ResStatus *, 
				RPC2_Integer *);
    friend void DumpLog(rec_dlist *, struct Volume *, char **, int *, int *);
    friend int DumpVolDiskData(int, VolumeDiskData *);
    
    // recoverable part 
    unsigned Version:8;		// version information for resolution system 
    unsigned malloced:8;
    int admin_limit;		// absolute limit on # of log entries changed by volutil
    int size;			// <= admin_limit; number of entries in volume log 
    recle **index;		// array of ptrs to log record blocks :
                                // size = admin_limit/LOGRECORD_BLOCKSIZE 
    int rec_max_seqno;
    bitmap recov_inuse;		// bitmap in rvm to indicate if an index in the log is being used 
    VnodeId	wrapvn;		// vnode number of wrap around vnode
    Unique_t	wrapun;		// uniquifier of wrap around vnode
    int		lastwrapindex;	// index of last log entry that was wrapped over

    // transient part - only in VM 
    int nused;			// entries being used currently 
    bitmap *vm_inuse;		// bitmap as above but only in VM 
    int max_seqno;

    // private routines 
    int Grow(int =-1);
    void FreeBlock(int);
    void Increase_rec_max_seqno(int i =SEQNO_GROWSIZE);
    void *IndexToAddr(int);	// given an index, returns the address of block 
    void PrintUnreachableRecords(bitmap *);
    int ChooseWrapAroundVnode(Volume *, int different =0);

  public:
    resstats *vmrstats;		// res statistics (only in VM)
    int reserved[10];		// for future use

    void *operator new(size_t);
    recov_vol_log(VolumeId =0, int adm =2048);	// default: max 2k log entries 
    ~recov_vol_log();
    void operator delete(void *, size_t);
    int init(int);
    void ResetTransients(VolumeId =0);
    
    void Increase_Admin_Limit(int);

    int AllocRecord(int *index, int *seqno); 		// in vm only 
    void DeallocRecord(int index); 			// in vm only
    int AllocViaWrapAround(int *, int *, Volume *, dlist * =NULL);	// reuse record
    recle *RecovPutRecord(int index);			// in rvm 
    void RecovFreeRecord(int index);			// in rvm 
    int bmsize();
    int LogSize();

    void purge();					// purge all the logs 
    void SalvageLog(bitmap *);
    void print();
    void print(FILE *);
    void print(int);
};

/* export definitions */
extern void CreateRootLog(Volume *, Vnode *);
extern void CreateResLog(Volume *, Vnode *);

// subresphase3.c
#endif _RECOV_VOLLOG_H    


    

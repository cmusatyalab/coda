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
 * recov_logalloc.c
 *	Created 2/13/92 -- Puneet Kumar
 *
 *	Definitions for routines to manage  a volume log in recoverable storage 
 */

#ifdef __cplusplus
extern "C" {
#endif
    
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <rpc2/rpc2.h>
#include "coda_string.h"

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <olist.h>
#include <vmindex.h>
#include <vice.h>
#include <rvmlib.h>
#include <bitmap.h>
#include <srv.h>
#include <volume.h>
#include <vlist.h>
#include <recov_vollog.h>
#include <resutil.h>
#include <lockqueue.h>
#include "recle.h"
#include "ops.h"
#include "resstats.h"

// must be called from within a transaction 
void *recov_vol_log::operator new(size_t len) 
{
    recov_vol_log *rcvl;

    rcvl = (recov_vol_log *)rvmlib_rec_malloc((int) len);
    CODA_ASSERT(rcvl);
    return(rcvl);
}

/* this mostly initializes an array of pointers to log entries */
recov_vol_log::recov_vol_log(VolumeId vid, int adm) :recov_inuse(adm, 1) 
{

    RVMLIB_REC_OBJECT(*this);
    Version = VERSION_NUMBER;
    malloced = 1; /* used to be dynamically set; but volume logs are only
		     ever allocated via new(); hence force this to one;
		     getting rid of it would mean a reinit (Satya, 5/22/95) */
    /* admin_limit must be a multiple of sizeof(long) */
    while (adm & (VOLLOG_GROWSIZE - 1)) 
	     adm++;
    admin_limit = adm;
    size = 0;
    
    if (admin_limit) {
	int index_size = admin_limit / LOGRECORD_BLOCKSIZE;
	index = (recle **)rvmlib_rec_malloc(index_size * sizeof(void *));
	CODA_ASSERT(index);
	rvmlib_set_range(index, index_size * sizeof(void *));
	memset((void *)index, 0, index_size * sizeof(void *));
    }
    else 
	index = NULL;
    
    rec_max_seqno = 0;
    wrapvn = (unsigned int) -1;
    wrapun = (unsigned int) -1;
    lastwrapindex = -1;
    ResetTransients(vid);
    rec_max_seqno = SEQNO_GROWSIZE;
}


void recov_vol_log::ResetTransients(VolumeId vid) 
{
    /* allocate bitmap first */
    vm_inuse = new bitmap(admin_limit, 0);
    CODA_ASSERT(vm_inuse);
    
    *vm_inuse = recov_inuse;
    nused = vm_inuse->Count();
    max_seqno = rec_max_seqno;
    vmrstats = new resstats(vid, LogSize());
}

recov_vol_log::~recov_vol_log() 
{
    if (index) rvmlib_rec_free(index); 
}

void recov_vol_log::operator delete(void *deadobj, size_t len) 
{
    rvmlib_rec_free(deadobj);
}



/* Private routines */


//increase number of log entries by VOLLOG_GROWSIZE (still < admin_limit) 
// called from within a transaction
int recov_vol_log::Grow(int offset) 
{
    if (size + VOLLOG_GROWSIZE > admin_limit) 
	    return(-1);
    
    int pos;		// where new block will be inserted
    if (offset == -1) 	// default position of new block is at the end 
	pos = size / LOGRECORD_BLOCKSIZE;
    else 
	pos = offset / LOGRECORD_BLOCKSIZE;
    
    CODA_ASSERT(index[pos] == NULL);
    
    rvmlib_set_range(&index[pos], sizeof(void *)); 
    
    index[pos] = (recle *)rvmlib_rec_malloc(LOGRECORD_BLOCKSIZE * sizeof(recle)); 
    CODA_ASSERT(index[pos]);
    
    recle *l = index[pos];
    rvmlib_set_range(index[pos], LOGRECORD_BLOCKSIZE * sizeof(recle));
    memset((void *)l, 0, LOGRECORD_BLOCKSIZE * sizeof(recle));
    
    
    rvmlib_set_range(&size, sizeof(int));
    size += LOGRECORD_BLOCKSIZE;
    
    return(0);
}

// called from within a transaction
void recov_vol_log::FreeBlock(int i) {
    CODA_ASSERT(index[i]);
    rvmlib_rec_free(index[i]);
    
    rvmlib_set_range(&index[i], sizeof(void *));
    index[i] = NULL;
    
    rvmlib_set_range(&size, sizeof(int));
    size -= LOGRECORD_BLOCKSIZE;
}
/* outside or within a transaction */
void recov_vol_log::Increase_rec_max_seqno(int i) 
{
    rvm_return_t status;
    
    if (!rvmlib_in_transaction()) {
	rvmlib_begin_transaction(restore);
	rvmlib_set_range(&rec_max_seqno, sizeof(int));
	rec_max_seqno += i;
	rvmlib_end_transaction(flush, &status);
	CODA_ASSERT(status == RVM_SUCCESS);
    } else { 
	rvmlib_set_range(&rec_max_seqno, sizeof(int));
	rec_max_seqno += i;
    }
}

/* return the address of a record */
void *recov_vol_log::IndexToAddr(int i) {
    int blocknumber = i / LOGRECORD_BLOCKSIZE;
    int slotnumber =  i % LOGRECORD_BLOCKSIZE;
    if (!index[blocknumber]) return(NULL);
    return(&(index[blocknumber][slotnumber]));
}

void recov_vol_log::PrintUnreachableRecords(bitmap *shadowbm) {
    for (int i = 0; i < shadowbm->Size(); i++) {
	int shadowvalue = shadowbm->Value(i);
	int recovvalue = recov_inuse.Value(i);
	if (recovvalue != shadowvalue) {
	    if (shadowvalue) 
		SLog(0, "Log rec at index %d is allocated in vm"
		     "but not in RVM .... BAD BAD\n", i);
	    else {
		SLog(0, "Log rec at index %d is unreachable\n",i);
		recle *r = (recle *)IndexToAddr(i);
		CODA_ASSERT(r);
		r->print(stdout);
	    }
	}
    }
}

int recov_vol_log::LogSize() {
    int lsize = 0;
    for (int i = 0; i < admin_limit; i++) {
	if (recov_inuse.Value(i)) {
	    recle *r = (recle *)IndexToAddr(i);
	    lsize += sizeof(recle) + r->size;
	}
    }
    return(lsize);
}

/* Public Routines */

void recov_vol_log::Increase_Admin_Limit(int newsize) 
{
    if (newsize <= admin_limit) 
	    return;
    while (newsize & (LOGRECORD_BLOCKSIZE - 1)) 
	    newsize++;
    
    int new_index_size = newsize / LOGRECORD_BLOCKSIZE;
    recle **new_index = (recle **)rvmlib_rec_malloc(new_index_size * sizeof(void *));
    CODA_ASSERT(new_index);
    rvmlib_set_range(new_index, new_index_size * sizeof(void *));
    memset(new_index, 0, new_index_size * sizeof(void *));
    memcpy(new_index, index, sizeof(void *) * (admin_limit / LOGRECORD_BLOCKSIZE));
    
    if (index) rvmlib_rec_free(index);
    rvmlib_set_range(&index, sizeof(recle **));
    index = new_index;
    
    rvmlib_set_range(&admin_limit, sizeof(int));
    admin_limit = newsize;
    
    /* change the bitmaps */
    recov_inuse.Grow(newsize);
    vm_inuse->Grow(newsize);

    vmrstats->lstats.nadmgrows++;
}

// not always called within a transaction 
int recov_vol_log::AllocRecord(int *index, int *seqno) 
{
    *seqno = -1;
    *index = vm_inuse->GetFreeIndex();
    if (*index == -1) { //no space available 
	SLog(0, "AllocRecord: No space left in volume log.\n");
	return(ENOSPC);
    }
    
    nused++;
    if (max_seqno == rec_max_seqno) 
	Increase_rec_max_seqno();	/* transaction executed */
    
    *seqno = ++max_seqno;
    SLog(10, "AllocRecord: returning index %d seqno %d\n", 
	 *index, *seqno);
    return(0);
}

// not called within a transaction 
void recov_vol_log::DeallocRecord(int index) 
{
	SLog(10, "Entering recov_vol_log::DeallocRecord(%d)\n", index);
    if (recov_inuse.Value(index)) {	// the rvm record better not be allocated
	SLog(0,"recov_vol_log::DeallocRecord(%d): recov bitmap says record allocated\n",
	     index);
	CODA_ASSERT(0);
    }
    if (!vm_inuse->Value(index)) {
	SLog(10, "recov_vol_log::DeallocRecord(%d) is already deallocated\n", 
	     index);
    } else
	vm_inuse->FreeIndex(index);
    nused--;	
}

/* called from within a transaction */
recle *recov_vol_log::RecovPutRecord(int index) {
    CODA_ASSERT(vm_inuse->Value(index));
    CODA_ASSERT(!recov_inuse.Value(index));
    recov_inuse.SetIndex(index);
    
    /* return pointer to log record */
    recle *l = (recle *)IndexToAddr(index);
    if (!l) {
	SLog(10, "RecovPutRecord: Growing Log\n");
	Grow(index);
	l = (recle *)IndexToAddr(index);
    }
    CODA_ASSERT(l);
    return(l);
}

// called from within a transaction 
void recov_vol_log::RecovFreeRecord(int index) {
    CODA_ASSERT(vm_inuse->Value(index));
    CODA_ASSERT(recov_inuse.Value(index));
    recov_inuse.FreeIndex(index);
    // should only be done after transaction commits
    // vm_inuse->FreeIndex(index);
}

// called from within a transaction 
void recov_vol_log::purge() {
    RVMLIB_REC_OBJECT(*this);
    int index_size = admin_limit / LOGRECORD_BLOCKSIZE;
    /* the variable length part of each log record should have already been purged */
    for (int i = 0; i < index_size; i++) 
	if (index[i]) 
	    rvmlib_rec_free(index[i]);
    rvmlib_rec_free(index);
    index = NULL;
    size = 0;
    rec_max_seqno = 0; 
    
    /* delete bitmaps */
    recov_inuse.purge();
    if (vm_inuse) {
	vm_inuse->purge();
	delete vm_inuse;
	vm_inuse = NULL;
    }
    nused = 0;
    vm_inuse = NULL;
    max_seqno = 0;
}

// called from within a transaction 
void recov_vol_log::SalvageLog(bitmap *shadowbm) {
    // check that shadow bitmap is same as recovered bitmap
    if (recov_inuse != *shadowbm) {
	SLog(0,
	       "recov_vol_log::SalvageLog: bitmaps are not equal\n");
	PrintUnreachableRecords(shadowbm);
	return;
    }
    // check that each entry that is free doesn't have a recoverable part
    int s = 0;
    int index_size = admin_limit / LOGRECORD_BLOCKSIZE;
    for (int i = 0; i < index_size; i++) {
	if (!index[i]) continue;
	recle *r = index[i];
	int recsusedinblock = 0;
	for (int j = 0; j < LOGRECORD_BLOCKSIZE; j++) {
	    int bmindex = (i * LOGRECORD_BLOCKSIZE) + j;
	    
	    if (recov_inuse.Value(bmindex)) {
		// allocated entry 
		recsusedinblock++;
		if (r[j].size) CODA_ASSERT(r[j].vle);
	    }
	    else 
		// free entry 
		CODA_ASSERT(r[j].vle == NULL);
	}
	if (!recsusedinblock) {
	    // free up block
	    SLog(0, 
		   "recov_vol_log::SalvageLog: Block %d could be freed\n",
		   i);
	    FreeBlock(i);
	}
	else
	    s += LOGRECORD_BLOCKSIZE;
	
    }
    CODA_ASSERT(size == s);
}

int recov_vol_log::bmsize() {
    return(recov_inuse.Size());
}
void recov_vol_log::print() {
    print(stdout);
}

void recov_vol_log::print(FILE *fp) {
    print(fileno(fp));
}

void recov_vol_log::print(int fd) {
    char buf[512];
    sprintf(buf, 
	    "Recoverable volume log \nversion: %u %s\nadm_limit %d size %d used %d\nrec_max_seqno %d current_seq_no %d\n",
	    Version, malloced ? "malloced" : "on stack",
	    admin_limit, size, nused, rec_max_seqno, max_seqno);
    write(fd, buf, strlen(buf));
    sprintf(buf, "index contents\n");
    write(fd, buf, strlen(buf));
    for (int i = 0; i < (admin_limit/LOGRECORD_BLOCKSIZE); i++) {
	sprintf(buf, "0x%p ", index[i]);
	write(fd, buf, strlen(buf));
    }
    sprintf(buf, " \n");
    write(fd, buf, strlen(buf));
    sprintf(buf, "recoverable ");
    write(fd, buf, strlen(buf));
    recov_inuse.print(fd);
    if (vm_inuse) {
	sprintf(buf, "VM ");
	write(fd, buf, strlen(buf));
	vm_inuse->print(fd);
    }
}


// different : choose a different vnode from the one already in use 
int 
recov_vol_log::ChooseWrapAroundVnode(Volume *vol, int different) 
{

    if ((!different) && ((long)wrapvn != -1) && ((long)wrapun != -1)) {
	SLog(0,
	       "ChooseWrapAroundVnode: returning same vnode 0x%x.%x in vol 0x%x\n", wrapvn, wrapun, V_id(vol));
	return(0);
    }

    int index_size = admin_limit/LOGRECORD_BLOCKSIZE;
    int blocknum = 0;
    if (lastwrapindex != -1) {
	blocknum = (lastwrapindex / LOGRECORD_BLOCKSIZE) + 1;
	if (blocknum >= index_size) blocknum = 0;
    }
    for (int i = blocknum; i < index_size; i++) {
	if (!(index[i])) {
	    // index[i] being zero --> that part of the volume log isn\'t allocated
	    // in RVM.  We are wrapping around because we have run out of space in
	    // the VM index that keeps track of the allocated records.  In other words
	    // if we have a big transaction (like a huge reintegrate) we might 
	    // run out of space in the volume log (in the VM index where we reserve
	    // space) even though the RVM part of it still has holes 
	    continue;
	}
	recle *r = index[i];

	// if different vnode is required, try finding the old one first
	// then start looking from there
	int startindex = 0;
	if (different) {
	    for (int j = 0; j < LOGRECORD_BLOCKSIZE; j++) {
		if ((r[j].dvnode == wrapvn) && 
		    (r[j].dunique == wrapun)) {
		    SLog(9,
			   "ChooseWrapAroundVnode: Starting at index i = %d j = %d\n",
			   i, j + 1);
		    startindex = j + 1;
		    break;
		}
	    }
	}
	for (int j = startindex; j < LOGRECORD_BLOCKSIZE; j++) {
	    if ((r[j].dvnode != 1) &&
		(r[j].dunique != 1)) {	// do not use the volume root for wraparound
		if (different) {
		    if ((wrapvn == r[j].dvnode) &&
			(wrapun == r[j].dunique)) {
			SLog(0,
			       "ChooseWrapAroundVnode:Skipping over %x.%x \n",
			       wrapvn, wrapun);
			continue;
		    }
		}

		rvm_return_t status;
		CODA_ASSERT(!rvmlib_in_transaction());
		rvmlib_begin_transaction(restore);
		RVMLIB_REC_OBJECT(wrapvn);
		wrapvn = r[j].dvnode;
		RVMLIB_REC_OBJECT(wrapun);
		wrapun = r[j].dunique;
		RVMLIB_REC_OBJECT(lastwrapindex);
		lastwrapindex = (i * LOGRECORD_BLOCKSIZE) + j;
		rvmlib_end_transaction(flush, &status);
		CODA_ASSERT(status == RVM_SUCCESS);
		return(0);
	    }
	}
    }
    SLog(0,
	   "ChooseWrapAroundVnode: No vnodes to choose from - returns ENOSPC\n");
    return(ENOSPC);
}

// try to consume the log of a vnode chosen by above routine
// constraints are that each log should have atleast one log entry
int recov_vol_log::AllocViaWrapAround(int *index, int *seqno, 
				      Volume *volptr, dlist *vlist)
{
    int errorcode = 0;
    ViceFid fid;
    Vnode *vptr = 0;
    *index  = -1;
    *seqno = -1;
    vmindex ind;
    int different = 0;
    rvm_return_t status = RVM_SUCCESS;
    int i;

    VnodeId prevwrapvn = wrapvn;
    Unique_t prevwrapun = wrapun;

    for (i = 0; i < 32 ; i++) {
	// chose an object whose log must be wrapped around
	if (ChooseWrapAroundVnode(volptr, different)) {
	    SLog(0, "AllocViaWrapAround: No vnodes whose logs can be reused\n");
	    break;
	}
	different = 0;
	// try and use log of vnode and uniquifier
	// provided the vnode has more than one entry in the log
	FormFid(fid, V_id(volptr), wrapvn, wrapun);

	// if vnode is already being modified then its log shouldn't be used
	{
	    if (vlist) {
		vle *v = FindVLE(*vlist, &fid);
		if (v) {
		    SLog(0,
			   "AllocViaWrapAround: Obj 0x%x.%x is being mod - try again\n",
			   wrapvn, wrapun);
		    different = 1;
		    continue;
		}
	    }
	}
		    
	if ((errorcode = GetFsObj(&fid, &volptr, &vptr, WRITE_LOCK, NO_LOCK, 1, 1, 0))) {
	    SLog(0,
		   "AllocViaWrapAround: Couldnt get object 0x%x.%x\n",
		   wrapvn, wrapun);
	    different = 1;
	    continue;
	}
	
	CODA_ASSERT(vptr);
	CODA_ASSERT(VnLog(vptr));
	if (VnLog(vptr)->count() <= 1)  {
	    SLog(0,
		   "AllocViaWrapAround: 0x%x.%x has only single vnode on list\n",
		   wrapvn, wrapun);
	    rvmlib_begin_transaction(restore);
	    Error fileCode = 0;
	    VPutVnode(&fileCode, vptr);
	    vptr = 0;
	    CODA_ASSERT(fileCode == 0);
	    rvmlib_end_transaction(flush, &status);
	    CODA_ASSERT(status == RVM_SUCCESS);

	    different = 1;
	    continue;
	}
	
	// reclaim the first log entry from this vnode
	{
	    // remove this entry from the list
	    // check the entry has no ptrs for other log records embedded
	    SLog(0,
		   "AllocViaWrapAround: Reclaiming first log rec of 0x%x.%x\n\n",
		   wrapvn, wrapun);
	    CODA_ASSERT(!rvmlib_in_transaction());
	    rvmlib_begin_transaction(restore);
	    recle *le = (recle *)VnLog(vptr)->get();
	    rec_dlist *childlog;
	    if ((childlog = le->HasList()) )
		PurgeLog(childlog, volptr, &ind);

	    // RESSTATS
	    {
		if ((wrapvn != prevwrapvn) || 
		    (wrapun != prevwrapun)) {
		    prevwrapvn = wrapvn;
		    prevwrapun = wrapun;
		    vmrstats->lstats.nwraps++;
		}
		VarlHisto(*(V_VolLog(volptr)->vmrstats)).countdealloc(le->size);
		Lsize(*(V_VolLog(volptr)->vmrstats)).chgsize(-(le->size + sizeof(recle)));
	    }

	    le->FreeVarl();
	    RecovFreeRecord(le->index);
	    *index = le->index;
	    Error fileCode = 0;
	    VPutVnode(&fileCode, vptr);
	    vptr = 0;
	    CODA_ASSERT(fileCode == 0);
	    rvmlib_end_transaction(flush, &status);
	    if (status != RVM_SUCCESS) return(ENOSPC);
	    FreeVMIndices(volptr, &ind);
	    break;
	}
    }

    if (*index == -1) {
	SLog(0, "AllocViaWrapAround: Gave up at %d iterations\n", i);
	return(ENOSPC);
    }

    // some index was allocated
    CODA_ASSERT(vm_inuse->Value(*index) == 0);
    vm_inuse->SetIndex(*index);
    
    if (max_seqno == rec_max_seqno) 
	Increase_rec_max_seqno();	/* transaction executed */
    
    *seqno = ++max_seqno;
    return(0);
}
    





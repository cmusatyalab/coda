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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/res/reslog.cc,v 4.3 1998/08/31 12:23:21 braam Exp $";
#endif /*_BLURB_*/







/*
 * reslog.c 
 * Created Sep 1990, Puneet Kumar
 * Implementation of log storage for resolution in VM 
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "coda_assert.h"
#include <stdio.h>
#include <struct.h>
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc2.h>
#include <util.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include <errors.h>
#include <srv.h>
#include <volume.h>
#include <cvnode.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <nettohost.h>
#include <volres.h>

#include "rescomm.h"
#include "res.h"
#include "pdlist.h"
#include "reslog.h"
#include "remotelog.h"
#include "resutil.h"
#include "logalloc.h"

/* LogStore manages the storage for the volume logs */
/* VolLogPtrs[i] stores the per vnode log list header */
PMemMgr *LogStore[MAXVOLS];
olist *VolLogPtrs[MAXVOLS];	/* olist *, because it is an array of olists*/

/* initialize the storage allocator for the resolution log */
void InitLogStorage() {
    for (int i = 0; i < MAXVOLS; i++) {
	/* initialize to empty storage */
	if (SRV_RVM(VolumeList[i]).data.volumeInfo)
	    if (SRV_RVM(VolumeList[i]).data.volumeInfo->maxlogentries)
		LogStore[i] = new PMemMgr((int)sizeof(rlent), 0, i, 
					  SRV_RVM(VolumeList[i]).data.volumeInfo->maxlogentries);
	    else
		LogStore[i] = new PMemMgr((int)sizeof(rlent), 0, i, MAXLOGSIZE);
	else
	    LogStore[i] = NULL;
/* bug fix: comment out original logic */
#if 0
	if ((SRV_RVM(VolumeList[i]).data.volumeInfo) && 
	    (SRV_RVM(VolumeList[i]).data.volumeInfo->maxlogentries))
	    LogStore[i] = new PMemMgr(sizeof(rlent), 0, i, 
		      SRV_RVM(VolumeList[i]).data.volumeInfo->maxlogentries);
	else
	    LogStore[i] = new PMemMgr(sizeof(rlent), 0, i, MAXLOGSIZE);
#endif 0

	VolLogPtrs[i] = NULL;
    }
}

/* should be called from within a transaction */
/* initialize the vm log ptrs for resolution */
void InitVolLog(int index) {
    if (!AllowResolution) return;

    if (VolLogPtrs[index] != NULL) return;

    int nlists = (int) SRV_RVM(VolumeList[index]).data.nlargeLists;
    /* set up template of olists for housing per vnode info */
    VolLogPtrs[index] = new olist[nlists];
    rec_smolist *vlist;
    olist *vl = VolLogPtrs[index];
    
    /* each iteration corresponds to one vnode number */
    for (int i = 0; i < nlists; i++) {
	vlist = &(SRV_RVM(VolumeList[index]).data.largeVnodeLists[i]);
	rec_smolist_iterator	next(*vlist);
	rec_smolink *p;
	while (p = next()){
	    /* every iteration for a  unique vnode with same vnode number */
	    VnodeDiskObject *vdo; 
	    vdo = strbase(VnodeDiskObject, p, nextvn);
	    VNResLog *vnlogp = new VNResLog(bitNumberToVnodeNumber(i, vLarge), 
					    vdo->uniquifier, index);
	    vl[i].append(vnlogp);
	}
    }
}

void MakeLogNonEmpty(Vnode *vptr) {
    pdlist *loglist;
    VNResLog *vnlog;
    
    if ((loglist = GetResLogList(vptr->disk.vol_index, vptr->vnodeNumber,
				 vptr->disk.uniquifier, &vnlog)) == NULL) {
	LogMsg(0, SrvDebugLevel, stdout,  "MakeLogNonEmpty: Creating new list for %x.%x",
		vptr->vnodeNumber, vptr->disk.uniquifier);
	loglist = AllocateVMResLogList(vptr->disk.vol_index, 
				       vptr->vnodeNumber, 
				       vptr->disk.uniquifier);
	CODA_ASSERT(loglist);
	
    }
    
    if (loglist->count() == 0) {
	LogMsg(9, SrvDebugLevel, stdout,  "MakeLogNonEmpty: Creating dummy record for %x.%x", 
		vptr->vnodeNumber, vptr->disk.uniquifier);
	CreateAfterCrashLogRecord(vptr, loglist);
    }
}

/* allocates the list header for the res log in a vnode in vm */
pdlist *AllocateVMResLogList(int volindex, 
			     VnodeId vnode, Unique_t u) {
    LogMsg(9, SrvDebugLevel, stdout,  "Entering AllocateVMResLogList(%x, %x, %x)", 
	    volindex, vnode, u);
    olist *l = VolLogPtrs[volindex];
    int vnodeindex = (int) vnodeIdToBitNumber(vnode);
    olist *mylist = &(l[vnodeindex]);
    VNResLog *vn;
    /* check if res log header already exists */
    olist_iterator next(*mylist);
    while (vn = (VNResLog *)next()) 
	if (vn->uniquifier == u && vn->vnode == vnode) {
	    LogMsg(9, SrvDebugLevel, stdout,  "AllocateVMResLog(%d, %x, %x): Already allocated",
		volindex, vnodeindex, u);
	    return(vn->loglist);
	}
    /* couldnt find log header - allocate and insert */
    LogMsg(9, SrvDebugLevel, stdout,  "Allocating new Res Log for (%x, %x, %x)",
	    volindex, vnode, u);
    vn = new VNResLog(vnode, u, volindex);
    mylist->insert(vn);
    return(vn->loglist);
}

int AllocateResLog(int volindex, VnodeId vnode, Unique_t u) {

    InitVolLog(volindex);
    pdlist *loglist = AllocateVMResLogList(volindex, vnode, u);
    if (loglist) 
	return(1);
    else
	return(0);
}



void DeAllocateVMResLogListHeader(int volindex, VnodeId vnode, Unique_t u) {
    /* delete the resolution log list header */
    VNResLog *vnlog;
    olist *l = VolLogPtrs[volindex];
    long vnodeindex = vnodeIdToBitNumber(vnode);
    olist *myvnlist = &(l[vnodeindex]);
    olist_iterator next(*myvnlist);
    while (vnlog = (VNResLog *)next()) 
	if (vnlog->uniquifier == u) {
	    LogMsg(19, SrvDebugLevel, stdout,  "DeAllocateVMResLogListHeader: Deleting log header for %x.%x",
		    vnode, u);
	    myvnlist->remove(vnlog);
	    delete vnlog;
	    vnlog = 0;
	    break;
	}
    /* Make sure that the iterator is not used after the delete */
}

/* given a vnodenumber, return the list header for resolution log */
/* The list may be empty.  NULL implies there is no list */
pdlist *GetResLogList(int volindex, VnodeId vnode, Unique_t u, VNResLog **rlog) {
    int offset = (int)vnodeIdToBitNumber(vnode);
    return(iGetResLogList(volindex, offset, u, rlog));
}

pdlist *iGetResLogList(int volindex, int vnodeindex, Unique_t u, VNResLog **rlog) {
    LogMsg(9, SrvDebugLevel, stdout,  "Entering iGetResLogList(%x, %x, %x, %x)",
	    volindex, vnodeindex, u, rlog);
    *rlog = NULL;
    olist *l = VolLogPtrs[volindex];
    LogMsg(69, SrvDebugLevel, stdout,  "iGetResLogList: At volume index %x, list is %x", 
	    volindex, l);
    LogMsg(69, SrvDebugLevel, stdout,  "iGetResLogList: Looking at vnode index %x", vnodeindex);
    olist *myvnlist = &(l[vnodeindex]);
    VNResLog *vnlog;
    olist_iterator	next(*myvnlist);
    while (vnlog = (VNResLog *)next())
	if (vnlog->uniquifier == u){
	    *rlog = vnlog;
	    LogMsg(9, SrvDebugLevel, stdout,  "iGetResLogList: Found loglist for %x.%x", 
		    vnodeindex, u);
	    return(vnlog->loglist);
	}
    LogMsg(69, SrvDebugLevel, stdout,  "iGetResLogList: Empty Log for %x.%x - returning NULL",
	    vnodeindex, u);
    return(NULL);
}

/* Creates a new after crash entry for a directory log */
void CreateAfterCrashLogRecord(Vnode *vptr, pdlist *list) {
    if (list->count() != 0) return;

    ViceFid Fid;
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;
    Fid.Volume = 0;
    int ind;
    ind  = InitVMLogRecord(vptr->disk.vol_index, &Fid, 
			   &Vnode_vv(vptr).StoreId, 
			   ResolveAfterCrash_OP, 0/* dummy arg */);
    CODA_ASSERT(ind != -1);
    rlent *rl = (rlent *)( LogStore[vptr->disk.vol_index]->IndexToAddr(ind));
    list->append(&(rl->link));
}

/* Truncate the resolution log for a vnode - keep the last entry */
void TruncResLog(int volindex, VnodeId vnode, Unique_t u) {
    int offset = (int) vnodeIdToBitNumber(vnode);
    iTruncResLog(volindex, offset, u);
}
void iTruncResLog(int volindex, int vnodeindex, Unique_t u) {
    pdlink *l;
    VNResLog *vmlog;

    /* Get the Log for vnode */
    pdlist *pdl = iGetResLogList(volindex, vnodeindex, u, &vmlog);
    if (pdl == NULL) return;
    
    /* Get rid of each log record */
    while ((pdl->count() > 1) &&
	   (l = pdl->get())) {
	rlent *rle = strbase(rlent, l, link);
	if ((rle->opcode == ViceRemoveDir_OP)||
	    (rle->opcode == ResolveViceRemoveDir_OP))
	    PurgeLog(volindex, rle->u.u_removedir.head, rle->u.u_removedir.count,
		     fldoff(rlent, link));
	if (((rle->opcode == ViceRename_OP) ||
	     (rle->opcode == ResolveViceRename_OP)) &&
	    rle->u.u_rename.rename_tgt.tgtexisted) {
	    ViceFid tfid;
	    tfid.Volume = 0;
	    tfid.Vnode = rle->u.u_rename.rename_tgt.TgtVnode;
	    tfid.Unique = rle->u.u_rename.rename_tgt.TgtUnique;
	    if (ISDIR(tfid)) {
		PurgeLog(volindex, 
			 rle->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head,
			 rle->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count,
			 fldoff(rlent, link));
	    }
	}
	++(LogStore[volindex]->opCountArray[GETOPINDEX(rle->opcode)].DeallocCount);
	LogStore[volindex]->FreeMem((char *)rle);
    }

    /* Update the Last Common Point */
    {
	l = pdl->first();
	if (l != NULL) {
	    rlent *rle  = strbase(rlent, l, link);
	    vmlog->LCP = rle->storeid;
	}
    }
}	

/*
 * Truncate the resolution log - keep no entries.
 * delete logs for all subtrees that have been removed 
 */
void PurgeLog(int volindex, int head, int count, int offset) {

    pdlist	plist(offset, LogStore[volindex], count, head);
    pdlink	*l;
    while (l = plist.get()) {
	rlent *rle = strbase(rlent, l, link);
	if ((rle->opcode == ViceRemoveDir_OP) ||
	    (rle->opcode == ResolveViceRemoveDir_OP))
	    PurgeLog(volindex, rle->u.u_removedir.head,
		     rle->u.u_removedir.count, fldoff(rlent, link));
	if (((rle->opcode == ViceRename_OP) ||
	     (rle->opcode == ResolveViceRename_OP)) &&
	    rle->u.u_rename.rename_tgt.tgtexisted) {
	    ViceFid tfid;
	    tfid.Volume = 0;
	    tfid.Vnode = rle->u.u_rename.rename_tgt.TgtVnode;
	    tfid.Unique = rle->u.u_rename.rename_tgt.TgtUnique;
	    if (ISDIR(tfid)) 
		PurgeLog(volindex, 
			 rle->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head,
			 rle->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count,
			 fldoff(rlent, link));
	}
	++(LogStore[volindex]->opCountArray[GETOPINDEX(rle->opcode)].DeallocCount);
	LogStore[volindex]->FreeMem((char *)rle);
    }
}
void PurgeLog(int volindex, VnodeId vnode, Unique_t unq) {
    pdlist	*pdl;

    /* Get Log for vnode  */
    /* In the real server have to get list given index of logentry */
    {
	VNResLog *rlog;
	pdl = GetResLogList(volindex, vnode, unq, &rlog);
	if (pdl == NULL) return;
    }

    /* remove each entry recursively */
    {
	pdlink *l;
	while (l = pdl->get()) {
	    rlent *rle = strbase(rlent, l, link);
	    if ((rle->opcode == ViceRemoveDir_OP)||
		(rle->opcode == ResolveViceRemoveDir_OP))
		PurgeLog(volindex, rle->u.u_removedir.head, rle->u.u_removedir.count,
			 fldoff(rlent, link));
	    if (((rle->opcode == ViceRename_OP) ||
		 (rle->opcode == ResolveViceRename_OP)) &&
		rle->u.u_rename.rename_tgt.tgtexisted) {
		ViceFid tfid;
		tfid.Volume = 0;
		tfid.Vnode = rle->u.u_rename.rename_tgt.TgtVnode;
		tfid.Unique = rle->u.u_rename.rename_tgt.TgtUnique;
		if (ISDIR(tfid)) {
		    PurgeLog(volindex, 
			     rle->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head,
			     rle->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count,
			     fldoff(rlent, link));
		}
	    }
	    ++(LogStore[volindex]->opCountArray[GETOPINDEX(rle->opcode)].DeallocCount);
	    LogStore[volindex]->FreeMem((char *)rle);
	}
    }
}
/* try to find a slot which can be reused for a different directory */
/* constraints are that each log should have atleast one log entry; */
/* try to destroy the log of a directory that has already been wrapped around */
void ChooseWrapAroundVnode(PMemMgr *mmgr, int volindex) {
    VNResLog *rlog;
    pdlist *loglist;
    if (mmgr->wrapvnode != -1 && mmgr->wrapunique != -1) {
	loglist = GetResLogList(volindex, mmgr->wrapvnode, mmgr->wrapunique, &rlog);
	if (loglist && (loglist->count() > 1)) {	
	    LogMsg(0, SrvDebugLevel, stdout,  "ChooseWrapAroundVnode - continue wrapping on fid(%x.%x)",
		    mmgr->wrapvnode, mmgr->wrapunique);
 	    return; 
	}
    }

    /* find a vnodenumber from log */
    rlent *rl = (rlent *)mmgr->baseAddr;
    for (int i = 0; i < mmgr->nEntries; i++) {
	loglist = GetResLogList(volindex, rl[i].dvnode, rl[i].dunique, &rlog);
	if (loglist && (loglist->count() > 1)) {
	    mmgr->wrapvnode = rl[i].dvnode;
	    mmgr->wrapunique = rl[i].dunique;
	    LogMsg(0, SrvDebugLevel, stdout,  "ChooseWrapAround: Chose %x.%x for wraparound", 
		    mmgr->wrapvnode, mmgr->wrapunique);
	    return;
	}
	else {
	    if (!loglist)
		LogMsg(0, SrvDebugLevel, stdout,  "ChooseWrapAround: couldnt find res log for %x.%x",
			rl[i].dvnode, rl[i].dunique);
	    if (loglist && (loglist->count() == 1)) 
		LogMsg(0, SrvDebugLevel, stdout,  "ChooseWrapAround: log is of length 1 for %x.%x",
			rl[i].dvnode, rl[i].dunique);
	}
    }

    LogMsg(0, SrvDebugLevel, stdout,  "ChooseWrapAroundVnode: No Vnode available");
    mmgr->wrapvnode = -1;
    mmgr->wrapunique = -1;
    rl = (rlent *)mmgr->baseAddr;
  { /* drop scope for int i below; to avoid identifier clash */
    for (int i = 0; i < mmgr->nEntries; i++) 
	rl[i].print();
  } /* drop scope for int i above; to avoid identifier clash */

}

int GetIndexViaWrapAround(PMemMgr *mmgr, int volindex) {
    ChooseWrapAroundVnode(mmgr, volindex);
    if (mmgr->wrapvnode == -1 || mmgr->wrapunique == -1) {
	LogMsg(0, SrvDebugLevel, stdout,  "GetIndexViaWrapAround : Res Log is really full");
	rlent *rl = (rlent *)mmgr->baseAddr;
	for (int i = 0; i < mmgr->nEntries; i++) 
	    rl[i].print();
	return(-1);
    }
    VNResLog *rlog;
    pdlist *loglist;
    loglist = GetResLogList(volindex, mmgr->wrapvnode, mmgr->wrapunique, &rlog);
    if (!loglist || (loglist->count() <= 1)) {
	LogMsg(0, SrvDebugLevel, stdout,  "GetIndexViaWrapAround - Chooser for vnode screwed up !");
	return(-1);
    }

    /* free first record - 
       if other records are attached to it remove them too */
    {
	LogMsg(49, SrvDebugLevel, stdout,  "GetIndexViaWrapAround: before get count = %d",
		loglist->count());
	pdlink *l = loglist->get();
	LogMsg(19, SrvDebugLevel, stdout,  "GetIndexViaWrapAround: after get count = %d",
		loglist->count());	
	rlent *rl = strbase(rlent, l, link);
	if ((rl->opcode == ViceRemoveDir_OP) ||
	    (rl->opcode == ResolveViceRemoveDir_OP))
	    PurgeLog(volindex, rl->u.u_removedir.head, 
		     rl->u.u_removedir.count, fldoff(rlent, link));
	if (((rl->opcode == ViceRename_OP) ||
	     (rl->opcode == ResolveViceRename_OP)) &&
	    rl->u.u_rename.rename_tgt.tgtexisted) {
	    ViceFid tfid;
	    tfid.Volume = 0;
	    tfid.Vnode = rl->u.u_rename.rename_tgt.TgtVnode;
	    tfid.Unique = rl->u.u_rename.rename_tgt.TgtUnique;
	    if (ISDIR(tfid)) 
		PurgeLog(volindex, 
			 rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head,
			 rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count,
			 fldoff(rlent, link));
	}
	++(LogStore[volindex]->opCountArray[GETOPINDEX(rl->opcode)].DeallocCount);
	mmgr->FreeMem((char *)rl);
	return(mmgr->AddrToIndex((char *)rl));
    }
}

/* allocate a log record in VM */
int InitVMLogRecord(int volindex, ViceFid *fid, ViceStoreId *stid, int op ...) {
    /* allocated the record */
    int index;
    rlent *rl;
    index = LogStore[volindex]->NewMem();
    CODA_ASSERT(index != -1);
    rl = (rlent *)(LogStore[volindex]->IndexToAddr(index));
    LogMsg(49, SrvDebugLevel, stdout,  "InitVMLogRecord - from NewMem got address 0x%x", rl);
    CODA_ASSERT(rl != NULL);
    va_list ap;
    va_start(ap, op);
    
    /* initialize the record */
    if (rl->init(fid, stid, op, ap)) {
	LogMsg(0, SrvDebugLevel, stdout,  "InitVMLogRecord(%x.%x) couldnt initialize record",
		fid->Vnode, fid->Unique);
	LogStore[volindex]->FreeMem((char *)rl);
	return(-1);
    }
    ++(LogStore[volindex]->opCountArray[GETOPINDEX(op)].AllocCount);
    return(index);
}

/* Append log record to volume log in rvm */
int AppendRVMLogRecord(Vnode *vptr, int index) {
    rlent *logrec;

    VNResLog *rlog;
    int volindex = vptr->disk.vol_index;
    VnodeId vid = vptr->vnodeNumber;
    Unique_t u = vptr->disk.uniquifier;
    pdlist *p = GetResLogList(volindex, vid, u, &rlog);
    if (p == NULL)
	/* log nonexistent - allocate a new log header */
	p = AllocateVMResLogList(volindex, vid, u);
    CODA_ASSERT(p != NULL);
    LogMsg(49, SrvDebugLevel, stdout,  "AppendRVMLogRecord: Going to append log at count = %d", p->count());

    logrec = (rlent *)(LogStore[volindex]->IndexToAddr(index));
    LogMsg(49, SrvDebugLevel, stdout,  "AppendRVMLogRecord: Going to append record index %d address 0x%x",
	    index, logrec);
    p->append(&(logrec->link));
    LogMsg(49, SrvDebugLevel, stdout,  "AppendRVMLogRecord: After append log count = %d", p->count());
    return(0);
}

void PrintResLog(pdlist *p, FILE *fp) {
    LogMsg(9, SrvDebugLevel, stdout,  "Entered PrintResLog - pdlist and file *");
    pdlist_iterator next(*p);
    pdlink *pl;
    while (pl = next()) {
	rlent *rl = strbase(rlent, pl, link);
	rl->print(fp);
	
	if ((rl->opcode == ViceRemoveDir_OP) ||
	    (rl->opcode == ResolveViceRemoveDir_OP)) {
	    fprintf(fp, "##### Printing sub directory %s ######\n", 
		    rl->u.u_removedir.name);
	    pdlist newpl(p->offset, p->storageMgr, rl->u.u_removedir.count,
			 rl->u.u_removedir.head);
	    PrintResLog(&newpl, fp);
	    fprintf(fp, "##### Finished printing sub directory %s #####\n",
		    rl->u.u_removedir.name);
	}
	if (rl->opcode == ViceRename_OP ||
	    rl->opcode == ResolveViceRename_OP) {
	    ViceFid fid;
	    fid.Vnode = rl->u.u_rename.rename_src.cvnode;
	    fid.Unique = rl->u.u_rename.rename_src.cunique;
	    if (ISDIR(fid) && rl->u.u_rename.rename_tgt.tgtexisted) {
		fprintf(fp, "##### Printing ghost sub directory %s ### \n",
			rl->u.u_rename.rename_tgt.newname);
		pdlist newpl(p->offset, p->storageMgr, 
			     rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count,
			     rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head);
		PrintResLog(&newpl, fp);
		fprintf(fp, "##### Printing ghost sub directory %s ### \n",
			rl->u.u_rename.rename_tgt.newname);
	    }
	}
    }
}

void PrintResLog(int volindex, VnodeId vnode, Unique_t u, FILE *fp) {
    LogMsg(9, SrvDebugLevel, stdout,  "Entered PrintResLog - volindex, vnode, unique");
    VNResLog *rlog;
    pdlist *p = GetResLogList(volindex, vnode, u, &rlog);
    if (p == NULL)
	printf("Resolution Log Empty\n");
    else {
	pdlist_iterator	next(*p);
	pdlink *pl;
	while (pl = next()) {
	    rlent *rl = strbase(rlent, pl, link);
	    rl->print(fp);
	    
	    if ((rl->opcode == ViceRemoveDir_OP) ||
		(rl->opcode == ResolveViceRemoveDir_OP)) {
		fprintf(fp, "##### Printing sub directory %s ######\n", 
			rl->u.u_removedir.name);
		pdlist newpl(p->offset, p->storageMgr, rl->u.u_removedir.count,
			     rl->u.u_removedir.head);
		PrintResLog(&newpl, fp);
		fprintf(fp, "##### Finished printing sub directory %s #####\n",
			rl->u.u_removedir.name);
	    }
	    if (rl->opcode == ViceRename_OP ||
		rl->opcode == ResolveViceRename_OP) {
		ViceFid fid;
		fid.Vnode = rl->u.u_rename.rename_src.cvnode;
		fid.Unique = rl->u.u_rename.rename_src.cunique;
		if (ISDIR(fid) && rl->u.u_rename.rename_tgt.tgtexisted) {
		    fprintf(fp, "##### Printing ghost sub directory %s ### \n",
			    rl->u.u_rename.rename_tgt.newname);
		    pdlist newpl(p->offset, p->storageMgr, 
				 rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count,
				 rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head);
		    PrintResLog(&newpl, fp);
		    fprintf(fp, "##### Printing ghost sub directory %s ### \n",
			    rl->u.u_rename.rename_tgt.newname);
		}
	    }
	}
    }
}

void fPrintResLog(pdlist *p, FILE *fp) {
    pdlist_iterator next(*p);
    pdlink *pl;
    while (pl = next()) {
	rlent *rl = strbase(rlent, pl, link);
	rl->print(fp);
    }
}

/* Dump a volume's log in network order 
 * into file with descriptor fd 
 */
void DumpVolResLog(PMemMgr *p, int fd) {
    int size = p->maxEntries * p->classSize;
    char *c = (char *)malloc(size);
    bcopy(p->baseAddr, c, size);
/*  FOR NOW DONT DO THE nettohost etc.
    htonVolResLog(c, size); dont need to do the malloc either;
*/
    write(fd, c, size);
    free(c);
}

/* return entry numbers in PMemMgr.
   vice/smon.c calls this routine.
 */
void GetResStatistics(PMemMgr *p, int *wm, int *ac, int *dc) {
    *wm = p->highWater;
    *ac = p->nEntriesAllocated;
    *dc = p->nEntriesDeallocated;
}

VolumeId VolindexToVolid(int i) {
    return SRV_RVM(VolumeList[i]).header.id;
}

/* read in a resolution log from a file; 
 * convert it into host order
 */
void ReadVolResLog(PMemMgr **p, int fd) {
    struct stat buf;
    CODA_ASSERT(fstat(fd, &buf) == 0);
    int nentries = (int) (buf.st_size/sizeof(rlent));
    *p = new PMemMgr((int) sizeof(rlent), nentries, -1, nentries);
    CODA_ASSERT(read(fd, (*p)->baseAddr, (int) (nentries * sizeof(rlent))) 
	   == (nentries * sizeof(rlent)));	
    for (int i = 0; i < (*p)->bitmapSize ; i++) 
	(*p)->bitmap[i] = 255;
    (*p)->nEntries = nentries;
/*  FOR NOW IGNORE ntoh stuff 
    ntohVolResLog((*p)->baseAddr, nentries * sizeof(rlent));
*/
}

/* given a buf of log entries in host order, 
 * convert them into network order. 
 */
void htonVolResLog(char *nvollog, int size) {
    int nentries = (int) (size / sizeof(rlent));
    rlent *rl;
    for (int i = 0; i < nentries; i++) {
	rl = (rlent *)(nvollog + (sizeof(rlent) * i));
	rl->hton();
    }
}

/* given a buf of log entries in network order, 
 * convert them into host order. 
 */
void ntohVolResLog(char *hvollog, int size) {
    int nentries = (int) (size / sizeof(rlent));
    rlent *rl;
    for (int i = 0; i < nentries; i++) {
	rl = (rlent *)(hvollog + (i * sizeof(rlent)));
	rl->ntoh();
    }
}


VNResLog::VNResLog(VnodeId v, Unique_t u, int volindex) {
    vnode = v;
    uniquifier = u;
    /* the pdlist constructor should be called directly by new */
    loglist = new pdlist(fldoff(rlent, link), LogStore[volindex]);
    LCP.Host = 0;
    LCP.Uniquifier = 0;
}

VNResLog::VNResLog(VnodeId v, Unique_t u, PMemMgr *p, int head, 
		   int count, ViceStoreId *stid) {
    vnode = v;
    uniquifier = u;
    LCP = *stid;
    loglist = new pdlist(fldoff(rlent, link), p);
    loglist->head = head;
    loglist->cnt = count;
}
    
VNResLog::~VNResLog()  {
    if (loglist)
	delete loglist;
}

rlent::rlent() {
}

rlent::~rlent() {

}

int rlent::init(ViceFid *dirfid, ViceStoreId *vstid, 
		int op, va_list ap) {

    storeid = *vstid;
    opcode = op;
    dvnode = dirfid->Vnode;
    dunique = dirfid->Unique;
    serverid = ThisHostAddr;
    link.init();

    switch(op) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	u.u_newstore.stType = va_arg(ap, unsigned int);
	if (u.u_newstore.stType == ACLStore) {
	    AL_AccessList *acl = va_arg(ap, AL_AccessList *);
	    CODA_ASSERT(acl->MySize <= SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE);

/*	    FOR NOW NO ACL IS STORED TO CONSERVE SPACE IN THE LOG RECORD - 
	    ONCE OPTIMIZED LOG RECORDS COME ABOUT WE WILL LOOK AT THIS
	    bcopy(acl, &(u.u_store.s.acl[0]), acl->MySize);
*/

	}
	else {
	    u.u_newstore.s.st.owner = va_arg(ap, UserId);
	    u.u_newstore.s.st.mode = va_arg(ap, RPC2_Unsigned);
	    u.u_newstore.s.st.mask = va_arg(ap, RPC2_Integer);
	}
	break;

      case ResolveViceRemove_OP:
      case ViceRemove_OP: /* removal of link */
	{
	    char *c = va_arg(ap, char *);
	    u.u_remove.name[DIROPNAMESIZE - 1] = '\0';
	    if (strlen(c) >= (DIROPNAMESIZE - 1))
		strncpy(u.u_remove.name, c, DIROPNAMESIZE - 1);
	    else 
		strcpy(u.u_remove.name, c);
	    u.u_remove.cvnode = va_arg(ap, VnodeId);
	    u.u_remove.cunique = va_arg(ap, Unique_t);
	    u.u_remove.cvv = *va_arg(ap, ViceVersionVector *);
	}
	break;
      case ResolveViceCreate_OP:
      case ViceCreate_OP: 
	{
	    char *c = va_arg(ap, char *);
	    u.u_create.name[DIROPNAMESIZE - 1] = '\0';
	    if (strlen(c) >= (DIROPNAMESIZE - 1))
		strncpy(u.u_create.name, c, DIROPNAMESIZE - 1);
	    else 
		strcpy(u.u_create.name, c);
	    u.u_create.cvnode = va_arg(ap, VnodeId);
	    u.u_create.cunique = va_arg(ap, Unique_t);
	}
	break;
      case ResolveViceRename_OP:
      case ViceRename_OP:
	{
	    u.u_rename.srctgt = va_arg(ap, unsigned int);
	    char *c = va_arg(ap, char *);
	    u.u_rename.rename_src.oldname[DIROPNAMESIZE - 1] = '\0';
	    if (strlen(c) >= (DIROPNAMESIZE - 1))
		strncpy(u.u_rename.rename_src.oldname, c, DIROPNAMESIZE - 1);
	    else
		strcpy(u.u_rename.rename_src.oldname, c);
	    u.u_rename.rename_src.cvnode = va_arg(ap, VnodeId);
	    u.u_rename.rename_src.cunique = va_arg(ap, Unique_t);
	    u.u_rename.rename_src.cvv = *va_arg(ap, ViceVersionVector *);
	    u.u_rename.OtherDirV = va_arg(ap, VnodeId);
	    u.u_rename.OtherDirU = va_arg(ap, Unique_t);
	    c = va_arg(ap, char *);
	    u.u_rename.rename_tgt.newname[DIROPNAMESIZE - 1] = '\0';
	    if (strlen(c) >= (DIROPNAMESIZE - 1))
		strncpy(u.u_rename.rename_tgt.newname, c, DIROPNAMESIZE - 1);
	    else 
		strcpy(u.u_rename.rename_tgt.newname, c);
	    u.u_rename.rename_tgt.tgtexisted = va_arg(ap, int);
	    if (u.u_rename.rename_tgt.tgtexisted) {
		u.u_rename.rename_tgt.TgtVnode = va_arg(ap, VnodeId);		
		u.u_rename.rename_tgt.TgtUnique = va_arg(ap, Unique_t);
		ViceFid cfid;
		cfid.Volume = 0;
		cfid.Vnode = u.u_rename.rename_tgt.TgtVnode;
		cfid.Unique = u.u_rename.rename_tgt.TgtUnique;
		if (ISDIR(cfid)) {
		    u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head = va_arg(ap, int);
		    u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count = va_arg(ap, int);
		}
		else
		    u.u_rename.rename_tgt.TgtGhost.TgtGhostVV = *va_arg(ap, ViceVersionVector *);
	    }
	    else {
		u.u_rename.rename_tgt.TgtVnode = 0;
		u.u_rename.rename_tgt.TgtUnique = 0;
		u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head = -1;
		u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count = 0;
	    }
	}
	break;
      case ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	{
	    char *c = va_arg(ap, char *);
	    u.u_symlink.name[DIROPNAMESIZE - 1] = '\0';
	    if (strlen(c) >= (DIROPNAMESIZE - 1))
		strncpy(u.u_symlink.name, c, DIROPNAMESIZE - 1);
	    else
		strcpy(u.u_symlink.name, c);
	    u.u_symlink.cvnode = va_arg(ap, VnodeId);
	    u.u_symlink.cunique = va_arg(ap, Unique_t);
	}
	break;
      case ResolveViceLink_OP:
      case ViceLink_OP:
	{
	    char *c = va_arg(ap, char *);
	    u.u_hardlink.name[DIROPNAMESIZE - 1] = '\0';
	    if (strlen(c) >= (DIROPNAMESIZE - 1))
		strncpy(u.u_hardlink.name, c, DIROPNAMESIZE - 1);
	    else
		strcpy(u.u_hardlink.name, c);
	    u.u_hardlink.cvnode = va_arg(ap, VnodeId);
	    u.u_hardlink.cunique = va_arg(ap, Unique_t);
	}
	break;
      case ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	{
	    char *c = va_arg(ap, char *);
	    u.u_makedir.name[DIROPNAMESIZE - 1] = '\0';
	    if (strlen(c) >= (DIROPNAMESIZE - 1))
		strncpy(u.u_makedir.name, c, DIROPNAMESIZE - 1);
	    else
		strcpy(u.u_makedir.name, c);
	    u.u_makedir.cvnode = va_arg(ap, VnodeId);
	    u.u_makedir.cunique = va_arg(ap, Unique_t);
	}
	break;
      case ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	{
	    char *c = va_arg(ap, char *);
	    u.u_removedir.name[DIROPNAMESIZE - 1] = '\0';
	    if (strlen(c) >= (DIROPNAMESIZE - 1))
		strncpy(u.u_removedir.name, c, DIROPNAMESIZE - 1);
	    else
		strcpy(u.u_removedir.name, c);
	    u.u_removedir.cvnode = va_arg(ap, VnodeId);
	    u.u_removedir.cunique = va_arg(ap, Unique_t);
	    u.u_removedir.head = va_arg(ap, int);
	    u.u_removedir.count = va_arg(ap, int);
	    u.u_removedir.childLCP = *(va_arg(ap, ViceStoreId *));
	    u.u_removedir.csid = *(va_arg(ap, ViceStoreId *));
	}
	break;
      case ResolveNULL_OP:
	LogMsg(9, SrvDebugLevel, stdout,  "rlent::init - NULL operation ");
	break;
      case ResolveAfterCrash_OP:
	LogMsg(9, SrvDebugLevel, stdout,  "rlent::init - Creating new log record after crash recovery");
	break;
      case ViceRepair_OP:
	LogMsg(9, SrvDebugLevel, stdout,  "rlent::init - new repair log record ");
	break;
      default:
	LogMsg(29, SrvDebugLevel, stdout,  "rlent::rlent Illegal opcode");
	return(-1);
    }
    return(0);
}

void rlent::hton() {
    int op = (int) opcode;
    htonsid(&storeid, &storeid);
    opcode = htonl(opcode);
    link.hton();
    dvnode = htonl(dvnode);
    dunique = htonl(dunique);
    
    unsigned long t, nt;
    switch(op) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	nt = u.u_newstore.stType;
	u.u_newstore.stType = htonl(nt);
	if (nt == ACLStore) 
/*
	    AL_htonAlist((AL_AccessList *)&(u.u_store.s.acl[0]));
*/
	    ;
	else {
	    u.u_newstore.s.st.owner = htonl(u.u_newstore.s.st.owner);
	    u.u_newstore.s.st.mode = htonl(u.u_newstore.s.st.mode);
	    u.u_newstore.s.st.mask = htonl(u.u_newstore.s.st.mask);
	}
	break;

      case ResolveViceRemove_OP:	
      case ViceRemove_OP:
	u.u_remove.cvnode = htonl(u.u_remove.cvnode);
	u.u_remove.cunique = htonl(u.u_remove.cunique);
	htonvv(&u.u_remove.cvv, &u.u_remove.cvv);
	break;
      case  ResolveViceCreate_OP:
      case ViceCreate_OP:
	u.u_create.cvnode = htonl(u.u_create.cvnode);
	u.u_create.cunique = htonl(u.u_create.cunique);
	break;
      case  ResolveViceRename_OP:
      case ViceRename_OP:
	{
	    unsigned long st = u.u_rename.srctgt;
	    u.u_rename.srctgt = htonl(u.u_rename.srctgt);
	    u.u_rename.rename_src.cvnode = htonl(u.u_rename.rename_src.cvnode);
	    u.u_rename.rename_src.cunique = htonl(u.u_rename.rename_src.cunique);
	    htonvv(&u.u_rename.rename_src.cvv, &u.u_rename.rename_src.cvv);
	    u.u_rename.OtherDirV = htonl(u.u_rename.OtherDirV);
	    u.u_rename.OtherDirU = htonl(u.u_rename.OtherDirU);
	    int te = u.u_rename.rename_tgt.tgtexisted;
	    u.u_rename.rename_tgt.tgtexisted = (int) htonl(u.u_rename.rename_tgt.tgtexisted);
	    if (te) {
		ViceFid cfid;
		cfid.Vnode = u.u_rename.rename_tgt.TgtVnode;
		cfid.Unique = u.u_rename.rename_tgt.TgtUnique;
		u.u_rename.rename_tgt.TgtVnode = htonl(u.u_rename.rename_tgt.TgtVnode);
		u.u_rename.rename_tgt.TgtUnique = htonl(u.u_rename.rename_tgt.TgtUnique);
		if (ISDIR(cfid)) {
		    u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head = (int) htonl(u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head);
		    u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count = (int) htonl(u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count);
		}
		else 
		    htonvv(&u.u_rename.rename_tgt.TgtGhost.TgtGhostVV, &u.u_rename.rename_tgt.TgtGhost.TgtGhostVV);
	    }
	}
	break;
      case  ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	u.u_symlink.cvnode = htonl(u.u_symlink.cvnode);
	u.u_symlink.cunique = htonl(u.u_symlink.cunique);
	break;
      case  ResolveViceLink_OP:
      case ViceLink_OP:
	u.u_hardlink.cvnode = htonl(u.u_hardlink.cvnode);
	u.u_hardlink.cunique = htonl(u.u_hardlink.cunique);
	break;
      case  ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	u.u_makedir.cvnode = htonl(u.u_makedir.cvnode);
	u.u_makedir.cunique = htonl(u.u_makedir.cunique);
	break;
      case  ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	u.u_removedir.cvnode = htonl(u.u_removedir.cvnode);
	u.u_removedir.cunique = htonl(u.u_removedir.cunique);
	u.u_removedir.head = (int) htonl(u.u_removedir.head);
	u.u_removedir.count = (int) htonl(u.u_removedir.count);
	htonsid(&u.u_removedir.childLCP, &u.u_removedir.childLCP);
	htonsid(&u.u_removedir.csid, &u.u_removedir.csid);
	break;
      default:
	break;
    }
}

void rlent::ntoh() {
    ntohsid(&storeid, &storeid);
    opcode = ntohl(opcode);
    link.ntoh();
    dvnode = ntohl(dvnode);
    dunique = ntohl(dunique);
    
    switch(opcode) {
      case  ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	u.u_newstore.stType = ntohl(u.u_newstore.stType);
	if (u.u_newstore.stType == ACLStore)
/*
	    AL_ntohAlist((AL_AccessList *)&(u.u_store.s.acl[0]));
*/
	    ;
	else {
	    u.u_newstore.s.st.owner = ntohl(u.u_newstore.s.st.owner);
	    u.u_newstore.s.st.mode = ntohl(u.u_newstore.s.st.mode);
	    u.u_newstore.s.st.mask = ntohl(u.u_newstore.s.st.mask);
	}
	break;

      case  ResolveViceRemove_OP:
      case ViceRemove_OP:
	u.u_remove.cvnode = ntohl(u.u_remove.cvnode);
	u.u_remove.cunique = ntohl(u.u_remove.cunique);
	ntohvv(&u.u_remove.cvv, &u.u_remove.cvv);
	break;
      case  ResolveViceCreate_OP:
      case ViceCreate_OP:
	u.u_create.cvnode = ntohl(u.u_create.cvnode);
	u.u_create.cunique = ntohl(u.u_create.cunique);
	break;
      case  ResolveViceRename_OP:
      case ViceRename_OP:
	{
	    u.u_rename.srctgt = ntohl(u.u_rename.srctgt);
	    u.u_rename.rename_src.cvnode = ntohl(u.u_rename.rename_src.cvnode);
	    u.u_rename.rename_src.cunique = ntohl(u.u_rename.rename_src.cunique);
	    ntohvv(&u.u_rename.rename_src.cvv, &u.u_rename.rename_src.cvv);
	    u.u_rename.OtherDirV = ntohl(u.u_rename.OtherDirV);
	    u.u_rename.OtherDirU = ntohl(u.u_rename.OtherDirU);
	    u.u_rename.rename_tgt.tgtexisted = (int) ntohl(u.u_rename.rename_tgt.tgtexisted);
	    if (u.u_rename.rename_tgt.tgtexisted) {
		ViceFid cfid;
		u.u_rename.rename_tgt.TgtVnode = ntohl(u.u_rename.rename_tgt.TgtVnode);
		u.u_rename.rename_tgt.TgtUnique = ntohl(u.u_rename.rename_tgt.TgtUnique);
		cfid.Vnode = u.u_rename.rename_tgt.TgtVnode;
		cfid.Unique = u.u_rename.rename_tgt.TgtUnique;
		if (ISDIR(cfid)) {
		    u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head = (int) ntohl(u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head);
		    u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count = (int) ntohl(u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count);
		}
		else 
		    ntohvv(&u.u_rename.rename_tgt.TgtGhost.TgtGhostVV, &u.u_rename.rename_tgt.TgtGhost.TgtGhostVV);
	    }
	}
	break;
      case  ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	u.u_symlink.cvnode = ntohl(u.u_symlink.cvnode);
	u.u_symlink.cunique = ntohl(u.u_symlink.cunique);
	break;
      case  ResolveViceLink_OP:
      case ViceLink_OP:
	u.u_hardlink.cvnode = ntohl(u.u_hardlink.cvnode);
	u.u_hardlink.cunique = ntohl(u.u_hardlink.cunique);
	break;
      case  ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	u.u_makedir.cvnode = ntohl(u.u_makedir.cvnode);
	u.u_makedir.cunique = ntohl(u.u_makedir.cunique);
	break;
      case  ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	u.u_removedir.cvnode = ntohl(u.u_removedir.cvnode);
	u.u_removedir.cunique = ntohl(u.u_removedir.cunique);
	u.u_removedir.head = (int) ntohl(u.u_removedir.head);
	u.u_removedir.count = (int) ntohl(u.u_removedir.count);
	ntohsid(&u.u_removedir.childLCP, &u.u_removedir.childLCP);
	ntohsid(&u.u_removedir.csid, &u.u_removedir.csid);
	break;
      default:
	break;
    }
}

ViceStoreId *rlent::GetStoreId() {
    return(&storeid);
}
void rlent::print() {
    print(stdout);
}
void rlent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}

void rlent::print(int fd) {
    char buf[512];
    sprintf(buf, "****** Log Record\nStoreId: %x.%x \n", 
	    storeid.Host, storeid.Uniquifier);
    write(fd, buf, (int) strlen(buf));
    sprintf(buf, "Directory(%x.%x)\nHosid = %x\n", dvnode, dunique, serverid);
    write(fd, buf, (int) strlen(buf));

    sprintf(buf, "Opcode: %s %s\n",
	    PRINTOPCODE(opcode), (opcode == ViceRename_OP ? 
				  (u.u_rename.srctgt == SOURCE ? 
				   "(src)":"(tgt)") : " "));
    write(fd, buf, (int) strlen(buf));
    
    ViceVersionVector *vv;
    switch (opcode) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	if (u.u_newstore.stType == ACLStore) {
	    AL_ExternalAccessList ea;
/*
	    if (AL_Externalize((AL_AccessList *)&(u.u_store.s.acl[0]), &ea) != 0)
		sprintf(buf, " stType = ACL: Couldnt translate access list\n");
	    else {
		sprintf(buf, " stType = ACL: %s", ea);
		AL_FreeExternalAlist(&ea);
	    }
*/
	    sprintf(buf, "ACL Store operation - NO ACL STORED YET\n");
	}
	else 
	    sprintf(buf, " stType = status; Mask = %o Owner: %u Mode %u\n", 
		    u.u_newstore.s.st.mask, u.u_newstore.s.st.owner, u.u_newstore.s.st.mode);
	break;

      case  ResolveViceRemove_OP:
      case ViceRemove_OP:
	vv = &u.u_remove.cvv;
	sprintf(buf, "Child: %s (%x.%x)[%d %d %d %d %d %d %d %d (%x.%x) (%d)]\n", 
		u.u_remove.name, u.u_remove.cvnode, u.u_remove.cunique, 
		vv->Versions.Site0, vv->Versions.Site1, vv->Versions.Site2,
		vv->Versions.Site3, vv->Versions.Site4, vv->Versions.Site5,
		vv->Versions.Site6, vv->Versions.Site7, vv->StoreId.Host,
		vv->StoreId.Uniquifier, vv->Flags);
	break;
      case  ResolveViceCreate_OP:
      case ViceCreate_OP:
	sprintf(buf, "Child: %s (%x.%x)\n",
		u.u_create.name, u.u_create.cvnode, u.u_create.cunique);
	break;
      case  ResolveViceRename_OP:
      case ViceRename_OP:
	if (u.u_rename.srctgt == SOURCE)
	    sprintf(buf, "SOURCE DIR: src child %s (%x.%x), \ntarget dir %x.%x\n",
		    u.u_rename.rename_src.oldname, 
		    u.u_rename.rename_src.cvnode,
		    u.u_rename.rename_src.cunique,
		    u.u_rename.OtherDirV,
		    u.u_rename.OtherDirU);
	else
	    sprintf(buf, "TARGET DIR: src child %s (%x.%x) src dir %x.%x\n",
		    u.u_rename.rename_src.oldname, 
		    u.u_rename.rename_src.cvnode,
		    u.u_rename.rename_src.cunique,
		    u.u_rename.OtherDirV,
		    u.u_rename.OtherDirU);
	
	write(fd, buf, (int) strlen(buf));
	if (u.u_rename.rename_tgt.tgtexisted) {
	    ViceFid cfid;
	    cfid.Vnode = u.u_rename.rename_tgt.TgtVnode;
	    cfid.Unique = u.u_rename.rename_tgt.TgtUnique;
	    if (ISDIR(cfid))
		sprintf(buf, "Target Existed: %s %x.%x, gh log head = %d cnt = %d\n",
			u.u_rename.rename_tgt.newname,
			u.u_rename.rename_tgt.TgtVnode,
			u.u_rename.rename_tgt.TgtUnique, 
			u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head, 
			u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count);
	    else {
		ViceVersionVector *vv = &u.u_rename.rename_tgt.TgtGhost.TgtGhostVV;
		sprintf(buf, "Target Existed: %s %x.%x, gh vv = [%d %d %d %d %d %d %d %d (%x.%x) (%d)]\n",
			u.u_rename.rename_tgt.newname,
			u.u_rename.rename_tgt.TgtVnode,
			u.u_rename.rename_tgt.TgtUnique, 
			vv->Versions.Site0, vv->Versions.Site1, 
			vv->Versions.Site2, vv->Versions.Site3, 
			vv->Versions.Site4, vv->Versions.Site5,
			vv->Versions.Site6, vv->Versions.Site7, 
			vv->StoreId.Host, vv->StoreId.Uniquifier, 
			vv->Flags);
	    }
	}
	else 
	    sprintf(buf, "Target Name: %s\n", u.u_rename.rename_tgt.newname);
	break;
      case  ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	sprintf(buf, "Child: %s (%x.%x)\n",
		u.u_symlink.name, u.u_symlink.cvnode, u.u_symlink.cunique);
	break;
      case  ResolveViceLink_OP:
      case ViceLink_OP:
	sprintf(buf, "Child: %s (%x.%x)\n",
		u.u_hardlink.name, u.u_hardlink.cvnode, u.u_hardlink.cunique);
	break;
      case  ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	sprintf(buf, "Child: %s (%x.%x)\n",
		u.u_makedir.name, u.u_makedir.cvnode, u.u_makedir.cunique);
	break;
      case  ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	sprintf(buf, "Child: %s (%x.%x)[%x.%x] LCP[%x.%x]\n",
		u.u_removedir.name, u.u_removedir.cvnode,
		u.u_removedir.cunique, u.u_removedir.csid.Host, 
		u.u_removedir.csid.Uniquifier,u.u_removedir.childLCP.Host,
		u.u_removedir.childLCP.Uniquifier);
	break;
      case ResolveNULL_OP:
	sprintf(buf, "ResolveNULL record\n");
	break;
      case ResolveAfterCrash_OP:
	sprintf(buf, "ResolveAfterCrash record\n");
	break;
      case ViceRepair_OP:
	sprintf(buf, "ViceRepair record\n");
	break;
      default:
	sprintf(buf, "!!!!!!!! Unknown Opcode - record not parsed !!!!!!\n");
	break;
    }
    write(fd, buf, (int) strlen(buf));
    
    sprintf(buf, "********* End of Record *******\n");
    write(fd, buf, (int) strlen(buf));
}

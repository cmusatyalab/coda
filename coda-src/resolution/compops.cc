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
 * compops.c
 *	routines for computing list of 
 *	compensating operations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <olist.h>
#include <srv.h>
#include "rsle.h"
#include "compops.h"
#include "parselog.h"

// ********** Private Routines *************
static olist *ExtractLog(olist *, unsigned long, ViceFid *);
static olist **ExtractRemoteLogs(olist *, ViceFid *, int *);
static arrlist *FindRemoteOps(arrlist *, olist **, int );
static arrlist *MergeRemoteOps(arrlist *, int);
static arrlist *RemoveLocalOps(arrlist *, arrlist *);
static void SortLog(olist *, arrlist *);
static arrlist *SortLog(olist *);
static arrlist *SortRemoteLogs(olist **, int);
static rsle *LatestCommonPoint(arrlist *, arrlist *);
static int CmpIndex(rsle **, rsle **);
static int CmpCompEntries(rsle **, rsle **);
static int CompareStoreId(ViceStoreId *, ViceStoreId *);
static int CmpSleEntries(rsle **, rsle **);
static int IsLater(rsle *a, rsle *b);
static void PrintArrList(arrlist *, char *);
static void PrintRemoteLogs(olist **, int );
static void PrintLogList(olist *);

arrlist *ComputeCompOps(olist *AllLogs, ViceFid *Fid) 
{
    int nrmtsites = 0;
    arrlist *sllog = NULL;
    olist **rmtlogs = NULL;
    arrlist *RemoteOps = NULL;
    arrlist *MergedRmtOps = NULL;
    arrlist *CompOps = NULL;
    olist *llog = NULL;
    LogMsg(0, SrvDebugLevel, stdout,
	   "ComputeCompOps: fid(0x%x.%x.%x)\n",
	   Fid->Volume, Fid->Vnode, Fid->Unique);
    
    // Extract local log 
    {
	llog = ExtractLog(AllLogs, ThisHostAddr, Fid);
	CODA_ASSERT(llog);
	sllog = SortLog(llog);
	LogMsg(10, SrvDebugLevel, stdout,
	       "***** Local Log is \n");
	if (SrvDebugLevel > 10) 
	    PrintLogList(llog);
    }
    
    // Extract remote host logs 
    {
	rmtlogs = ExtractRemoteLogs(AllLogs, Fid, &nrmtsites);
	if (SrvDebugLevel > 10) 
	    PrintRemoteLogs(rmtlogs, nrmtsites);
    }

    // Compute partial ops w.r.t. each remote site
    {
	RemoteOps = FindRemoteOps(sllog, rmtlogs, nrmtsites);
	if (!RemoteOps) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "Local Log Contains\n");
	    olist_iterator next(*llog);
	    rsle *r;
	    while ((r = (rsle *)next())) 
		r->print(stdout);
	    goto Exit;
	}
	if (SrvDebugLevel > 10) 
	    PrintArrList(RemoteOps, "Remote Operations");
    }
    
    // Merge lists of Non Local operations
    {
        MergedRmtOps = MergeRemoteOps(RemoteOps, nrmtsites);
	CompOps = RemoveLocalOps(MergedRmtOps, sllog);
	if (SrvDebugLevel > 10) {
	    PrintArrList(MergedRmtOps, "Merged Non-local Operations");
	    PrintArrList(CompOps, "Unsorted Compensating entries");
	}
	qsort(CompOps->list, CompOps->cursize, sizeof(void *), 
		(int (*)(const void *, const void *))CmpCompEntries);
    }
    
    // Clean up
  Exit:
    {
	if (sllog) delete sllog;
	if (rmtlogs) free(rmtlogs);
	if (RemoteOps) delete[] RemoteOps;
	if (MergedRmtOps) delete MergedRmtOps;
    }

    return(CompOps);
}

// ExtractLog
//	Find the log entries for a vnode spooled by a given host
static olist *ExtractLog(olist *logs, 
			  unsigned long hostid, 
			  ViceFid *Fid) {
    LogMsg(9, SrvDebugLevel, stdout, 
	   "Entering ExtractLog (0x%x), (0x%x.%x.%x)\n",
	   hostid, Fid->Volume, Fid->Vnode, Fid->Unique);
    
    he *hep = FindHE(logs, hostid);
    if (hep == NULL) {
	LogMsg(0, SrvDebugLevel, stdout,
	       "ExtractLog: No entries for host %x found\n",
	       hostid);
	return(NULL);
    }
    
    olist_iterator next(hep->vlist);
    remoteloglist *rllp;
    while ((rllp = (remoteloglist *)next()) != NULL) 
	if (rllp->vnode == Fid->Vnode &&
	    rllp->unique == Fid->Unique)
	    break;
    if (rllp) {
	LogMsg(9, SrvDebugLevel, stdout, 
	       "ExtractLog: Found loglist for (0x%x) (0x%x.%x.%x)\n",
	       hostid, Fid->Volume, Fid->Vnode, Fid->Unique);	       
	return(&(rllp->slelist));
    }
    else {
	LogMsg(0, SrvDebugLevel, stdout, 
	       "ExtractLog: No loglist for (0x%x) (0x%x.%x.%x)\n",
	       hostid, Fid->Volume, Fid->Vnode, Fid->Unique);	       
	return(NULL);
    }
}

static olist **ExtractRemoteLogs(olist *logs, ViceFid *Fid, int *nlists) {
    olist **rmtlogs = (olist **)malloc(logs->count() * sizeof(olist *));
    CODA_ASSERT(rmtlogs);

    olist_iterator next(*logs);
    he *hoste;
    int index = 0;
    while ((hoste = (he *)next())) {
	if (hoste->hid == (long)ThisHostAddr) continue;
	rmtlogs[index] = ExtractLog(logs, hoste->hid, Fid);
	index++;
    }
    *nlists = index;
    return(rmtlogs);
}

static arrlist *FindRemoteOps(arrlist *sllog, olist **rlogs,
			       int nrmtsites) {
    rsle **CommonPoints = NULL;
    arrlist *srlogs = NULL;
    arrlist *nonlocalops = NULL;

    // Sort remote logs 
    {
	srlogs = SortRemoteLogs(rlogs, nrmtsites);
    }
    // Find each remote log's common point with local log
    {
	CommonPoints = (rsle **)malloc(sizeof(rsle *) * nrmtsites);
	for (int i = 0; i < nrmtsites; i++) {
	    CommonPoints[i] = LatestCommonPoint(sllog, &srlogs[i]);
	    if (!CommonPoints[i]) {
		LogMsg(0, SrvDebugLevel, stdout, 
		       "FindRemoteOps: Couldnt find common point with remote site\n");
		goto Exit;
	    }
	}
    }

    // Form lists of NonLocal Operations 
    {
	nonlocalops = new arrlist[nrmtsites];
	for (int i = 0; i < nrmtsites; i++) {
	    olist_iterator next(*(rlogs[i]));
	    rsle *r;
	    // scan remote log until common point
	    while ((r = (rsle *)next())) 
		if (r == CommonPoints[i]) break;
	    CODA_ASSERT(r);
	    // copy rest of remote log 
	    while ((r = (rsle *)next())) 
		nonlocalops[i].add((void *)r);
	}
    }

    // Clean up
  Exit:
    {
	if (srlogs) delete[] srlogs;
	if (CommonPoints) free(CommonPoints);
    }
    return nonlocalops;
}

// MergeRemoteOps
//	put together unique entries from all remote ops
static arrlist *MergeRemoteOps(arrlist *rmtops, int nsites) {
    arrlist allops;
    arrlist *mergedops = NULL;

    // combine all ops 
    {
	for (int i = 0; i < nsites; i++) {
	    arrlist_iterator next(&rmtops[i]);
	    void *p;
	    while ((p = next())) 
		allops.add(p);
	}
    }

    // Sort the entries 
    {
	qsort(allops.list, allops.cursize, sizeof(void *), 
		(int (*)(const void *, const void *))CmpSleEntries);
    }


    // remove duplicates 
    {
	mergedops = new arrlist;
	ViceStoreId prevstoreid;
	prevstoreid.Host = prevstoreid.Uniquifier = 0;
	arrlist_iterator next(&allops);
	rsle *r;
	while ((r = (rsle *)next()) )
	    if (!SID_EQ(r->storeid, prevstoreid)) {
		mergedops->add((void *)r);
		prevstoreid = r->storeid;
	    }
    }
    return(mergedops);
}

// Remove the local ops that also appear in the merged rmt ops
// IMPORTANT: This assumes that sllog and ops have already been 
// 		sorted by storeid
static arrlist *RemoveLocalOps(arrlist *ops, arrlist *sllog) 
{
    int res = -1;
    arrlist *newlist = new arrlist(ops->cursize);
    arrlist_iterator nexto(ops);
    arrlist_iterator nextl(sllog);
    rsle *or;
    rsle *lr = NULL;
    int gotnextl = 0;
    while ((gotnextl)|| (lr = (rsle *)nextl())) {
	gotnextl = 0;
	while ((or = (rsle *)nexto()) &&
	       ((res = CompareStoreId(&or->storeid, &lr->storeid)) < 0))
	    newlist->add((void *)or);
	if (res == 0) continue;
	if (!or) break;

	// or is > lr
	while ((lr = (rsle *)nextl()) &&
	       ((res = CompareStoreId(&or->storeid, &lr->storeid)) > 0))
	    ;
	if (!lr) {
	    newlist->add((void *)or);
	    break;
	}
	if (res == 0) 
	    continue;
	if (res < 0) {
	    newlist->add((void *)or);
	    gotnextl = 1;
	}
    }
    while ((or = (rsle *)nexto()))
	newlist->add((void *)or);
    return(newlist);
}


static void SortLog(olist *log, arrlist *slp) {
    olist_iterator next(*log);
    void *p;
    while ((p = (void *)next()))
	slp->add(p);
    qsort(slp->list, slp->cursize, sizeof(void *), 
	  (int (*)(const void *, const void *))CmpSleEntries);
}
    
static arrlist *SortLog(olist *log) {
    arrlist *slp = new arrlist(log->count());
    CODA_ASSERT(slp);
    SortLog(log, slp);
    return(slp);
}

static arrlist *SortRemoteLogs(olist **logs, int nlogs) {
    arrlist *slogs = new arrlist[nlogs];
    for (int i = 0; i < nlogs; i++) 
	SortLog(logs[i], &slogs[i]);
    return(slogs);
}


// LatestCommonPoint
//	Given two sorted logs a and b
//	Return pointer to latest entry 
//		in b that also occurs in a
static rsle *LatestCommonPoint(arrlist *a, arrlist *b) {
    rsle *LatestCommonEntry = NULL;
    int na = a->cursize;
    int nb = b->cursize;
    int aindex, bindex;
    for (aindex = 0, bindex = 0;
	 aindex < na && bindex < nb;
	 aindex++) {
	ViceStoreId astid = ((rsle *)a->list[aindex])->storeid;
	ViceStoreId bstid = ((rsle *)b->list[bindex])->storeid;
	int cmpresult = CompareStoreId(&astid, &bstid);
	LogMsg(39, SrvDebugLevel, stdout,  
	       "LatestCommonPoint: cmpresult = %d;aindex = %d,bindex = %d", 
	       cmpresult, aindex, bindex);
	while (cmpresult > 0 && bindex < (nb  - 1)) {
	    bindex++;
	    bstid = ((rsle *)b->list[bindex])->storeid;
	    cmpresult = CompareStoreId(&astid, &bstid);
	}
	if ((cmpresult == 0) &&
	    (!LatestCommonEntry  || 
	     IsLater((rsle *)b->list[bindex], LatestCommonEntry)) &&
	    ISNONRESOLVEOP(((rsle *)a->list[aindex])->opcode) &&
	    ISNONRESOLVEOP(((rsle *)b->list[bindex])->opcode))
	    LatestCommonEntry = (rsle *)b->list[bindex];
    }

    LogMsg(9, SrvDebugLevel, stdout,
	   "LatestCommonPoint: returns %s\n", 
	   LatestCommonEntry ? "an entry" : "NULL");
    return(LatestCommonEntry);
}

static int CompareStoreId(ViceStoreId *a, ViceStoreId *b) {
    if (a->Host < b->Host) return(-1);
    else if (a->Host > b->Host) return(1);
    else if (a->Uniquifier < b->Uniquifier) return(-1);
    else if (a->Uniquifier > b->Uniquifier) return(1);
    else return(0);
}

static int CmpIndex(rsle **a, rsle **b) {
    // index is overloaded to contain serverid 
    if ((*a)->index < (*b)->index) return(-1);
    if ((*a)->index > (*b)->index) return(1);
    return(0);
}

/* Compare Log entries
 *	Storeid is the major sorting index.
 *	Serverid is the secondary sorting index
 */
static int CmpSleEntries(rsle **a, rsle **b) {
    int res = CompareStoreId(&((*a)->storeid), &((*b)->storeid));
    if (res) return(res);
    else return(CmpIndex(a, b));
}

//  CmpCompEntries
//	Hostid(index) is the primary key 
//	seq no is the secondary key
static int CmpCompEntries(rsle **a, rsle **b) {
    int res = CmpIndex(a, b);
    if (res) return(res);
    if ((*a)->seqno < (*b)->seqno) return(-1);
    if ((*a)->seqno > (*b)->seqno) return(1);
    return(0); 
}

// IsLater
//	Return 1 if a is later in time than b
static int IsLater(rsle *a, rsle *b) {
    if (a->seqno > b->seqno) return(1);
    else return(0);
}


// routines to print out different logs 
// for debugging
static void PrintArrList(arrlist *a, char *s) 
{
    printf("*** %s Begin *** \n", s);
    arrlist_iterator next(a);
    rsle *r;
    while ((r = (rsle *)next())) {
	LogMsg(0, SrvDebugLevel, stdout, 
	       "-----------------\n");
	r->print();
	LogMsg(0, SrvDebugLevel, stdout, 
	       "-----------------\n");
    }
    printf("*** %s End *** \n", s);    
}

void PrintCompOps(arrlist *a) {
    PrintArrList(a, "Compensating Operations");
}

static void PrintLogList(olist *l) {
    olist_iterator next(*l);
    rsle *r;
    while ((r = (rsle *)next())) {
	LogMsg(0, SrvDebugLevel, stdout, 
	       "-----------------\n");
	r->print();
	LogMsg(0, SrvDebugLevel, stdout, 
	       "-----------------\n");
    }
}
static void PrintRemoteLogs(olist **r, int nrmtsites) {
    printf("Printing remote logs\n");
    printf("There are %d remotesites \n", nrmtsites);
    for (int i = 0; i < nrmtsites; i++) {
	printf("$$$$$$$$ Log for site %d $$$$$$\n", i);
	PrintLogList(r[i]);
    }
}

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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/res/remotelog.cc,v 1.1.1.1 1996/11/22 19:12:51 rvb Exp";
#endif /*_BLURB_*/






/* 
 *  remotelog.c 
 * 	Convert the pdlist form to a array form 
 *	so that the remote host can understand 
 *	the log entries.
 */

/* 
 * Log entries for a directory are stored in a volume log.
 * Each log entry can be accessed via the pdlist iterator function.
 * We shouldnt be shipping entire volume logs for resolution.
 * Instead given a directory, we build a data structure containing 
 * log entries for that directory and all its descendant dirs that have
 * been deleted.  The logentries are copied into arrays of rlents.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <assert.h>
#include <stdio.h>
#include <libc.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <olist.h>
#include "res.h"
#include "pdlist.h"
#include "reslog.h"
#include "remotelog.h"
#include "resutil.h"


rmtle *FindRMTLE(olist *list, VnodeId vn, Unique_t un) {
    olist_iterator next(*list);
    rmtle *l;

    while (l = (rmtle *)next()) {
	if (l->dvnode == vn && l->dunique == un)
	    return(l);
    }
    return(0);
}

he *FindHE(olist *list, long hostaddress) {
    olist_iterator next(*list);
    he *h;
    while (h = (he *)next()) {
	if (h->hid == hostaddress)
	    return(h);
    }
    return(0);
}
rmtle *AddRMTLE(olist *list, VnodeId vn, Unique_t un) {
    rmtle *l = FindRMTLE(list, vn, un);
    if (l == 0)
	list->insert(l = new rmtle(vn, un));
    return(l);
}

rmtle *AddLocalRMTLE(olist *list, VnodeId vn, Unique_t un, pdlist *p) {
    rmtle *l = FindRMTLE(list, vn, un);
    if (l == 0)
	list->append(l = new rmtle(vn, un, p));
    return(l);
}

rmtle *AddRemoteRMTLE(olist *list, VnodeId vn, Unique_t un, 
		      int n, rlent *log) {
    rmtle *l = FindRMTLE(list, vn, un);
    if (l == 0)
	list->append(l = new rmtle(vn, un, n, log));
    return(l);
}

/*
  * FlattenLocalRMTLE:
  *	given a list of logs (pdlists) 
  *	put all their contents together into a single 
  *	flat buffer.
  */
char *FlattenLocalRMTLElist(olist *llist, int *bufsize) {
    int size;

    *bufsize = 0;
    /* count number of log entries */
    {
	size = 0;
	rmtle *ll;
	olist_iterator lnext(*llist);
	while (ll = (rmtle *)lnext()) {
	    LogMsg(59, SrvDebugLevel, stdout,  "FlattenLocalRMTLElist: Counting: %x.%x has %d log entries",
		    ll->dvnode, ll->dunique, ll->u.local.log->count());
	    size += ll->u.local.log->count();
	}
	LogMsg(59, SrvDebugLevel, stdout,  "FlattenLocalRMTLElist: %d total log entries", size);
	if (size == 0) return(0);
    }
    
    char *buf = new char[size * sizeof(rlent)];
    assert(buf);

    /* copy the res log entries into the buf */
    {
	olist_iterator lnext2(*llist);
	int i = 0;
	/* get each vnodes log */
	rmtle *ll;
	while (ll = (rmtle *)lnext2()) {
	    pdlist_iterator nextrlent(*(ll->u.local.log));
	    pdlink *pl;
	    LogMsg(59, SrvDebugLevel, stdout,  "FlattenLocal...: %x.%x has %d entries",
		    ll->dvnode, ll->dunique, ll->u.local.log->count());
	    /* get each log entry from a vnode's log */
	    while ((pl = (pdlink *)nextrlent()) && (i < size)) {
		rlent *reslogent = strbase(rlent, pl, link);
		LogMsg(59, SrvDebugLevel, stdout,  "FlattenLocalRMTLElist - getting entry %d", i);
		bcopy(reslogent, &(buf[i * sizeof(rlent)]), 
		      (int) sizeof(rlent));
		i++;
	    }
	}
	assert(i == size);
    }
    *bufsize = (int) (size * sizeof(rlent));
    return(buf);
}

/*
 * BuildRemoteRMTLElist
 *  Given a flat buffer - build a remote form of the rmtle
 */
#ifdef _UNDEF_
/* NOT BEING USED RIGHT NOW */
olist *BuildRemoteRMTLElist(char *buf, int size) {
    int nentries = size / sizeof(rlent);
    assert(buf != NULL);
    assert(nentries > 0);
    
    olist *list = new olist();
    rlent *prevrl = 0;
    rmtle *thisrmtle = 0;
    int thislogsize = 0;
    for (int i = 0; i < nentries; i++) {
	rlent *rl = (rlent *)&(buf[i * sizeof(rlent)]);
	if (prevrl == 0 || 
	    rl->dvnode != prevrl->dvnode ||
	    rl->dunique != prevrl->dunique) {

	    if (thisrmtle) 
		thisrmtle->u.remote.nentries = thislogsize;

	    /* start a new remote entry */
	    thisrmtle = AddRemoteRMTLE(list, rl->dvnode, 
				       rl->dunique, 1, rl);
	    thislogsize = 1;
	}
	else
	    thislogsize++;
	prevrl = rl;
    }
    return(list);
}
#endif _UNDEF_

#define ISDIR(fid)  ((fid).Vnode & 1)

/*
 * GetRmSubTreeLocalRMTLE
 *  Given an object id, build the local form of the 
 *  rmtle structure for all the deleted children's res log 
 */
void GetRmSubTreeLocalRMTLE(int volindex, VnodeId vn, 
			    Unique_t un, olist *list, pdlist *plist) {
    pdlist *tmpPlist;
    /* create a new list header to insert in the rm tree log */
    {
	/* get the res log for the vnode */
	if (!plist) {
	    VNResLog *rlog;
	    plist = GetResLogList(volindex, vn, un, &rlog);
	    if (plist == NULL)
		return;
	}
	/* add log to list */
	{
	    tmpPlist = new pdlist(plist->offset, plist->storageMgr, 
				  plist->cnt, plist->head);
	    assert(tmpPlist);
	    AddLocalRMTLE(list, vn, un, tmpPlist);
	}
    }

    /* add all removed childrens log to list */
    {
	pdlist_iterator next(*tmpPlist);
	pdlink *pl;
	while (pl = next()) {
	    rlent *rl = strbase(rlent, pl, link);
	    if ((rl->opcode == ViceRemoveDir_OP) || 
		(rl->opcode == ResolveViceRemoveDir_OP)) 
		GetRmSubTreeLocalRMTLE(volindex, 
				       rl->u.u_removedir.cvnode,
				       rl->u.u_removedir.cunique,
				       list, plist->offset, plist->storageMgr,
				       rl->u.u_removedir.count,
				       rl->u.u_removedir.head);
	    if ((rl->opcode == ViceRename_OP ||
		rl->opcode == ResolveViceRename_OP) &&
		rl->u.u_rename.rename_tgt.tgtexisted) {
		ViceFid tgtFid;	
		tgtFid.Vnode = rl->u.u_rename.rename_tgt.TgtVnode;
		tgtFid.Unique = rl->u.u_rename.rename_tgt.TgtUnique;
		if (ISDIR(tgtFid)) 
		    GetRmSubTreeLocalRMTLE(volindex, 
					   rl->u.u_rename.rename_tgt.TgtVnode,
					   rl->u.u_rename.rename_tgt.TgtUnique,
					   list, plist->offset, 
					   plist->storageMgr, 
					   rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count,
					   rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head);
	    }
	}
    }
}
void GetRmSubTreeLocalRMTLE(int volindex, VnodeId vn, 
			    Unique_t un, olist *list, int offset, PMemMgr *stmgr,
			    int count, int head) {
    pdlist *tmpPlist;
    /* create a new list header to insert in the rm tree log */
    /* add log to list */
    {
	tmpPlist = new pdlist(offset, stmgr, count, head);
	assert(tmpPlist);
	AddLocalRMTLE(list, vn, un, tmpPlist);
    }

    /* add all removed childrens log to list */
    {
	pdlist_iterator next(*tmpPlist);
	pdlink *pl;
	while (pl = next()) {
	    rlent *rl = strbase(rlent, pl, link);
	    if ((rl->opcode == ViceRemoveDir_OP) ||
		(rl->opcode == ResolveViceRemoveDir_OP))
		GetRmSubTreeLocalRMTLE(volindex, 
				       rl->u.u_removedir.cvnode,
				       rl->u.u_removedir.cunique,
				       list, offset, stmgr, 
				       rl->u.u_removedir.count,
				       rl->u.u_removedir.head);
	    if ((rl->opcode == ViceRename_OP ||
		rl->opcode == ResolveViceRename_OP) &&
		rl->u.u_rename.rename_tgt.tgtexisted) {
		ViceFid tgtFid;	
		tgtFid.Vnode = rl->u.u_rename.rename_tgt.TgtVnode;
		tgtFid.Unique = rl->u.u_rename.rename_tgt.TgtUnique;
		if (ISDIR(tgtFid)) 
		    GetRmSubTreeLocalRMTLE(volindex, 
					   rl->u.u_rename.rename_tgt.TgtVnode,
					   rl->u.u_rename.rename_tgt.TgtUnique,
					   list, offset, stmgr,
					   rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.count,
					   rl->u.u_rename.rename_tgt.TgtGhost.TgtGhostLog.head);
	    }
	}
    }
}
#undef ISDIR

void PurgeLocalRMTLElist(olist *llist) {
    rmtle *lrle;
    while ((lrle = (rmtle *)llist->get()) != 0) {
	delete lrle->u.local.log;
	delete lrle;
    }
}

/*
 * BuildRemoteResLogIndexByHost()
 * Given a buffer of rlents with nmelements.
 * (This is the merged log from the coordinator)
 * Build the resolution log index for each host by vnode 
 */
void BuildRemoteResLogIndexByHost(rlent *buf, 
				  int nmelements, 
				  olist *hlist)
{
    long prevhost = 0;
    long prevvn = -1;
    long prevunique = -1;
    
    he *currentHE = NULL;
    rmtle *currentRMTLE = NULL;

    for (int i = 0; i < nmelements; i++) {
	rlent *rle = &(buf[i]);

	/* add new host to list */
	{
	    if (rle->serverid != prevhost) {
		prevhost = rle->serverid;
		currentHE = new he(rle->serverid);
		prevvn = -1;
		prevunique = -1;
		hlist->append(currentHE);
	    }
	}

	/* add new vnode to vnode list */
	{
	    if ((rle->dvnode != prevvn) || 
		(rle->dunique != prevunique)) {
		prevvn = rle->dvnode;
		prevunique = rle->dunique;
		currentRMTLE = new rmtle(prevvn, prevunique, 
					 0, rle);
		currentHE->vlist.append(currentRMTLE);
	    }
	}

	/* add latest rlent to current vnode log list */
	currentRMTLE->u.remote.nentries++;
    }
}

void PurgeRemoteResLogIndexByHost(olist *hlist) {
    he *tmpHE;
    while (tmpHE = (he *)(hlist->get())){
	olist *tmpvlist = &tmpHE->vlist;
	rmtle *tmpRMTLE;
	tmpRMTLE = (rmtle *)(tmpvlist->get());
	while (tmpRMTLE != 0) {
	    delete tmpRMTLE;
	    tmpRMTLE = (rmtle *)(tmpvlist->get());
	}
	delete tmpHE;
    }
}


	    

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







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include "coda_assert.h"
#include <stdio.h>
#include <rpc2.h>
#include <util.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <errors.h>
#include <srv.h>
#include <codadir.h>
#include <res.h>
#include <vrdb.h>
#include <lockqueue.h>

#include "rescomm.h"
#include "resutil.h"

he *FindHE(olist *list, long hostaddress) {
    olist_iterator next(*list);
    he *h;
    while ( (h = (he *)next())) {
	if (h->hid == hostaddress)
	    return(h);
    }
    return(0);
}

long RS_NewConnection(RPC2_Handle RPCid, RPC2_Integer set, 
		      RPC2_Integer sl, RPC2_Integer et, 
		      RPC2_Integer at, RPC2_CountedBS *cid)
{
    conninfo *cip = new conninfo(RPCid, sl);
    conninfo::CInfoTab->insert(&cip->tblhandle);
    conninfo::ncinfos++;
    return(0);
}

/* Mark an object inconsistent */
long RS_MarkInc(RPC2_Handle RPCid, ViceFid *Fid) 
{
    Volume *volptr = 0;
    Vnode *vptr = 0;
    int errorcode = 0;
    rvm_return_t status = RVM_SUCCESS;
    
    SLog(9,  "ResMarkInc: Fid = (%x.%x.%x)", FID_(Fid));
    /* Fetch the object and mark inconsistent */
    if (!XlateVid(&Fid->Volume)){
	SLog(0,  "ResMarkInc: XlateVid(%x) failed", Fid->Volume);
	errorcode = EINVAL;
	goto FreeLocks;
    }
    SLog(9,  "ResMarkInc: Going to Fetch Object");
    errorcode = GetFsObj((ViceFid*)Fid, &volptr, &vptr, 
			 WRITE_LOCK, NO_LOCK, 0, 0, 0);
    if (errorcode) goto FreeLocks;
    /* Should be checking if the volume is locked by the coordinator */
    /* Force Inconsistency for now */
    SLog(9,  "ResMarkInc: Marking object inconsistent");
    SetIncon(vptr->disk.versionvector);

FreeLocks:
    rvmlib_begin_transaction(restore);
    SLog(9,  "ResMarkInc: Putting back vnode and volume");
    if (vptr){
	Error fileCode = 0;
	VPutVnode(&fileCode, vptr);
	CODA_ASSERT(fileCode == 0);
    }
    PutVolObj(&volptr, NO_LOCK);
    rvmlib_end_transaction(flush, &(status));
    SLog(2,  "ResMarkInc returns code %d", errorcode);
    return(errorcode);
}

extern unsigned long VMCounter;
extern unsigned long startuptime;

void AllocStoreId(ViceStoreId *s) {
    s->Host = ThisHostAddr;
    s->Uniquifier = startuptime + VMCounter;
    VMCounter++;
}

/* check for return codes other than time outs */
long CheckRetCodes(unsigned long *rc, unsigned long *rh, 
		    unsigned long *hosts) {
    struct in_addr addr;
    long error = 0;
    for (int i = 0; i < VSG_MEMBERS; i++){
	hosts[i] = rh[i];
	if (rc[i] == ETIMEDOUT) hosts[i] = 0;
	if (rh[i] && rc[i] && rc[i] != ETIMEDOUT){
	    /* non rpc error - drop this host too */
	    hosts[i] = 0;
	    error = rc[i];
	    addr.s_addr = htonl(rh[i]);
	    SLog(0,  "CheckRetCodes - an accessible server returned an error (server %s error %d)",
		   inet_ntoa(addr), rc[i]);
	}
    }
    return(error);
}

/* check for return codes for resolution other than timeouts:

   - hosts will hold the ip address of hosts that responded
   - result will be VNOVNODE if that was the only error
     returned by the available servers
   - result will hold the first error encountered if not VNOVNODE
*/

long CheckResRetCodes(unsigned long *rc, unsigned long *rh, 
		      unsigned long *hosts) 
{
    struct in_addr addr;
    long error = 0, result = 0;

    for (int i = 0; i < VSG_MEMBERS; i++) {
	    hosts[i] = rh[i];
	    if (rc[i] == ETIMEDOUT) 
		    hosts[i] = 0;
	    if (rh[i] && rc[i] && rc[i] != ETIMEDOUT) {
		    /* non rpc error - drop this host too
		     * except if it has a runt. */
		    if (rc[i] != VNOVNODE) hosts[i] = 0;
		    error = rc[i];
		    addr.s_addr = ntohl(rh[i]);
		    SLog(0,  "CheckRetCodes: server %s returned error %d",
			 inet_ntoa(addr), rc[i]);
	    }
	    if ( result == 0 || result == VNOVNODE )
		    result = error;
    }
    return(result);
}

/**************** ilink class functions *******************/
/* implmentation of functions that parse the list of inconsistencies 
 * at end of phase 1 during resolution and create a unique entry list
 */
ilink *AddILE(dlist &dl, char *name, long vnode, long unique, long pvnode,
	      long punique, long type) {
    ilink *il = new ilink(name, vnode, unique, pvnode, punique, type);
    if (dl.IsMember(il)) {
	delete il;
	il = NULL;
    }
    else {
	SLog(39,  "AddILE: Adding new inc entry to dlist:");
	SLog(39,  "%s (%x.%x) child of (%x.%x),  type = %d",
		name, vnode, unique, pvnode, punique, type);
	dl.insert(il);	
    }
    return(il);
}
void BSToDlist(RPC2_BoundedBS *BS, dlist *dl) {

    char *beginc = (char *)BS->SeqBody;
    char *endc = ((char *)BS->SeqBody) + BS->SeqLen;
    
    while (beginc < endc) {
	char *name;
	long vnode, unique;
	long pvnode, punique;
	long type;
	ParseIncBSEntry(&beginc, &name, &vnode, &unique, &pvnode, &punique,&type);
	AddILE(*dl, name, vnode, unique, pvnode, punique, type);
    }
}

void DlistToBS(dlist *dl, RPC2_BoundedBS *BS) {
    BS->SeqLen = 0;
    ilink *il = 0;
    dlist_iterator next(*dl, DlAscending);
    while ((il = (ilink *)next())) {
	ViceFid Fid;
	ViceFid pFid;
	Fid.Volume = 0;
	Fid.Vnode = il->vnode;
	Fid.Unique = il->unique;
	pFid.Vnode = il->pvnode;
	pFid.Unique = il->punique;
	AllocIncBSEntry(BS, il->name, &Fid, &pFid, il->type);
    }
}
void CleanIncList(dlist *inclist) {
    ilink *il;
    if (inclist) {
	while ((il = (ilink *)inclist->get())) 
	    delete il;
	delete inclist;
    }
}

void ParseIncBSEntry(char **c, char **name, long *vn, long *unique, 
		     long *pvn, long *punique, long *type) {
    long *l = (long *)(*c);
    *c += sizeof(long);
    *vn = ntohl(*l);
    l = (long *)(*c);
    *unique = ntohl(*l);
    *c += sizeof(long);
    l = (long *)(*c);
    *pvn = ntohl(*l);
    *c += sizeof(long);
    l = (long *)(*c);
    *punique = ntohl(*l);
    *c += sizeof(long);
    l = (long *)*c;
    *type = ntohl(*l);
    *c += sizeof(long);
    *name = *c;
    int namelength = strlen(*c) + 1;
    int modlength = 0;
    if ((modlength = (namelength % sizeof(long)))) 
	namelength += sizeof(long) - modlength;
    *c += namelength;
}
/* AllocIncBSEntry
 *	Allocates the Piggy Back Entry if there is enough space in buffer 
 */
void AllocIncBSEntry(RPC2_BoundedBS *bbs, char *name, ViceFid *Fid, 
		     ViceFid *pFid, long type) {
    
    SLog(39,  "Allocating inc entry for:");
    SLog(39,  "Entry: %s (%x.%x) child of %x.%x type %d",
	    name, Fid->Vnode, Fid->Unique, pFid->Vnode, pFid->Unique, type);
    char *c = ((char *)bbs->SeqBody) + bbs->SeqLen;
    int namelength = strlen(name) + 1;
    int modlength = 0;
    if ((modlength = (namelength % sizeof(long)))) 
	namelength += sizeof(long) - modlength;
    if ((int)(bbs->SeqLen + namelength + SIZEOF_INCFID) > bbs->MaxSeqLen) {
	SLog(0,  "AllocIncPBEntry: NO MORE SPACE IN BUFFER");
	CODA_ASSERT(0);
    }
    long *l = (long *)c;
    *l = htonl(Fid->Vnode);
    l++;
    *l = htonl(Fid->Unique);
    l++;
    *l = htonl(pFid->Vnode);
    l++;
    *l = htonl(pFid->Unique);
    l++;
    *l = htonl(type);
    l++;
    c = (char *)l;
    strcpy(c, name);
    bbs->SeqLen += namelength + SIZEOF_INCFID;
}

int CompareIlinkEntry(ilink *i, ilink *j) {
    if (i->vnode < j->vnode) return(-1);
    if (i->vnode > j->vnode) return(1);
    if (i->unique < j->unique) return(-1);
    if (i->unique > j->unique) return(1);
    if (i->pvnode < j->pvnode) return(-1);
    if (i->pvnode > j->pvnode) return(1);
    if (i->punique < j->punique) return(-1);
    if (i->punique > j->punique) return(1);
    return(strcmp(i->name, j->name));
}

/* ************ respath functions************* */
long GetPath(ViceFid *fid, int maxcomponents, 
	     int *ncomponents, ResPathElem *components) {
    SLog(2, "Entering GetPath for 0x%x.%x\n", 
	   fid->Vnode, fid->Unique);
    olist plist;	/* list of parent fids */
    long errorcode = 0;
    Volume *volptr = 0;
    Vnode *vptr = 0;
    
    /* assume that volume is already locked exclusively */
    CODA_ASSERT(GetVolObj(fid->Volume, &volptr, VOL_NO_LOCK, 0, 0) == 0);
    
    ViceFid tmpfid;
    tmpfid.Volume = fid->Volume;
    tmpfid.Vnode = fid->Vnode;
    tmpfid.Unique = fid->Unique;
    
    /* get status of all the objects on path from volume root */
    while (!errorcode && tmpfid.Vnode != 0 && tmpfid.Unique != 0) {
	if ((errorcode = GetFsObj(&tmpfid, &volptr, &vptr, 
				 READ_LOCK, VOL_NO_LOCK, 1, 0, 0)))
	    break;
	
	ResStatus rs;
	ObtainResStatus(&rs, &(vptr->disk));
	respath *r = new respath(tmpfid.Vnode, tmpfid.Unique, 
				 &vptr->disk.versionvector, &rs);
	plist.insert(r);
	
	tmpfid.Vnode = vptr->disk.vparent;
	tmpfid.Unique = vptr->disk.uparent;
	
	if (vptr) {
	    Error fileCode = 0;
	    VPutVnode(&fileCode, vptr);
	    CODA_ASSERT(fileCode == 0);
	    vptr = 0;
	}
    }
    /* put all the entries into an array */
    if (!errorcode) {
	*ncomponents = plist.count();
	if (*ncomponents >= maxcomponents) {
	    SLog(0,
		   "GetPath: Too many levels in tree - need bigger array\n");
	    *ncomponents = 0;
	    errorcode = ENOSPC;
	}
	else 
	    SLog(2, 
		   "GetPath: going to compress %d entries\n",
		   *ncomponents);
	bzero((void *)components, sizeof(ResPathElem) * *ncomponents);
	for (int i = 0; i < *ncomponents; i++) {
	    respath *r = (respath *)plist.get();
	    CODA_ASSERT(r);
	    components[i].vn = r->vnode;
	    components[i].un = r->unique;
	    components[i].vv = r->vv;
	    components[i].st = r->st;
	    SLog(2,
		   "GetPath: Entry %d 0x%x.%x storeid 0x%x.%x\n",
		   i, components[i].vn, components[i].un, 
		   components[i].vv.StoreId.Host, 
		   components[i].vv.StoreId.Uniquifier);
	    delete r;
	}
	CODA_ASSERT(plist.count() == 0);
    }
    
    if (vptr) {
	Error fileCode = 0;
	VPutVnode(&fileCode, vptr);
	CODA_ASSERT(fileCode == 0);
    }
    if (volptr) 
	PutVolObj(&volptr, VOL_NO_LOCK);
    
    SLog(2, 
	   "GetPath returns %d\n", errorcode);
    return(errorcode);
}

// Only compare fid and VV 
static int CmpComponent(ResPathElem *a, ResPathElem *b) {
    if ((a->vn != b->vn) ||
	(a->un != b->un) ||
	(VV_Cmp(&a->vv, &b->vv) != VV_EQ))
	return(-1);
    return(0);
}

static int GetUnEqFid(int nreplicas, int *nentries, 
		       ResPathElem **paths, ViceFid *UnEqFid) {
    int BadIndex = -1;
    UnEqFid->Volume = 0;
    UnEqFid->Vnode = 0;
    UnEqFid->Unique = 0;
    for (int i = 0; i < nentries[0]; i++) {
	for (int j = 1; j < nreplicas; j++) {
	    CODA_ASSERT(nentries[j] > i);		// culprit must be found before end
	    if (CmpComponent(&paths[j][i], &paths[0][i])) {
		BadIndex = i;
		if ((paths[j][i].vn != paths[0][i].vn) || 
		    (paths[j][i].un != paths[0][i].un)) {
		    // There has been a rename deeper down in the tree
		    // For now just return error
		    return(-1);
		}
		UnEqFid->Vnode = paths[0][i].vn;
		UnEqFid->Unique = paths[0][i].un;
	    }
	}
	if (BadIndex != -1) break;
    }
    return(BadIndex);
}

static void GetUnEqVV(int *sizes, ResPathElem **paths, 
		       int BadIndex, ViceVersionVector **UnEqVV) {
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (sizes[i] && (sizes[i] > BadIndex)) 
	    UnEqVV[i] = &paths[i][BadIndex].vv;
	else UnEqVV[i] = NULL;
    }
}

static void GetUnEqResStatus(int *sizes, ResPathElem **paths, 
			      int BadIndex, ResStatus **rstatusp) {
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (sizes[i] && (sizes[i] > BadIndex)) 
	    rstatusp[i] = &paths[i][BadIndex].st;
	else rstatusp[i] = NULL;
    }
    
}

/* compare the path components of nreplicas
 * return 0 if they all match; -1 otherwise
 */
int ComparePath(int nreplicas, int *nentries, ResPathElem **paths) {
    for (int i = 1; i < nreplicas; i++) 
	if (nentries[i]  != nentries[0]) {
	    SLog(2, 
		   "ComparePath: nentries do not match at [0]=%d, [%d]=%d\n",
		   nentries[0], i, nentries[i]);
	    return(-1);
	}
    
    // compare all but the last component 
  { /* drop scope for int i below; to avoid identifier clash */
    for (int i = 1; i < nreplicas; i++) 
	for (int j = 0; j < nentries[i] - 1; j++) 
	    if (CmpComponent(&paths[i][j], &paths[0][j])) {
		SLog(0, 
		       "Component %d for %d and 0 do not match\n",
		       j, i);
		return(-1);
	    }
  } /* drop scope for int i above; to avoid identifier clash */

    // compare only the fid of the last component 
  { /* drop scope for int i below; to avoid identifier clash */
    int lastindex = nentries[0] - 1;
    for (int i = 1; i < nreplicas; i++) {
	if ((paths[0][lastindex].vn != paths[i][lastindex].vn) ||
	    (paths[0][lastindex].un != paths[i][lastindex].un)) {
	    SLog(0,
		   "Fids of last component for path %d do not match with path 0\n",
		   i);
	    return(-1);
	}
    }
  } /* drop scope for int i above; to avoid identifier clash */
    return(0);
}

int ComparePath(int *sizes, ResPathElem **paths, res_mgrpent *mgrp, int *depth, 
		ViceFid *UnEqFid, ViceVersionVector **UnEqVV, ResStatus **UnEqResStatus){
    ResPathElem *compresspaths[VSG_MEMBERS];
    int compresssizes[VSG_MEMBERS];
    int nreplicas = 0;
    int index = 0;
    *depth = -1;
    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (sizes[i] && !mgrp->rrcc.retcodes[i] && mgrp->rrcc.handles[i]) {
	    compresssizes[index] = sizes[i];
	    compresspaths[index] = paths[i];
	    if (*depth == -1) *depth = sizes[i];
	    index++;
	    nreplicas++;
	}
    if (!ComparePath(nreplicas, compresssizes, compresspaths)) 
	// all paths are equal 
	return(0);
    else {
	int uneqdepth = GetUnEqFid(nreplicas, compresssizes, compresspaths, UnEqFid);
	if (uneqdepth >= 0) {
	    GetUnEqVV(sizes, paths, uneqdepth, UnEqVV);
	    GetUnEqResStatus(sizes, paths, uneqdepth, UnEqResStatus);
	}
	*depth = uneqdepth;
	return(-1);
    }
}

void ObtainResStatus(ResStatus *status, VnodeDiskObjectStruct *vdop) {
    status->status = 0;
    status->Author = vdop->author;
    status->Owner = vdop->owner;
    status->Date = vdop->unixModifyTime;
    status->Mode = vdop->modeBits;
}

void GetResStatus(unsigned long *succflags, ResStatus **status_p, 
		  ViceStatus *finalstatus) {
    bzero((void *)finalstatus, sizeof(ViceStatus));
    int gotmbits = 0;
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (succflags[i]) {
	    if (status_p[i]->Owner) {
		// a genuine (non-resolve generated) vnode
		
		if (!finalstatus->Owner) {
		    finalstatus->Owner = status_p[i]->Owner;
		    finalstatus->Author = status_p[i]->Author;
		    finalstatus->Mode = status_p[i]->Mode;
		    gotmbits = 1;
		}
	    }
	    // get the latest time && mode bits if not set already 
	    if (status_p[i]->Date > finalstatus->Date) 
		finalstatus->Date = status_p[i]->Date;
	    if (!gotmbits && status_p[i]->Mode) 
		finalstatus->Mode = status_p[i]->Mode;
	}
    }
}

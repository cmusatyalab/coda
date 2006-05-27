/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "coda_string.h"

#include <netinet/in.h>
#include "coda_assert.h"
#include <stdio.h>
#include <rpc2/rpc2.h>
#include <rpc2/errors.h>
#include <util.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif

#include <srv.h>
#include <codadir.h>
#include <res.h>
#include <vrdb.h>
#include <volume.h>
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
long CheckRetCodes(RPC2_Integer *rc, unsigned long *rh, 
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

long CheckResRetCodes(RPC2_Integer *rc, unsigned long *rh, 
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
    if (bbs->SeqLen + namelength + SIZEOF_INCFID > bbs->MaxSeqLen) {
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

void ObtainResStatus(ResStatus *status, VnodeDiskObjectStruct *vdop)
{
    status->status = 0;
    status->Author = vdop->author;
    status->Owner = vdop->owner;
    status->Date = vdop->unixModifyTime;
    status->Mode = vdop->modeBits;
}

void GetResStatus(unsigned long *succflags, ResStatus **status_p, 
		  ViceStatus *finalstatus)
{
    memset((void *)finalstatus, 0, sizeof(ViceStatus));
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

/* Combine the directory contents and the ACL in a single blob.
 * Used during resolution to get the directory and ACL contents for
 * RS_FetchDirContents and RS_InstallVV. Don't forget to `free(buf)'! */
void *Dir_n_ACL(struct Vnode *vptr, int *size)
{
	char	      *buf, *ptr;
	PDirHandle     dh;
	AL_AccessList *acl;
	int            dirsize, aclsize, quotasize = 0;

	dh      = VN_SetDirHandle(vptr);
	dirsize = DH_Length(dh);

	acl     = VVnodeACL(vptr);
	aclsize = VAclSize(vptr);

	if (vptr->vnodeNumber == 1 && vptr->disk.uniquifier == 1)
	    quotasize = 2 * sizeof(int);

	ptr = buf = (char *)malloc(dirsize + aclsize + quotasize);
	if (!buf) {
	    *size = 0;
	    return NULL;
	}

	/* copy directory data */
	memcpy(ptr, DH_Data(dh), dirsize);
	ptr += dirsize;

	VN_PutDirHandle(vptr);

	/* copy acl */
	memset(ptr, 0, aclsize);
	memcpy(ptr, acl, acl->MySize);
	ptr += aclsize;
	SLog(9,"Dir_n_ACL: acl->MySize = %d acl->TotalNoOfEntries = %d\n",
	     acl->MySize, acl->TotalNoOfEntries);

	/* copy volume quotas */
	if (quotasize) {
	    *((int *)ptr) = htonl(V_maxquota(vptr->volumePtr));
	    ptr += sizeof(int);
	    *((int *)ptr) = htonl(V_minquota(vptr->volumePtr));
	}

	*size = dirsize + aclsize + quotasize;

	return (void *)buf;
}


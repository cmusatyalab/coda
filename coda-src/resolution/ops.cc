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

#include <stdio.h>
#include <strings.h>
#include <prs.h>
#include <al.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <prs.h>
#include <al.h>
#include <srv.h>
#include <volume.h>
#include <vlist.h>
#include <resutil.h>
#include <recov_vollog.h>
#include "rsle.h"
#include "recle.h"
#include "ops.h"
#include "resstats.h"

void aclstore::init(char *a) {
    type = ACLSTORE;
    bcopy(a, acl, SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE);
}

void aclstore::print(int fd) {
    char buf[512];
    AL_ExternalAccessList ea;
    
    if (AL_Externalize((AL_AccessList *)acl, &ea) != 0)
	sprintf(buf, "    stType = ACL: Couldnt translate access list\n");
    else {
	sprintf(buf, "    stType = ACL: %s", ea);
	AL_FreeExternalAlist(&ea);
    }
    write(fd, buf, (int)strlen(buf));
}

void ststore::init(UserId o, RPC2_Unsigned m, 
		   UserId a, Date_t d, ViceVersionVector *v) {
    type = STSTORE;
    owner = o;
    mode = m;
    author = a;
    mtime = d;
    if (v)
	vv = *v;
    else 
	vv = NullVV;
}

void ststore::print(int fd) {
    char buf[512];
    SLog(1, 
	   "ststore:print Owner %u, Mode %u\n",
	   owner, mode);
    sprintf(buf, "    stType = status; Owner: %u Mode %u Author %u Date %u \n", 
		    owner, mode, author, mtime);
    write(fd, buf, (int)strlen(buf));
    FILE *fp = fdopen(fd, "w");
    if (fp) {
	SLog(1,
	       "ststore:print going to print vv\n");
	PrintVV(fp, &vv);
    }
    else 
	SLog(0,
	       "ststore::print Couldnt fdopen file\n");
}

void newstore::init(UserId o, RPC2_Unsigned m, UserId a, Date_t d, 
		    RPC2_Integer mk, ViceVersionVector *v) {
    type = STSTORE;
    owner = o;
    mode = m;
    author = a;
    mtime = d;
    mask = mk;
    if (v)
	vv = *v;
    else 
	vv = NullVV;
}

void newstore::print(int fd) {
    char buf[512];
    SLog(1, 
	   "newstore:print Owner %u, Mode %u\n",
	   owner, mode);
    sprintf(buf, "    newstore Owner: %u Mode %u Author %u Date %u Mask %o \n",
		    owner, mode, author, mtime, mask);
    write(fd, buf, (int)strlen(buf));
    FILE *fp = fdopen(fd, "w");
    if (fp) {
	SLog(1,
	       "newstore:print going to print vv\n");
	PrintVV(fp, &vv);
    }
    else 
	SLog(0,
	       "ststore::print Couldnt fdopen file\n");
}

void create_rle::init(VnodeId v, Unique_t u, UserId o, char *s) {
    cvnode = v;
    cunique = u;
    owner = o;
    strcpy(name, s);
}

void create_rle::print(int fd) {
    char buf[512];
    SLog(1, 
	   "create_rle::print name %s, vn 0x%x.%x\n",
	   name, cvnode, cunique);
    sprintf(buf, "    %s [0x%x.%x] owner %u\n", &name[0], cvnode, cunique, owner);
    write(fd, buf, (int)strlen(buf));
}

void symlink_rle::init(VnodeId v, Unique_t u, UserId o, char *s) {
    cvnode = v;
    cunique = u;
    owner = o;
    strcpy(name, s);
}

void symlink_rle::print(int fd) {
    char buf[512];
    SLog(1, 
	   "symlink_rle::print name %s 0x%x.%x\n",
	   name, cvnode, cunique);
    sprintf(buf, "    %s [0x%x.%x] owner %u\n", &name[0], cvnode, cunique, owner);
    write(fd, buf, (int)strlen(buf));
}


void link_rle::init(VnodeId v, Unique_t u, ViceVersionVector *vv, char *s) {
    cvnode = v;
    cunique = u;
    cvv = *vv;
    strcpy(&name[0], s);
}

void link_rle::print(int fd) {
    char buf[512];
    SLog(1, 
	   "link_rle::print name %s vnode 0x%x.%x\n",
	   name, cvnode, cunique);
    sprintf(buf, "    %s [0x%x.%x][%d %d %d %d %d %d %d %d (%x.%x)(0x%x)]\n",
	    &name[0], cvnode, cunique, 
	    cvv.Versions.Site0, cvv.Versions.Site1, 
	    cvv.Versions.Site2, cvv.Versions.Site3, 
	    cvv.Versions.Site4, cvv.Versions.Site5,
	    cvv.Versions.Site6, cvv.Versions.Site7, 
	    cvv.StoreId.Host, cvv.StoreId.Uniquifier, 
	    cvv.Flags);
    write(fd, buf, (int)strlen(buf));
}


void mkdir_rle::init(VnodeId v, Unique_t u, UserId o, char *s) {
    cvnode = v;
    cunique = u;
    owner = o;
    strcpy(name, s);
}

void mkdir_rle::print(int fd) {
    char buf[512];
    SLog(1, 
	   "mkdir_rle:print name is %s, vn = %d, unique = %d\n",
	   name, cvnode, cunique);
    sprintf(buf, "    %s [0x%x.%x] owner %u\n", &name[0], cvnode, cunique, owner);
    write(fd, buf, (int)strlen(buf));
}

void rm_rle::init(VnodeId v, Unique_t u, ViceVersionVector *vv, char *s) {
    cvnode = v;
    cunique = u;
    cvv = *vv;
    strcpy(name, s);
}

void rm_rle::print(int fd) {
    char buf[512];
    SLog(1, 
	   "rm_rle::print name %s vnode 0x%x.%x\n",
	   name, cvnode, cunique);
    sprintf(buf, "    %s [0x%x.%x][%d %d %d %d %d %d %d %d (%x.%x)(0x%x)]\n",
	    &name[0], cvnode, cunique, 
	    cvv.Versions.Site0, cvv.Versions.Site1, 
	    cvv.Versions.Site2, cvv.Versions.Site3, 
	    cvv.Versions.Site4, cvv.Versions.Site5,
	    cvv.Versions.Site6, cvv.Versions.Site7, 
	    cvv.StoreId.Host, cvv.StoreId.Uniquifier, 
	    cvv.Flags);
    write(fd, buf, (int)strlen(buf));
}

void rmdir_rle::init(VnodeId v, Unique_t u, rec_dlist *rdl, ViceStoreId *lcp, 
		     ViceStoreId *sid, char *s) {
    cvnode = v;
    cunique = u;
    childlist = rdl;
    childLCP = *lcp;
    csid = *sid;
    strcpy(name, s);
}

void rmdir_rle::print(int fd) {
    char buf[512];
    SLog(1, 
	   "rmdir_rle::print name %s vnode 0x%x.%x\n",
	   name, cvnode, cunique);
    sprintf(buf, "    %s [0x%x.%x] del storeid [0x%x.%x]\n", 
	    &name[0], cvnode, cunique, csid.Host, csid.Uniquifier);
    write(fd, buf, (int)strlen(buf));
}

void rename_rle::init(unsigned short srctgt, VnodeId odv, Unique_t odu, VnodeId cv, Unique_t cu, 
		      ViceVersionVector *srcvv, char *old, char *newstr, 
		      VnodeId tv, Unique_t tu, ViceVersionVector *tgtvv, rec_dlist *list) {
    type = srctgt;
    otherdirv = odv; otherdiru = odu;
    svnode = cv; sunique = cu; svv = *srcvv;
    strcpy(&oldname[0], old);
    newname_offset = strlen(old) + 1;
    strcpy(&oldname[0] + newname_offset, newstr);
    if (tv) {
	tvnode = tv;
	tunique = tu;
	
	tvv = tgtvv ? *tgtvv : NullVV;
	tlist = list;
    }
    else {
	tvnode = 0;
	tunique = 0;
	tvv = NullVV;
	tlist = NULL;
    }
}

void rename_rle::print(int fd) {
    char buf[512];
    SLog(1, 
	   "name %s dir 0x%x.%x\n",
	   oldname, otherdirv, otherdiru);
    sprintf(buf, "    %s other dir (0x%x.%x) %s (0x%x.%x)[%d %d %d %d %d %d %d %d 0x%x.%x 0x%x]\n renamed to %s\n",
	    type == SOURCE ? "(src)" : "(target)",
	    otherdirv, otherdiru, oldname, svnode, sunique, 
	    svv.Versions.Site0, svv.Versions.Site1, 
	    svv.Versions.Site2, svv.Versions.Site3, 
	    svv.Versions.Site4, svv.Versions.Site5,
	    svv.Versions.Site6, svv.Versions.Site7, 
	    svv.StoreId.Host, svv.StoreId.Uniquifier, 
	    svv.Flags, (char *)oldname + newname_offset);
    write(fd, buf, (int)strlen(buf));
    if (tvnode && tunique) {
	sprintf(buf, "    Deleted target: 0x%x.%x [%d %d %d %d %d %d %d %d 0x%x.%x 0x%x]\n", 
		tvnode, tunique, 
		tvv.Versions.Site0, tvv.Versions.Site1, 
		tvv.Versions.Site2, tvv.Versions.Site3, 
		tvv.Versions.Site4, tvv.Versions.Site5,
		tvv.Versions.Site6, tvv.Versions.Site7, 
		tvv.StoreId.Host, tvv.StoreId.Uniquifier, 
		tvv.Flags);
	write(fd, buf, (int)strlen(buf));
    }
}


void setquota_rle::init(int oquota, int nquota) {
    SLog(0,
           "setquota_rle::init quota changed from %d to %d\n",
	   oquota, nquota);
    oldquota = oquota;
    newquota = nquota;
}


void setquota_rle::print(int fd) {
    char buf[512];

    SLog(1,
           "setquota_rle::print quota changed from %d to %d\n",
	   oldquota, newquota);

    sprintf(buf, "    quota changed from %d to %d\n", oldquota, newquota);
    write(fd, buf, (int)strlen(buf));
}



/* Create the log list header for the root directory vnode and 
   spool the "mkdir ."  log record */
/* called from within the xaction */
void CreateRootLog(Volume *vol, Vnode *vptr) {
    int index = -1;
    int seqno = -1;

    /* allocate the record */
    CODA_ASSERT((V_VolLog(vol)->AllocRecord(&index, &seqno)) == 0);
    
    ViceStoreId stid;
    stid.Host = Vnode_vv(vptr).StoreId.Host;
    stid.Uniquifier  = Vnode_vv(vptr).StoreId.Uniquifier;
    rsle sl(&stid, vptr->vnodeNumber, vptr->disk.uniquifier, 
	    ViceMakeDir_OP, index, seqno);
    
    sl.init(ViceMakeDir_OP, ".", vptr->vnodeNumber, vptr->disk.uniquifier);
    recle *rle = V_VolLog(vol)->RecovPutRecord(index);
    CODA_ASSERT(rle);
    
    /* initialize the log list header */
    CODA_ASSERT(VnLog(vptr) == NULL);
    VnLog(vptr) = new rec_dlist();
    CODA_ASSERT(VnLog(vptr));

    /* copy the log record into rvm */
    rle->InitFromsle(&sl);
    VnLog(vptr)->append(rle);

    // RESSTATS
    Lsize(*(V_VolLog(vol)->vmrstats)).chgsize(rle->size + sizeof(recle));
    VarlHisto(*(V_VolLog(vol)->vmrstats)).countalloc(rle->size);
}

void CreateResLog(Volume *vol, Vnode *vptr) {
    int index = -1;
    int seqno = -1;

    if (VnLog(vptr)) return;
    CODA_ASSERT(V_VolLog(vol));

    /* initialize the log list header */
    VnLog(vptr) = new rec_dlist();
    CODA_ASSERT(VnLog(vptr));
}

/*
  SpoolVMLogRecord: Create a log record in VM and reserve a slot
  for it in recoverable storage.
  Note that this routine is overloaded.
 */

int SpoolVMLogRecord(vle *v, Volume *vol, ViceStoreId *stid, int op, va_list ap) 
{
    CODA_ASSERT(v);
    SLog(9,  "Entering SpoolVMLogRecord_vle(0x%x.%x.%x)",
	    V_id(vol), v->vptr->vnodeNumber, v->vptr->disk.uniquifier);

    int errorcode = 0;
    int index = -1;
    int seqno = -1;

    /* reserve a slot for the record in volume log */
    if (V_VolLog(vol)->AllocRecord(&index, &seqno)) {
	SLog(0, "SpoolVMLogRecord - no space left in volume return ENOSPC\n");
	return(ENOSPC);
    }

    /* form the log record in vm */
    rsle *rsl = new rsle(stid, v->vptr->vnodeNumber, v->vptr->disk.uniquifier, 
			 op, index, seqno);
    CODA_ASSERT(rsl);
    rsl->init(op, ap);

    /* append record to intention list  */
    v->rsl.append(rsl);
    SLog(9,  "Leaving SpoolVMLogRecord_vle() - returns SUCCESS\n");
    return(0);
}

int SpoolVMLogRecord(vle *v, Volume *vol, ViceStoreId *stid, int op ...) 
{
    SLog(9,  "Entering SpoolVMLogRecord_vle(0x%x.%x.%x)",
	    V_id(vol), v->vptr->vnodeNumber, v->vptr->disk.uniquifier);

    va_list ap;
    va_start(ap, op);
    int errorCode = SpoolVMLogRecord(v, vol, stid, op, ap);
    va_end(ap);

    SLog(9, "Leaving SpoolVMLogRecord() - returns %d\n", errorCode);
    return(errorCode);
}

int SpoolVMLogRecord(dlist *vlist, Vnode *vptr, Volume *vol, ViceStoreId *stid, 
		     int op ...) 
{
    SLog(9,  "Entering SpoolVMLogRecord_dlist(0x%x.%x.%x) ",
	    V_id(vol), vptr->vnodeNumber, vptr->disk.uniquifier);

    va_list ap;
    va_start(ap, op);
    int errorCode = SpoolVMLogRecord(vlist, vptr, vol, stid, op, ap);
    va_end(ap);

    SLog(9, "Leaving SpoolVMLogRecord() - returns %d\n", errorCode);
    return(errorCode);
}

int SpoolVMLogRecord(dlist *vlist, vle *v,  Volume *vol, ViceStoreId *stid, 
		     int op ...) 
{
    SLog(9,  "Entering SpoolVMLogRecord_dlist(0x%x.%x.%x) ",
	    V_id(vol), v->vptr->vnodeNumber, v->vptr->disk.uniquifier);

    va_list ap;
    va_start(ap, op);
    int errorCode = SpoolVMLogRecord(vlist, v, vol, stid, op, ap);
    va_end(ap);

    SLog(9,  "Leaving SpoolVMLogRecord() - returns %d\n", errorCode);
    return(errorCode);
}

int SpoolVMLogRecord(dlist *vlist, Vnode *vptr, Volume *vol, ViceStoreId *stid, 
		     int op, va_list ap) 
{
    ViceFid fid;
    FormFid(fid, V_id(vol), vptr->vnodeNumber, vptr->disk.uniquifier);
    vle *v = FindVLE(*vlist, &fid);
    if (!v) {
	SLog(0, "SpoolVMLogRecord_dlist - no vle found for %s\n",
	       FID_(&fid));
	return(EINVAL);
    }
    return(SpoolVMLogRecord(vlist, v, vol, stid, op, ap));
}

int SpoolVMLogRecord(dlist *vlist, vle *v, Volume *vol, ViceStoreId *stid, 
		     int op, va_list ap) 
{
    CODA_ASSERT(v);
    SLog(9, "Entering SpoolVMLogRecord_vle(0x%x.%x.%x)",
	 V_id(vol), v->vptr->vnodeNumber, v->vptr->disk.uniquifier);

    int errorcode = 0;
    int index = -1;
    int seqno = -1;

    /* reserve a slot for the record in volume log */
    if (V_VolLog(vol)->AllocRecord(&index, &seqno)) {
	if (V_VolLog(vol)->AllocViaWrapAround(&index, &seqno, vol, vlist)) {
	    SLog(0, "SpoolVMLogRecord - no space left in volume");
	    SLog(0, "- returns ENOSPC\n");
	    return(ENOSPC);
	}
    }

    /* form the log record in vm */
    rsle *rsl = new rsle(stid, v->vptr->vnodeNumber, 
			 v->vptr->disk.uniquifier, op, index, seqno);
    CODA_ASSERT(rsl);
    rsl->init(op, ap);
    //append record to intention list 
    v->rsl.append(rsl);
    SLog(9,  "Leaving SpoolVMLogRecord_vle() - returns SUCCESS\n");
    return(0);
}

int SpoolRenameLogRecord(int opcode, dlist *vl, Vnode *svnode, Vnode *tvnode, 
			 Vnode *sdvnode, Vnode *tdvnode, 
			 Volume *volptr, char *OldName, 
			 char *NewName, ViceStoreId *StoreId) {
    int SameParent = (sdvnode == tdvnode);
    int errorCode = 0;
    if (tvnode) {
	/* target exists and is deleted */
	if (tvnode->disk.type == vDirectory) {
	    /* directory deletion - attach dir's log to parent */
	    if (SameParent) {
		if (errorCode = SpoolVMLogRecord(vl, sdvnode, volptr, StoreId,
						 opcode, SOURCE, 
						 OldName, NewName, 
						 (VnodeId)tdvnode->vnodeNumber, (Unique_t)tdvnode->disk.uniquifier, 
						 (VnodeId)svnode->vnodeNumber, (Unique_t)svnode->disk.uniquifier, 
						 &(Vnode_vv(svnode)), 1 /* target exists */,
						 (VnodeId)tvnode->vnodeNumber, (Unique_t)tvnode->disk.uniquifier, 
						 &(Vnode_vv(tvnode)), VnLog(tvnode))) {
		    SLog(0, 
			   "SpoolRenameLogRecord: Error %d in SpoolVMLogRecord\n", 
			   errorCode);
		    return(errorCode);
		}
	    }
	    else {
		if (errorCode = SpoolVMLogRecord(vl, sdvnode, volptr, StoreId,
						 opcode, SOURCE, 
						 OldName, NewName, 
						 (VnodeId)tdvnode->vnodeNumber, (Unique_t)tdvnode->disk.uniquifier, 
						 (VnodeId)svnode->vnodeNumber, (Unique_t)svnode->disk.uniquifier, 
						 &(Vnode_vv(svnode)), 1 /* target exists */,
						 (VnodeId)tvnode->vnodeNumber, (Unique_t)tvnode->disk.uniquifier, 
						 &(Vnode_vv(tvnode)), NULL/* attach deleted objects log to its parent*/)) {
		    SLog(0, 
			   "SpoolRenameLogRecord: Error %d in SpoolVMLogRecord\n", 
			   errorCode);
		    return(errorCode);
		}

		if (errorCode = SpoolVMLogRecord(vl, tdvnode, volptr, StoreId,
						 opcode, TARGET, 
						 OldName, NewName, 
						 (VnodeId)sdvnode->vnodeNumber, (Unique_t)sdvnode->disk.uniquifier, 
						 (VnodeId)svnode->vnodeNumber, (Unique_t)svnode->disk.uniquifier, 
						 &(Vnode_vv(svnode)), 1 /* target exists */,
						 (VnodeId)tvnode->vnodeNumber, (Unique_t)tvnode->disk.uniquifier, 
						 &(Vnode_vv(tvnode)), VnLog(tvnode))) {
		    SLog(0, 
			   "SpoolRenameLogRecord: Error %d in SpoolVMLogRecord\n", 
			   errorCode);
		    return(errorCode);
		}
	    }
	}
	else {
	    /* target is a file */
	    if (errorCode = SpoolVMLogRecord(vl, sdvnode, volptr, StoreId,
					     opcode, SOURCE, 
					     OldName, NewName, 
					     (VnodeId)tdvnode->vnodeNumber, (Unique_t)tdvnode->disk.uniquifier, 
					     (VnodeId)svnode->vnodeNumber, (Unique_t)svnode->disk.uniquifier, 
					     &(Vnode_vv(svnode)), 1 /* target exists */,
					     (VnodeId)tvnode->vnodeNumber, (Unique_t)tvnode->disk.uniquifier, 
					     &(Vnode_vv(tvnode)), NULL)) {
		SLog(0, 
		       "SpoolRenameLogRecord: Error %d in SpoolVMLogRecord\n", 
		       errorCode);
		return(errorCode);
	    }
	    if (!SameParent) {
		if (errorCode = SpoolVMLogRecord(vl, tdvnode, volptr, StoreId,
						 opcode, TARGET, 
						 OldName, NewName, 
						 (VnodeId)sdvnode->vnodeNumber, (Unique_t)sdvnode->disk.uniquifier, 
						 (VnodeId)svnode->vnodeNumber, (Unique_t)svnode->disk.uniquifier, 
						 &(Vnode_vv(svnode)), 1 /* target exists */,
						 (VnodeId)tvnode->vnodeNumber, (Unique_t)tvnode->disk.uniquifier, 
						 &(Vnode_vv(tvnode)), NULL)) {
		    SLog(0, 
			   "SpoolRenameLogRecord: Error %d in SpoolVMLogRecord\n", 
			   errorCode);
		    return(errorCode);
		}
	    }
	}
    }
    else {
	/* no target existed */
	if (errorCode = SpoolVMLogRecord(vl, sdvnode, volptr, StoreId,
					 opcode, SOURCE, 
					 OldName, NewName, 
					 (VnodeId)tdvnode->vnodeNumber, (Unique_t)tdvnode->disk.uniquifier, 
					 (VnodeId)svnode->vnodeNumber, (Unique_t)svnode->disk.uniquifier, 
					 &(Vnode_vv(svnode)), 0 /* target does not exist */,
					 0, 0, NULL, NULL)) {
	    SLog(0, 
		   "SpoolRenameLogRecord: Error %d in SpoolVMLogRecord\n", 
		   errorCode);
	    return(errorCode);
	}
	if (!SameParent) {
	    if (errorCode = SpoolVMLogRecord(vl, tdvnode, volptr, StoreId,
					     opcode, TARGET, 
					     OldName, NewName, 
					     (VnodeId)sdvnode->vnodeNumber, (Unique_t)sdvnode->disk.uniquifier, 
					     (VnodeId)svnode->vnodeNumber, (Unique_t)svnode->disk.uniquifier, 
					     &(Vnode_vv(svnode)), 0 /* target does not exist */,
					     0, 0, NULL, NULL)) {
		SLog(0, 
		       "SpoolRenameLogRecord: Error %d in SpoolVMLogRecord\n", 
		       errorCode);
		return(errorCode);
	    }
	}
    }
    return(errorCode);
}

/* called from within a transaction */
/* return list of indices (ind parameter below) freed from volume log 
   so that the vm bitmap can also be modified if xaction succeeds */
void TruncateLog(Volume *vol, Vnode *vptr, vmindex *ind) 
{
    SLog(9, "Entering TruncRVMLog (0x%x.%x.%x)\n",
	 V_id(vol), vptr->vnodeNumber, vptr->disk.uniquifier);
	   
    rec_dlist *log = VnLog(vptr);
    /* number of entries that will be freed */
    int count = log->count() - 1;	
    if (count > 0) 
	for (int i= 0; i < count; i++) {
	    /* remove entry from list */
	    recle *le = (recle *)log->get();	

	    rec_dlist *childlog;
	    if (childlog = le->HasList()) 
		PurgeLog(childlog, vol, ind);

	    // RESSTATS
	    VarlHisto(*(V_VolLog(vol)->vmrstats)).countdealloc(le->size);
	    Lsize(*(V_VolLog(vol)->vmrstats)).chgsize(-(le->size + 
							sizeof(recle)));

	    /* destroy the variable length part */
	    le->FreeVarl();

	    /* free up slot in rvm */
	    V_VolLog(vol)->RecovFreeRecord(le->index);

	    /* remember slot to be freed in vm bitmap after transaction ends */
	    ind->add(le->index);
	}
    
    SLog(9, "Leaving TruncRVMLog()\n");
}

/* free up space in vm bitmap */
void FreeVMIndices(Volume *vol, vmindex *ind) 
{
    vmindex_iterator next(ind);
    int i;
    while ((i = next()) != -1) 
	V_VolLog(vol)->DeallocRecord(i);
}

void PurgeLog(rec_dlist *list, Volume *vol, vmindex *ind) 
{
    recle *le;
    while (le = (recle *)list->get()) {
	// recursively purge all children logs too 
	rec_dlist *childlog;
	if (childlog = le->HasList()) 
	    PurgeLog(childlog, vol, ind);

	// RESSTATS
	VarlHisto(*(V_VolLog(vol)->vmrstats)).countdealloc(le->size);
	Lsize(*(V_VolLog(vol)->vmrstats)).chgsize(-(le->size + sizeof(recle)));

	// destroy the variable length part 
	le->FreeVarl();
	
	// free up slot in rvm 
	V_VolLog(vol)->RecovFreeRecord(le->index);

	// remember slot to be freed in vm bitmap after transaction ends 
	ind->add(le->index);
    }
    // now deallocate the list header itself 
    delete list;
}


void DumpLog(rec_dlist *log, Volume *vp, char **buf, int *bufsize, int *nentries) {
    int maxsize = V_VolLog(vp)->size * (sizeof(recle) + sizeof(rename_rle));
    *buf = (char *)malloc(maxsize);
    CODA_ASSERT(buf);
    int lastentry = 0;

    *nentries = *nentries + log->count();	// assume nentries has been initialized by caller
    rec_dlist_iterator next(*log);
    recle *r;
    while (r = (recle *)next()) {
	char *rbuf;
	int rbufsize;
	rbuf = r->DumpToBuf(&rbufsize);

	if ((maxsize - lastentry) < rbufsize) {
	    // not enough space - need to realloc
	    CODA_ASSERT(maxsize > 0);
	    int newmaxsize = maxsize * 2;
	    while ((newmaxsize - lastentry) < rbufsize) 
		newmaxsize = maxsize * 2;
	    char *newbuf = (char *)malloc(newmaxsize);
	    CODA_ASSERT(newbuf);
	    bcopy(*buf, newbuf, lastentry);
	    free(*buf);
	    *buf = newbuf;
	    maxsize = newmaxsize;
	}
	bcopy(rbuf, &((*buf)[lastentry]), rbufsize);
	delete[] rbuf;
	lastentry += rbufsize;

	// dump tree of log entries if one exists 
	rec_dlist *childlist;
	if (childlist = r->HasList()) {
	    char *childdump;
	    int childdumplength;
	    DumpLog(childlist, vp, &childdump, &childdumplength, nentries);
	    if ((maxsize - lastentry) < childdumplength) {
		// not enough space - need to realloc
		CODA_ASSERT(maxsize > 0);
		int newmaxsize = maxsize * 2;
		while ((newmaxsize - lastentry) < childdumplength) 
		    newmaxsize = maxsize * 2;
		char *newbuf = (char *)malloc(newmaxsize);
		CODA_ASSERT(newbuf);
		bcopy(*buf, newbuf, lastentry);
		free(*buf);
		*buf = newbuf;
		maxsize = newmaxsize;
	    }
	    bcopy(childdump, &((*buf)[lastentry]), childdumplength);
	    lastentry += childdumplength;
	    free(childdump);
	}
    }
    *bufsize = lastentry;
}
void PrintLog(rec_dlist *log, FILE *fp) {
    SLog(9,  "Entered PrintLog for log = 0x%x",
	   log);
    rec_dlist_iterator next(*log);
    recle *r;
    while (r = (recle *)next()) {
	rec_dlist *clog;
	r->print(fp);
	if (clog = r->HasList()) {
	    fprintf(fp, "####### Printing Subtree ##########\n");
	    PrintLog(clog, fp);
	    fprintf(fp, "####### Finished Subtree ##########\n");
	}
    }
    SLog(9, "Leaving PrintLog(log *, FILE *)\n");
}

void PrintLog(Vnode *vptr, FILE *fp) {
    SLog(9,  "Entered PrintLog for (0x%x.%x.%x)",
	   V_id(vptr->volumePtr), vptr->vnodeNumber, vptr->disk.uniquifier);
    if (VnLog(vptr)) 
	PrintLog(VnLog(vptr), fp);
    SLog(9, "Leaving PrintLog()\n");
}




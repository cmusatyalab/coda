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
#include <stdarg.h>
#include <strings.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include <olist.h>
#include <rec_dlist.h>
#include <vcrcommon.h>
#include <cvnode.h>
#include <volume.h>
#include <srv.h>
#include <vmindex.h>
#include <bitmap.h>
#include <recov_vollog.h>
#include "recle.h"
#include "ops.h"
#include "rsle.h"
#include "resstats.h"

rsle::rsle(ViceStoreId *sid, VnodeId dvnode, Unique_t dunique, int op, int ind, int sno) {
    index = ind;
    seqno = sno;
    storeid = *sid; 
    dvn = dvnode;
    du = dunique;
    opcode = op;
    name1 = name2 = NULL;
    namesalloced = 0;
}

rsle::rsle() {
    index = -1;
    seqno = -1;
    storeid.Host = 0;
    storeid.Uniquifier = 0;
    dvn = (long unsigned int)-1;
    du = (long unsigned int)-1;
    opcode = 0;
    name1 = name2 = NULL;
    namesalloced = 0;
}

rsle::~rsle() {
    if (namesalloced) {
	if (name1)  delete[] name1;
	if (name2)  delete[] name2;
    }
    name1 = NULL;
    name2 = NULL;
}

 
void rsle::init(int op ...) {
    va_list ap;
    va_start(ap, op);
    init(op, ap);
    va_end(ap);
}

void rsle::init(int op, va_list ap) {

    UserId owner;
    RPC2_Unsigned mode;
    UserId author;
    Date_t d;
    ViceVersionVector *vv;

    int sttype, newsttype;
    switch(op) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	newsttype = va_arg(ap, int);
	if (newsttype == STSTORE) {
	    UserId owner = va_arg(ap, UserId);
	    RPC2_Unsigned mode = va_arg(ap, RPC2_Unsigned);
	    UserId author = va_arg(ap, UserId);
	    Date_t d = va_arg(ap, Date_t);
	    RPC2_Integer mask = va_arg(ap, RPC2_Integer);
	    ViceVersionVector *vv = va_arg(ap, ViceVersionVector *);
	    LogMsg(10, SrvDebugLevel, stdout,
		   "rsle::init newstore type owner = %u, mode = %o author = %u Date = %u mask = %o\n",
		   owner, mode, author, d, mask);
	    u.newst.init(owner, mode, author, d, mask, vv);
	}
	else {
	    CODA_ASSERT(newsttype == ACLSTORE);
	    u.acl.init(va_arg(ap, char *));
	}
	break;

      case ResolveViceCreate_OP:
      case ViceCreate_OP: 
	{
	    char *c = va_arg(ap, char *);
	    namesalloced = 1;
	    name1 = new char[strlen(c) + 1];
	    CODA_ASSERT(name1);
	    strcpy(name1, c);
	    u.create.cvnode = va_arg(ap, VnodeId);
	    u.create.cunique = va_arg(ap, Unique_t);
	    u.create.owner = va_arg(ap, UserId);
	}
	break;
      case ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	{
	    char *c = va_arg(ap, char *);
	    namesalloced = 1;
	    name1 = new char[strlen(c) + 1];
	    CODA_ASSERT(name1);
	    strcpy(name1, c);
	    u.slink.cvnode = va_arg(ap, VnodeId);
	    u.slink.cunique = va_arg(ap, Unique_t);
	    u.slink.owner = va_arg(ap, UserId);
	}
	break;
      case ResolveViceLink_OP:
      case ViceLink_OP:
	{
	    char *c = va_arg(ap, char *);
	    namesalloced = 1;
	    name1 = new char[strlen(c) + 1];
	    CODA_ASSERT(name1);
	    strcpy(name1, c);
	    u.link.cvnode = va_arg(ap, VnodeId);
	    u.link.cunique = va_arg(ap, Unique_t);
	    u.link.cvv = *(va_arg(ap, ViceVersionVector *));
	}
	break;
      case ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	{
	    char *c = va_arg(ap, char *);
	    namesalloced = 1;
	    name1 = new char[strlen(c) + 1];
	    CODA_ASSERT(name1);
	    strcpy(name1, c);
	    u.mkdir.cvnode = va_arg(ap, VnodeId);
	    u.mkdir.cunique = va_arg(ap, Unique_t);
	    u.mkdir.owner = va_arg(ap, UserId);
	}
	break;
      case ResolveViceRemove_OP:
      case ViceRemove_OP:
	{
	    char *c = va_arg(ap, char *);
	    namesalloced = 1;
	    name1 = new char[strlen(c) + 1];
	    CODA_ASSERT(name1);
	    strcpy(name1, c);
	    u.rm.cvnode = va_arg(ap, VnodeId);
	    u.rm.cunique = va_arg(ap, Unique_t);
	    u.rm.cvv = *(va_arg(ap, ViceVersionVector*));
	}
	break;
      case ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	{
	    char *c = va_arg(ap, char *);
	    namesalloced = 1;
	    name1 = new char[strlen(c) + 1];
	    CODA_ASSERT(name1);
	    strcpy(name1, c);

	    u.rmdir.cvnode = va_arg(ap, VnodeId);
	    u.rmdir.cunique = va_arg(ap, Unique_t);
	    u.rmdir.childlist = va_arg(ap, rec_dlist *);
	    u.rmdir.childLCP = *(va_arg(ap, ViceStoreId *));
	    u.rmdir.csid = *(va_arg(ap, ViceStoreId *));
	}
	break;
      case ResolveViceRename_OP:
      case ViceRename_OP:
	{
	    u.mv.type = (unsigned short)va_arg(ap, unsigned int);
	    LogMsg(39, SrvDebugLevel, stdout, 
		   "rsle:init(Rename) got  type as %u\n", u.mv.type);
	    char *c = va_arg(ap, char *);
	    namesalloced = 1;
	    name1 = new char[strlen(c) + 1];
	    CODA_ASSERT(name1);
	    /* oldname is in name 1 */
	    strcpy(name1, c);
	    LogMsg(39, SrvDebugLevel, stdout, 
		   "rsle:init(Rename) got  name1 %s\n", name1);
	    c = va_arg(ap, char *);
	    name2 = new char[strlen(c) + 1];
	    CODA_ASSERT(name2);
	    /* newname is in name2 */
	    strcpy(name2, c);
	    LogMsg(39, SrvDebugLevel, stdout, 
		   "rsle:init(Rename) got  name2 %s\n", name2);
	    
	    u.mv.otherdirv = va_arg(ap, VnodeId);
	    u.mv.otherdiru = va_arg(ap, Unique_t);
	    u.mv.svnode = va_arg(ap, VnodeId);
	    u.mv.sunique = va_arg(ap, Unique_t);
	    LogMsg(39, SrvDebugLevel, stdout, 
		   "rsle:init(Rename) got d and s vnodes %x.%x %x.%x\n",
		   u.mv.otherdirv, u.mv.otherdiru, u.mv.svnode, u.mv.sunique);
	    ViceVersionVector *vvp;
	    vvp = va_arg(ap, ViceVersionVector *);
	    u.mv.svv = *vvp;
	    LogMsg(39, SrvDebugLevel, stdout,
		   "rsle:init(Rename) got source version vector\n");
	    int tgtexisted = va_arg(ap, int);
	    if (tgtexisted) {
		u.mv.tvnode = va_arg(ap, VnodeId);		
		u.mv.tunique = va_arg(ap, Unique_t);
		u.mv.tvv = *va_arg(ap, ViceVersionVector *);
		ViceFid cfid;
		cfid.Volume = 0;
		cfid.Vnode = u.mv.tvnode;
		cfid.Unique = u.mv.tunique;
		if (ISDIR(cfid)) 
		    u.mv.tlist = va_arg(ap, rec_dlist *);
		else
		    u.mv.tlist = NULL;
	    }
	    else {
		u.mv.tvnode = 0;
		u.mv.tunique = 0;
		u.mv.tvv = NullVV;
		u.mv.tlist = NULL;
	    }
	}
	break;
      case ResolveViceSetVolumeStatus_OP:
      case ViceSetVolumeStatus_OP:
	u.sq.oldquota = va_arg(ap, int);
	u.sq.newquota = va_arg(ap, int);
	break;
      case ResolveNULL_OP:
	break;
      case ViceRepair_OP:
	break;
      default:
	LogMsg(0, SrvDebugLevel, stdout, "rsle::rsle Illegal opcode(%d)\n", op);
    }
    va_end(ap);
}

/* called from within a transaction */
void rsle::CommitInRVM(Volume *vol, Vnode *vptr) {
    CODA_ASSERT(index >= 0);
    recle *rle = V_VolLog(vol)->RecovPutRecord(index); // commit promise log in rvm 
    CODA_ASSERT(rle);
    rle->InitFromsle(this);		// copy into rvm - allocate var length part
    VnLog(vptr)->append(rle);		// insert record into vnode's log 
    
    // #ifdef RESSTATS
    Lsize(*(V_VolLog(vol)->vmrstats)).chgsize(rle->size + sizeof(recle));
    VarlHisto(*(V_VolLog(vol)->vmrstats)).countalloc(rle->size);
}

void rsle::Abort(Volume *vol) {
    /* free up slot in bitmap that has been reserved */
    if (index >= 0) 
	V_VolLog(vol)->DeallocRecord(index);
}

// initialize entry from recle buffer - 
// and move buffer pointer to past end of buffer 
void rsle::InitFromRecleBuf(char **buf) {
    long *l = (long *)*buf;
    if (l[0] != DUMP_ENTRY_BEGIN_STAMP) {
	LogMsg(0, SrvDebugLevel, stdout, 
	       "rsle::InitFromBuf Bad begin stamp 0x%x\n",
	       l[0]);
    	return;
    }
    long *lastlong = (long *)(*buf + l[1] - sizeof(long));
    if (*lastlong != DUMP_ENTRY_END_STAMP) {
	LogMsg(0, SrvDebugLevel, stdout, 
	       "rsle::InitFromBuf Bad end stamp 0x%x\n",
	       *lastlong);
    	return;
    }
    *buf = *buf + l[1];
    recle *r = (recle *)&l[2];
    index = (int) r->serverid;	//overload index field when rsle used for remote entry 
    seqno = r->seqno;
    storeid = r->storeid;
    dvn = r->dvnode;
    du = r->dunique;
    opcode = r->opcode;
    name1 = name2 = NULL;
    namesalloced = 0;

    char *varp = ((char *)r + sizeof(recle));
    while ((long)varp % sizeof(long)) //word alignment
	varp++;
    
    /* Decls that used to be inside switch {}, but cause C++ 3.0 to choke */
    ststore *stp;
    newstore *newstp;
    aclstore *ap;
    create_rle *cp;
    symlink_rle *slp;
    link_rle *lp;
    mkdir_rle *mp;
    rm_rle *rp;
    rmdir_rle *rdp;
    rename_rle *mvp;
    setquota_rle *sq;

    switch (opcode) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	newstp = (newstore *)varp;
	if (newstp->type == STSTORE) {
	    u.newst.type  = newstp->type;
	    u.newst.owner = newstp->owner;
	    u.newst.mode = newstp->mode;
	    u.newst.author = newstp->author;
	    u.newst.mtime = newstp->mtime;
	    u.newst.mask = newstp->mask;
	    u.newst.vv = newstp->vv;
	}
	else {
	    ap = (aclstore *)newstp;
	    u.acl.type = ACLSTORE;
	    bcopy(ap->acl, u.acl.acl, (int) sizeof(u.acl.acl));
	}
	break;
      case ResolveViceCreate_OP:
      case ViceCreate_OP: 
	{
	    cp = (create_rle *)varp;
	    u.create.cvnode = cp->cvnode;
	    u.create.cunique = cp->cunique;
	    u.create.owner = cp->owner;
	    u.create.name[0] = '\0';
	    name1 = &(cp->name[0]);
	}
	break;
      case ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	{
	    slp = (symlink_rle *)varp;
	    u.slink.cvnode = slp->cvnode;
	    u.slink.cunique = slp->cunique;
	    u.slink.owner = slp->owner;
	    u.slink.name[0] = '\0';
	    name1 = &(slp->name[0]);
	}
	break;
      case ResolveViceLink_OP:
      case ViceLink_OP:
	{
	    lp = (link_rle *)varp;
	    u.link.cvnode = lp->cvnode;
	    u.link.cunique = lp->cunique;
	    u.link.cvv = lp->cvv;
	    u.link.name[0] = '\0';
	    name1 = &(lp->name[0]);
	}
	break;
      case ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	{
	    mp = (mkdir_rle *)varp;
	    u.mkdir.cvnode = mp->cvnode;
	    u.mkdir.cunique = mp->cunique;
	    u.mkdir.owner = mp->owner;
	    u.mkdir.name[0] = '\0';
	    name1 = &(mp->name[0]);
	}
	break;
      case ResolveViceRemove_OP:
      case ViceRemove_OP:
	{
	    rp = (rm_rle *)varp;
	    u.rm.cvnode = rp->cvnode;
	    u.rm.cunique = rp->cunique;
	    u.rm.cvv = rp->cvv;
	    u.rm.name[0] = '\0';
	    name1 = &(rp->name[0]);
	}
	break;
      case ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	{
	    rdp = (rmdir_rle *)varp;
	    u.rmdir.cvnode = rdp->cvnode;
	    u.rmdir.cunique = rdp->cunique;
	    u.rmdir.childlist = NULL;
	    u.rmdir.childLCP = rdp->childLCP;
	    u.rmdir.csid = rdp->csid;
	    u.rmdir.name[0] = '\0';
	    name1 = &rdp->name[0];
	}
	break;
      case ResolveViceRename_OP:
      case ViceRename_OP:
	{
	    mvp = (rename_rle *)varp;
	    u.mv.type = mvp->type;
	    u.mv.otherdirv = mvp->otherdirv;
	    u.mv.otherdiru = mvp->otherdiru;
	    u.mv.svnode = mvp->svnode;
	    u.mv.sunique = mvp->sunique;
	    u.mv.svv = mvp->svv;
	    u.mv.tvnode = mvp->tvnode;
	    u.mv.tunique = mvp->tunique;
	    u.mv.tvv = mvp->tvv;
	    u.mv.tlist = NULL;
	    u.mv.oldname[0] = '\0';
	    name1 = &(mvp->oldname[0]);
	    name2 = &(mvp->oldname[0]) + mvp->newname_offset;
	}
	break;
      case ResolveViceSetVolumeStatus_OP:
      case ViceSetVolumeStatus_OP:
        sq = (setquota_rle *)varp;
        u.sq.oldquota = sq->oldquota;
        u.sq.newquota = sq->newquota;
        break;
      case ResolveNULL_OP:
	break;
      case ViceRepair_OP:
	break;
      default:
	LogMsg(0, SrvDebugLevel, stdout, 
	       "rsle::InitFromRecleBuf Illegal opcode(%d)\n", opcode);
	
    }
}

void rsle::print() {
    print(stdout);
    fflush(stdout);
}
void rsle::print(FILE *fp){
    print(fileno(fp));
    fflush(fp);
}
void rsle::print(int fd) {
    char buf[512];
    sprintf(buf, 
	    "index 0x%x seqno %d stid 0x%x.%x\nDir (0x%x.%x)\nopcode %s\n\0",
	    index, seqno, storeid.Host, storeid.Uniquifier, dvn, du, 
	    PRINTOPCODE(opcode));
    write(fd, buf, (int)strlen(buf));
    switch (opcode) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	if (u.newst.type == STSTORE) 
	    u.newst.print(fd);
	else 
	    u.acl.print(fd);
	break;
      case ResolveViceCreate_OP:
      case ViceCreate_OP: 
	u.create.print(fd);
	break;
      case ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	u.slink.print(fd);
	break;
      case ResolveViceLink_OP:
      case ViceLink_OP:
	u.link.print(fd);
	break;
      case ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	u.mkdir.print(fd);
	break;
      case ResolveViceRemove_OP:
      case ViceRemove_OP:
	u.rm.print(fd);
	break;
      case ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	u.rmdir.print(fd);
	break;
      case ResolveViceRename_OP:
      case ViceRename_OP:
	u.rm.print(fd);
	break;
      case ResolveViceSetVolumeStatus_OP:
      case ViceSetVolumeStatus_OP:
        u.sq.print(fd);
        break;
      case ResolveNULL_OP:
	sprintf(buf, "ResolvNull record\n");
	write(fd, buf, (int)strlen(buf));
	break;
      case ViceRepair_OP:
	sprintf(buf, "ViceRepair record \n");
	write(fd, buf, (int)strlen(buf));
	break;
      default:
	sprintf(buf, "Illegal opcode\n");
	write(fd, buf, (int)strlen(buf));
    }

    if (name1) {
	sprintf(buf, "name1 %s\n", name1);
	write(fd, buf, (int)strlen(buf));
    }
    if (name2) {
	sprintf(buf, "name2 %s\n", name2);
	write(fd, buf, (int)strlen(buf));
    }
}

char *ExtractNameFromrsle(rsle *a) {
    switch((a)->opcode) {
      case ResolveViceRemove_OP:
      case ViceRemove_OP:
      case ResolveViceCreate_OP:
      case ViceCreate_OP:
      case ResolveViceSymLink_OP:
      case ViceSymLink_OP:
      case ResolveViceLink_OP:
      case ViceLink_OP:
      case ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
      case ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	return(a->name1);
      case ResolveViceRename_OP:
      case ViceRename_OP:
	if (a->u.mv.type == SOURCE) 
	    return(a->name1);
	else 
	    return(a->name2);
      default:
	return(NULL);
    }
}

void ExtractChildFidFromrsle(rsle *a, ViceFid *fa) {
    fa->Vnode = fa->Unique = 0;
    switch((a)->opcode) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	fa->Vnode = a->dvn;
	fa->Unique = a->du;
	break;
      case ResolveViceRemove_OP:
      case ViceRemove_OP:
	fa->Vnode = a->u.rm.cvnode;
	fa->Unique = a->u.rm.cunique;
	break;
      case ResolveViceCreate_OP:
      case ViceCreate_OP:
	fa->Vnode = a->u.create.cvnode;
	fa->Unique = a->u.create.cunique;
	break;
      case ResolveViceRename_OP:
      case ViceRename_OP:
	if (a->u.mv.type == SOURCE || 
	    !a->u.mv.tvnode) {
	    fa->Vnode = a->u.mv.svnode;
	    fa->Unique = a->u.mv.sunique;
	}
	else {
	    fa->Vnode = a->u.mv.tvnode;
	    fa->Unique = a->u.mv.tunique;
	}
	LogMsg(1, SrvDebugLevel, stdout,  
	       " Fid 0x%x.%x", fa->Vnode, fa->Unique);
	break;
      case ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	fa->Vnode = a->u.slink.cvnode;
	fa->Unique = a->u.slink.cunique;
	break;
      case ResolveViceLink_OP:
      case ViceLink_OP:
	fa->Vnode = a->u.link.cvnode;
	fa->Unique = a->u.link.cunique;
	break;
      case ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	fa->Vnode = a->u.mkdir.cvnode;
	fa->Unique = a->u.mkdir.cunique;
	break;
      case ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	fa->Vnode = a->u.rmdir.cvnode;
	fa->Unique = a->u.rmdir.cunique;
	break;
      case ResolveViceSetVolumeStatus_OP:
      case ViceSetVolumeStatus_OP:
      case ResolveNULL_OP:
	fa->Vnode = a->dvn;		/* XXX hack! */
	fa->Unique = a->du;		/* XXX hack! */
	break;
      default:
	LogMsg(0, SrvDebugLevel, stdout,  
	       "ExtractChildFidFromrsle: Illegal opcode %d",
		a->opcode);
	CODA_ASSERT(0);
	break;
    }
}

int ExtractVNTypeFromrsle(rsle *a) {
    switch(a->opcode) {
      case ResolveViceRemove_OP:
      case ViceRemove_OP:
      case ResolveViceCreate_OP:
      case ViceCreate_OP:
      case ResolveViceLink_OP:
      case ViceLink_OP:
	return(vFile);
      case ResolveViceRename_OP:
      case ViceRename_OP:
	{
	    ViceFid tgtFid;
	    /* XXX BE CAREFUL WITH CHILD FIDS AND RENAMES */
	    ExtractChildFidFromrsle(a, &tgtFid);
	    if (ISDIR(tgtFid))
		return(vDirectory);
	    else
		return(vFile);	/* XXX - what about symlinks ? */
	}
	break;
      case ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	return(vSymlink);
      case ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
      case ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
      case ResolveViceSetVolumeStatus_OP:
      case ViceSetVolumeStatus_OP:
	return(vDirectory);
      default:
	CODA_ASSERT(0);
	break;
    }
	
}

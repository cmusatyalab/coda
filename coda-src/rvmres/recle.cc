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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rvmres/Attic/recle.cc,v 4.5 1998/10/05 17:15:09 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdarg.h>
#include <rpc2.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <rec_dlist.h>
#include <vice.h>
#include <srv.h>
#include <rvmlib.h>
#include <vlist.h>
#include "ops.h"
#include "recle.h"
#include "rsle.h"

recle::recle() {
    abort();
}

recle::~recle() {
    abort();
}

void recle::InitFromsle(rsle *sl) {
    LogMsg(9, SrvDebugLevel, stdout, "Entering recle::InitFromSle()\n");
    LogMsg(9, SrvDebugLevel, stdout, "Opcode is %s\n", PRINTOPCODE(sl->opcode));

    RVMLIB_REC_OBJECT(*this);
    rec_dlink *dl = (rec_dlink *)this;
    dl->Init();
    index = sl->index;
    storeid = sl->storeid;
    seqno = sl->seqno;
    opcode = sl->opcode;
    dvnode = sl->dvn;
    dunique = sl->du;
    serverid = ThisHostAddr;

    /* allocate and initialize the variable length part */
    create_rle *c;
    symlink_rle *s;
    link_rle *l;
    mkdir_rle *mk;
    rm_rle *rm;
    rmdir_rle *rmd;
    rename_rle *mvle;
    setquota_rle *sq;
    switch (opcode) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	if (sl->u.acl.type == ACLSTORE) {
	    size = sizeof(aclstore);
	    vle = new (size) recvarl(size);
	    assert(vle);
	    rvmlib_set_range(vle, vle->size());
	    aclstore *acls = (aclstore *)(&(vle->vfld[0]));
	    acls->init(sl->u.acl.acl);
	}
	else {
	    size = sizeof(newstore);
	    vle = new (size) recvarl(size);
	    assert(vle);
	    rvmlib_set_range(vle, vle->size());
	    newstore *newst = (newstore *)(&(vle->vfld[0]));
	    newst->init(sl->u.newst.owner, sl->u.newst.mode, sl->u.newst.author, 
		     sl->u.newst.mtime, sl->u.newst.mask, &sl->u.st.vv);
	}
	break;

      case  ResolveViceCreate_OP:
      case ViceCreate_OP:
	size = sizeof(create_rle) + strlen(sl->name1); // size of information; vle is 4 bytes more
	vle = new (size) recvarl(size);
	assert(vle);
	rvmlib_set_range(vle, vle->size());
	c = (create_rle *)(&(vle->vfld[0]));
	c->init(sl->u.create.cvnode, sl->u.create.cunique, sl->u.create.owner, 
		sl->name1);
	break;
      case  ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	size = sizeof(symlink_rle) + strlen(sl->name1); // size of information; vle is 4 bytes more
	vle = new (size) recvarl(size);
	assert(vle);
	rvmlib_set_range(vle, vle->size());
	s = (symlink_rle *)(&(vle->vfld[0]));
	s->init(sl->u.slink.cvnode, sl->u.slink.cunique, sl->u.slink.owner, sl->name1);
	break;
      case  ResolveViceLink_OP:
      case ViceLink_OP:
	size = sizeof(link_rle) + strlen(sl->name1); // size of information; vle is 4 bytes more
	vle = new (size) recvarl(size);
	assert(vle);
	rvmlib_set_range(vle, vle->size());
	l = (link_rle *)(&(vle->vfld[0]));
	l->init(sl->u.link.cvnode, sl->u.link.cunique, &sl->u.link.cvv, sl->name1);
	break;
      case  ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	size = sizeof(mkdir_rle) + strlen(sl->name1); // size of information; vle is 4 bytes more
	vle = new (size) recvarl(size);
	assert(vle);
	rvmlib_set_range(vle, vle->size());
	mk = (mkdir_rle *)(&(vle->vfld[0]));
	mk->init(sl->u.mkdir.cvnode, sl->u.mkdir.cunique, sl->u.mkdir.owner, sl->name1);
	break;
      case  ResolveViceRemove_OP:
      case ViceRemove_OP:
	size = sizeof(rm_rle) + strlen(sl->name1); // size of information; vle is 4 bytes more
	vle = new (size) recvarl(size);
	assert(vle);
	rvmlib_set_range(vle, vle->size());
	rm = (rm_rle *)(&(vle->vfld[0]));
	rm->init(sl->u.rm.cvnode, sl->u.rm.cunique, &sl->u.rm.cvv, sl->name1);
	break;	
      case  ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	size = sizeof(rmdir_rle) + strlen(sl->name1); // size of information; vle is 4 bytes more
	vle = new (size) recvarl(size);
	assert(vle);
	rvmlib_set_range(vle, vle->size());
	rmd = (rmdir_rle *)(&(vle->vfld[0]));
	rmd->init(sl->u.rmdir.cvnode, sl->u.rmdir.cunique, sl->u.rmdir.childlist,
		    &sl->u.rmdir.childLCP, &sl->u.rmdir.csid, sl->name1);
	break;
      case  ResolveViceRename_OP:
      case ViceRename_OP:
	size = sizeof(rename_rle) + strlen(sl->name1) + strlen(sl->name2) + 2;
	vle = new (size) recvarl(size);
	assert(vle);
	rvmlib_set_range(vle, vle->size());
	mvle = (rename_rle *)(&(vle->vfld[0]));
	mvle->init(sl->u.mv.type, sl->u.mv.otherdirv, sl->u.mv.otherdiru,
		   sl->u.mv.svnode, sl->u.mv.sunique, &sl->u.mv.svv, 
		   sl->name1, sl->name2, sl->u.mv.tvnode, sl->u.mv.tunique, 
		   &sl->u.mv.tvv, sl->u.mv.tlist);
	break;
      case ResolveViceSetVolumeStatus_OP:
      case ViceSetVolumeStatus_OP:
	size = sizeof(setquota_rle);
	vle = new (size) recvarl(size);
	assert(vle);
	rvmlib_set_range(vle, vle->size());
	sq = (setquota_rle *)(&(vle->vfld[0]));
	sq->init(sl->u.sq.oldquota, sl->u.sq.newquota);
	break;
      case ResolveNULL_OP:
      case ViceRepair_OP:
	size = 0;	
	vle = NULL;
	break;
      default:
	LogMsg(0, SrvDebugLevel, stdout, "InitFromsle: unknown opcode %d\n", opcode);
	break;
    }
    LogMsg(9, SrvDebugLevel, stdout, "Leaving InitFromSle\n");
}

int recle::FreeVarl() { 
    LogMsg(9, SrvDebugLevel, stdout, "Entering FreeVarl() 0x%x.%x %s\n", 
	   dvnode, dunique, PRINTOPCODE(opcode));
    if (vle) {
	rvmlib_set_range(&vle, sizeof(recvarl *));
	vle->destroy();
	vle = NULL;
    }
    LogMsg(9, SrvDebugLevel, stdout, "Leaving FreeVarl()\n");
    return(0);
}

/* return pointer to list if this log entry has a pointer to a list of log entries */
rec_dlist *recle::HasList() {
    if ((opcode == ViceRemoveDir_OP) || (opcode == ResolveViceRemoveDir_OP)) {
	rmdir_rle *r = (rmdir_rle *)(&(vle->vfld[0]));
	return(r->childlist);
    }
    else if ((opcode == ViceRename_OP) || (opcode == ResolveViceRename_OP)) {
	rename_rle *mv = (rename_rle *)(&(vle->vfld[0]));
	return(mv->tlist);
    }
    else return NULL;
}


/* recle entries are dumped to a char buffer for shipping 
   them to remote sites for resolution.
   Each entry is dumped as follows:
      .
      .
      .
   DUMP_ENTRY_BEGIN_STAMP
   length of this entry 
   the fixed length part
   the variable length part
   DUMP_ENTRY_END_STAMP
      .
      .
      .
*/

//return size of buffer to which this recle can be dumped
int recle::GetDumpSize() {
    /* fixed length part */
    int dumpsize = (int) (sizeof(recle) + (3 * sizeof(long)));
    /* word align dumpsize */
    while (dumpsize % sizeof(long)) 
	dumpsize++;
    
    /* variable length part */
    dumpsize += size;
    while (dumpsize % sizeof(long)) 
	dumpsize++;

    return(dumpsize);
}

char *recle::DumpToBuf(int *bufsize) {
    *bufsize = GetDumpSize();
    char *buf = new char[*bufsize];
    assert(buf);
    long *l = (long *)buf;
    long *lastlong = (long *)(buf + *bufsize - sizeof(long));
    l[0] = DUMP_ENTRY_BEGIN_STAMP;
    l[1] = *bufsize;
    char *rlep = &(buf[2 * sizeof(long)]);
    /* copy the fixed length part */
    bcopy((char *)this, rlep, (int) sizeof(recle));
    rlep += (int) sizeof(recle);

    /* word align the variable length part */
    while (((long)rlep) % sizeof(long)) 
	rlep++;

    assert((long)(rlep + size) <= (long)lastlong);
    bcopy((const void *)&(vle->vfld[0]), (void *) rlep, size);
    
    *lastlong = DUMP_ENTRY_END_STAMP;
    return(buf);
}
void recle::print() {
    print(stdout);
}

void recle::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}

void recle::print(int fd) {
    char buf[512];
    sprintf(buf, "    **Server: 0x%x StoreId: 0x%x.%x \n", 
	    serverid, storeid.Host, storeid.Uniquifier);
    write(fd, buf, (int) strlen(buf));

    sprintf(buf, "    Directory(0x%x.%x)\n    Opcode: %s \n\0",
	    dvnode, dunique, PRINTOPCODE(opcode));
    write(fd, buf, (int) strlen(buf));
    sprintf(buf, "    index is %d, sequence number %d, var length is %d\n\0",
	    index, seqno, size);
    write(fd, buf, (int) strlen(buf));

    /* Decls that used to be inside switch{} but make C++ 3.0 unhappy */
    aclstore *acls;
    ststore *sp;
    newstore *newsp;
    rm_rle *rm;
    create_rle *c;
    rename_rle *mv;
    symlink_rle *s;
    link_rle *l;
    mkdir_rle *mkdir;
    rmdir_rle *rmdir;
    setquota_rle *sq;

    switch (opcode) {
      case ResolveViceNewStore_OP:
      case ViceNewStore_OP:
	acls = (aclstore *)&(vle->vfld[0]);
	if (acls->type == ACLSTORE)
	    acls->print(fd);
	else {
	    newsp = (newstore *)&(vle->vfld[0]);
	    assert(newsp->type == STSTORE);
	    newsp->print(fd);
	}
	break;
      case  ResolveViceRemove_OP:
      case ViceRemove_OP:
	rm = (rm_rle *)&(vle->vfld[0]);
	rm->print(fd);
	break;
      case  ResolveViceCreate_OP:
      case ViceCreate_OP:
	c = (create_rle *)&(vle->vfld[0]);
	c->print(fd);
	break;
      case  ResolveViceRename_OP:
      case ViceRename_OP:
	mv = (rename_rle *)&(vle->vfld[0]);
	mv->print(fd);
	break;
      case  ResolveViceSymLink_OP:
      case ViceSymLink_OP:
	s = (symlink_rle *)&(vle->vfld[0]);
	s->print(fd);
	break;
      case  ResolveViceLink_OP:
      case ViceLink_OP:
	l = (link_rle *)&(vle->vfld[0]);
	l->print(fd);
	break;
      case  ResolveViceMakeDir_OP:
      case ViceMakeDir_OP:
	mkdir = (mkdir_rle *)&(vle->vfld[0]);
	mkdir->print(fd);
	break;
      case  ResolveViceRemoveDir_OP:
      case ViceRemoveDir_OP:
	rmdir = (rmdir_rle *)&(vle->vfld[0]);
	rmdir->print(fd);
	break;
      case ResolveViceSetVolumeStatus_OP:
      case ViceSetVolumeStatus_OP:
	sq = (setquota_rle *)&(vle->vfld[0]);
	sq->print(fd);
	break;
      case ResolveNULL_OP:
	sprintf(buf, "    ResolveNULL record\n");
	write(fd, buf, (int) strlen(buf));
	break;
      case ViceRepair_OP:
	sprintf(buf, "    ViceRepair record\n");
	write(fd, buf, (int) strlen(buf));
	break;
      default:
	sprintf(buf, "!!!!!!!! Unknown Opcode - record not parsed !!!!!!\n");
	write(fd, buf, (int) strlen(buf));
	break;
    }
    
    sprintf(buf, "    ** End of Record **\n\n");
    write(fd, buf, (int) strlen(buf));
    
}

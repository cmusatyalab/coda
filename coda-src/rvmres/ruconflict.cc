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
#include <util.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include <dlist.h>
#include <cvnode.h>
#include <vcrcommon.h>
#include <codadir.h>
#include <srv.h>
#include <vlist.h>
#include <inconsist.h>
#include <resutil.h>
#include <remotelog.h>
#include "rsle.h"
#include "ruconflict.h"
#include "parselog.h"

// ************* Private Routines ***********
static ViceVersionVector *FindDeletedFileVV(olist *, unsigned long, 
					     ViceFid *, char *, ViceFid *);
static int ChildDirRUConf(RUParm *, ViceFid *, Vnode *);


/* RUConflict
 *	Check if there is a remove/update conflict for object 
 *	referenced in r->u.rm.childfid.
 *	For Files, Symbolic Links:
 *		R/U conflict exists iff VV of deleted obj is inc with
 *			existing object's VV
 *	For Directories:
 *		R/U conflict exists iff an operation  in the 
 *		log of the subtree doesnt exist in the log of deleted 
 *		subtree.
 */
#define FileRemove 0
#define DirRemove 1
int RUConflict(rsle *r, dlist *vlist, olist *AllLogs, ViceFid *dFid) {

    LogMsg(9, SrvDebugLevel, stdout,  "Entering RUConflict for %x.%x",
	    dFid->Vnode, dFid->Unique);
    ViceFid cFid;
    vle *cv = 0;
    int rtype = -1;
    
    CODA_ASSERT((r->opcode == RES_Remove_OP) ||
	   (r->opcode == ResolveViceRemove_OP) ||
	   (r->opcode == RES_RemoveDir_OP) ||
	   (r->opcode == ResolveViceRemoveDir_OP));
    if (r->opcode == RES_Remove_OP || r->opcode == ResolveViceRemove_OP)
	rtype = FileRemove;
    else 
	rtype = DirRemove;

    // get object first 
    {
	cFid.Volume = dFid->Volume;
	ExtractChildFidFromrsle(r, &cFid);
	cv = FindVLE(*vlist, &cFid);
	CODA_ASSERT(cv);
    }
    
    /* handle file r/u conflicts */
    {
	if (rtype == FileRemove) 
	    return(FileRUConf(r, cv->vptr));
    }
    
    /* handle dir r/u conflicts */
    {
	RUParm rup(vlist, AllLogs, r->index, dFid->Volume);
	NewDirRUConf(&rup, r->name1, cFid.Vnode, cFid.Unique);
	LogMsg(9, SrvDebugLevel, stdout,  
	       "RUConflict: NewDirRUConflict returns %d", rup.rcode);
	return(rup.rcode);
    }
}
#undef FileRemove 0
#undef DirRemove 1

int FileRUConf(rsle *r, Vnode *vptr) {
    ViceVersionVector *DeletedVV = &r->u.rm.cvv;
    return(FileRUConf(DeletedVV, vptr));
}

int FileRUConf(ViceVersionVector *DeletedVV, Vnode *vptr) {
    if (!DeletedVV) return(1);	
    if (!vptr) return(0);

    int res = VV_Cmp(&Vnode_vv(vptr), DeletedVV);
    if (res == VV_EQ || res == VV_SUB) {
	LogMsg(9, SrvDebugLevel, stdout,  
	       "FileRUConflict: no R/U conflict for 0x%x.%x",
	       vptr->vnodeNumber, vptr->disk.uniquifier);
	return(0);
    }
    else {
	LogMsg(9, SrvDebugLevel, stdout,
	       "FileRUConflict: R/U conflict for %x.%x",
	       vptr->vnodeNumber, vptr->disk.uniquifier);
	return(1);
    }
}

/* NewDirRUConf:
 *	Called on each child via Enumerate Dir 
 *	Detects remove/update conflicts on objects.
 */
int ENewDirRUConf(PDirEntry de, void *data)
{
	RUParm *rup = (RUParm *) data;
	VnodeId vnode;
	Unique_t unique;
	char *name = de->name;
	FID_NFid2Int(&de->fid, &vnode, &unique);
	
	NewDirRUConf(rup, name, vnode, unique);
}

int NewDirRUConf(RUParm *rup, char *name, long vnode, long unique) {
    
    if (rup->rcode) return(1);
    if (!strcmp(name, ".") || !strcmp(name, "..")) return(0);
    
    ViceFid cFid, pFid;
    vle *cv = 0;
    vle *pv = 0;
    // get object and parent's object 
    {
	FormFid(cFid, rup->vid, vnode, unique);
	cv = FindVLE(*(rup->vlist), &cFid);
	CODA_ASSERT(cv);
	
	FormFid(pFid, rup->vid, cv->vptr->disk.vparent, 
		cv->vptr->disk.uparent);
	pv = FindVLE(*(rup->vlist), &pFid);
	CODA_ASSERT(pv);
    }
    
    // file ruconflict if child is a non-directory (file, symlink)
    {
	if (cv->vptr->disk.type != vDirectory) {
	    ViceVersionVector *DeletedVV = 
		FindDeletedFileVV(rup->AllLogs, rup->srvrid, 
				  &cFid, name, &pFid);
	    return(rup->rcode = FileRUConf(DeletedVV, cv->vptr));

	}
    }
    
    // directory ruconflict if child is a dir
    {
	if (!ChildDirRUConf(rup, &cFid, cv->vptr)) {
	    // check recursively for children too 
	    PDirHandle dh;
	    dh = VN_SetDirHandle(cv->vptr);
	    if (DH_IsEmpty(dh)) 
		DH_EnumerateDir(dh, ENewDirRUConf, (void *)rup);
	    VN_PutDirHandle(cv->vptr);
	}
    }
    return(rup->rcode);
}

static ViceVersionVector *FindDeletedFileVV(olist *AllLogs, unsigned long hostid, 
					     ViceFid *filefid, char *name, ViceFid *pFid) {
    ViceVersionVector *VV = NULL;
    olist *rmtloglist = NULL;
    // find the remote parent's log 
    {
	rmtloglist = FindRemoteLog(AllLogs, hostid, pFid);
	CODA_ASSERT(rmtloglist);
    }
    
    // search log for child's deletion entry
    {
	olist_iterator next(*rmtloglist);
	rsle *ep = NULL;
	while (ep = (rsle *)next()) {
	    if ((ep->opcode == RES_Remove_OP ||
		 ep->opcode == ResolveViceRemove_OP) &&
		(ep->u.rm.cvnode == filefid->Vnode) &&
		(ep->u.rm.cunique == filefid->Unique) &&
		(!strcmp(ep->name1, name))) {
		VV = &(ep->u.rm.cvv);
		break;
	    }
	    if ((ep->opcode == RES_Rename_OP ||
		 ep->opcode == ResolveViceRename_OP) &&
		(ep->u.mv.tvnode) &&
		(ep->u.mv.tvnode == filefid->Vnode) &&
		(ep->u.mv.tunique == filefid->Unique) &&
		(!strcmp(ep->name2, name))) {
		VV = &(ep->u.mv.tvv);
		break;
	    }
	}
    }
    return(VV);
}


static int ChildDirRUConf(RUParm *rup, ViceFid *cFid, Vnode *cvptr) {
    //get log for directory where it was removed 
    olist *DeletedDirLog = NULL;
    {
	DeletedDirLog = FindRemoteLog(rup->AllLogs, rup->srvrid, cFid);
    }
    if (!DeletedDirLog) return(rup->rcode = 1);

    if (!VnLog(cvptr)) 
	// dir was just created - no conflict 
	return(0);
    
    // get last entry in dir's local log
    recle *LastLocalle;
    {
	LastLocalle = (recle *)(VnLog(cvptr)->last());
	CODA_ASSERT(LastLocalle);
    }
    
    // check if last entry is in remote log 
    {
	olist_iterator next(*DeletedDirLog);
	rsle *rslep;
	while (rslep = (rsle *)next())
	    if (SID_EQ(rslep->storeid, LastLocalle->storeid) &&
		(rslep->opcode == LastLocalle->opcode)) 
		break;
	if (!rslep)  
	    rup->rcode = 1;
    }
    return(rup->rcode);
}



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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/resolution/rename.cc,v 4.3.8.2 1998/10/07 20:25:09 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>
#include <rpc2.h>
#ifndef __CYGWIN32__
#include <dirent.h>
#endif
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <olist.h>
#include <dlist.h>
#include <srv.h>
#include <cvnode.h>
#include <vcrcommon.h>
#include <vlist.h>
#include <treeremove.h>
#include <coda_dir.h>

#include <resutil.h>
#include <ops.h>
#include <operations.h>

#include "ruconflict.h"

#include "rsle.h"
#include "resstats.h"

PRIVATE int ResolveCrossDirRename(rsle *, ViceFid *, Volume *, dlist *, dlist *);
PRIVATE void AddToIncList(dlist *, dlist *, Volume *, ViceFid *, int =0);
PRIVATE int CheckResolveRenameSemantics(rsle *, Volume *, ViceFid *, dlist *, vle **, vle **, vle **, 
					vle **,olist *, dlist *, dlist *, int *);
PRIVATE int CleanRenameTarget(rsle *, dlist *, Volume *, VolumeId , olist *, int *);

int CheckAndPerformRename(rsle *r, Volume *volptr, VolumeId VSGVolnum, 
			  ViceFid *dFid, dlist *vlist, olist *AllLogs, 
			  dlist *inclist, int *blocks) {
    LogMsg(1, SrvDebugLevel, stdout,
	   "Entering CheckAndPerformRename\n");
    
    dlist *newinclist = 0;
    int errorCode = 0;
    vle *sv = 0;
    vle *tv = 0;
    vle *sdv = 0;
    vle *tdv = 0;

    // handle Cross directory renames separately
    if ((r->u.mv.otherdirv != r->dvn) ||
	(r->u.mv.otherdiru != r->du)) {
	ResolveCrossDirRename(r, dFid, volptr, vlist, inclist);
	return(0);
    }

    // check semantics for rename 
    {
	newinclist = new dlist((CFN)CompareIlinkEntry);
	if (errorCode = CheckResolveRenameSemantics(r, volptr, dFid, vlist, &sv, &tv, 
						    &sdv, &tdv, AllLogs, inclist, 
						    newinclist, blocks))
	    LogMsg(0, SrvDebugLevel, stdout,
		   "Error %d from CheckResolveRenameSemantics\n",
		   errorCode);
	
    }
    
    if (!errorCode) {
	int tblocks = 0;
	assert(CleanRenameTarget(r, vlist, volptr, VSGVolnum, 
				 AllLogs, &tblocks) == 0);
	*blocks += tblocks;
	tblocks = 0;
	PerformRename(NULL, VSGVolnum, volptr, sdv->vptr, tdv->vptr, 
		      sv->vptr, (tv ? tv->vptr : 0), 
		      r->name1, r->name2,
		      sdv->vptr->disk.unixModifyTime, 0, &r->storeid, 
		      &sdv->d_cinode, &tdv->d_cinode, &sv->d_cinode, NULL);
	if (tv && tv->vptr->delete_me) {
	    tblocks = -nBlocks(tv->vptr->disk.length);
	    assert(AdjustDiskUsage(volptr, tblocks) == 0);
	    *blocks += tblocks;
	    if (tv->vptr->disk.type != vDirectory) {
		tv->f_sinode = tv->vptr->disk.inodeNumber;
		tv->vptr->disk.inodeNumber = 0;
	    }
	}
	/* XXX - MIGHT HAVE TO UPDATE THE VERSION VECTOR FOR THE CHILD ! */
	errorCode = SpoolRenameLogRecord((int) ResolveViceRename_OP, (dlist *) vlist, 
			     sv->vptr, (Vnode *) (tv ? tv->vptr : NULL), 
			     sdv->vptr, tdv->vptr, volptr, 
			     (char *)r->name1, (char *)r->name2, &r->storeid);
    }
    // merge the inconsistencies 
    if (errorCode && errorCode == EINCONS) {
	LogMsg(0, SrvDebugLevel, stdout,  
	       "Incorrect Res Rename: src = %s (%x.%x), tgt = %s (%x.%x)s",
	       r->name1, 
	       r->u.mv.svnode,	r->u.mv.sunique,
	       r->name2, r->u.mv.tvnode, r->u.mv.tunique);
    
	ilink *il;
	while (il = (ilink *)newinclist->get()) {
	    ViceFid fid;
	    FormFid(fid, V_id(volptr), il->vnode, il->unique);
	    
	    vle *v;
	    v = FindVLE(*vlist, &fid);
	    assert(v); 
	    assert(v->vptr);
	    MarkObjInc(&fid, v->vptr);
	    if (inclist->IsMember(il))
		delete il;
	    else
		inclist->insert(il);
	}
	errorCode = 0;
	V_VolLog(volptr)->vmrstats->conf.mv++;
    }
    
    if (newinclist) {
	assert(newinclist->count() == 0);
	delete newinclist;
    }
    LogMsg(1, SrvDebugLevel, stdout,  
	   "ResolveRename returning %d", errorCode);
    return(errorCode);
}

// Resolve Rename across directories 
// For now just mark both source and target directories inconsistent.
PRIVATE int ResolveCrossDirRename(rsle *r, ViceFid *dFid, Volume *volptr, 
				  dlist *vlist, dlist *inclist) {
    LogMsg(1, SrvDebugLevel, stdout,
	   "Entering ResolveCrossDirRename()\n");
    ViceFid FidA, FidB;
    // get fids of parents
    {
	FormFid(FidA, dFid->Volume, r->dvn, r->du);
	FormFid(FidB, dFid->Volume, r->u.mv.otherdirv, 
		r->u.mv.otherdiru);
    }
    //mark both parents inconsistent and add to inclist
    {
	AddToIncList(inclist, vlist, volptr, &FidA, 1);
	AddToIncList(inclist, vlist, volptr, &FidB, 1);
    }
    V_VolLog(volptr)->vmrstats->conf.mv++;
    return(0);
}

// mark an object inconsistent and form and add the ilist entry 
PRIVATE void AddToIncList(dlist *inclist, dlist *vlist, Volume *volptr, ViceFid *fid, int markobjinc) {
    vle *ov = FindVLE(*vlist, fid);
    if (ov && ov->vptr) {
	if (markobjinc) {
	    MarkObjInc(fid, ov->vptr);
	    LogMsg(0, SrvDebugLevel, stdout,
		   "Marking 0x%x.%x as inconsistent (probably due to rename)\n",
		   fid->Vnode, fid->Unique);
	}
	// special case for volume root - it has no name 
	if (fid->Vnode == 1 && fid->Unique == 1) 
	    AddILE(*inclist, ".", 1, 1, 1, 1, vDirectory);
	else {
	    char name[MAXNAMLEN];
	    assert(GetNameInParent(ov->vptr, vlist, volptr, name));
	    AddILE(*inclist, name, fid->Vnode, fid->Unique, 
		   ov->vptr->disk.vparent, ov->vptr->disk.uparent, ov->vptr->disk.type);
	}
    }    
}


PRIVATE int CheckResolveRenameSemantics(rsle *r, Volume *volptr, ViceFid *dFid, dlist *vlist, 
					vle **srcv, vle **tgtv, vle **srcdv, vle **tgtdv,olist *AllLogs, 
					dlist *inclist, dlist *newinclist, int *blocks) {
    LogMsg(9, SrvDebugLevel, stdout,  "Entering CheckResolveRenameSemantics");

    if (SrvDebugLevel > 9) 
	r->print();
    *srcv = *tgtv = *srcdv = *tgtdv = 0;
    ViceFid OldDid, NewDid;
    ViceFid SrcFid, TgtFid;
    vle *opv = 0;
    vle *npv = 0;
    vle *sv = 0;
    vle *tv = 0;
    int SrcNameExists = FALSE;
    int SrcNameFidBindingOK = FALSE;
    int SrcObjExists = FALSE;
    int SrcParentPtrOK = FALSE;
    char name[MAXNAMLEN];
    int errorCode = 0;

    /* check that both src and target directories exist */
    {
	// form the Fids
	if (r->u.mv.type == SOURCE) {
	    FormFid(OldDid, V_id(volptr), r->dvn, r->du);
	    FormFid(NewDid, V_id(volptr), r->u.mv.otherdirv, r->u.mv.otherdiru);
	}
	else {
	    FormFid(NewDid, V_id(volptr), r->dvn, r->du);
	    FormFid(OldDid, V_id(volptr), r->u.mv.otherdirv, r->u.mv.otherdiru);
	}
	
	opv = FindVLE(*vlist, &OldDid);
	if (!opv || !opv->vptr) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "ChkResRenSem: Old Dir(%x.%x.%x) doesnt exist",
		    OldDid.Volume, OldDid.Vnode, OldDid.Unique);
	    return(EINVAL);
	}
	if (!FID_EQ(OldDid, NewDid)) {
	    npv = FindVLE(*vlist, &NewDid);
	    if (!npv || !npv->vptr) {
		LogMsg(0, SrvDebugLevel, stdout,  "ChkResRenSem: New Dir(%x.%x.%x) doesnt exist",
			NewDid.Volume, NewDid.Vnode, NewDid.Unique);
		return(EINVAL);
	    }
	}
	else 
	    npv = opv;
    }

    // source object checks 
    {
	ViceFid tmpFid;
	DirHandle odh;
	ViceFid spFid;	/* source parent Fid */

	// check that source exists in same parent with same fid 
	FormFid(SrcFid, V_id(volptr),  r->u.mv.svnode, r->u.mv.sunique);
	SetDirHandle(&odh, opv->vptr);
	if (Lookup((long *)&odh, r->name1,(long *)&tmpFid) == 0) {
	    SrcNameExists = TRUE;
	    tmpFid.Volume = SrcFid.Volume;
	    SrcNameFidBindingOK = FID_EQ(tmpFid, SrcFid);
	}
	sv = FindVLE(*vlist, &SrcFid);
	if (sv && sv->vptr && !sv->vptr->delete_me)
	    SrcObjExists = TRUE;
	
	if (SrcObjExists) {
	    FormFid(spFid, V_id(volptr), sv->vptr->disk.vparent, sv->vptr->disk.uparent);
	    SrcParentPtrOK = FID_EQ(spFid, OldDid);
	    if (!SrcParentPtrOK) {
		//set child's parent to be marked inc 
		LogMsg(0, SrvDebugLevel, stdout,  
		       "ChkResRenSem: Parent(%x.%x) on src vnode(%x.%x) not same as parent %x.%x",
		       spFid.Vnode, spFid.Unique, SrcFid.Vnode, 
		       SrcFid.Unique, OldDid.Vnode, OldDid.Unique);
		AddToIncList(newinclist, vlist, volptr, &spFid, 0);
	    }
	}
	
	if (!SrcNameExists || !SrcNameFidBindingOK || 
	    !SrcObjExists || !SrcParentPtrOK) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "ChkResRenSem Src: NE = %d FBindOK = %d OE = %d PPOK = %d",
		   SrcNameExists, SrcNameFidBindingOK, SrcObjExists, 
		   SrcParentPtrOK);
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "ChkResRenSem: Marking both Old(%x.%x) and New dirs(%x.%x) inc",
		   OldDid.Vnode, OldDid.Unique, NewDid.Vnode, NewDid.Unique);
	    errorCode = EINCONS;
	    goto Exit;
	}
    }
    
    //target object checks 
    {
	if (!r->u.mv.tvnode && !r->u.mv.tunique) {
	    // target didn't exist at time of removal
	    // check that it doesn't exist now
	    DirHandle ndh;
	    ViceFid tmpfid;
	    tmpfid.Volume = V_id(volptr);
	    SetDirHandle (&ndh, npv->vptr);
	    if (Lookup((long *)&ndh, r->name2,(long *)&tmpfid) == 0) {
		LogMsg(0, SrvDebugLevel, stdout,  
		       "ChkResRenSem: Target name %s already exists wrongly",
		       r->name2);
		errorCode = EINCONS;
		goto Exit;
	    }
	}
	else {
	    // target is supposed to exist 
	    FormFid(TgtFid, V_id(volptr), r->u.mv.tvnode, r->u.mv.tunique);
	    DirHandle ndh;
	    ViceFid tmpFid;
	    ViceFid tpFid;	//target vnode's parent fid 
	    int TgtNameExists, TgtNameFidBindingOK;
	    int TgtObjExists, TgtParentPtrOK;
	    
	    TgtNameExists = FALSE;
	    TgtNameFidBindingOK = FALSE;
	    TgtObjExists = FALSE;
	    TgtParentPtrOK = TRUE;

	    
	    tmpFid.Volume = V_id(volptr);
	    SetDirHandle(&ndh, npv->vptr);
	    if (Lookup((long *)&ndh, r->name2, (long *)&tmpFid) == 0) {
		TgtNameExists = TRUE;
		TgtNameFidBindingOK = FID_EQ(tmpFid, TgtFid);
	    }
	    tv = FindVLE(*vlist, &TgtFid);
	    if (tv && tv->vptr && !tv->vptr->delete_me)
		TgtObjExists = TRUE;
	    if (TgtObjExists) {
		FormFid(tpFid, V_id(volptr), tv->vptr->disk.vparent, tv->vptr->disk.uparent);
		TgtParentPtrOK = FID_EQ(tpFid, NewDid);
		if (!TgtParentPtrOK) {
		    LogMsg(0, SrvDebugLevel, stdout,  
			   "ChkResRenSem: Parent(%x.%x) on tgtvnode(%x.%x)",	
			    tpFid.Vnode, tpFid.Unique, TgtFid.Vnode, TgtFid.Unique);
		    LogMsg(0, SrvDebugLevel, stdout,  
			   "    doesn't match Target Dir %x.%x",
			   NewDid.Vnode, NewDid.Unique);
		    AddToIncList(newinclist, vlist, volptr, &tpFid, 0);
		}
	    }
	    if (!TgtNameExists || !TgtNameFidBindingOK || 
		!TgtObjExists || !TgtParentPtrOK) {
		LogMsg(0, SrvDebugLevel, stdout,  
		       "ChkResRenSem: Tgt NE %d FBindOK %d OE %d PPOK %d",
			TgtNameExists, TgtNameFidBindingOK, 
			TgtObjExists, TgtParentPtrOK);
		LogMsg(0, SrvDebugLevel, stdout,  
		       "ChkResRenSem: Marking both Old(%x.%x) and New dirs(%x.%x) inc",
			OldDid.Vnode, OldDid.Unique, NewDid.Vnode, NewDid.Unique);
		errorCode = EINCONS;
		goto Exit;
	    }
	    //check for remove update conflicts on target 
	    {
		if (!ISDIR(TgtFid)) {
		    //file remove/update conflict 
		    assert(tv->vptr);
		    if (FileRUConf(&r->u.mv.tvv, tv->vptr)) {
			LogMsg(0, SrvDebugLevel, stdout,  
			       "ChkResRenSem: RUConflict on target %x.%x",
			       TgtFid.Vnode, TgtFid.Unique);
			errorCode = EINCONS;
			goto Exit;
		    }
		}
		else  {
		    RUParm rup(vlist, AllLogs, r->index, V_id(volptr));
		    NewDirRUConf(&rup, r->name2, TgtFid.Vnode, TgtFid.Unique);
		    LogMsg(9, SrvDebugLevel, stdout,  
			   "ChkResRenSem: NewDirRUConf returns %d", rup.rcode);
		    if (rup.rcode) {
			AddILE(*newinclist, r->name2, TgtFid.Vnode, TgtFid.Unique, 
			       NewDid.Vnode, NewDid.Unique, vDirectory);
			errorCode = EINCONS;
			goto Exit;
		    }
		}
	    }
	}
    }
    
    /* check normal rename semantics */
    if (errorCode = CheckRenameSemantics(NULL, &opv->vptr, &npv->vptr, 
					 &sv->vptr, 
					 r->name1, tv ? &tv->vptr : 0, 
					 r->name2, &volptr, 0, NULL, 
					 NULL, NULL, NULL, NULL, 
					 NULL, NULL, NULL, NULL, 
					 NULL, NULL, 0, 1)) {
	LogMsg(0, SrvDebugLevel, stdout,  
	       "ChkResRenSem: Error %d from vicerenamechksemantics ",
	       errorCode);
	errorCode = EINCONS;
    }
    
  Exit:
    if (errorCode) {
	assert(opv);
	assert(opv->vptr);
	assert(npv);
	assert(npv->vptr);
	AddToIncList(newinclist, vlist, volptr, &OldDid, 0);
	if (opv != npv)
	    AddToIncList(newinclist, vlist, volptr, &NewDid, 0);
    }
    else {
	*srcv = sv;
	*tgtv = tv;
	*srcdv = opv;
	*tgtdv = npv;
    }
    LogMsg(9, SrvDebugLevel, stdout,  
	   "ChkResRenSem: returns %d", 
	   errorCode);
    return(errorCode);
}

// CleanRenameTarget
// If it is a non-empty directory then remove the children
PRIVATE int CleanRenameTarget(rsle *r, dlist *vlist, Volume *volptr,
			      VolumeId VSGVolnum, olist *AllLogs, int *blocks) {
    LogMsg(9, SrvDebugLevel, stdout,  "Entering CleanRenameTarget");
    if (!r->u.mv.tvnode && !r->u.mv.tunique)
	return(0);
    ViceFid tFid;
    FormFid(tFid, V_id(volptr), r->u.mv.tvnode, r->u.mv.tunique);
    
    if (!ISDIR(tFid)) return(0);
    vle *tv = FindVLE(*vlist, &tFid);
    if (!tv || !tv->vptr) {
	LogMsg(0, SrvDebugLevel, stdout,  
	       "CleanRenameTarget: Couldn't find target(%x.%x) obj in vlist",
		tFid.Vnode, tFid.Unique);
	return(0);
    }
    
    DirHandle	tdh;
    SetDirHandle(&tdh, tv->vptr);
    if (IsEmpty((long *)&tdh) == 0) {
	LogMsg(0, SrvDebugLevel, stdout,  
	       "CleanRenameTarget: Target %x.%x is already empty",
		tFid.Vnode, tFid.Unique);
	return(0);
    }
    TreeRmBlk pkdparm;
    pkdparm.init(0, VSGVolnum, volptr, 0, &r->storeid, vlist, 
		 1, AllLogs, r->index, blocks);
    EnumerateDir((long *)&tdh, (int (*) (void * ...))PerformTreeRemoval, (long)&pkdparm);
    return(0);
}

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



/* this file contains local subtree representation code */

#ifdef __cplusplus
extern "C" {
#endif

#include <struct.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vcrcommon.h>

/* from venus */
#include "fso.h"
#include "local.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "worker.h"


/* ********** begining of lrdb methods ********** */
/* must be called from within a transaction */
VenusFid *lrdb::LGM_LookupLocal(VenusFid *global)
{
    OBJ_ASSERT(this, global);
    LOG(1000, ("lrdb::LGM_LookupLocal: global = %s\n", FID_(global)));
    lgm_iterator next(local_global_map);
    lgment *lgm;
    while ((lgm = next())) {
	if (FID_EQ(global, lgm->GetGlobalFid())) {
	    VenusFid *local = lgm->GetLocalFid();
	    LOG(1000, ("lrdb::LGM_LookupLocal: found local = %s\n", FID_(local)));
	    return local;
	}
    }
    LOG(1000, ("lrdb::LGM_LookupLocal: can not find local\n"));
    return NULL;
}

VenusFid *lrdb::LGM_LookupGlobal(VenusFid *local)
{
    OBJ_ASSERT(this, local);
    LOG(1000, ("lrdb::LGM_LookupLocal: local = %s\n", FID_(local)));
    lgm_iterator next(local_global_map);
    lgment *lgm;
    while ((lgm = next())) {
	if (FID_EQ(local, lgm->GetLocalFid())) {
	    VenusFid *global = lgm->GetGlobalFid();
	    LOG(1000, ("lrdb::LGM_LookupLocal: found global = %s\n",
		       FID_(global)));
	    return global;
	}
    }
    LOG(1000, ("lrdb::LGM_LookupGlobal: can not find global\n"));
    return NULL;
}

void lrdb::LGM_Insert(VenusFid *local, VenusFid *global)
{
    OBJ_ASSERT(this, local && global);
    /* make sure that the mapping is not already in the list */
    LOG(100, ("lrdb::LGM_Insert:local = %s. global = %s\n",
	      FID_(local), FID_(global)));
    OBJ_ASSERT(this, !LGM_LookupLocal(global) && !LGM_LookupGlobal(local));
    lgment *lgm = new lgment(local, global);
    local_global_map.append(lgm);
}

/* must be called from within a transaction */
void lrdb::LGM_Remove(VenusFid *local, VenusFid *global)
{
    OBJ_ASSERT(this, local && global);
    LOG(100, ("lrdb::LGM_Remove:local = %s. global = %s\n",
	      FID_(local), FID_(global)));
    /* make sure the mapping is there and correct */
    lgm_iterator next(local_global_map);
    lgment *lgm, *target = (lgment *)NULL;
    while ((lgm = next())) {
	if (FID_EQ(local, lgm->GetLocalFid()) &&
	    FID_EQ(global, lgm->GetGlobalFid())) {
	    LOG(100, ("lrdb::LGM_Remove: found the map entry\n"));
	    target = lgm;
	    break;
	}
    }
    /* we insist that the target entry exit in the map */
    OBJ_ASSERT(this, target);
    OBJ_ASSERT(this, local_global_map.remove(target) == target);
    delete target;
}

/* need not be called from within a transaction */
VenusFid *lrdb::RFM_LookupGlobalRoot(VenusFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding global-root-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupGlobalRoot: FakeRootFid = %s\n",
	       FID_(FakeRootFid)));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(), FakeRootFid)) {
	    LOG(1000, ("lrdb::RFM_LookupGlobalRoot: found\n"));
	    return rfm->GetGlobalRootFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupGlobalRoot: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
VenusFid *lrdb::RFM_LookupLocalRoot(VenusFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding local-root-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupLocalRoot: FakeRootFid = %s\n",
	       FID_(FakeRootFid)));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(), FakeRootFid)) {
	    LOG(1000, ("lrdb::RFM_LookupLocalRoot: found\n"));
	    return rfm->GetLocalRootFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupLocalRoot: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
VenusFid *lrdb::RFM_LookupRootParent(VenusFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding root-parent-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupRootParent: FakeRootFid = %s\n",
	       FID_(FakeRootFid)));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(), FakeRootFid)) {
	    LOG(1000, ("lrdb::RFM_LookupRootParent: found\n"));
	    return rfm->GetRootParentFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupParentRoot: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
VenusFid *lrdb::RFM_LookupGlobalChild(VenusFid *FakeRootFid)
{
    /* given the fake-root-fid, find out its corresponding global-child-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupGlobalChild: FakeRootFid = %s\n",
	       FID_(FakeRootFid)));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(), FakeRootFid)) {
	    LOG(1000, ("lrdb::RFM_LookupGlobalChild: found\n"));
	    return rfm->GetGlobalChildFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupGlobalChild: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
VenusFid *lrdb::RFM_LookupLocalChild(VenusFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding local-child-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupLocalChild: FakeRootFid = %s\n",
	       FID_(FakeRootFid)));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(), FakeRootFid)) {
	    LOG(1000, ("lrdb::RFM_LookupLocalChild: found\n"));
	    return rfm->GetLocalChildFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupLocalChild: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
fsobj *lrdb::RFM_LookupRootMtPt(VenusFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding root-mtpt */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupRootMtPt: FakeRootFid = %s\n",
	       FID_(FakeRootFid)));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(),  FakeRootFid)) {
	    LOG(1000, ("lrdb::RFM_LookupLocalChild: found\n"));
	    return rfm->GetRootMtPt();
	}
    }
    LOG(1000, ("lrdb::RFM_LookupRootMtPt: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
VenusFid *lrdb::RFM_ParentToFakeRoot(VenusFid *RootParentFid)
{
    /* given root-parent-fid, find out the corresponding fake-root-fid */
    OBJ_ASSERT(this, RootParentFid);
    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: RootParentFid = %s\n",
	       FID_(RootParentFid)));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetRootParentFid(), RootParentFid)) {
	    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: found\n"));
	    return rfm->GetFakeRootFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: not found\n"));
    return NULL;    
}

/* need not be called from within a transaction */
VenusFid *lrdb::RFM_FakeRootToParent(VenusFid *FakeRootFid)
{
    /* given the root-parent-fid, find out its corresponding fake-root-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: FakeRootFid = %s\n",
	       FID_(FakeRootFid)));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(), FakeRootFid)) {
	    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: found\n"));
	    return rfm->GetRootParentFid();
	} 
    }
    LOG(1000, ("lrdb::RFM_FakeRootToParent: not found\n"));
    return NULL;
}

void lrdb::RFM_CoverRoot(VenusFid *FakeRootFid)
{
    /* FakeRootFid must be a fake-root-fid already in the map */
    OBJ_ASSERT(this, FakeRootFid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(), FakeRootFid)) {
	    Recov_BeginTrans();
		   rfm->CoverRoot();
	    Recov_EndTrans(MAXFP);
	    return;
	}
    }
    OBJ_ASSERT(this, 0);
}

/* need not be called from within a transaction */
int lrdb::RFM_IsRootParent(VenusFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetRootParentFid(), Fid))
	    return 1;
    }
    return 0;
}

/* need not be called from within a transaction */
int lrdb::RFM_IsFakeRoot(VenusFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (FID_EQ(rfm->GetFakeRootFid(), Fid))
	    return 1;
    }
    return 0;
}

int lrdb::RFM_IsGlobalRoot(VenusFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (rfm->RootCovered()) continue;
	if (FID_EQ(rfm->GetGlobalRootFid(), Fid))
	    return 1;
    }
    return 0;
}

int lrdb::RFM_IsGlobalChild(VenusFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (rfm->RootCovered()) continue;
	if (FID_EQ(rfm->GetGlobalChildFid(), Fid))
	    return 1;
    }
    return 0;
}

int lrdb::RFM_IsLocalRoot(VenusFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (rfm->RootCovered()) continue;
	if (FID_EQ(rfm->GetLocalRootFid(), Fid))
	    return 1;
    }
    return 0;
}

int lrdb::RFM_IsLocalChild(VenusFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (rfm->RootCovered()) continue;
	if (FID_EQ(rfm->GetLocalChildFid(), Fid))
	    return 1;
    }
    return 0;
}

/* must be called from within a transaction */
void lrdb::RFM_Insert(VenusFid *FakeRootFid, VenusFid *GlobalRootFid, VenusFid *LocalRootFid, 
		      VenusFid *RootParentFid, VenusFid *GlobalChildFid,
		      VenusFid *LocalChildFid, char *Name, fsobj *MtPt)
{
    OBJ_ASSERT(this, FakeRootFid && GlobalRootFid && LocalRootFid && RootParentFid);
    OBJ_ASSERT(this, GlobalChildFid && LocalChildFid && Name);
    LOG(1000, ("lrdb::RFM_Insert: FakeRootFid = %s\n", FID_(FakeRootFid)));
    LOG(1000, ("lrdb::RFM_Intert: GlobalRootFid = %s LocalRootFid = %s\n",
	       FID_(GlobalRootFid), FID_(LocalRootFid)));
    LOG(1000, ("lrdb::RFM_Intert: GlobalChildFid = %s LocalChildFid = %s\n",
	       FID_(GlobalChildFid), FID_(LocalChildFid)));
    LOG(1000, ("lrdb::RFM_Insert: RootParentFid = %s Name = %s\n",
	       FID_(RootParentFid), Name));
    rfment *rfm = new rfment(FakeRootFid, GlobalRootFid, LocalRootFid, RootParentFid, 
			     GlobalChildFid, LocalChildFid, Name);
    root_fid_map.insert(rfm);

    if (MtPt)
	rfm->SetRootMtPt(MtPt);
}

/* must be called from within a transaction */
void lrdb::RFM_Remove(VenusFid *FakeRootFid)
{
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("RFM_Remove: FakeRootFid = %s\n", FID_(FakeRootFid)));
    /* first find out the map entry */
    rfm_iterator next(root_fid_map);
    rfment *rfm, *target = (rfment *)NULL;
    while ((rfm = next())) {
	if (FID_EQ(FakeRootFid, rfm->GetFakeRootFid())) {
	    LOG(1000, ("lrdb::RFM_Remove: found the entry\n"));
	    target = rfm;
	    break;
	}
    }
    /* we insist here that map entry exit */
    OBJ_ASSERT(this, target);
    OBJ_ASSERT(this, root_fid_map.remove(target) == target);
    delete target;
}

/* must be called from within a transaction */

/* generate local fid used in the faked subtrees */
VenusFid lrdb::GenerateLocalFakeFid(ViceDataType fidtype)
{
    ViceFid fid;
    VenusFid vf;

    RVMLIB_REC_OBJECT(local_fid_unique_gen);
    local_fid_unique_gen++;
    if ( fidtype == Directory ) 
	    FID_MakeLocalDir(&fid, local_fid_unique_gen);
    else 
	    FID_MakeLocalFile(&fid, local_fid_unique_gen);

    MakeVenusFid(&vf, LocalRealm->Id(), &fid);
    LOG(1000, ("lrdb::GenerateLocalFakeFid: return %s\n", FID_(&vf)));
    return vf;
}

/* generate a fake-fid whose volume is the special "Local" volume */
/* must be called from within a transaction */
VenusFid lrdb::GenerateFakeLocalFid()
{
	ViceFid fid;
	VenusFid vf;

	FID_MakeLocalSubtreeRoot(&fid, local_fid_unique_gen);

	RVMLIB_REC_OBJECT(local_fid_unique_gen);
	local_fid_unique_gen++;

	MakeVenusFid(&vf, LocalRealm->Id(), &fid);
	LOG(1000, ("lrdb::GenerateFakeLocalFid: return %s\n", FID_(&vf)));
	return vf;
}

/* must be called from within a transaction */
void lrdb::TranslateFid(VenusFid *OldFid, VenusFid *NewFid)
{
    OBJ_ASSERT(this, OldFid && NewFid);
    LOG(100, ("lrdb::TranslateFid: OldFid = %s NewFid = %s\n",
	      FID_(OldFid), FID_(NewFid)));

    {	/* translate fid for the local-global fid map list */
	if (!FID_IsLocalFake(NewFid)) {
	    /* 
	     * only when NewFid is not a local fid, i.e., the fid replacement
	     * was caused cmlent::realloc, instead of cmlent::LocalFakeify. 
	     */
	    lgm_iterator next(local_global_map);
	    lgment *lgm;
	    while ((lgm = next())) {
		if (FID_EQ(lgm->GetGlobalFid(), OldFid))
		    lgm->SetGlobalFid(NewFid);
	    }
	}
    }

    {	/* tranlate fid for the root-fid-map list */
	/* 
	 * this is not necessary because all the fids recorded in an rfment
	 * entry are either local-fid which won't be replaced by reintegration,
	 * or they are global-fids stored on the server already. 
	 */
    }
}

/* need not be called from within a transaction */
void lrdb::PurgeRootFids()
{
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	VenusFid *FakeRootFid = rfm->GetFakeRootFid();
	VenusFid *GlobalRootFid = rfm->GetGlobalRootFid();
	VenusFid *LocalRootFid = rfm->GetLocalRootFid();
	VenusFid *LocalChildFid = rfm->GetLocalChildFid();
	VenusFid *GlobalChildFid = rfm->GetGlobalChildFid();
	LOG(1000, ("lrdb::PurgeRootFids: FakeRootFid = %s\n", FID_(FakeRootFid)));
	LOG(1000, ("lrdb::PurgeRootFids: GlobalRootFid = %s\n", FID_(GlobalRootFid)));
	LOG(1000, ("lrdb::PurgeRootFids: LocalRootFid = %s\n", FID_(LocalRootFid)));
	LOG(1000, ("lrdb::PurgeRootFids: GlobalChildFid = %s\n", FID_(GlobalChildFid)));
	LOG(1000, ("lrdb::PurgeRootFids: LocalChildFid = %s\n", FID_(LocalChildFid)));
	(void)k_Purge(FakeRootFid, 1);
	(void)k_Purge(GlobalRootFid, 1);
	(void)k_Purge(LocalRootFid, 1);
	(void)k_Purge(GlobalChildFid, 1);
	(void)k_Purge(LocalChildFid, 1);
    }
}

/* need not be called from within a transaction */
void lrdb::DirList_Clear()
{
    LOG(1000, ("lrdb::DirList_Clear: |dir_list| = %d\n", dir_list.count()));
    vdirent *dir;
    while ((dir = (vdirent *)dir_list.get()))
      delete dir;
    OBJ_ASSERT(this, dir_list.count() == 0);
}

/* need not be called from within a transaction */
void lrdb::DirList_Insert(VenusFid *fid, char *Name)
{
    OBJ_ASSERT(this, Name);
    LOG(1000, ("lrdb::DirList_Insert: Fid = %s and Name = %s\n",
	       FID_(fid), Name));
    vdirent *newdir = new vdirent(fid, Name);
    dir_list.insert(newdir);
}

/*
  BEGIN_HTML
  <a name="uncacheddir"><strong> this method handles the situation during the localization 
  process when there are uncached objects under a directory being localized. </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
void lrdb::DirList_Process(fsobj *DirObj)
{
    OBJ_ASSERT(this, DirObj);
    VenusFid *ParentFid = &DirObj->fid;
    LOG(1000, ("lrdb::DirList_Process: DirObj = %s\n", FID_(ParentFid)));
    dir_iterator next(dir_list);
    vdirent *dir;
    while ((dir = next())) {
	char *ChildName = dir->GetName();
	if (STREQ(ChildName, ".") || STREQ(ChildName, ".."))
	  continue;
	VenusFid *ChildFid = dir->GetFid();
	if (FSDB->Find(ChildFid) == NULL) {
	    /* found an un-cached child object in Parent */
	    LOG(100, ("lrdb::DirList_Process: found uncached child object (%s, %s)\n",
		      ChildName, FID_(ChildFid)));
	    /* repalce ChildFid with a local fid */
	    VenusFid LocalFid;
	    Recov_BeginTrans();
		   /* we don't know the type of ChildFid, just assume it is a file */
		   LocalFid = GenerateLocalFakeFid(File);
		   /* replace (ChildName, ChildFid) with (ChildName, LocalFid) */
		   DirObj->dir_Delete(ChildName);
		   DirObj->dir_Create(ChildName, &LocalFid);
	    Recov_EndTrans(MAXFP);
	} else {
	    LOG(100, ("lrdb::DirList_Process: found cached child object (%s, %s)\n",
		      ChildName, FID_(ChildFid)));	    
	}
    }
}


/* must be called from within a transaction */
void *lrdb::operator new(size_t len)
{
    lrdb *l = 0;

    l = (lrdb *)rvmlib_rec_malloc((int) len);
    CODA_ASSERT(l);
    return(l);
}

/* must be called from within a transaction */
lrdb::lrdb()
{
    RVMLIB_REC_OBJECT(*this);
    repair_tid_gen = REP_INIT_TID;
    local_fid_unique_gen = 1;
    Lock_Init(&rfm_lock);
    ResetTransient();
}

/* must be called from within a transaction */
lrdb::~lrdb()
{
    abort();
}

void lrdb::operator delete(void *deadobj, size_t len)
{
    abort();
}


/* need not be called from within a transaction */
void lrdb::ResetTransient()
{
    memset((void *)&dir_list, 0, (int)sizeof(dlist));
    repair_root_fid = (VenusFid *)NULL;
    current_search_cml = (cmlent *)NULL;
    repair_session_mode = REP_SCRATCH_MODE;
    subtree_view = SUBTREE_MIXED_VIEW;
    repair_session_tid = - repair_tid_gen;
    memset((void *)&repair_obj_list, 0, (int)sizeof(dlist));
    memset((void *)&repair_vol_list, 0, (int)sizeof(dlist));
    memset((void *)&repair_cml_list, 0, (int)sizeof(dlist));
}

void lrdb::print(FILE *fp)
{
    print(fileno(fp));
}

void lrdb::print()
{
    print(fileno(stdout));
}

void lrdb::print(int fd)
{
    fdprint(fd, "repair_session_tid = %d\n", repair_session_tid);
    fdprint(fd, "repair_tid_gen = %d\n", repair_tid_gen);
    switch (subtree_view) {
    case SUBTREE_MIXED_VIEW:
	fdprint(fd, "subtree_view = SUBTREE_MIXED_VIEW\n");
	break;
    case SUBTREE_LOCAL_VIEW:
	fdprint(fd, "subtree_view = SUBTREE_LOCAL_VIEW\n");
	break;
    case SUBTREE_GLOBAL_VIEW:
	fdprint(fd, "subtree_view = SUBTREE_GLOBAL_VIEW\n");
	break;
    default:
	fdprint(fd, "bogus subtree_view = %d\n", subtree_view);
    }
    if (repair_root_fid != NULL) {
	fdprint(fd, "repair_root_fid = %s\n", FID_(repair_root_fid));
    } else {
	fdprint(fd, "repair_root_fid = NULL\n");
    }
    if (current_search_cml != NULL) {
	fdprint(fd, "current_search_cml = %x\n", current_search_cml);
    } else {
	fdprint(fd, "current_search_cml = NULL\n");
    }
    if (repair_session_mode == REP_SCRATCH_MODE) {
	fdprint(fd, "repair_session_mode = REP_SCRATCH_MODE\n");
    } else {
	fdprint(fd, "repair_session_mode = REP_DIRECT_MODE\n");
    }
    {	/* print out LocalObjList */
	fdprint(fd, "there are %d entries in repair_obj_list\n", repair_obj_list.count());
	opt_iterator next(repair_obj_list);
	optent *opt;
	fdprint(fd, "=======================================================================\n");
	while ((opt = next())) {
	    fsobj *obj = opt->GetFso();
	    if (obj == NULL) {
		fdprint(fd, "bad entry: can't get fsobj\n");
		continue;
	    }
	    obj->print(fd);
	}
	fdprint(fd, "=======================================================================\n");
    }
    {	/* print out RepairVolList */
	fdprint(fd, "there are %d entries in repair_vol_list\n", repair_vol_list.count());
	vpt_iterator next(repair_vol_list);
	vptent *vpt;
	fdprint(fd, "=======================================================================\n");
	while ((vpt = next())) {
	    volent *vol = vpt->GetVol();
	    if (vol == NULL) {
		fdprint(fd, "bad entry: can't get volent\n");
		continue;
	    }
	    vol->print(fd);
	}
	fdprint(fd, "=======================================================================\n");
    }
    {	/* print out repair_cml_list */
	fdprint(fd, "there are %d entries in repair_cml_list\n", repair_cml_list.count());
	mpt_iterator next(repair_cml_list);
	mptent *mpt;
	fdprint(fd, "=======================================================================\n");
	while ((mpt = next())) {
	    cmlent *cml = mpt->GetCml();
	    if (cml == NULL) {
		fdprint(fd, "bad entry: can't get cmlent\n");
		continue;
	    }
	    cml->print(fd);
	}
	fdprint(fd, "=======================================================================\n");
    }
    {	/* print out every entry of the global-local-fid map */
	fdprint(fd, "there are %d entries in the global-local-fid map\n", local_global_map.count());
	lgm_iterator next(local_global_map);
	lgment *lgm;
	while ((lgm = next())) {
	    lgm->print(fd);
	    fdprint(fd, "\n");
	}
    }
    {	/* print out every entry of the global-local-fid map */
	fdprint(fd, "there are %d entries in the root-fid map\n", root_fid_map.count());
	rfm_iterator next(root_fid_map);
	rfment *rfm;
	while ((rfm = next())) {
	    fdprint(fd, "====================================\n");
	    rfm->print(fd);
	    fdprint(fd, "====================================\n");
	}
    }
}

/* ********** end of lrdb methods ********** */


/* ********** begining of lgment methods ********** */
void *lgment::operator new(size_t len)
{
    lgment *l = 0;
    
    l = (lgment *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(l);
    return(l);
}

lgment::lgment(VenusFid *l, VenusFid *g)
{
    OBJ_ASSERT(this, l && g);
    LOG(1000, ("lgment::lgment: Local = %s Global = %s\n", FID_(l), FID_(g)));

    RVMLIB_REC_OBJECT(*this);
    local = *l;
    global = *g;
}

lgment::~lgment()
{
    LOG(1000, ("lgment::~lgment: Local = %s Global = %s\n", FID_(&local), FID_(&global)));
    /* nothing to do! */
}

void lgment::operator delete(void *deadobj, size_t len)
{
    rvmlib_rec_free(deadobj);
}

VenusFid *lgment::GetLocalFid()
{
    return &local;
}

VenusFid *lgment::GetGlobalFid()
{	
    return &global;
}

/* must be called from within a transation */
void lgment::SetLocalFid(VenusFid *lfid)
{
    OBJ_ASSERT(this, lfid);
    RVMLIB_REC_OBJECT(local);
    local = *lfid;
}

/* must be called from within a transation */
void lgment::SetGlobalFid(VenusFid *gfid)
{
    OBJ_ASSERT(this, gfid);
    RVMLIB_REC_OBJECT(global);
    global = *gfid;
}

void lgment::print(FILE *fp)
{
    print(fileno(fp));
}

void lgment::print()
{
    print(fileno(stdout));
}

void lgment::print(int fd)
{
    fdprint(fd, "(local = %s global = %s)", FID_(&local), FID_(&global));
}

lgm_iterator::lgm_iterator(rec_dlist& dl) : rec_dlist_iterator(dl) 
{
}

lgment *lgm_iterator::operator()()
{
    return (lgment *)rec_dlist_iterator::operator()();
}
/* ********** end of lgment methods ********** */

/* ********** beginning of rfment methods ********** */
/* must be called from within a transaction */
void *rfment::operator new(size_t len)
{
    rfment *r = 0;
    r = (rfment *)rvmlib_rec_malloc((int)sizeof(rfment));
    CODA_ASSERT(r);
    return(r);
}

/* must be called from within a transaction */
rfment::rfment(VenusFid *Fake, VenusFid *Global, VenusFid *Local, VenusFid *Parent, 
	       VenusFid *GlobalChild, VenusFid *LocalChild, char *CompName)
{
    OBJ_ASSERT(this, Fake && Global && Local && Parent && GlobalChild && LocalChild && CompName);
    LOG(1000, ("rfment::rfment:: FakeRootFid = %s\n", FID_(Fake)));
    LOG(1000, ("rfment::rfment:: GlobalRootFid = %s\n", FID_(Global)));
    LOG(1000, ("rfment::rfment:: LocalRootFid = %s\n", FID_(Local)));
    LOG(1000, ("rfment::rfment:: RootParentFid = %s\n", FID_(Parent)));
    LOG(1000, ("rfment::rfment:: GlobalChildFid = %s\n", FID_(GlobalChild)));
    LOG(1000, ("rfment::rfment:: LocalChildFid = %s\n", FID_(LocalChild)));
    LOG(1000, ("rfment::rfment:: CompName = %s\n", CompName));


    RVMLIB_REC_OBJECT(*this);
    fake_root_fid    = *Fake;
    global_root_fid  = *Global;
    local_root_fid   = *Local;
    root_parent_fid  = *Parent;
    global_child_fid = *GlobalChild;
    local_child_fid  = *LocalChild;
    name = (char *)rvmlib_rec_malloc((int)(strlen(CompName) + 1));
    CODA_ASSERT(name);
    strcpy(name, CompName);
    view = SUBTREE_MIXED_VIEW;
    covered = 0;
    root_mtpt = 0;
}

/* must be called from within a transaction */
rfment::~rfment()
{
    LOG(1000, ("rfment::~rfment()\n"));
    rvmlib_rec_free(name);
}

/* must be called from within a transaction */
void rfment::operator delete(void *deadobj, size_t len)
{
    LOG(1000, ("rfment::operator delete\n"));
    rvmlib_rec_free(deadobj);
}

VenusFid *rfment::GetFakeRootFid()
{
    return &fake_root_fid;
}

VenusFid *rfment::GetGlobalRootFid()
{
    return &global_root_fid;
}

VenusFid *rfment::GetLocalRootFid()
{
    return &local_root_fid;
}

VenusFid *rfment::GetRootParentFid()
{
    return &root_parent_fid;
}

VenusFid *rfment::GetGlobalChildFid()
{
    return &global_child_fid;
}

VenusFid *rfment::GetLocalChildFid()
{
    return &local_child_fid;
}

char *rfment::GetName()
{
    return name;
}

unsigned short rfment::RootCovered()
{
    return covered;
}

/* must be called from within a transaction */
void rfment::CoverRoot()
{
    RVMLIB_REC_OBJECT(covered);
    covered = 1;
}

/* must be called from within a transaction */
void rfment::SetView(char V)
{
    RVMLIB_REC_OBJECT(view);
    view = V;
}

char rfment::GetView()
{
    return view;
}

int rfment::IsVolRoot()
{
    return (root_mtpt != NULL);
}

/* must be called from within a transaction */
void rfment::SetRootMtPt(fsobj *mtpt)
{
    RVMLIB_REC_OBJECT(root_mtpt);
    root_mtpt = mtpt;
}

fsobj *rfment::GetRootMtPt()
{
    return root_mtpt;
}

void rfment::print(FILE *fp)
{
    print(fileno(fp));
}

void rfment::print()
{
    print(fileno(stdout));
}

void rfment::print(int fd)
{
    fdprint(fd, "\tfake_root_fid = %s\n", FID_(&fake_root_fid));
    fdprint(fd, "\tglobal_root_fid = %s\n", FID_(&global_root_fid));
    fdprint(fd, "\tlocal_root_fid = %s\n", FID_(&local_root_fid));
    fdprint(fd, "\troot_parent_fid = %s\n", FID_(&root_parent_fid));
    fdprint(fd, "\tglobal_child_fid = %s\n", FID_(&global_child_fid));
    fdprint(fd, "\tLocalChildFid = %s\n", FID_(&local_child_fid));
    fdprint(fd, "\tname = %s\n", name);
    fdprint(fd, "\troot_mtpt = %x\n", root_mtpt);
    fdprint(fd, "\troot is %s\n", covered ? "covered" : "not covered");
    switch (view) {
    case SUBTREE_MIXED_VIEW:
	fdprint(fd, "\tview = Mixed-View\n");
	break;
    case SUBTREE_GLOBAL_VIEW:
	fdprint(fd, "\tview = Global-View\n");
	break;
    case SUBTREE_LOCAL_VIEW:
	fdprint(fd, "\tview = Local-View\n");
	break;
    default:
	fdprint(fd, "\tbogus view = %d\n", view);
    }
}

rfm_iterator::rfm_iterator(rec_dlist& dl) : rec_dlist_iterator(dl)
{
}

rfment *rfm_iterator::operator()() 
{
    return (rfment *)rec_dlist_iterator::operator()();
}
/* ********** end of rfment methods ********** */


/* ********** beginning of vdirent ********** */
/* need not be called from within a transaction */
vdirent::vdirent(VenusFid *Fid, char *Name)
{
    OBJ_ASSERT(this, Name);
    LOG(10, ("vdirent::vdirent: fid = %s and name = %s\n", FID_(Fid), Name));
    fid = *Fid;
    strcpy(name, Name);
}

vdirent::~vdirent()
{
}

VenusFid *vdirent::GetFid()
{
    return &fid;
}

char *vdirent::GetName()
{
    return name;
}

void vdirent::print(FILE *fp)
{
    print(fileno(fp));
}

void vdirent::print()
{
    print(fileno(stdout));
}

void vdirent::print(int fd)
{
    fdprint(fd, "fid = %s and name = %s\n", FID_(&fid), name);
}

dir_iterator::dir_iterator(dlist& dl) : dlist_iterator(dl)
{
}

vdirent *dir_iterator::operator()()
{
    return (vdirent *)dlist_iterator::operator()();
}
/* ********** end of vdirent ********** */


/* ********** beginning of optent methods ********** */
optent::optent(fsobj *o)
{
    OBJ_ASSERT(this, obj = o);
}

optent::~optent()
{
}

fsobj *optent::GetFso()
{
    return obj;
}

void optent::SetTag(int t)
{
    tag = t;
}

int optent::GetTag()
{
    return tag;
}

void optent::print(FILE *fp)
{
    print(fileno(fp));
}

void optent::print()
{
    print(fileno(stdout));
}

void optent::print(int fd)
{
    fdprint(fd, "obj = %x tag = %d\n", obj, tag);
}

opt_iterator::opt_iterator(dlist &dl) : dlist_iterator(dl)
{
}

optent *opt_iterator::operator()()
{
    return (optent *)dlist_iterator::operator()();
}
/* ********** end of optent methods ********** */

/* ********** begining of vptent methods ********** */
vptent::vptent(repvol *v) 
{
    OBJ_ASSERT(this, v);
    vpt = v;
}

vptent::~vptent()
{
}

repvol *vptent::GetVol()
{
    return vpt;
}

void vptent::print(FILE *fp)
{
    print(fileno(fp));
}

void vptent::print()
{
    print(fileno(stdout));
}

void vptent::print(int fd)
{
    fdprint(fd, "vpt = 0x%x\n", vpt);
}

vpt_iterator::vpt_iterator(dlist& dl) : dlist_iterator(dl)
{
}

vptent *vpt_iterator::operator()()
{
    return ((vptent *)dlist_iterator::operator()());
}
/* ********** end of vptent methods ********** */


/* must not be called from within a transaction */
void LRInit()
{
    /* Allocate the IOT database if requested. */    
    if (InitMetaData) {
	eprint("Initial LRDB allocation");
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(LRDB);
	LRDB = new lrdb;
	Recov_EndTrans(0);
    } else {
	LRDB->ResetTransient();

	{   /* check local-subtree root nodes */
	    rfm_iterator next(LRDB->root_fid_map);
	    rfment *rfm;
	    while ((rfm = next())) {
		if (rfm->RootCovered() || (rfm->GetView() == SUBTREE_MIXED_VIEW)) 
		  continue;
		VenusFid *FakeRootFid = rfm->GetFakeRootFid();
		VenusFid *RootParentFid = rfm->GetRootParentFid();
		fsobj *RootParentObj = FSDB->Find(RootParentFid);
		CODA_ASSERT(RootParentObj && RootParentObj->IsLocalObj());
		char *Name = rfm->GetName();
		RootParentObj->RecoverRootParent(FakeRootFid, Name);
		Recov_BeginTrans();
		       rfm->SetView(SUBTREE_MIXED_VIEW);
		Recov_EndTrans(MAXFP);
	    }
	}

	{   /* GC local-repair mutations (if any) */
	    repvol_iterator next;
	    repvol *v;
	    while ((v = next()))
		v->ClearRepairCML();
	}
    }

    /* fire up the daemon */
    LRD_Init();
}

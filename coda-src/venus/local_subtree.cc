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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/coda-src/venus/RCS/local_subtree.cc,v 1.1 1996/11/22 19:11:08 braam Exp $";
#endif /*_BLURB_*/



/* this file contains local subtree representation code */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <struct.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vcrcommon.h>
#include <mond.h>

/* from venus */
#include "fso.h"
#include "local.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "worker.h"


/* ********** Mist Routines ********** */
PRIVATE void MakeDirList(long hook, char *name, long vnode, long vunique) {
    LOG(100, ("MakeDirList: Fid = 0x%x.%x.%x and Name = %s\n",
	      hook, vnode, vunique, name));
    LRDB->DirList_Insert(hook, vnode, vunique, name);
}

void lrdb::GetSubtreeStats(ViceFid *lrfid)
{
    /* Sanity checks */
    OBJ_ASSERT(this,lrfid &&  RFM_IsLocalRoot(lrfid));
    fsobj *lrobj = FSDB->Find(lrfid);
    OBJ_ASSERT(this, lrobj);

    LOG(100, ("lrdb::GetSubtreeStats: local root fid = 0x%x.%x.%x\n",
	      lrfid->Volume, lrfid->Vnode, lrfid->Unique));

    dlist stack;
    optent *opt = new optent(lrobj);		/* INIT: insert local root into stack */
    opt->SetTag(1);				/* INIT: initial subtree height is 1 */
    int size = 0, height = 0, mutation = 0;	/* INIT: initial stats value */
    stack.prepend(opt);				/* INIT: push root into the stack */
    
    while (stack.count() > 0) {			/* DFS Search */
	opt = (optent *)stack.get();		/* POP */
	fsobj *fso = opt->GetFso();		/* POP: get current fso node */
	int chgt = opt->GetTag();		/* POP: get current height */
	delete opt;				/* POP: GC search stack node */
	
	if (DIRTY(fso)) 			/* STATS: accumulate mutations if any */
	  mutation += fso->mle_bindings->count();
	size++;					/* STATS: increment subtree size */
	if (height < chgt) height = chgt;	/* STATS: increment subtree height if possible */
	
	if (fso->children != 0) {			/* PUSH: check children */
	    dlist_iterator next(*(fso->children));	/* PUSH: iterate child list */
	    dlink *d;
	    while (d = next()) {
		fsobj *cf = strbase(fsobj, d, child_link);
		opt = new optent(cf);			/* PUSH: found another child */
		opt->SetTag(chgt + 1);			/* PUSH: adjust node height */
		stack.prepend(opt);			/* PUSH: insert into the stack */
	    }
	} else {
	    if (fso->IsMtPt()) {			/* PUSH: check for mount point */
		FSO_ASSERT(this, fso->u.root);
		opt = new optent(fso->u.root);		/* PUSH: treat mount point as child */
		opt->SetTag(chgt + 1);			/* PUSH: adjust node height */
		stack.prepend(opt);			/* PUSH: insert into the stack */
	    }
	}
    }
    OBJ_ASSERT(this, stack.count() == 0);
    LOG(100, ("lrdb::GetSubtreeStats: size = %d height = %d mutation = %d\n",
	      size, height, mutation));

    /* report stats */
    subtree_stats.SubtreeNum++;
    if (subtree_stats.MaxSubtreeSize < size) subtree_stats.MaxSubtreeSize = size;
    if (subtree_stats.MaxSubtreeHgt < height) subtree_stats.MaxSubtreeHgt = height;
    if (subtree_stats.MaxMutationNum < mutation) subtree_stats.MaxMutationNum = mutation;
    subtree_stats.TotalSubtreeSize += size;
    subtree_stats.TotalSubtreeHgt += height;
    subtree_stats.TotalMutationNum += mutation;
}

/* ********** begining of lrdb methods ********** */
/* must be called from within a transaction */
ViceFid *lrdb::LGM_LookupLocal(ViceFid *global)
{
    OBJ_ASSERT(this, global);
    LOG(1000, ("lrdb::LGM_LookupLocal: global = 0x%x.%x.%x\n",
	       global->Volume, global->Vnode, global->Unique));
    lgm_iterator next(local_global_map);
    lgment *lgm;
    while (lgm = next()) {
	if (!bcmp(global, lgm->GetGlobalFid(), (int)sizeof(ViceFid))) {
	    ViceFid *local = lgm->GetLocalFid();
	    LOG(1000, ("lrdb::LGM_LookupLocal: found local = 0x%x.%x.%x\n",
		       local->Volume, local->Vnode, local->Unique));
	    return local;
	}
    }
    LOG(1000, ("lrdb::LGM_LookupLocal: can not find local\n"));
    return NULL;
}

ViceFid *lrdb::LGM_LookupGlobal(ViceFid *local)
{
    OBJ_ASSERT(this, local);
    LOG(1000, ("lrdb::LGM_LookupLocal: local = 0x%x.%x.%x\n",
	       local->Volume, local->Vnode, local->Unique));
    lgm_iterator next(local_global_map);
    lgment *lgm;
    while (lgm = next()) {
	if (!bcmp(local, lgm->GetLocalFid(), (int)sizeof(ViceFid))) {
	    ViceFid *global = lgm->GetGlobalFid();
	    LOG(1000, ("lrdb::LGM_LookupLocal: found global = 0x%x.%x.%x\n",
		       global->Volume, global->Vnode, global->Unique));
	    return global;
	}
    }
    LOG(1000, ("lrdb::LGM_LookupGlobal: can not find global\n"));
    return NULL;
}

void lrdb::LGM_Insert(ViceFid *local, ViceFid *global)
{
    OBJ_ASSERT(this, local && global);
    /* make sure that the mapping is not already in the list */
    LOG(100, ("lrdb::LGM_Insert:local = 0x%x.%x.%x. global = 0x%x.%x.%x\n",
	      local->Volume, local->Vnode, local->Unique,
	      global->Volume, global->Vnode, global->Unique));
    OBJ_ASSERT(this, !LGM_LookupLocal(global) && !LGM_LookupGlobal(local));
    lgment *lgm = new lgment(local, global);
    local_global_map.append(lgm);
}

/* must be called from within a transaction */
void lrdb::LGM_Remove(ViceFid *local, ViceFid *global)
{
    OBJ_ASSERT(this, local && global);
    LOG(100, ("lrdb::LGM_Remove:local = 0x%x.%x.%x. global = 0x%x.%x.%x\n",
	      local->Volume, local->Vnode, local->Unique,
	      global->Volume, global->Vnode, global->Unique));
    /* make sure the mapping is there and correct */
    lgm_iterator next(local_global_map);
    lgment *lgm, *target = (lgment *)NULL;
    while (lgm = next()) {
	if (!bcmp(local, lgm->GetLocalFid(), (int)sizeof(ViceFid)) &&
	    !bcmp(global, lgm->GetGlobalFid(), (int)sizeof(ViceFid))) {
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
ViceFid *lrdb::RFM_LookupGlobalRoot(ViceFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding global-root-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupGlobalRoot: FakeRootFid = 0x%x.%x.%x\n",
	       FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), FakeRootFid, (int)sizeof(ViceFid))) {
	    LOG(1000, ("lrdb::RFM_LookupGlobalRoot: found\n"));
	    return rfm->GetGlobalRootFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupGlobalRoot: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
ViceFid *lrdb::RFM_LookupLocalRoot(ViceFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding local-root-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupLocalRoot: FakeRootFid = 0x%x.%x.%x\n",
	       FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), FakeRootFid, (int)sizeof(ViceFid))) {
	    LOG(1000, ("lrdb::RFM_LookupLocalRoot: found\n"));
	    return rfm->GetLocalRootFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupLocalRoot: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
ViceFid *lrdb::RFM_LookupRootParent(ViceFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding root-parent-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupRootParent: FakeRootFid = 0x%x.%x.%x\n",
	       FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), FakeRootFid, (int)sizeof(ViceFid))) {
	    LOG(1000, ("lrdb::RFM_LookupRootParent: found\n"));
	    return rfm->GetRootParentFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupParentRoot: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
ViceFid *lrdb::RFM_LookupGlobalChild(ViceFid *FakeRootFid)
{
    /* given the fake-root-fid, find out its corresponding global-child-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupGlobalChild: FakeRootFid = 0x%x.%x.%x\n",
	       FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), FakeRootFid, (int)sizeof(ViceFid))) {
	    LOG(1000, ("lrdb::RFM_LookupGlobalChild: found\n"));
	    return rfm->GetGlobalChildFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupGlobalChild: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
ViceFid *lrdb::RFM_LookupLocalChild(ViceFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding local-child-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupLocalChild: FakeRootFid = 0x%x.%x.%x\n",
	       FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), FakeRootFid, (int)sizeof(ViceFid))) {
	    LOG(1000, ("lrdb::RFM_LookupLocalChild: found\n"));
	    return rfm->GetLocalChildFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_LookupLocalChild: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
fsobj *lrdb::RFM_LookupRootMtPt(ViceFid *FakeRootFid)
{
    /* given fake-root-fid, find out the corresponding root-mtpt */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_LookupRootMtPt: FakeRootFid = 0x%x.%x.%x\n",
	       FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), FakeRootFid, (int)sizeof(ViceFid))) {
	    LOG(1000, ("lrdb::RFM_LookupLocalChild: found\n"));
	    return rfm->GetRootMtPt();
	}
    }
    LOG(1000, ("lrdb::RFM_LookupRootMtPt: not found\n"));
    return NULL;
}

/* need not be called from within a transaction */
ViceFid *lrdb::RFM_ParentToFakeRoot(ViceFid *RootParentFid)
{
    /* given root-parent-fid, find out the corresponding fake-root-fid */
    OBJ_ASSERT(this, RootParentFid);
    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: RootParentFid = 0x%x.%x.%x\n",
	       RootParentFid->Volume, RootParentFid->Vnode, RootParentFid->Unique));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetRootParentFid(), RootParentFid, (int)sizeof(ViceFid))) {
	    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: found\n"));
	    return rfm->GetFakeRootFid();
	} 
    }    
    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: not found\n"));
    return NULL;    
}

/* need not be called from within a transaction */
ViceFid *lrdb::RFM_FakeRootToParent(ViceFid *FakeRootFid)
{
    /* given the root-parent-fid, find out its corresponding fake-root-fid */
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: FakeRootFid = 0x%x.%x.%x\n",
	       FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), FakeRootFid, (int)sizeof(ViceFid))) {
	    LOG(1000, ("lrdb::RFM_ParentToFakeRoot: found\n"));
	    return rfm->GetRootParentFid();
	} 
    }
    LOG(1000, ("lrdb::RFM_FakeRootToParent: not found\n"));
    return NULL;
}

void lrdb::RFM_CoverRoot(ViceFid *FakeRootFid)
{
    /* FakeRootFid must be a fake-root-fid already in the map */
    OBJ_ASSERT(this, FakeRootFid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), FakeRootFid, (int)sizeof(ViceFid))) {
	    ATOMIC(
		   rfm->CoverRoot();
	    , MAXFP)
	    return;
	}
    }
    OBJ_ASSERT(this, 0);
}

/* need not be called from within a transaction */
int lrdb::RFM_IsRootParent(ViceFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetRootParentFid(), Fid, (int)sizeof(ViceFid))) {
	    return 1;
	}
    }
    return 0;
}

/* need not be called from within a transaction */
int lrdb::RFM_IsFakeRoot(ViceFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!bcmp(rfm->GetFakeRootFid(), Fid, (int)sizeof(ViceFid))) {
	    return 1;
	}
    }
    return 0;
}

int lrdb::RFM_IsGlobalRoot(ViceFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (rfm->RootCovered()) continue;
	if (!bcmp(rfm->GetGlobalRootFid(), Fid, (int)sizeof(ViceFid))) {
	    return 1;
	}
    }
    return 0;
}

int lrdb::RFM_IsGlobalChild(ViceFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (rfm->RootCovered()) continue;
	if (!bcmp(rfm->GetGlobalChildFid(), Fid, (int)sizeof(ViceFid))) {
	    return 1;
	}
    }
    return 0;
}

int lrdb::RFM_IsLocalRoot(ViceFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (rfm->RootCovered()) continue;
	if (!bcmp(rfm->GetLocalRootFid(), Fid, (int)sizeof(ViceFid))) {
	    return 1;
	}
    }
    return 0;
}

int lrdb::RFM_IsLocalChild(ViceFid *Fid)
{
    OBJ_ASSERT(this, Fid);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (rfm->RootCovered()) continue;
	if (!bcmp(rfm->GetLocalChildFid(), Fid, (int)sizeof(ViceFid))) {
	    return 1;
	}
    }
    return 0;
}

/* must be called from within a transaction */
void lrdb::RFM_Insert(ViceFid *FakeRootFid, ViceFid *GlobalRootFid, ViceFid *LocalRootFid, 
		      ViceFid *RootParentFid, ViceFid *GlobalChildFid,
		      ViceFid *LocalChildFid, char *Name)
{
    OBJ_ASSERT(this, FakeRootFid && GlobalRootFid && LocalRootFid && RootParentFid);
    OBJ_ASSERT(this, GlobalChildFid && LocalChildFid && Name);
    LOG(1000, ("lrdb::RFM_Insert: FakeRootFid = 0x%x.%x.%x\n", FakeRootFid->Volume, 
	       FakeRootFid->Vnode, FakeRootFid->Unique));
    LOG(1000, ("lrdb::RFM_Intert: GlobalRootFid = 0x%x.%x.%x LocalRootFid = 0x%x.%x.%x\n",
	       GlobalRootFid->Volume, GlobalRootFid->Vnode, GlobalRootFid->Unique,
	       LocalRootFid->Volume, LocalRootFid->Vnode, LocalRootFid->Unique));
    LOG(1000, ("lrdb::RFM_Intert: GlobalChildFid = 0x%x.%x.%x LocalChildFid = 0x%x.%x.%x\n",
	       GlobalChildFid->Volume, GlobalChildFid->Vnode, GlobalChildFid->Unique,
	       LocalChildFid->Volume, LocalChildFid->Vnode, LocalChildFid->Unique));
    LOG(1000, ("lrdb::RFM_Insert: RootParentFid = 0x%x.%x.%x Name = %s\n",
	       RootParentFid->Volume, RootParentFid->Vnode, RootParentFid->Unique, Name));
    rfment *rfm = new rfment(FakeRootFid, GlobalRootFid, LocalRootFid, RootParentFid, 
			     GlobalChildFid, LocalChildFid, Name);
    root_fid_map.insert(rfm);
}

/* must be called from within a transaction */
void lrdb::RFM_Remove(ViceFid *FakeRootFid)
{
    OBJ_ASSERT(this, FakeRootFid);
    LOG(1000, ("RFM_Remove: FakeRootFid = 0x%x.%x.%x\n",
	       FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
    /* first find out the map entry */
    rfm_iterator next(root_fid_map);
    rfment *rfm, *target = (rfment *)NULL;
    while (rfm = next()) {
	if (!bcmp(FakeRootFid, rfm->GetFakeRootFid(), (int)sizeof(ViceFid))) {
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
ViceFid lrdb::GenerateLocalFakeFid(ViceDataType fidtype)
{
    /* generate local fid used in the faked subtrees */
    ViceFid fid;
    fid.Volume = LocalFakeVid;
    fid.Vnode = (fidtype == Directory) ? LocalDirVnode : LocalFileVnode;
    RVMLIB_REC_OBJECT(local_fid_unique_gen);
    fid.Unique = local_fid_unique_gen++;
    LOG(1000, ("lrdb::GenerateLocalFakeFid: return 0x%x.%x.%x\n",
	       fid.Volume, fid.Vnode, fid.Unique));
    return fid;
}

/* must be called from within a transaction */
ViceFid lrdb::GenerateFakeLocalFid()
{
    /* generate a fake-fid whose volume is the special "Local" volume */
    ViceFid fid;
    fid.Volume = LocalFakeVid;
    fid.Vnode = FakeVnode;
    RVMLIB_REC_OBJECT(local_fid_unique_gen);
    fid.Unique = local_fid_unique_gen++;
    LOG(1000, ("lrdb::GenerateFakeLocalFid: return 0x%x.%x.%x\n",
	       fid.Volume, fid.Vnode, fid.Unique));
    return fid;
}

/* must be called from within a transaction */
void lrdb::TranslateFid(ViceFid *OldFid, ViceFid *NewFid)
{
    OBJ_ASSERT(this, OldFid && NewFid);
    LOG(100, ("lrdb::TranslateFid: OldFid = 0x%x.%x.%x NewFid = 0x%x.%x.%x\n",
	      OldFid->Volume, OldFid->Vnode, OldFid->Unique,
	      NewFid->Volume, NewFid->Vnode, NewFid->Unique));

    {	/* translate fid for the local-global fid map list */
	if (!IsLocalFid(NewFid)) {
	    /* 
	     * only when NewFid is not a local fid, i.e., the fid replacement
	     * was caused cmlent::realloc, instead of cmlent::LocalFakeify. 
	     */
	    lgm_iterator next(local_global_map);
	    lgment *lgm;
	    while (lgm = next()) {
		if (!bcmp(lgm->GetGlobalFid(), OldFid, (int)sizeof(ViceFid))) {
		    lgm->SetGlobalFid(NewFid);
		}
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
    while (rfm = next()) {
	ViceFid *FakeRootFid = rfm->GetFakeRootFid();
	ViceFid *GlobalRootFid = rfm->GetGlobalRootFid();
	ViceFid *LocalRootFid = rfm->GetLocalRootFid();
	ViceFid *LocalChildFid = rfm->GetLocalChildFid();
	ViceFid *GlobalChildFid = rfm->GetGlobalChildFid();
	LOG(1000, ("lrdb::PurgeRootFids: FakeRootFid = 0x%x.%x.%x\n",
		   FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique));
	LOG(1000, ("lrdb::PurgeRootFids: GlobalRootFid = 0x%x.%x.%x\n",
		   GlobalRootFid->Volume, GlobalRootFid->Vnode, GlobalRootFid->Unique));
	LOG(1000, ("lrdb::PurgeRootFids: LocalRootFid = 0x%x.%x.%x\n",
		   LocalRootFid->Volume, LocalRootFid->Vnode, LocalRootFid->Unique));
	LOG(1000, ("lrdb::PurgeRootFids: GlobalChildFid = 0x%x.%x.%x\n",
		   GlobalChildFid->Volume, GlobalChildFid->Vnode, GlobalChildFid->Unique));
	LOG(1000, ("lrdb::PurgeRootFids: LocalChildFid = 0x%x.%x.%x\n",
		   LocalChildFid->Volume, LocalChildFid->Vnode, LocalChildFid->Unique));
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
    while (dir = (vdirent *)dir_list.get())
      delete dir;
    OBJ_ASSERT(this, dir_list.count() == 0);
}

/* need not be called from within a transaction */
void lrdb::DirList_Insert(VolumeId Volume, VnodeId Vnode, Unique_t Unique, char *Name)
{
    OBJ_ASSERT(this, Name);
    LOG(1000, ("lrdb::DirList_Insert: Fid = 0x%x.%x.%x and Name = %s\n",
	       Volume, Vnode, Unique, Name));
    vdirent *newdir = new vdirent(Volume, Vnode, Unique, Name);
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
    ViceFid *ParentFid = &DirObj->fid;
    LOG(1000, ("lrdb::DirList_Process: DirObj = 0x%x.%x.%x\n",
	       ParentFid->Volume, ParentFid->Vnode, ParentFid->Unique));
    dir_iterator next(dir_list);
    vdirent *dir;
    while (dir = next()) {
	char *ChildName = dir->GetName();
	if (!strcmp(ChildName, ".") || !strcmp(ChildName, ".."))
	  continue;
	ViceFid *ChildFid = dir->GetFid();
	if (FSDB->Find(ChildFid) == NULL) {
	    /* found an un-cached child object in Parent */
	    LOG(100, ("lrdb::DirList_Process: found uncached child object (%s, 0x%x.%x.%x)\n",
		      ChildName, ChildFid->Volume, ChildFid->Vnode, ChildFid->Unique));
	    /* repalce ChildFid with a local fid */
	    ViceFid LocalFid;
	    ATOMIC(
		   /* we don't know the type of ChildFid, just assume it is a file */
		   LocalFid = GenerateLocalFakeFid(File);
		   /* replace (ChildName, ChildFid) with (ChildName, LocalFid) */
		   DirObj->dir_Delete(ChildName);
		   DirObj->dir_Create(ChildName, &LocalFid);
	    , MAXFP)
	} else {
	    LOG(100, ("lrdb::DirList_Process: found cached child object (%s, 0x%x.%x.%x)\n",
		      ChildName, ChildFid->Volume, ChildFid->Vnode, ChildFid->Unique));	    
	}
    }
}


/* must be called from within a transaction */
void *lrdb::operator new(size_t len)
{
    lrdb *l = 0;

    l = (lrdb *)RVMLIB_REC_MALLOC((int) len);
    assert(l);
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
    bzero(&dir_list, (int)sizeof(dlist));
    repair_root_fid = (ViceFid *)NULL;
    current_search_cml = (cmlent *)NULL;
    repair_session_mode = REP_SCRATCH_MODE;
    subtree_view = SUBTREE_MIXED_VIEW;
    repair_session_tid = - repair_tid_gen;
    bzero(&repair_obj_list, (int)sizeof(dlist));
    bzero(&repair_vol_list, (int)sizeof(dlist));
    bzero(&repair_cml_list, (int)sizeof(dlist));
    bzero(&subtree_stats, (int)sizeof(LocalSubtreeStats));
    bzero(&repair_stats, (int)sizeof(RepairSessionStats));
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
	fdprint(fd, "repair_root_fid = 0x%x.%x.%x\n", repair_root_fid->Volume, 
		repair_root_fid->Vnode, repair_root_fid->Unique);	
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
	while (opt = next()) {
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
	while (vpt = next()) {
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
	while (mpt = next()) {
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
	while (lgm = next()) {
	    lgm->print(fd);
	    fdprint(fd, "\n");
	}
    }
    {	/* print out every entry of the global-local-fid map */
	fdprint(fd, "there are %d entries in the root-fid map\n", root_fid_map.count());
	rfm_iterator next(root_fid_map);
	rfment *rfm;
	while (rfm = next()) {
	    fdprint(fd, "====================================\n");
	    rfm->print(fd);
	    fdprint(fd, "====================================\n");
	}
    }
    {	/* print out the stats */
	fdprint(fd, "subtree_stats = [%d %d %d %d %d %d %d]\n", subtree_stats.SubtreeNum, 
		subtree_stats.MaxSubtreeSize, subtree_stats.MaxSubtreeHgt, 
		subtree_stats.MaxMutationNum, subtree_stats.TotalSubtreeSize,
		subtree_stats.TotalSubtreeHgt, subtree_stats.TotalMutationNum);
    
	fdprint(fd, "repair_stats = [%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d]\n",
		repair_stats.SessionNum, repair_stats.CommitNum,
		repair_stats.AbortNum, repair_stats.CheckNum,
		repair_stats.PreserveNum, repair_stats.DiscardNum,
		repair_stats.RemoveNum, repair_stats.GlobalViewNum,
		repair_stats.LocalViewNum, repair_stats.KeepLocalNum,
		repair_stats.ListLocalNum, repair_stats.RepMutationNum,
		repair_stats.MissTargetNum, repair_stats.MissParentNum,
		repair_stats.AclDenyNum, repair_stats.UpdateUpdateNum,
		repair_stats.NameNameNum, repair_stats.RemoveUpdateNum);
    }
}

/* ********** end of lrdb methods ********** */


/* ********** begining of lgment methods ********** */
void *lgment::operator new(size_t len)
{
    lgment *l = 0;
    
    l = (lgment *)RVMLIB_REC_MALLOC((int)len);
    assert(l);
    return(l);
}

lgment::lgment(ViceFid *l, ViceFid *g)
{
    OBJ_ASSERT(this, l && g);
    LOG(1000, ("lgment::lgment: Local = 0x%x.%x.%x Global = 0x%x.%x.%x\n",
	       l->Volume, l->Vnode, l->Unique, g->Volume, g->Vnode, g->Unique));

    RVMLIB_REC_OBJECT(*this);
    bcopy(l, &local, (int)sizeof(ViceFid));
    bcopy(g, &global, (int)sizeof(ViceFid));
}

lgment::~lgment()
{
    LOG(1000, ("lgment::~lgment: Local = 0x%x.%x.%x Global = 0x%x.%x.%x\n",
	       local.Volume, local.Vnode, local.Unique, 
	       global.Volume, global.Vnode, global.Unique));
    /* nothing to do! */
}

void lgment::operator delete(void *deadobj, size_t len)
{
    RVMLIB_REC_FREE(deadobj);
}

ViceFid *lgment::GetLocalFid()
{
    return &local;
}

ViceFid *lgment::GetGlobalFid()
{	
    return &global;
}

/* must be called from within a transation */
void lgment::SetLocalFid(ViceFid *lfid)
{
    OBJ_ASSERT(this, lfid);
    RVMLIB_REC_OBJECT(local);
    bcopy(lfid, &local, (int)sizeof(ViceFid));
}

/* must be called from within a transation */
void lgment::SetGlobalFid(ViceFid *gfid)
{
    OBJ_ASSERT(this, gfid);
    RVMLIB_REC_OBJECT(global);
    bcopy(gfid, &global, (int)sizeof(ViceFid));
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
    fdprint(fd, "(local = 0x%x.%x.%x ", local.Volume, local.Vnode, local.Unique);
    fdprint(fd, "global = 0x%x.%x.%x)", global.Volume, global.Vnode, global.Unique);
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
    r = (rfment *)RVMLIB_REC_MALLOC((int)sizeof(rfment));
    assert(r);
    return(r);
}

/* must be called from within a transaction */
rfment::rfment(ViceFid *Fake, ViceFid *Global, ViceFid *Local, ViceFid *Parent, 
	       ViceFid *GlobalChild, ViceFid *LocalChild, char *CompName)
{
    OBJ_ASSERT(this, Fake && Global && Local && Parent && GlobalChild && LocalChild && CompName);
    LOG(1000, ("rfment::rfment:: FakeRootFid = 0x%x.%x.%x\n",
	       Fake->Volume, Fake->Vnode, Fake->Unique));
    LOG(1000, ("rfment::rfment:: GlobalRootFid = 0x%x.%x.%x\n",
	       Global->Volume, Global->Vnode, Global->Unique));
    LOG(1000, ("rfment::rfment:: LocalRootFid = 0x%x.%x.%x\n",
	       Local->Volume, Local->Vnode, Local->Unique));
    LOG(1000, ("rfment::rfment:: RootParentFid = 0x%x.%x.%x\n",
	       Parent->Volume, Parent->Vnode, Parent->Unique));
    LOG(1000, ("rfment::rfment:: GlobalChildFid = 0x%x.%x.%x\n",
	       GlobalChild->Volume, GlobalChild->Vnode, GlobalChild->Unique));
    LOG(1000, ("rfment::rfment:: LocalChildFid = 0x%x.%x.%x\n",
	       LocalChild->Volume, LocalChild->Vnode, LocalChild->Unique));
    LOG(1000, ("rfment::rfment:: CompName = %s\n", CompName));


    RVMLIB_REC_OBJECT(*this);
    bcopy(Fake, &fake_root_fid, (int)sizeof(ViceFid));
    bcopy(Global, &global_root_fid, (int)sizeof(ViceFid));
    bcopy(Local, &local_root_fid, (int)sizeof(ViceFid));
    bcopy(Parent, &root_parent_fid, (int)sizeof(ViceFid));
    bcopy(GlobalChild, &global_child_fid, (int)sizeof(ViceFid));
    bcopy(LocalChild, &local_child_fid, (int)sizeof(ViceFid));
    name = (char *)RVMLIB_REC_MALLOC((int)(strlen(CompName) + 1));
    assert(name);
    strcpy(name, CompName);
    view = SUBTREE_MIXED_VIEW;
    covered = 0;
    root_mtpt = 0;
}

/* must be called from within a transaction */
rfment::~rfment()
{
    LOG(1000, ("rfment::~rfment()\n"));
    RVMLIB_REC_FREE(name);
}

/* must be called from within a transaction */
void rfment::operator delete(void *deadobj, size_t len)
{
    LOG(1000, ("rfment::operator delete\n"));
    RVMLIB_REC_FREE(deadobj);
}

ViceFid *rfment::GetFakeRootFid()
{
    return &fake_root_fid;
}

ViceFid *rfment::GetGlobalRootFid()
{
    return &global_root_fid;
}

ViceFid *rfment::GetLocalRootFid()
{
    return &local_root_fid;
}

ViceFid *rfment::GetRootParentFid()
{
    return &root_parent_fid;
}

ViceFid *rfment::GetGlobalChildFid()
{
    return &global_child_fid;
}

ViceFid *rfment::GetLocalChildFid()
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
    fdprint(fd, "\tfake_root_fid = 0x%x.%x.%x\n",
	    fake_root_fid.Volume, fake_root_fid.Vnode, fake_root_fid.Unique);
    fdprint(fd, "\tglobal_root_fid = 0x%x.%x.%x\n",
	    global_root_fid.Volume, global_root_fid.Vnode, global_root_fid.Unique);
    fdprint(fd, "\tlocal_root_fid = 0x%x.%x.%x\n",
	    local_root_fid.Volume, local_root_fid.Vnode, local_root_fid.Unique);
    fdprint(fd, "\troot_parent_fid = 0x%x.%x.%x\n",
	    root_parent_fid.Volume, root_parent_fid.Vnode, root_parent_fid.Unique);
    fdprint(fd, "\tglobal_child_fid = 0x%x.%x.%x\n",
	    global_child_fid.Volume, global_child_fid.Vnode, global_child_fid.Unique);
    fdprint(fd, "\tLocalChildFid = 0x%x.%x.%x\n",
	    local_child_fid.Volume, local_child_fid.Vnode, local_child_fid.Unique);
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
vdirent::vdirent(VolumeId Volume, VnodeId Vnode, Unique_t Unique, char *Name)
{
    OBJ_ASSERT(this, name);
    LOG(10, ("vdirent::vdirent: fid = 0x%x.%x.%x and name = %s\n",
	     Volume, Vnode, Unique, name));
    fid.Volume = Volume;
    fid.Vnode = Vnode;
    fid.Unique = Unique;
    strcpy(name, Name);
}

vdirent::~vdirent()
{
}

ViceFid *vdirent::GetFid()
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
    fdprint(fd, "fid = 0x%x.%x.%x and name = %s\n",
	    fid.Volume, fid.Vnode, fid.Unique, name);
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
vptent::vptent(volent *v) 
{
    OBJ_ASSERT(this, v);
    vpt = v;
}

vptent::~vptent()
{
}

volent *vptent::GetVol()
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
	TRANSACTION(
		    RVMLIB_REC_OBJECT(LRDB);
		    LRDB = new lrdb;
	)
    } else {
	LRDB->ResetTransient();

	{   /* check local-subtree root nodes */
	    rfm_iterator next(LRDB->root_fid_map);
	    rfment *rfm;
	    while (rfm = next()) {
		if (rfm->RootCovered() || (rfm->GetView() == SUBTREE_MIXED_VIEW)) 
		  continue;
		ViceFid *FakeRootFid = rfm->GetFakeRootFid();
		ViceFid *RootParentFid = rfm->GetRootParentFid();
		fsobj *RootParentObj = FSDB->Find(RootParentFid);
		ASSERT(RootParentObj && RootParentObj->IsLocalObj());
		char *Name = rfm->GetName();
		RootParentObj->RecoverRootParent(FakeRootFid, Name);
		ATOMIC(
		       rfm->SetView(SUBTREE_MIXED_VIEW);
		, MAXFP)
	    }
	}

	{   /* GC local-repair mutations (if any) */
	    vol_iterator next;
	    volent *vol;
	    while (vol = next()) {
		vol->ClearRepairCML();
	    }
	}
    }

    /* fire up the daemon */
    LRD_Init();
}

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


/* this file contains code for local repair routines */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vcrcommon.h>

/* venus */
#include "fso.h"
#include "local.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"


/* ********** beginning of lrdb methods ********** */
/* must not be called from within a transaction */


/*
  BEGIN_HTML
  <a name="beginrepair"><strong> begining a local-global repair session </strong></a>
  END_HTML
*/
void lrdb::BeginRepairSession(VenusFid *RootFid, int RepMode, char *msg)
{
    /*
     * IN:  RootFid is the fid of the new repair session's subtree root node.
     *	    RepMode is the mode (scratch or direct) of the repair session.
     * OUT: msg is the string that contains the error code to the caller.
     */
    OBJ_ASSERT(this, RootFid && !FID_IsLocalFake(RootFid) && msg);
    LOG(100, ("lrdb::BeginRepairSession: RootFid = %s RepMode = %d\n",
	      FID_(RootFid), RepMode));

    if (repair_root_fid) {
	strcpy(msg, "1"); /* local/global repair session already in progress */
	return;
    }

    if (!RFM_IsFakeRoot(RootFid)) {
	strcpy(msg, "2"); /* server/server repair session */
	return;	
    }

    /* Need to do a strcpy(msg, "3"); somewhere about here when 
     * expanding global => verdi, mozart, marais (or the like) 
     * 
     * At some point, we might want to do away with this
     * "magic number" stuff and use an enum type or #define's
     */

    repair_root_fid = RootFid;
    repair_session_mode = RepMode;	/* default repair session mode */

    {	/* iterate the RFM map and set the repair session initial view */
	rfm_iterator next(root_fid_map);
	rfment *rfm;
	while ((rfm = next())) {
	    if (FID_EQ(rfm->GetFakeRootFid(), repair_root_fid)) {
		Recov_BeginTrans();
		RVMLIB_REC_OBJECT(subtree_view);
		subtree_view = rfm->GetView();
		Recov_EndTrans(MAXFP);
		break;
	    }
	}
    }

    if (repair_session_mode == REP_SCRATCH_MODE) { /* set cml-tid for repair mutation */
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(repair_tid_gen);
	repair_tid_gen++;
	Recov_EndTrans(MAXFP);
	repair_session_tid = - repair_tid_gen;
    } else {
	repair_session_tid = -1;
    }

    InitCMLSearch(RootFid);

    /* set repair flag for relavant volumes */
    vpt_iterator next(repair_vol_list);
    vptent *vpt;
    while ((vpt = next())) {
	repvol *vol = vpt->GetVol();
	VolumeId Vols[VSG_MEMBERS];
	uid_t LockUids[VSG_MEMBERS];
	unsigned long LockWSs[VSG_MEMBERS];
	vol->EnableRepair(ALL_UIDS, Vols, LockUids, LockWSs);
    }

    strcpy(msg, "0"); /* local/global repair session successfully begun! */
}

/*
  BEGIN_HTML
  <a name="endrepair"><strong> actions to end a repair session </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
void lrdb::EndRepairSession(int Commit, char *msg)
{
    /*
     * IN:  Commit is the integer indicating whether to commit repair mutations
     * OUT: msg is the string containing error message.
     */
    OBJ_ASSERT(this, (Commit == 0 || Commit == 1) && msg != NULL);
    int rc, delocalization;

    /* initialize the return message */
    sprintf(msg, "repair session completed");
    if (repair_root_fid == NULL) {
	sprintf(msg, "there is no on-going repair session");
	return;
    }

    if (subtree_view != SUBTREE_MIXED_VIEW) {
	sprintf(msg, "must set mixed repair-view");
	return;
    }

    /* decide whether we need to cleanup the localized subtrees */
    if (current_search_cml != NULL) {
	LOG(100, ("lrdb::EndRepairSession: unprocessed local mutation left\n"));
	delocalization = 0;
    } else {
	if (repair_session_mode == REP_DIRECT_MODE) {
	    delocalization = 1;
	} else {
	    if (Commit)
	      delocalization = 1;
	    else
	      delocalization = 0;
	}
    }
    LOG(100, ("lrdb::EndRepairSession: no delocalization = %d\n", delocalization));

    {	/* step 1: Commit(reintegrate) or Abort repair mutation for REP_SCRATCH_MODE sessions */
	if (repair_session_mode == REP_SCRATCH_MODE) {
	    if (Commit) {
		vpt_iterator next(repair_vol_list);
		vptent *vpt;
		while ((vpt = next())) {
		    repvol *vol = vpt->GetVol();
		    OBJ_ASSERT(this, vol);
		    LOG(100, ("lrdb::EndRepairSession: reintegrate mutation for volume %x\n",
			      vol->GetVolumeId()));
		    {	/* freeze cml entries */
			cml_iterator next(*(vol->GetCML()), CommitOrder);
			cmlent *m;
			Recov_BeginTrans();
			while ((m = next()))
			  if (m->GetTid() == repair_session_tid)
			    m->Freeze();
			Recov_EndTrans(MAXFP);
		    }
		    CODA_ASSERT(vol->IsReplicated());
		    rc = ((repvol *)vol)->IncReintegrate(repair_session_tid);
		    if (rc != 0) {
			sprintf(msg, "commit failed(%d) on volume %lx",
				rc, vol->GetVolumeId());
			return;
		    }
		}
	    } else {
		LOG(100, ("lrdb::EndRepairSession: abort repair mutations in CML\n"));
		vpt_iterator next(repair_vol_list);
		vptent *vpt;	
		while ((vpt = next())) {
		    repvol *vol = vpt->GetVol();
		    OBJ_ASSERT(this, vol);
		    vol->IncAbort(repair_session_tid);
		}
	    }
	}
    }

    {	/* step 2: cleanup repair_cml_list */
	LOG(100, ("lrdb::EndRepairSession: total %d elements in repair_cml_list\n", repair_cml_list.count()));

	/* remove both mptent and its pointed cmlent objects up until current_search_cml */
	mptent *mpt;
	int dcount = 0, rcount = 0;
	while ((mpt = (mptent *)repair_cml_list.get()) && (mpt->GetCml() != current_search_cml)) {
	    cmlent *m = mpt->GetCml();
	    if (Commit) {
		Recov_BeginTrans();
		delete m;
		Recov_EndTrans(MAXFP);
	    }
	    delete mpt;
	    dcount++;
	}

	while ((mpt = (mptent *)repair_cml_list.get())) {
	    delete mpt;
	    rcount++;
	}
	OBJ_ASSERT(this, repair_cml_list.count() == 0);
	LOG(100, ("lrdb::EndRepairSession: %d mutations processed and %d left\n", dcount, rcount));
    }
    
    {	/* step 3: de-localization the subtree */
	if (delocalization) {
	    DeLocalization();
	}
    }

    {	/* step 4: check every involved volume's state transition and CML state */
	vpt_iterator next(repair_vol_list);
	vptent *vpt;	
	while ((vpt = next())) {
	    repvol *vol = vpt->GetVol();
	    OBJ_ASSERT(this, vol);
	    vol->CheckTransition();
	    (void)vol->DisableRepair(ALL_UIDS);
	}
    }

    {	/* step 5: Misc Garbage Collection */
	optent *opt;
	while ((opt = (optent *)repair_obj_list.get())) 
	  delete opt;
	OBJ_ASSERT(this, repair_obj_list.count() == 0);

	vptent *vpt;
	while ((vpt = (vptent *)repair_vol_list.get()))
	  delete vpt;
	OBJ_ASSERT(this, repair_vol_list.count() == 0);

	/* reset session variables */
	current_search_cml = NULL;
	repair_root_fid = NULL;
    }
}

/*
  BEGIN_HTML
  <a name="checklocal"><strong> the checklocal repair command is implemented by 
  the ContinueRepairSession() method. </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void lrdb::ContinueRepairSession(char *msg)
{
    OBJ_ASSERT(this, msg != NULL);

    {	/* sanity checks */
	if (repair_root_fid == NULL) {
	    sprintf(msg,"there is no ongoing repair session");
	    return;
	}

	if (subtree_view != SUBTREE_MIXED_VIEW) {
	    sprintf(msg, "must set mixed repair-view");
	    return;
	}

	if (current_search_cml == NULL) {
	    sprintf(msg, "all local mutations processed");
	    return;
	}
    }
    {   /* perform local mutation checks, produce repair tool message */
        char opmsg[1024];
        char checkmsg[1024];
        int mcode, rcode;
        current_search_cml->GetLocalOpMsg(opmsg);
        current_search_cml->CheckRepair(checkmsg, &mcode, &rcode);
        sprintf(msg, "local mutation: %s\n%s", opmsg, checkmsg);
    }
}

/*
  BEGIN_HTML
  <a name="discard"><strong> discard the current mutation operation </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void lrdb::DiscardLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
    if (current_search_cml == NULL) {
	sprintf(msg, "no further mutation left\n");
	return;
    }
    if (subtree_view != SUBTREE_MIXED_VIEW) {
	sprintf(msg, "must set mixed repair-view");
	return;
    }
    char opmsg[1024];
    current_search_cml->GetLocalOpMsg(opmsg);
    sprintf(msg, "discard local mutation %s", opmsg);
    AdvanceCMLSearch();
}

/*
  BEGIN_HTML
  <a name="discardall"><strong> discard all the local mutation operations </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void lrdb::DiscardAllLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
    if (current_search_cml == NULL) {
	sprintf(msg, "no further mutation left\n");
	return;
    }
    if (subtree_view != SUBTREE_MIXED_VIEW) {
	sprintf(msg, "must set mixed repair-view");
	return;
    }
    sprintf(msg, "discard all local mutations");
    current_search_cml = NULL;
}

/*
  BEGIN_HTML
  <a name="preserve"><strong> preserving the current mutation operation </strong></a> 
  END_HTML
*/ 
/* need not be called from within a transaction */
void lrdb::PreserveLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
    if (current_search_cml == NULL) {
	sprintf(msg, "no futher mutation left\n");
	return;
    }    
    if (subtree_view != SUBTREE_MIXED_VIEW) {
	sprintf(msg, "must set mixed repair-view");
	return;
    }
    char opmsg[1024], checkmsg[1024];
    current_search_cml->GetLocalOpMsg(opmsg);

    int mcode, rcode;
    current_search_cml->CheckRepair(checkmsg, &mcode, &rcode);
    if (rcode == REPAIR_FAILURE) {
	/* it is impossible to perform the orignial local mutation */
	sprintf(msg, "%s\n can not re-do %s in the global area", checkmsg, opmsg);
	return;
    }

    int rc = current_search_cml->DoRepair(msg, rcode);
    if (rc == 0) {
	AdvanceCMLSearch();
    } 
}

/*
  BEGIN_HTML
  <a name="preserveall"><strong> repeatedly preserve the current
  mutation operation and advance to the next mutation operation until
  the end </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void lrdb::PreserveAllLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
    if (current_search_cml == NULL) {
	sprintf(msg, "no futher mutation left\n");
	return;
    }    
    if (subtree_view != SUBTREE_MIXED_VIEW) {
	sprintf(msg, "must set mixed repair-view");
	return;
    }

    char opmsg[1024], checkmsg[1024];    
    int mcode, rcode, rc, opcnt = 0;
    while (current_search_cml != NULL) {
	current_search_cml->GetLocalOpMsg(opmsg);
	if (current_search_cml->GetTid() > 0) {
	    sprintf(msg, "%s belongs to transaction %d\n %d local mutation(s) replayed\n",
		    opmsg, current_search_cml->GetTid(), opcnt);
	    return;
	}
	mcode = 0;
	current_search_cml->CheckRepair(checkmsg, &mcode, &rcode);
	if (rcode == REPAIR_FAILURE) {
	    /* it is impossible to perform the orignial local mutation */	    
	    sprintf(msg, "%d local mutation(s) redone in the global subtree\n %s\n can not re-do %s in the global replica", opcnt, checkmsg, opmsg);
	    return;
	}
	/* mcode is left set when CheckRepair found a non-fatal error */
	if (mcode || !(rc = current_search_cml->DoRepair(checkmsg, rcode))) {
	    AdvanceCMLSearch();
	    opcnt++;
	} else {
	    sprintf(msg, "%d local mutation(s) redone in the global subtree\n %s\n can not re-do %s in the global replica", opcnt, checkmsg, opmsg);
	    return;
	}
    }
    sprintf(msg, "All %d local mutation(s) redone in the global subtree\n", opcnt);
}

/*
  BEGIN_HTML
  <a name="cmllist"><strong> constuct a list of local mutation
  operations that are associated with the current object being repaired </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
void lrdb::InitCMLSearch(VenusFid *FakeRootFid)
{
    OBJ_ASSERT(this, FakeRootFid);
    OBJ_ASSERT(this, repair_vol_list.count() == 0);
    OBJ_ASSERT(this, current_search_cml == NULL);
    LOG(100, ("lrdb::InitCMLSearch: FakeRootFid = %s\n", FID_(FakeRootFid)));
    VenusFid *LocalRootFid = RFM_LookupLocalRoot(FakeRootFid);
    VenusFid *GlobalRootFid = RFM_LookupGlobalRoot(FakeRootFid);
    OBJ_ASSERT(this, LocalRootFid && GlobalRootFid);

    {	/* first gather all related volumes and local objects by traversing the subtree */
	fsobj *LocalRoot = FSDB->Find(LocalRootFid);
	OBJ_ASSERT(this, LocalRoot);
	dlist Stack;
	optent *opt = new optent(LocalRoot);
	Stack.prepend(opt);			/* Init the Stack with local root */
	while (Stack.count() > 0) {		/* While Stack is not empty */
	    opt = (optent *)Stack.get();	/* Pop the Stack */
	    fsobj *obj = opt->GetFso();		/* get the current tree node fsobj object */
	    repair_obj_list.prepend(opt);		/* stick the node into the local obj list */
	    OBJ_ASSERT(this, obj && obj->IsLocalObj());
	    VenusFid *LFid = &obj->fid;
	    VenusFid *GFid = LGM_LookupGlobal(LFid);
	    OBJ_ASSERT(this, FID_IsLocalFake(LFid) && GFid != NULL);	    

	    {	/* built repair_vol_list */
		volent *Vol = VDB->Find(MakeVolid(GFid));
                CODA_ASSERT(Vol && Vol->IsReplicated());
                repvol *vp = (repvol *)Vol;
		vpt_iterator next(repair_vol_list);
		vptent *vpt;
		while ((vpt = next())) {
		    if (vpt->GetVol() == vp) break;
		}
		if (vpt == NULL) {
		    /* volume not already in list, insert it */
		    vpt = new vptent(vp);	
		    repair_vol_list.append(vpt);
		}
		vp->release();
	    }

	    if (obj->children != 0) {		/* Push the Stack */
		dlist_iterator next(*(obj->children));
		dlink *d;
		while ((d = next())) {
		    fsobj *cf = strbase(fsobj, d, child_link);
		    if (GCABLE(cf)) continue;
		    opt = new optent(cf);
		    Stack.prepend(opt);
		}
	    } else {
		/* check for covered mount point */
		if (obj->IsMtPt()) {
		    /* PUSH the mount root into the stack */
		    FSO_ASSERT(this, obj->u.root);
		    opt = new optent(obj->u.root);
		    Stack.prepend(opt);
		}
	    }
	}
	OBJ_ASSERT(this, repair_vol_list.count() > 0 && repair_obj_list.count() > 0);
    }

    {	/* find and put all the related cmlents into repair_cml_list list */
	vpt_iterator next(repair_vol_list);
	vptent *vpt;
	while ((vpt = next())) {
	    cml_iterator next(*(vpt->GetVol()->GetCML()), CommitOrder);
	    cmlent *m;
	    while ((m = next())) {
		/* check that this cmlent belongs to the subtree rooted	at LocalRoot */
		if (m->InLocalRepairSubtree(LocalRootFid)) {
		    m->SetRepairFlag();
		    mptent *mpt = new mptent(m);
		    repair_cml_list.append(mpt);
		}
		/* check if this cmlent is a new mutation belong this subtree(global part) */
		if (m->InGlobalRepairSubtree(GlobalRootFid)) {
		    m->SetTid(repair_session_tid);
		    m->SetRepairMutationFlag();
		}
	    }
	}
	LOG(100, ("lrdb::InitCMLSearch: found %d cmlent in repair_cml_list\n", 
		  repair_cml_list.count()));
    }

    /* Initialize the CML search point */
    if (repair_cml_list.count() > 0) {
	current_search_cml = ((mptent *)repair_cml_list.first())->GetCml();
	OBJ_ASSERT(this, current_search_cml);
    } 
}

/*
  BEGIN_HTML
  <a name="listlocal"><strong> traverse the local subtree, gather and
  print all the involved local mutation operations </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
void lrdb::ListCML(VenusFid *FakeRootFid, FILE *fp)
{
    /* list the CML records of subtree rooted at FakeRootFid in text form */
    OBJ_ASSERT(this, FakeRootFid);
    dlist vol_list;

    LOG(100, ("lrdb::ListCML: FakeRootFid = %s\n", FID_(FakeRootFid)));
    VenusFid *LocalRootFid = RFM_LookupLocalRoot(FakeRootFid);
    VenusFid *GlobalRootFid = RFM_LookupGlobalRoot(FakeRootFid);
    OBJ_ASSERT(this, LocalRootFid && GlobalRootFid);

    {	/* travese the subtree of the local replica */
	fsobj *LocalRoot = FSDB->Find(LocalRootFid);
	OBJ_ASSERT(this, LocalRoot);
	dlist Stack;
	optent *opt = new optent(LocalRoot);
	Stack.prepend(opt);			/* Init the Stack with local root */
	while (Stack.count() > 0) {		/* While Stack is not empty */
	    opt = (optent *)Stack.get();	/* Pop the Stack */
	    fsobj *obj = opt->GetFso();		/* get the current tree node fsobj object */
	    OBJ_ASSERT(this, obj && obj->IsLocalObj());
	    VenusFid *LFid = &obj->fid;
	    VenusFid *GFid = LGM_LookupGlobal(LFid);
	    OBJ_ASSERT(this, FID_IsLocalFake(LFid) && GFid != NULL);	    

	    {	/* built vol_list */
		volent *Vol = VDB->Find(MakeVolid(GFid));
                CODA_ASSERT(Vol && Vol->IsReplicated());
                repvol *vp = (repvol *)Vol;
		vpt_iterator next(vol_list);
		vptent *vpt;
		while ((vpt = next())) {
		    if (vpt->GetVol() == vp) break;
		}
		if (vpt == NULL) {
		    /* volume not already in list, insert it */
		    vpt = new vptent(vp);	
		    vol_list.append(vpt);
		}
		vp->release();
	    }
	    
	    if (obj->children != 0) {		/* Push the Stack */
		dlist_iterator next(*(obj->children));
		dlink *d;
		while ((d = next())) {
		    fsobj *cf = strbase(fsobj, d, child_link);
		    if (GCABLE(cf)) continue;
		    opt = new optent(cf);
		    Stack.prepend(opt);
		}
	    } else {
		/* check for covered mount point */
		if (obj->IsMtPt()) {
		    /* PUSH the mount root into the stack */
		    FSO_ASSERT(this, obj->u.root);
		    opt = new optent(obj->u.root);
		    Stack.prepend(opt);
		}
	    }
	}
	OBJ_ASSERT(this, vol_list.count() > 0);
    }
    
    {	/* gather related cmlents into cml_list list */
	vpt_iterator next(vol_list);
	vptent *vpt;
	while ((vpt = next())) {
	    cml_iterator next(*(vpt->GetVol()->GetCML()), CommitOrder);
	    cmlent *m;
	    while ((m = next())) {
		/* check that this cmlent belongs to the subtree rooted	at LocalRoot */
		if (m->InLocalRepairSubtree(LocalRootFid)) {
		    m->writeops(fp);
		}
	    }
	}
    }

    {	/* garbage collect vol_list */
	vptent *vpt;
	while ((vpt = (vptent *)vol_list.get()))
	  delete vpt;
	OBJ_ASSERT(this, vol_list.count() == 0);
    }
}

/*
  BEGIN_HTML
  <a name="iterate"><strong> iterate a step further on the list of the
  local mutation operations associated with the current repair session </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void lrdb::AdvanceCMLSearch()
{
    mpt_iterator next(repair_cml_list);
    mptent *mpt;
    while ((mpt = next())) {
	if (mpt->GetCml() == current_search_cml)
	  break;
    }
    OBJ_ASSERT(this, mpt);
    mpt = next();
    if (mpt == NULL) {
	current_search_cml = (cmlent *)NULL;
    } else {
	current_search_cml = mpt->GetCml();
    }
}

/*
  BEGIN_HTML
  <a name="reversefakeify"><strong> the reverse fakeify process of
  getting rid of the representation of an object in local-global
  conflict </strong></a> 
  END_HTML
*/
/* must not be called from within a transaction */
void lrdb::DeLocalization()
{
    OBJ_ASSERT(this, repair_root_fid);
    VenusFid *RootParentFid = RFM_LookupRootParent(repair_root_fid);
    VenusFid *LocalRootFid = RFM_LookupLocalRoot(repair_root_fid);
    VenusFid *GlobalRootFid = RFM_LookupGlobalRoot(repair_root_fid);
    VenusFid *LocalChildFid = RFM_LookupLocalChild(repair_root_fid);
    VenusFid *GlobalChildFid = RFM_LookupGlobalChild(repair_root_fid);
    fsobj *RootMtPt = RFM_LookupRootMtPt(repair_root_fid);
    OBJ_ASSERT(this, RootParentFid && LocalRootFid && GlobalRootFid);
    OBJ_ASSERT(this, LocalChildFid && GlobalChildFid);

    k_Purge();

    {	/* GC all the local objects in repair_obj_list, GC LGM and RFM at the same time */
	optent *opt;
	while ((opt = (optent *)repair_obj_list.get())) {
	    fsobj *obj = opt->GetFso();
	    OBJ_ASSERT(this, obj);
	    VenusFid *lfid = &obj->fid;
	    OBJ_ASSERT(this, FID_IsLocalFake(lfid));
	    
	    {   /* remove the LGM entry */
		VenusFid *gfid = LGM_LookupGlobal(lfid);
		OBJ_ASSERT(this, gfid && !FID_IsLocalFake(gfid));
		Recov_BeginTrans();
		LGM_Remove(lfid, gfid);
		Recov_EndTrans(MAXFP);
	    }
	    {   /* remove the RFM entry if possible */
		rfm_iterator next(root_fid_map);
		rfment *rfm;
		while ((rfm = next())) {
		    if (FID_EQ(rfm->GetLocalRootFid(), lfid))
		      break;
		}
		if (rfm != NULL) {
		    VenusFid *frfid = rfm->GetFakeRootFid();
		    OBJ_ASSERT(this, frfid != NULL);
		    Recov_BeginTrans();
			   RFM_Remove(frfid);
		    Recov_EndTrans(MAXFP);
	        }
	    }
	    {   /* will add new code to recover a local object here */
		Recov_BeginTrans();
		       obj->Kill();
		Recov_EndTrans(MAXFP);
	    }
	}
    }

    { 	/* kill the top three nodes, and de-local root-parent-node */
	fsobj *RepairRootObj = FSDB->Find(repair_root_fid);	/* always in FSDB */
	fsobj *RootParentObj = FSDB->Find(RootParentFid);	/* always in FSDB */
	fsobj *LocalChildObj = FSDB->Find(LocalChildFid);	/* may not always be in FSDB(local fake-link never crossed) */
	fsobj *GlobalChildObj = FSDB->Find(GlobalChildFid);	/* may not always be in FSDB(global fake-link never crossed) */
	OBJ_ASSERT(this, RepairRootObj && RootParentObj);

	/* undo the localization on RootParentObj */
	RootParentObj->DeLocalRootParent(RepairRootObj, GlobalRootFid, RootMtPt);
	Recov_BeginTrans();
	       if (LocalChildObj != NULL) 
	          LocalChildObj->Kill();
	       if (GlobalChildObj != NULL)
	          GlobalChildObj->Kill();
	       RepairRootObj->Kill();
       Recov_EndTrans(MAXFP);
    }
}

/*
  get both the local and global replicas of the operands of the current
  mutation operation
*/

/* need not be called from within a transaction */
int lrdb::FindRepairObject(VenusFid *fid, fsobj **global, fsobj **local)
{
    int rc, i = 0;

    do {
	i++;
	rc = do_FindRepairObject(fid, global, local);
    } while((rc == ERETRY || rc == ESYNRESOLVE) && i <= 5);

    return(rc);
}

/* need not be called from within a transaction */
int lrdb::do_FindRepairObject(VenusFid *fid, fsobj **global, fsobj **local)
{
    OBJ_ASSERT(this, fid);
    vproc *vp = VprocSelf();
    int rcode = 0, gcode = 0;
    LOG(100, ("lrdb::FindRepairObject: %s\n", FID_(fid)));
    
    /* first step: obtain local and global objects according to "fid" */
    if (FID_IsLocalFake(fid)) {
	OBJ_ASSERT(this, *local = FSDB->Find(fid));	/* local object always cached */
	*global = (fsobj *)NULL;			/* initialized global object */

	/* map local fid into its global counterpart and find the global object */
	VenusFid  *GFid;
	OBJ_ASSERT(this, GFid = LGM_LookupGlobal(fid));
	rcode = FSDB->Get(global, GFid, vp->u.u_uid, RC_DATA, 0, &gcode);
	if (rcode == 0) {
	    LOG(100, ("lrdb::FindRepairObject: found global fsobj for %s\n",
		      FID_(GFid)));
	    (*global)->UnLock(RD); /* FSDB::Get read-locked *global, must unlock */
	    /* even if rcode is zero, gcode may still be EINCONS because of succeeded fakeification */
	    if (gcode == EINCONS || (*global)->IsFake())
		return EINCONS;
	} else {
	    LOG(100, ("lrdb::FindRepairObject: FSDB::GET %s failed (%d)(%d)\n",
		      FID_(GFid), rcode, gcode));
	    if (rcode == EIO && gcode == EINCONS) 	
	      return EINCONS;
	    else 
	      return rcode;
	}
    } else {
	*local = (fsobj *)NULL;	 	/* fid is global, so local object must be NULL */
	*global = (fsobj *)NULL;	/* initialized global object */
	rcode = FSDB->Get(global, fid, vp->u.u_uid, RC_DATA, 0 , &gcode);
	if (rcode == 0) {
	    LOG(100, ("lrdb::FindRepairObject: found global fsobj for %s\n",
		      FID_(fid)));
	    (*global)->UnLock(RD); /* FSDB::Get read-locked *global, must unlock */
	    /* even if rcode is zero, gcode may still be EINCONS because of succeeded fakeification */
	    if (gcode == EINCONS || (*global)->IsFake())
		return EINCONS;
	} else {
	    LOG(100, ("lrdb::FindRepairObject: FSDB::GET %s failed(%d)(%d)\n",
		      FID_(fid), rcode, gcode));
	    if (rcode == EIO && gcode == EINCONS) 	
	      return EINCONS;
	    else 
	      return rcode;
	}	
    }

    /* step 2: make sure the "global" has its ancestors cached up until GlobalRootObj */
    fsobj *parent, *OBJ;
    int count = 0;			/* record the step going upward */
    VenusFid *PFid;

    /* step 2.1: first pass--going up to grab the ancestors of "*global" */
    OBJ_ASSERT(this, OBJ = *global);
    while ((OBJ->pfso == NULL) && !RFM_IsGlobalRoot(&OBJ->fid)) {
	PFid = &OBJ->pfid;
	OBJ_ASSERT(this, !FID_EQ(PFid, &NullFid));
	gcode = 0;
	rcode = FSDB->Get(&parent, PFid, vp->u.u_uid, RC_DATA, 0 , &gcode);
	if (rcode == 0) {
	    parent->UnLock(RD);		/* unlock read-locked parent */
	    /* even if rcode is zero, gcode may still be EINCONS because of succeeded fakeification */
	    if (gcode == EINCONS)	/* do not check IsFake() because parent can be the fake-root */
		return EINCONS;		
	    OBJ = parent;		/* going up */
	    count++;
	} else {
	    LOG(100, ("lrdb::FindRepairObject: FSDB::Get ancestor %s failed (%d)(%d)\n",
		      FID_(PFid), rcode, gcode));
	    if (rcode == EIO && gcode == EINCONS) 	
	      return EINCONS;
	    else 
	      return rcode;
	}
    }    

    /* may need to fetch one more if OBJ's parent is GlobalRootObj */
    if (!RFM_IsGlobalRoot(&OBJ->fid)) {
	PFid = &OBJ->pfid;
	OBJ_ASSERT(this, !FID_EQ(PFid, &NullFid));
	gcode = 0;
	rcode = FSDB->Get(&parent, PFid, vp->u.u_uid, RC_DATA, 0, &gcode);
	if (rcode == 0) {
	    parent->UnLock(RD);		/* unlock read-locked parent */
	    /* even if rcode is zero, gcode may still be EINCONS because of succeeded fakeification */
	    if (gcode == EINCONS)	/* do not check IsFake() because parent can be the fake-root */
		return EINCONS;		
	} else {
	    LOG(100, ("lrdb::FindRepairObject: FSDB::Get ancestor %s failed (%d)(%d)\n",
		      FID_(PFid), rcode, gcode));
	    if (rcode == EIO && gcode == EINCONS) 	
	      return EINCONS;
	    else 
	      return rcode;
	}	
    }

    /* step 2.2: second pass--going up and set "comp" and children link for the ancestors */
    char comp[MAXNAMELEN];
    OBJ = *global;
    while (count >= 0) {
	if (RFM_IsGlobalRoot(&OBJ->fid))
	  break;
	VenusFid *PFid = &OBJ->pfid;
	OBJ_ASSERT(this, !FID_EQ(PFid, &NullFid));
	OBJ_ASSERT(this, parent = FSDB->Find(PFid)); /* parent must have been cached already */
	OBJ_ASSERT(this, parent->dir_LookupByFid(comp, &OBJ->fid) == 0);
	OBJ->SetComp(comp);

	if (OBJ->pfso == NULL) {
	    OBJ->SetParent(PFid->Vnode, PFid->Unique);
	}
	OBJ = OBJ->pfso;
	count--;
    }
    return 0;
}

/* need not be called from within a transaction */
fsobj *lrdb::GetGlobalParentObj(VenusFid *GlobalChildFid)
{
    /* 
     * given the fid of a global object, find its global parent fsobj object. we 
     * assume the local object and its global counterpart both exist. the safe way 
     * to find the global parent of ChildFid is to first find its local counterpart
     * (must be in FSDB) and its local parent Fid, and then the global parent Fid 
     * and finally use FSDB::Get() to get the global parent.
     */
    OBJ_ASSERT(this, GlobalChildFid && !FID_IsLocalFake(GlobalChildFid));
    LOG(100, ("lrdb::GetGlobalParentObj: GlobalChildFid = %s\n",
	      FID_(GlobalChildFid)));

    VenusFid *LocalChildFid = LGM_LookupLocal(GlobalChildFid);
    OBJ_ASSERT(this, LocalChildFid);
    VenusFid *GlobalParentFid = NULL;


    {	/* first check to see if LocalChildFid is also the LocalRootFid */
	rfm_iterator next(root_fid_map);
	rfment *rfm;
	while ((rfm = next())) {
	    if (FID_EQ(rfm->GetLocalRootFid(), LocalChildFid)) {
		GlobalParentFid = rfm->GetRootParentFid();
		LOG(100, ("lrdb::GetGlobalParentObj: ChildFid is RootFid\n"));
		break;
	    }
	}
    }
    if (GlobalParentFid == NULL) {
	fsobj *LocalChildObj = FSDB->Find(LocalChildFid);
	OBJ_ASSERT(this, LocalChildObj);
	VenusFid *LocalParentFid = &LocalChildObj->pfid;
	OBJ_ASSERT(this, !FID_EQ(LocalParentFid, &NullFid));
	GlobalParentFid = LGM_LookupGlobal(LocalParentFid);
	OBJ_ASSERT(this, GlobalParentFid);
    }
    LOG(100, ("lrdb::GetGlobalParentObj: ParentFid = %s\n",
	      FID_(GlobalParentFid)));
    fsobj *GlobalParentObj;
    vproc *vp = VprocSelf();
    if (FSDB->Get(&GlobalParentObj, GlobalParentFid, vp->u.u_uid, RC_DATA) != 0) {
	LOG(100, ("lrdb::GetGlobalParentObj: can not FSDB::Get global parent\n"));
	return (fsobj *)NULL;
    } else {
	LOG(100, ("lrdb::GetGlobalParentObj: found the Global Parent\n"));
	GlobalParentObj->UnLock(RD);
	return GlobalParentObj;
    }
}

/* need not be called from within a transaction */
char lrdb::GetSubtreeView()
{
    return subtree_view;
}

/*
  BEGIN_HTML
  <a name="changeview"><strong> changing the object view during the
  current repair session </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
void lrdb::SetSubtreeView(char NewView, char *msg)
{
    if (repair_root_fid == NULL) {
	sprintf(msg,"there is no ongoing repair session");
	return;
    }

    OBJ_ASSERT(this, msg);
    char ParentPath[MAXPATHLEN];
    char OldView;

    /* purge related fids from kernel mini-cache */
    PurgeRootFids();

    strcpy(msg, " ");		/* initilize the output message */
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while ((rfm = next())) {
	if (rfm->RootCovered()) continue;
	VenusFid *RootParentFid = rfm->GetRootParentFid();
	VenusFid *FakeRootFid = rfm->GetFakeRootFid();
	VenusFid *GlobalChildFid = rfm->GetGlobalChildFid();
	VenusFid *LocalChildFid = rfm->GetLocalChildFid();
	VenusFid *GlobalRootFid = rfm->GetGlobalRootFid();
	OBJ_ASSERT(this, RootParentFid && FakeRootFid && GlobalChildFid && LocalChildFid && GlobalRootFid);
	fsobj *RootParentObj = FSDB->Find(RootParentFid);
	char *Name = rfm->GetName();
	OBJ_ASSERT(this, RootParentObj != NULL && Name != NULL);
	OldView = rfm->GetView();
	RootParentObj->GetPath(ParentPath, 1);
	if (NewView == OldView) {
	    sprintf(msg, "%s\n repair-view unchanged for subtree rooted at %s/%s",  
		    msg, ParentPath, Name);
	    continue;
	} 	    
	switch (NewView) {
	case SUBTREE_GLOBAL_VIEW:
	    sprintf(msg, "%s\n set global repair-view for subtree rooted at %s/%s",  
		    msg, ParentPath, Name);
	    break;
	case SUBTREE_LOCAL_VIEW:
	    sprintf(msg, "%s\n set local repair-view for subtree rooted at %s/%s", 
		    msg, ParentPath, Name);
	    break;
	case SUBTREE_MIXED_VIEW:
	    sprintf(msg, "%s\n set global repair-view for subtree rooted at %s/%s", 
		    msg, ParentPath, Name);
	    break;
	default:
	    CHOKE("lrdb::SetSubtreeView: bogus new view (%d)", NewView);
	}
	if (OldView == SUBTREE_MIXED_VIEW) {   /* check the top nodes of the subtree */
	    /* check fake root object */
	    fsobj *FakeRootObj = FSDB->Find(FakeRootFid);
	    OBJ_ASSERT(this, FakeRootObj != NULL);

	    /* check global root object */
	    vproc *vp = VprocSelf();
	    fsobj *GlobalRootObj = (fsobj *)NULL;
	    if (FSDB->Get(&GlobalRootObj, GlobalRootFid, vp->u.u_uid, RC_DATA, "global") != 0) {
		LOG(100, ("lrdb::SetSubtreeView: can't get data-valid global root object\n"));
		sprintf(msg, "%s failed", msg);
		continue;
	    }
	    OBJ_ASSERT(this, GlobalRootObj);
	    GlobalRootObj->UnLock(RD);

	    /* check global child object */
	    fsobj *child = (fsobj *)NULL;
	    VenusFid dummy;
	    OBJ_ASSERT(this, FakeRootObj->Lookup(&child, &dummy, "global", vp->u.u_uid, CLU_CASE_SENSITIVE) == 0);
	    child->UnLock(RD);

	    /* check local child object */
	    child = (fsobj *)NULL;
	    OBJ_ASSERT(this, FakeRootObj->Lookup(&child, &dummy, "local", vp->u.u_uid, CLU_CASE_SENSITIVE) == 0);
	    child->UnLock(RD);
	}

	fsobj *LocalChildObj = FSDB->Find(LocalChildFid);
	fsobj *GlobalChildObj = FSDB->Find(GlobalChildFid);
	OBJ_ASSERT(this, (LocalChildObj != NULL) && (GlobalChildObj != NULL));

	Recov_BeginTrans();
	       rfm->SetView(NewView);
	Recov_EndTrans(MAXFP);
	if (FID_EQ(repair_root_fid, FakeRootFid)) {
	    Recov_BeginTrans();
		   RVMLIB_REC_OBJECT(subtree_view);
		   subtree_view = NewView;
	    Recov_EndTrans(MAXFP);
	}
	if (OldView == SUBTREE_MIXED_VIEW && NewView == SUBTREE_GLOBAL_VIEW) {
	    RootParentObj->MixedToGlobal(FakeRootFid, GlobalChildFid, Name);
	    sprintf(msg, "%s succeeded", msg);
	    continue;
	}
	if (OldView == SUBTREE_MIXED_VIEW && NewView == SUBTREE_LOCAL_VIEW) {
	    RootParentObj->MixedToLocal(FakeRootFid, LocalChildFid, Name);
	    sprintf(msg, "%s succeeded", msg);
	    continue;
	}
	if (OldView == SUBTREE_GLOBAL_VIEW && NewView == SUBTREE_MIXED_VIEW) {
	    RootParentObj->GlobalToMixed(FakeRootFid, GlobalChildFid, Name);
	    LocalChildObj->SetComp("local");
	    GlobalChildObj->SetComp("global");
	    sprintf(msg, "%s succeeded", msg);
	    continue;
	}
	if (OldView == SUBTREE_LOCAL_VIEW && NewView == SUBTREE_MIXED_VIEW) {
	    RootParentObj->LocalToMixed(FakeRootFid, LocalChildFid, Name);
	    LocalChildObj->SetComp("local");
	    GlobalChildObj->SetComp("global");
	    sprintf(msg, "%s succeeded", msg);
	    continue;
	}
	if (OldView == SUBTREE_LOCAL_VIEW && NewView == SUBTREE_GLOBAL_VIEW) {
	    RootParentObj->LocalToMixed(FakeRootFid, LocalChildFid, Name);
	    RootParentObj->MixedToGlobal(FakeRootFid, GlobalChildFid, Name);
	    sprintf(msg, "%s succeeded", msg);
	    continue;
	}
	if (OldView == SUBTREE_GLOBAL_VIEW && NewView == SUBTREE_LOCAL_VIEW) {
	    RootParentObj->GlobalToMixed(FakeRootFid, GlobalChildFid, Name);
	    RootParentObj->MixedToLocal(FakeRootFid, LocalChildFid, Name);
	    sprintf(msg, "%s succeeded", msg);
	    continue;
	}
	CHOKE("lrdb::SetSubtreeView: bogus views (%d, %d)\n", OldView, NewView);
    }
}

/* must not be called from within a transaction */
void lrdb::ReplaceRepairFid(VenusFid *NewGlobalFid, VenusFid *LocalFid)
{   
    OBJ_ASSERT(this, NewGlobalFid != NULL && LocalFid != NULL);
    LOG(10, ("lrdb::ReplaceRepairFid: NewFid = %s OldFid = %s\n",
	     FID_(NewGlobalFid), FID_(LocalFid)));
    /* we only need to replace the fid in local-global-fid map */
    lgm_iterator next(local_global_map);
    lgment *lgm;
    while ((lgm = next())) {
	if (FID_EQ(lgm->GetLocalFid(), LocalFid)) {
	    Recov_BeginTrans();
		   lgm->SetGlobalFid(NewGlobalFid);
	    Recov_EndTrans(MAXFP);
	    return;
	}
    }
    CHOKE("lrdb::TranlsateRepairFid: LocalFid not in the LGM map");
}

/* must not be called from within a transactin */
void lrdb::RemoveSubtree(VenusFid *FakeRootFid)
{
    /* 
     * remove the entire subtree rooted at FakeRootFid.
     * the input paramter indicates whether we need to
     * clean the related CML records or not.
     */
    LOG(0, ("lrdb::RemoveSubtree: FakeRootFid = %s\n", FID_(FakeRootFid)));

    VenusFid *RootParentFid = RFM_LookupRootParent(FakeRootFid);
    VenusFid *LocalRootFid = RFM_LookupLocalRoot(FakeRootFid);
    VenusFid *GlobalRootFid = RFM_LookupGlobalRoot(FakeRootFid);
    VenusFid *LocalChildFid = RFM_LookupLocalChild(FakeRootFid);
    VenusFid *GlobalChildFid = RFM_LookupGlobalChild(FakeRootFid);
    fsobj *RootMtPt = RFM_LookupRootMtPt(FakeRootFid);
    OBJ_ASSERT(this, RootParentFid && LocalRootFid && GlobalRootFid);
    OBJ_ASSERT(this, LocalChildFid && GlobalChildFid);
    dlist gc_obj_list;
    dlist gc_vol_list;
    dlist gc_cml_list;

    {	/* Step 1: gather all related volumes and local objects by traversing the subtree */
	fsobj *LocalRoot = FSDB->Find(LocalRootFid);
	OBJ_ASSERT(this, LocalRoot);
	dlist Stack;
	optent *opt = new optent(LocalRoot);
	Stack.prepend(opt);			/* Init the Stack with local root */
	while (Stack.count() > 0) {		/* While Stack is not empty */
	    opt = (optent *)Stack.get();	/* Pop the Stack */
	    fsobj *obj = opt->GetFso();		/* get the current tree node fsobj object */
	    gc_obj_list.prepend(opt);		/* stick the node into the local obj list */
	    OBJ_ASSERT(this, obj && obj->IsLocalObj());
	    VenusFid *LFid = &obj->fid;
	    VenusFid *GFid = LGM_LookupGlobal(LFid);
	    OBJ_ASSERT(this, FID_IsLocalFake(LFid) && GFid != NULL);	    

	    {	/* built gc_vol_list */
		volent *Vol = VDB->Find(MakeVolid(GFid));
		OBJ_ASSERT(this, Vol && Vol->IsReplicated());
                repvol *vp = (repvol *)Vol;
		vpt_iterator next(gc_vol_list);
		vptent *vpt;
		while ((vpt = next())) {
		    if (vpt->GetVol() == vp) break;
		}
		if (vpt == NULL) {
		    /* volume not already in list, insert it */
		    vpt = new vptent(vp);	
		    gc_vol_list.append(vpt);
		}
		vp->release();
	    }

	    if (obj->children != 0) {		/* Push the Stack */
		dlist_iterator next(*(obj->children));
		dlink *d;
		while ((d = next())) {
		    fsobj *cf = strbase(fsobj, d, child_link);
		    if (GCABLE(cf)) continue;
		    opt = new optent(cf);
		    Stack.prepend(opt);
		}
	    } else {
		/* check for covered mount point */
		if (obj->IsMtPt()) {
		    /* PUSH the mount root into the stack */
		    FSO_ASSERT(this, obj->u.root);
		    opt = new optent(obj->u.root);
		    Stack.prepend(opt);
		}
	    }
	}
	OBJ_ASSERT(this, gc_vol_list.count() > 0 && gc_obj_list.count() > 0);
    }
 
    {	/* step 2: find and put all the related cmlents int gc_cml_list list */
	vpt_iterator next(gc_vol_list);
	vptent *vpt;
	while ((vpt = next())) {
	    cml_iterator next(*(vpt->GetVol()->GetCML()), CommitOrder);
	    cmlent *m;
	    while ((m = next())) {
		/* check that this cmlent belongs to the subtree rooted	at LocalRoot */
		if (m->InLocalRepairSubtree(LocalRootFid)) {
		    mptent *mpt = new mptent(m);
		    gc_cml_list.append(mpt);
		}
	    }
	}
	/* gc_cml_list could be empty */
	LOG(100, ("lrdb::RemoveSubtree: found %d cmlent in gc_cml_list\n", 
		  gc_cml_list.count()));
    }

    {	/* step 3: GC the related CML recods, gc_cml_list and gc_vol_list */
	mptent *mpt;
	vptent *vpt;
	while ((mpt = (mptent *)gc_cml_list.get())) {
	    cmlent *m = mpt->GetCml();
	    Recov_BeginTrans();
		   delete m;
	    Recov_EndTrans(MAXFP);
	    delete mpt;
	}

	while ((vpt = (vptent *)gc_vol_list.get())) {
	    delete vpt;
	}	
	OBJ_ASSERT(this, gc_cml_list.count() == 0 && gc_vol_list.count() == 0);
    }
    
    {	/* step 4: remove the local subtree and the related LGM and RFM entries */
	/* GC all the local objects in repair_obj_list, GC LGM and RFM at the same time */
	LOG(0, ("lrdb::RemoveSubtree: %d local objects to be garbage collected\n", gc_obj_list.count()));
	optent *opt;
	while ((opt = (optent *)gc_obj_list.get())) {
	    fsobj *obj = opt->GetFso();
	    OBJ_ASSERT(this, obj);
	    VenusFid *lfid = &obj->fid;
	    OBJ_ASSERT(this, FID_IsLocalFake(lfid));
	    
	    {   /* remove the LGM entry */
		VenusFid *gfid = LGM_LookupGlobal(lfid);
		OBJ_ASSERT(this, gfid && !FID_IsLocalFake(gfid));
		Recov_BeginTrans();
		       LGM_Remove(lfid, gfid);
		Recov_EndTrans(MAXFP);
	    }

	    {   /* remove the RFM entry if possible */
		ObtainWriteLock(&rfm_lock);
		rfm_iterator next(root_fid_map);
		rfment *rfm;
		while ((rfm = next())) {
		    if (FID_EQ(rfm->GetLocalRootFid(), lfid))
		      break;
		}
		if (rfm != NULL) {
		    VenusFid *frfid = rfm->GetFakeRootFid();
		    OBJ_ASSERT(this, frfid != NULL);
		    Recov_BeginTrans();
			   RFM_Remove(frfid);
		    Recov_EndTrans(MAXFP);
		}
		ReleaseWriteLock(&rfm_lock);
	    }
	    {   /* GC the local object, and its pointer entry */
		Recov_BeginTrans();
		       obj->Kill();
		Recov_EndTrans(MAXFP);
		delete opt;
	    }
	}
	OBJ_ASSERT(this, gc_obj_list.count() == 0);
    }

    {	/* step 5: kill the top three nodes, and de-local root-parent-node */
	fsobj *RepairRootObj = FSDB->Find(FakeRootFid); 	/* always in FSDB */
	fsobj *RootParentObj = FSDB->Find(RootParentFid);	/* always in FSDB */
	fsobj *LocalChildObj = FSDB->Find(LocalChildFid);	/* may not always be in FSDB */
	fsobj *GlobalChildObj = FSDB->Find(GlobalChildFid);	/* may not always be in FSDB */
	OBJ_ASSERT(this, RepairRootObj && RootParentObj);

	/* undo the localization on RootParentObj */
	RootParentObj->DeLocalRootParent(RepairRootObj, GlobalRootFid, RootMtPt);
	Recov_BeginTrans();
	       if (LocalChildObj != NULL) 
	           LocalChildObj->Kill();
	       if (GlobalChildObj != NULL)
	           GlobalChildObj->Kill();
	       RepairRootObj->Kill();
	Recov_EndTrans(MAXFP);
    }
}

/* must be called from within a transaction */
int lrdb::Cancel(cmlent *m) {
    OBJ_ASSERT(this, m);    

    if (repair_root_fid == NULL)
	return(0);  /* Not in a local repair session */

    mpt_iterator next(repair_cml_list);
    mptent *mpt;
    while ((mpt = next())) {
	if (mpt->GetCml() == m)
	  break;
    }
    
    if (m == current_search_cml)
	AdvanceCMLSearch();

    if (mpt != NULL) {
	repair_cml_list.remove(mpt);
	return(1);
    }
    return(0);
}
/* ********** end of lrdb methods ********** */

/* ********** beginning of mptent methods ********** */
mptent::mptent(cmlent *m)
{
    cml = m;
}

mptent::~mptent()
{
}

cmlent *mptent::GetCml()
{
    return cml;
}

void mptent::print(FILE *fp)
{
    print(fileno(fp));
}

void mptent::print()
{
    print(fileno(stdout));
}

void mptent::print(int fd)
{
    fdprint(fd, "cml = 0x%x\n", cml);
}

mpt_iterator::mpt_iterator(dlist &dl) : dlist_iterator(dl)
{
}

mptent *mpt_iterator::operator()() 
{
    return (mptent *)dlist_iterator::operator()();
}
/* ********** end of mptent methods ********** */


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




/* this file contains local-repair related volent methods */

#ifdef __cplusplus
extern "C" {
#endif

#include <struct.h>

#include <rpc2/errors.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vcrcommon.h>

/* from venus */
#include "local.h"
#include "venusvol.h"


/* must be called from within a transaction */
void repvol::TranslateCMLFid(ViceFid *global, ViceFid *local)
{
    VOL_ASSERT(this, global && local);
    LOG(100, ("volent::TranslateCMLFid: global = 0x%x.%x.%x local = 0x%x.%x.%x\n",
	      global->Volume, global->Vnode, global->Unique,
	      local->Volume, local->Vnode, local->Unique));
    VOL_ASSERT(this, vid == global->Volume);
    cml_iterator next(CML, CommitOrder);
    cmlent *m;
    while ((m = next())) {
	m->TranslateFid(global, local);
    }
}

/* must not be called from within a transaction */
void repvol::ClearRepairCML()
{
    Recov_BeginTrans();
    rec_dlist_iterator next(CML.list);
    rec_dlink *d = next();			
    
    while (1) {
	    if (!d) break;
	    cmlent *m = strbase(cmlent, d, handle);
	    if (m->IsRepairMutation()) {
		    m->print(logFile);
		    d = next();	
		    m->abort();
	    } else {
		    d = next();
	    }
    }
    Recov_EndTrans(DMFP);
}

/* must not be called from within a transaction */
int repvol::GetReintId()
{
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(reint_id_gen);
    reint_id_gen++;
    Recov_EndTrans(MAXFP);
    return reint_id_gen;
}


/* need not be called from within a transaction */
void repvol::CheckTransition()
{
    /*
     * this method is called when this volume just went 
     * through GlobalReintegrate(). If its CML is cleared,
     * we need to set off a state transition.
     */
    if (state == Hoarding || state == Emulating || state == Resolving)
      return;
    VOL_ASSERT(this, state == Logging);
    if (CML.count() == 0)
      CML.owner = UNSET_UID;
    if ((CML.count() == 0 || (CML.count() > 0 && !ContainUnrepairedCML()))
	&& flags.logv == 0)
      flags.transition_pending = 1;
}

/* must not be called from within a transaction */
void repvol::IncAbort(int tid)
{
    CML.IncAbort(tid);
    if (CML.count() == 0)
      CML.owner = UNSET_UID;
}

/* need not be called from within a transaction */
int repvol::ContainUnrepairedCML()
{
    cml_iterator next(CML, CommitOrder);
    cmlent *m;
    while ((m = next())) {
	if (m->IsToBeRepaired())
	  return 1;
    }
    return 0;
}


/*
  BEGIN_HTML
  <a name="checklocalsubtree"><strong> this method checks whether there are 
  still unrepaired localized subtrees in this volume. </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
void repvol::CheckLocalSubtree()
{
    /* try to unset the has_local_subtree bit if possible */
    if (!flags.has_local_subtree) return;
    /* 
     * search the LRDB local/global map to see whether
     * this volume has any local objects left.
     */
    lgm_iterator next(LRDB->local_global_map);
    lgment *lgm;
    ViceFid *gfid;
    int contain_local_obj = 0;
    while ((lgm = next())) {
	gfid = lgm->GetGlobalFid();
	if (gfid->Volume == vid) {
	    contain_local_obj = 1;
	    break;
	}
    }
    if (!contain_local_obj) {
	LOG(0, ("repvol::CheckLocalSubtree: (%s)reset has_local_subtree flag!\n", name));
	Recov_BeginTrans();
	       RVMLIB_REC_OBJECT(flags);
	       flags.has_local_subtree = 0;
	Recov_EndTrans(MAXFP);
    }
}

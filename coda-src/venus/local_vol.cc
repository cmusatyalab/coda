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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/local_vol.cc,v 4.3 1997/12/16 16:08:32 braam Exp $";
#endif /*_BLURB_*/




/* this file contains local-repair related volent methods */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <struct.h>

#include <errors.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vcrcommon.h>

/* from venus */
#include "local.h"
#include "venusvol.h"


/* must be called from within a transaction */
void volent::TranslateCMLFid(ViceFid *global, ViceFid *local)
{
    VOL_ASSERT(this, global && local);
    LOG(100, ("volent::TranslateCMLFid: global = 0x%x.%x.%x local = 0x%x.%x.%x\n",
	      global->Volume, global->Vnode, global->Unique,
	      local->Volume, local->Vnode, local->Unique));
    VOL_ASSERT(this, vid == global->Volume);
    cml_iterator next(CML, CommitOrder);
    cmlent *m;
    while (m = next()) {
	m->TranslateFid(global, local);
    }
}

/* must not be called from within a transaction */
void volent::ClearRepairCML()
{
    ATOMIC(
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
    , DMFP)    
}

/* must not be called from within a transaction */
int volent::GetReintId()
{
    ATOMIC(
	   RVMLIB_REC_OBJECT(reint_id_gen);
	   reint_id_gen++;
    , MAXFP)
    return reint_id_gen;
}


/* need not be called from within a transaction */
void volent::CheckTransition()
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
void volent::IncAbort(int tid)
{
    CML.IncAbort(tid);
    if (CML.count() == 0)
      CML.owner = UNSET_UID;
}

/* need not be called from within a transaction */
int volent::ContainUnrepairedCML()
{
    cml_iterator next(CML, CommitOrder);
    cmlent *m;
    while (m = next()) {
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
void volent::CheckLocalSubtree()
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
    while (lgm = next()) {
	gfid = lgm->GetGlobalFid();
	if (gfid->Volume == vid) {
	    contain_local_obj = 1;
	    break;
	}
    }
    if (!contain_local_obj) {
	LOG(0, ("volent::CheckLocalSubtree: (%s)reset has_local_subtree flag!\n", name));
	ATOMIC(
	       RVMLIB_REC_OBJECT(flags);
	       flags.has_local_subtree = 0;
	, MAXFP)
    }
}

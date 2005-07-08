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


/*
  BEGIN_HTML
  <a name="discard"><strong> discard the current mutation operation </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void repvol::DiscardLocalMutation(char *msg)
{
    int rc;
    char opmsg[1024];

    CODA_ASSERT(msg != NULL);
    cml_iterator next(CML, CommitOrder);
    cmlent *m = next();
    if(!m) {
      sprintf(msg, "Client Modify Log is empty for this volume!\n");
      return;
    }

    m->GetLocalOpMsg(opmsg);

    if(!m->IsToBeRepaired()) {
      sprintf(msg, "\tLocal mutation:\n\t%s\n\tnot in conflict!", opmsg);
      return;
    }
    CODA_ASSERT(m->IsFrozen());

    LOG(0,("lrdb::DiscardLocalMutation: dropping head of CML: %s\n", opmsg));
    Recov_BeginTrans();
    CML.cancelFreezes(1);
    rc = m->cancel();
    CML.cancelFreezes(0);
    Recov_EndTrans(CMFP);

    if(rc) {
      LOG(0, ("lrdb::DiscardLocalMutation: cancel failed!\n"));
      sprintf(msg, "discard of local mutation failed");
    }
    LOG(0, ("lrdb::DiscardLocalMutation ended\n"));
    sprintf(msg, "discarded local mutation %s", opmsg);
}

/*
  BEGIN_HTML
  <a name="discardall"><strong> discard all the local mutation operations </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void repvol::DiscardAllLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
    sprintf(msg, "discard all local mutations");
}

/*
  BEGIN_HTML
  <a name="preserve"><strong> preserving the current mutation operation </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void repvol::PreserveLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
#if 0
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
#endif
}

/*
  BEGIN_HTML
  <a name="preserveall"><strong> repeatedly preserve the current
  mutation operation and advance to the next mutation operation until
  the end </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void repvol::PreserveAllLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
#if 0
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
#endif
}

/*
  BEGIN_HTML
  <a name="listlocal"><strong> traverse fid's volume, gather and
  print all the involved local mutation operations </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
void repvol::ListCML(FILE *fp)
{
#if 0
    /* list the CML records of subtree rooted at FakeRootFid in text form */
    dlist vol_list;

    LOG(100, ("lrdb::ListCML: fid = %s\n", FID_(fid)));
    {	/* travese the subtree of the local replica */
	fsobj *LocalRoot = FSDB->Find(fid);
	OBJ_ASSERT(this, LocalRoot);
	dlist Stack;
	optent *opt = new optent(LocalRoot);
	Stack.prepend(opt);			/* Init the Stack with local root */
	while (Stack.count() > 0) {		/* While Stack is not empty */
	    opt = (optent *)Stack.get();	/* Pop the Stack */
	    fsobj *obj = opt->GetFso();		/* get the current tree node fsobj object */
	    VenusFid *LFid = &obj->fid;
	    {	/* built vol_list */
		volent *Vol = VDB->Find(MakeVolid(LFid));
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
	    while ((m = next()))
	      m->writeops(fp);
	}
    }

    {	/* garbage collect vol_list */
	vptent *vpt;
	while ((vpt = (vptent *)vol_list.get()))
	  delete vpt;
	OBJ_ASSERT(this, vol_list.count() == 0);
    }
#endif
}


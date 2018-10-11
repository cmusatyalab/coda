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
  <a name="checklocal"><strong> The checklocal repair command is implemented by
  the ContinueRepairSession() method. </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void ClientModifyLog::CheckCMLHead(char *msg)
{
    cml_iterator next(*this, CommitOrder);
    cmlent *m = next();

    OBJ_ASSERT(this, msg != NULL);

    if(!m) {
      if(msg)
	sprintf(msg, "no local mutations\n");
      return;
    }

    {   /* perform local mutation checks, produce repair tool message */
        char opmsg[1024];
        char checkmsg[1024];
        int mcode, rcode;
        m->GetLocalOpMsg(opmsg);
        m->CheckRepair(checkmsg, &mcode, &rcode);
        sprintf(msg, "local mutation: %s\n%s", opmsg, checkmsg);
        LOG(0, ("ClientModifyLog::CheckCMLHead: %s", msg));
    }
}

/*
  BEGIN_HTML
  <a name="discard"><strong> discard the current mutation operation </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
int ClientModifyLog::DiscardLocalMutation(char *msg)
{
    int rc;
    char opmsg[1024];

    cml_iterator next(*this, CommitOrder);
    cmlent *m = next();
    if(!m) {
      if(msg) sprintf(msg, "no local mutations for this volume\n");
      return EINVAL;
    }

    m->GetLocalOpMsg(opmsg);

    if(!m->IsToBeRepaired()) {
      if(msg) sprintf(msg, "\tLocal mutation:\n\t%s\n\tnot in conflict!\n",
		      opmsg);
      return EINVAL;
    }

    /* XXX: Dependencies need to be checked here! */

    LOG(0, ("ClientModifyLog::DiscardLocalMutation: dropping head of CML:"
	    "%s\n", opmsg));
    CODA_ASSERT(m->IsFrozen());
    Recov_BeginTrans();
    cancelFreezes(1);
    rc = m->cancel();
    cancelFreezes(0);
    Recov_EndTrans(CMFP);

    if(rc != 1) {
      LOG(0, ("ClientModifyLog::DiscardLocalMutation: cancel failed: %d\n", rc));
      sprintf(msg, "discard of local mutation failed");
    }
    else {
      sprintf(msg, "discarded local mutation %s\n", opmsg);
      rc = 0;
    }

    return rc;
}

/*
  BEGIN_HTML
  <a name="discardall"><strong> discard all the local mutation operations </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void reintvol::DiscardAllLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
    sprintf(msg, "use purgeml instead\n");
}

/*
  BEGIN_HTML
  <a name="preserve"><strong> preserving the current mutation operation </strong></a>
  END_HTML
*/
/* need not be called from within a transaction */
void ClientModifyLog::PreserveLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
    char opmsg[1024], checkmsg[1024];
    int mcode, rcode;

    cml_iterator next(*this, CommitOrder);
    cmlent *m;

    if((m = next())) {
      int rc;
      m->GetLocalOpMsg(opmsg);
      m->CheckRepair(checkmsg, &mcode, &rcode);
      if (rcode == REPAIR_FAILURE) {
	/* it is impossible to perform the original local mutation */
	sprintf(msg, "impossible to reintegrate %s\n", checkmsg);
	return;
      }
      rc = m->DoRepair(msg, rcode);
      if(!rc) { /* success! get rid of it locally, or it'll hang around */
	Recov_BeginTrans();
	cancelFreezes(1);
	rc = m->cancel();
	cancelFreezes(0);
	Recov_EndTrans(CMFP);
	sprintf(msg, "reintegrated:\n\t%s\n", opmsg);
      }
      else
	sprintf(msg, "%s\ncould not reintegrate %s\n", checkmsg, opmsg);
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
void ClientModifyLog::PreserveAllLocalMutation(char *msg)
{
    OBJ_ASSERT(this, msg);
    char opmsg[1024], checkmsg[1024];
    int mcode, rcode, rc, opcnt = 0;

    cml_iterator next(*this, CommitOrder);
    cmlent *m;

    while((m = next())) {
	m->GetLocalOpMsg(opmsg);
	if (m->GetTid() > 0) {
	    sprintf(msg, "%s belongs to transaction %d\n %d local mutation(s) replayed\n", opmsg, m->GetTid(), opcnt);
	    return;
	}
	mcode = 0;
	m->CheckRepair(checkmsg, &mcode, &rcode);
	if (rcode == REPAIR_FAILURE) {
	    /* it is impossible to perform the original local mutation */
	    sprintf(msg, "%d local mutation(s) reintegrated \n %s\n cannot reintegrate %s\n", opcnt, checkmsg, opmsg);
	    return;
	}
	/* mcode is left set when CheckRepair found a non-fatal error */
	if (mcode || !(rc = m->DoRepair(checkmsg, rcode))) {
	    opcnt++;
	} else {
	    sprintf(msg, "%d local mutation(s) reintegrated\n %s\n cannot reintegrate %s\n", opcnt, checkmsg, opmsg);
	    return;
	}
    }
    sprintf(msg, "All %d local mutation(s) reintegrated\n", opcnt);
}

/*
  BEGIN_HTML
  <a name="listlocal"><strong> traverse fid's volume, gather and
  print all the involved local mutation operations </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
int ClientModifyLog::ListCML(FILE *fp)
{
    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    int count = 0;
    CODA_ASSERT(fp);

    while((m = next())) {
      m->writeops(fp);
      count++;
    }

    return count;
}

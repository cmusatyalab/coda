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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/venus/local_daemon.cc,v 1.1.1.1 1996/11/22 23:02:26 rvb Exp";
#endif /*_BLURB_*/




/* this file contains local-repair daemon code */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/param.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vcrcommon.h>
#include "vproc.h"
#include "venusrecov.h"
#include "venus.private.h"
#include "local.h"

/* ***** Private constants ***** */
PRIVATE const int LRDaemonStackSize = 32768;
PRIVATE const int LRDaemonInterval = 5;
PRIVATE const int CheckSubtreeInterval = 5 * 60;

/* ***** Private variables ***** */
PRIVATE char lrdaemon_sync;

void LRDBDaemon() 
{

    /* Hack! Vproc must yield before data member become valid */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(LRDaemonInterval, &lrdaemon_sync);

    long LastCheckSubtree = 0;

    for (;;) {
	VprocWait(&lrdaemon_sync);

	START_TIMING();
	long curr_time = Vtime();

	/* periodic events */
	if (curr_time - LastCheckSubtree >= CheckSubtreeInterval) {
	    LRDB->CheckLocalSubtree();
	    LastCheckSubtree = curr_time;
	}

	END_TIMING();
	LOG(100, ("LRDBDaemon: elapsed = %3.1f (%3.1f, %3.1f)\n",
		 elapsed, elapsed_ru_utime, elapsed_ru_stime));
    }
}

/*
  BEGIN_HTML
  <a name="checklocalsubtree"><strong> periodically check whether there are 
  still unrepaired localized subtrees. </strong></a>
  END_HTML
*/

void lrdb::CheckLocalSubtree()
{
    if (InRepairSession()) 
      /* 
       * can't iterate root_fid_map while a local/global repair session 
       * is going on which may very well remove elements from the list.
       */
      return;
    
    ObtainReadLock(&rfm_lock);
    rfm_iterator next(root_fid_map);
    rfment *rfm;
    while (rfm = next()) {
	if (!rfm->RootCovered()) {
	    ViceFid *RootFid = rfm->GetRootParentFid();
	    OBJ_ASSERT(this, RootFid != NULL);
	    fsobj *RootParentObj = FSDB->Find(RootFid);
	    OBJ_ASSERT(this, RootParentObj != NULL);
	    char RootPath[MAXPATHLEN];
	    RootParentObj->GetPath(RootPath, 1);
	    eprint("Local inconsistent object at %s/%s, please check!\n",
		   RootPath, rfm->GetName());
	}
    }
    ReleaseReadLock(&rfm_lock);
}

void LRD_Init() {
    (void)new vproc("LRDaemon", (PROCBODY) &LRDBDaemon,
		    VPT_LRDaemon, LRDaemonStackSize);
}

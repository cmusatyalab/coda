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
#include "advice_daemon.h"

/* ***** Private constants ***** */
static const int LRDaemonStackSize = 32768;
static const int LRDaemonInterval = 5;
static const int CheckSubtreeInterval = 5 * 60;

/* ***** Private variables ***** */
static char lrdaemon_sync;

void LRDBDaemon(void) 
{

    /* Hack! Vproc must yield before data member become valid */
    VprocYield();

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

/* MARIA testing */
char *PrintFid(ViceFid *fid) {
    static char fidString[128];
    snprintf(fidString, 128, "%x.%x.%x", fid->Volume, fid->Vnode, fid->Unique);
    return(fidString);
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
    while ((rfm = next())) {
	if (!rfm->RootCovered()) {
	    ViceFid *RootFid = rfm->GetRootParentFid();
	    OBJ_ASSERT(this, RootFid != NULL);
	    fsobj *RootParentObj = FSDB->Find(RootFid);
	    OBJ_ASSERT(this, RootParentObj != NULL);
	    char RootPath[MAXPATHLEN];
	    RootParentObj->GetPath(RootPath, 1);
	    eprint("Local inconsistent object at %s/%s, please check!\n",
		   RootPath, rfm->GetName());
	    MarinerLog("Local inconsistent object at %s/%s, please check!\n",
		       RootPath, rfm->GetName());
	    char fullpath[MAXPATHLEN];
	    snprintf(fullpath, MAXPATHLEN, "%s/%s", RootPath, rfm->GetName());
	    ViceFid *objFid = rfm->GetGlobalRootFid();
	    CODA_ASSERT(objFid);
	    LOG(0, ("LocalInconsistentObj: objFid=%x.%x.%x\n",
		    objFid->Volume, objFid->Vnode, objFid->Unique));
	    NotifyUsersObjectInConflict(fullpath, objFid);
	}
    }
    ReleaseReadLock(&rfm_lock);
}

void LRD_Init(void) {
    (void)new vproc("LRDaemon", &LRDBDaemon, VPT_LRDaemon, LRDaemonStackSize);
}

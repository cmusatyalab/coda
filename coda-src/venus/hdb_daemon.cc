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







/*
 *
 *    Hoard database daemon.
 *
 *    The daemon insists that all commands have been issued with an effective uid of root (V_UID).
 *    This ensures that we are dealing with a "locally authoritative" user/program.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>

#include <lwp.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from venus */
#include "hdb.h"
#include "user.h"
#include "venus.private.h"
#include "vproc.h"


/*  *****  Private constants  *****  */

static const int HDBDaemonInterval = 300;
static const int HdbWalkInterval = 10 * 60;
static const int HDBDaemonStackSize = 65536;
static const int HDBDaemonPriority = LWP_NORMAL_PRIORITY-1;

static long LastHdbWalk;

/* *****  Private types  ***** */

struct hdbd_msg : public olink {
    char wait_blk;
    enum hdbd_request type;
    void *request;
    int result;
};


/*  *****  Private variables  *****  */

static char hdbdaemon_sync;
static olist hdbd_msgq;


/*  *****  Private routines  ***** */

static void HDBD_HandleRequests();


/*  *****  The HDB Daemon  *****  */

void HDBD_Init() {
    (void)new vproc("HDBDaemon", (PROCBODY) &HDBDaemon,
		     VPT_HDBDaemon, HDBDaemonStackSize, HDBDaemonPriority);
}

long HDBD_GetNextHoardWalkTime() {
  long currTime = Vtime();
  LOG(0, ("HDBD_GetNextHoardWalkTime() returns %ld + %ld - %ld\n", 
	  LastHdbWalk, (long)HdbWalkInterval, currTime));
  return(LastHdbWalk + (long)HdbWalkInterval - currTime);
}

void HDBDaemon() {

    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(HDBDaemonInterval, &hdbdaemon_sync);

    LastHdbWalk = /*0*/Vtime();	    /* skip initial walk at startup! */

    for (;;) {
LOG(0, ("HDBDaemon about to sleep on hdbdaemon_sync\n"));
	VprocWait(&hdbdaemon_sync);
LOG(0, ("HDBDaemon just woke up\n"));

	START_TIMING();
	long curr_time = Vtime();

	/* Handle requests BEFORE periodic events. */
	HDBD_HandleRequests();

	/* Periodic events. */
	if (PeriodicWalksAllowed) {
	    /* Walk HDB. */
	    if (curr_time - LastHdbWalk >= HdbWalkInterval) {
		struct hdb_walk_msg m;
		m.ruid = V_UID;
		(void)HDB->Walk(&m);
		LastHdbWalk = curr_time;
	    }
	}

	/* Handle requests AFTER periodic events (in case we blocked during their execution). */
	HDBD_HandleRequests();

	END_TIMING();
	LOG(10, ("HDBDaemon: elapsed = %3.1f (%3.1f, %3.1f)\n",
		 elapsed, elapsed_ru_utime, elapsed_ru_stime));

	/* Bump sequence number. */
	vp->seq++;
    }
}


int HDBD_Request(hdbd_request type, void *request, vuid_t euid, vuid_t ruid) {
    /* Ensure request was issued by "locally authoritative" entity. */
    if (euid != V_UID && !AuthorizedUser(ruid)) {
	LOG(0, ("HDBD_Request (%s): <%d, %d> not authorized\n",
		PRINT_HDBDREQTYPE(type), euid, ruid));
	return(EACCES);
    }

    /* Form message. */
    hdbd_msg m;
    m.type = type;
    m.request = request;
    m.result = 0;

    /* Send it, and wait for reply. */
    hdbd_msgq.append(&m);
    VprocSignal(&hdbdaemon_sync);
    LOG(0, ("WAITING(HDBD_Request): %s\n", PRINT_HDBDREQTYPE(type)));
    START_TIMING();
    VprocWait(&m.wait_blk);
    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f, returns %d\n", elapsed, m.result));

    /* Return result. */
    return(m.result);
}


static void HDBD_HandleRequests() {
    hdbd_msg *m = 0;
    while (m = (hdbd_msg *)hdbd_msgq.get()) {
	switch(m->type) {
	    case HdbAdd:
		m->result = HDB->Add((hdb_add_msg *)m->request);
		break;

	    case HdbDelete:
		m->result = HDB->Delete((hdb_delete_msg *)m->request);
		break;

	    case HdbClear:
		m->result = HDB->Clear((hdb_clear_msg *)m->request);
		break;

	    case HdbList:
		m->result = HDB->List((hdb_list_msg *)m->request);
		break;

	    case HdbWalk:
		m->result = HDB->Walk((hdb_walk_msg *)m->request);
		break;

	    case HdbEnable:
		m->result = HDB->Enable((hdb_walk_msg *)m->request);
		break;

	    case HdbDisable:
		m->result = HDB->Disable((hdb_walk_msg *)m->request);
		break;
		
	    case HdbVerify:
		m->result = HDB->Verify((hdb_verify_msg *)m->request);
		break;

	    default:
		CHOKE("HDBD_HandleRequests: bogus type (%d)", m->type);
	}

	/* Send back the result. */
	VprocSignal(&m->wait_blk);
    }
}

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





/*****************************************
 * vol-maxid.c                           *
 * Get or set the maximum used volume id *
 *****************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>

#include <unistd.h>
#include <stdlib.h>

#include <struct.h>
#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <volutil.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "cvnode.h"
#include "volume.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"

/*
  BEGIN_HTML
  <a name="S_VolGetMaxVolId"><strong>Service the getmaxvol request</strong></a>
  END_HTML
 */
long S_VolGetMaxVolId(RPC2_Handle cid, RPC2_Integer * maxid) {
    *maxid = VGetMaxVolumeId();
    return (RPC2_SUCCESS);
}


/*
  BEGIN_HTML
  <a name="S_VolSetMaxVolId"><strong>Service the setmaxvol request</strong></a>
  END_HTML
 */
long S_VolSetMaxVolId(RPC2_Handle cid, RPC2_Integer newid) {
    int status;

    /* Make sure this request won't change the server id! */
    if ( (SRV_RVM(MaxVolId) & 0xff000000) != (newid & 0xff000000)) {
        LogMsg(0, VolDebugLevel, stdout, "VSetMaxVolumeId: New volume id has a different server id!  Not changing id.");
	return(RPC2_FAIL);
    }

    if ( SRV_RVM(MaxVolId) > newid) {
	LogMsg(0, VolDebugLevel, stdout,  "VSetMaxVolumeId: MaxVolId > newid, not setting MaxVolId");
	return (RPC2_FAIL);
    }

    RVMLIB_BEGIN_TRANSACTION(restore);
    VSetMaxVolumeId(newid);
    RVMLIB_END_TRANSACTION(flush, &(status));
    LogMsg(0, VolDebugLevel, stdout, "S_VolSetMaxVolId: returning 0\n");
    return (0);
}



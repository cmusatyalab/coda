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
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
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
  S_VolSetMaxVolId: Service the setmaxvol request
 */
long S_VolSetMaxVolId(RPC2_Handle cid, RPC2_Integer newid) 
{
    rvm_return_t status;

    /* Make sure this request won't change the server id! */
    if ( (SRV_RVM(MaxVolId) & 0xff000000) != (newid & 0xff000000)) {
        VLog(0, "VSetMaxVolumeId: New volume id has a different server id! "
	       "Not changing id.");
	return(RPC2_FAIL);
    }

    if ( (int) SRV_RVM(MaxVolId) > newid) {
	VLog(0, "VSetMaxVolumeId: MaxVolId > newid, not setting MaxVolId");
	return (RPC2_FAIL);
    }

    rvmlib_begin_transaction(restore);
    VSetMaxVolumeId(newid);
    rvmlib_end_transaction(flush, &(status));
    VLog(0, "S_VolSetMaxVolId: returning 0.\n");
    return (0);
}



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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-maxid.cc,v 4.1 1997/01/08 21:52:32 rvb Exp $";
#endif /*_BLURB_*/





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

#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#include <mach.h>
#endif /* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif /* __NetBSD__ || LINUX */

#include <struct.h>
#include <lwp.h>
#include <lock.h>
#include <rpc2.h>

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
    if ( (CAMLIB_REC(MaxVolId) & 0xff000000) != (newid & 0xff000000)) {
        LogMsg(0, VolDebugLevel, stdout, "VSetMaxVolumeId: New volume id has a different server id!  Not changing id.");
	return(RPC2_FAIL);
    }

    if ( CAMLIB_REC(MaxVolId) > newid) {
	LogMsg(0, VolDebugLevel, stdout,  "VSetMaxVolumeId: MaxVolId > newid, not setting MaxVolId");
	return (RPC2_FAIL);
    }

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED);
    VSetMaxVolumeId(newid);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status);
    LogMsg(0, VolDebugLevel, stdout, "S_VolSetMaxVolId: returning 0\n");
    return (0);
}



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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vice/Attic/obsolete.cc,v 4.1 1998/11/25 19:23:33 braam Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#ifdef __linux__
#include <linux/if_ether.h>
#endif

#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <util.h>
#include <rvmlib.h>
#include <partition.h>
#include <auth2.h>
#include <prs.h>
#include <al.h>
#include <callback.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <errors.h>
#include <voltypes.h>
#include <vsg.h>
#include <vrdb.h>
#include <vldb.h>
#include <srv.h>
#include <vlist.h>
#include <vice.private.h>
#include <operations.h>
#include <ops.h>
#include "coppend.h"


/* NOTE:
   
   we would really like to get rid of these calls but unfortunately
   the stub routine constants have been "borrowed" by code that has
   nothing to do with rpc's:

   1. on the client the cml using packing routines that grope into
   the internals of vice.h

   2. on the server the resolution logs do the same.

   The re-packing of the log entries will have to be re-visited before
   these routines can go.

   In order for the change to be backward compatible also the rp2gen
   routines which generate multi code will have to be educated about
   the

   "call-number: function( ... )"  convention.

*/


long FS_ViceRemove(RPC2_Handle cid, ViceFid *Did, RPC2_String Name, ViceStatus *DirStatus,
 ViceStatus *Status, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceRemoveDir(RPC2_Handle cid, ViceFid *Did, RPC2_String Name, ViceStatus *Status, ViceStatus *TgtStatus, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
{
	CODA_ASSERT(0);
	return 0;
}




long FS_ViceCreate(RPC2_Handle cid, ViceFid *Did, ViceFid *BidFid, RPC2_String Name, ViceStatus *Status, ViceFid *Fid, ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceRename(RPC2_Handle cid, ViceFid *OldDid, RPC2_String OldName, ViceFid *NewDid, RPC2_String NewName, ViceStatus *OldDirStatus, ViceStatus *NewDirStatus, ViceStatus *SrcStatus, ViceStatus *TgtStatus, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceSymLink(RPC2_Handle cid, ViceFid *Did, RPC2_String OldName, RPC2_String NewName, ViceFid *Fid, ViceStatus *Status, ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceLink(RPC2_Handle cid, ViceFid *Did, RPC2_String Name, ViceFid *Fid, ViceStatus *Status, ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceMakeDir(RPC2_Handle cid, ViceFid *Did, RPC2_String Name, ViceStatus *Status, ViceFid *NewDid, ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
{
	CODA_ASSERT(0);
	return 0;
}





long FS_ViceUnused1(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused2(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused3(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused4(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused5(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused6(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused7(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused8(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused9(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused10(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused11(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused12(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused14(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}


long FS_ViceUnused13(RPC2_Handle cid)
{
	CODA_ASSERT(0);
	return 0;
}



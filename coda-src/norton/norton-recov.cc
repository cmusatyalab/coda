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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/norton/norton-recov.cc,v 1.3 1997/01/07 18:40:52 rvb Exp";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#ifdef	__MACH__
#include <mach/boolean.h>
#endif
    
#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>


/* Return the MaxVolId from recoverable storage */
int GetMaxVolId() {
    return(CAMLIB_REC(MaxVolId) & 0x00FFFFFF);
}


/* Get a volume header from recoverable storage given the appropriate index 
 * Returns pointer to header if successful, NULL otherwise
 */
VolumeHeader *VolHeaderByIndex(int myind) {
    VolumeId maxid = GetMaxVolId();

    if ((myind < 0) || (myind >= maxid) || (myind >= MAXVOLS)) {
	return(NULL);
    }
    return(&CAMLIB_REC(VolumeList[myind]).header);
}


/* Get a volume from recoverable storage 
 * Returns pointer to volume if successful, NULL otherwise 
 */

VolHead *VolByIndex(int myind) {
    VolumeId maxid = GetMaxVolId();

    maxid = (CAMLIB_REC(MaxVolId) & 0x00FFFFFF);
    if ((myind < 0) || (myind >= maxid) || (myind >= MAXVOLS)) {
	return(NULL);
    }

    return(&CAMLIB_REC(VolumeList[myind]));
}

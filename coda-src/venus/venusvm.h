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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/venus/venusvm.h,v 1.1.1.1 1996/11/22 19:11:53 rvb Exp";
#endif /*_BLURB_*/








/*
 *
 * Specification of the Venus Vmon module.
 *
 */


#ifndef	_VENUSVM_H_
#define _VENUSVM_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <mond.h>

/* from util */
#include <olist.h>

#define	VMON	1


/*  ***** Macro *****  */
#define VMSE    (rvg->recov_VMSE)
#define SEActiveList     (rvg->recov_VMSE.ActiveList)

/*  ***** OverFlow Stuff *****  */
enum OverFlow {      VMOVERFLOW,
		     RVMOVERFLOW,
};


/*  ***** Type *****  */

struct vmselist : public olink {
  public:
    olist ActiveList;
    int count;           /* zero cleared in Recov_InitSeg() for init option */
};


/* Forward declarations. */
extern void VmonPrint();
extern void VmonPrint(FILE *);
extern void VmonPrint(int);




extern char *VmonHost;
extern int VmonPortal;

extern void VmonInit();
extern void VmonEnqueueSession(VmonSessionId, VolumeId, UserId, VmonAVSG *,
				RPC2_Unsigned, RPC2_Unsigned, RPC2_Unsigned,
			        VmonSessionEventArray *, SessionStatistics *,
				CacheStatistics *);
extern void VmonEnqueueCommEvent(RPC2_Unsigned, RPC2_Unsigned, VmonCommEventType);

extern void VMOND_Init();
extern void VmonDaemon();

#endif	not _VENUSVM_H_

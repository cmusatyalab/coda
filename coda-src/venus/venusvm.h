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
extern int VmonPort;

extern void VmonInit();
extern void VmonEnqueueSession(VmonSessionId, VolumeId, UserId, VmonAVSG *,
				RPC2_Unsigned, RPC2_Unsigned, RPC2_Unsigned,
			        VmonSessionEventArray *, SessionStatistics *,
				CacheStatistics *);
extern void VmonEnqueueCommEvent(RPC2_Unsigned, RPC2_Unsigned, VmonCommEventType);

extern void VMOND_Init(void);
extern void VmonDaemon(void);

#endif	not _VENUSVM_H_

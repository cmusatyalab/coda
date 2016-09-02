/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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
 * Headers for inconsistency handling in CODA.
 *
 */

#ifndef _INCONSIST_H_
#define _INCONSIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <vice.h>
#include <vcrcommon.h>

/* The possible results of a two-way version vector compare. */
typedef enum {
    VV_EQ  = 0,
    VV_DOM = 1,
    VV_SUB = 2,
    VV_INC = 3
} VV_Cmp_Result;

#define VV_INCON    0x01      /* mask for inconsistency flag */
#define	VV_LOCAL    0x02      /* mask for local flag */
#define VV_BARREN   0x04      /* mask for barren flag - small vnode without a valid inode */
#define VV_COP2PENDING 0x08   /* mask for cop2 pending flag */


#define	IsIncon(vv)	    ((vv).Flags & VV_INCON)
#define	SetIncon(vv)	    ((vv).Flags |= VV_INCON)
#define	ClearIncon(vv)	    ((vv).Flags &= ~VV_INCON)
#define	IsLocal(vv)	    ((vv).Flags & VV_LOCAL)
#define	SetLocal(vv)	    ((vv).Flags |= VV_LOCAL)
#define	ClearLocal(vv)	    ((vv).Flags &= ~VV_LOCAL)
#define IsBarren(vv)        ((vv).Flags & VV_BARREN)
#define SetBarren(vv)       ((vv).Flags |= VV_BARREN)
#define ClearBarren(vv)     ((vv).Flags &= ~VV_BARREN)
#define COP2Pending(vv)	    ((vv).Flags & VV_COP2PENDING)
#define SetCOP2Pending(vv)  ((vv).Flags |= VV_COP2PENDING)
#define ClearCOP2Pending(vv) ((vv).Flags &= ~VV_COP2PENDING)

#define SID_EQ(a, b)     ((a).HostId == (b).HostId && (a).Uniquifier == (b).Uniquifier)

extern const ViceStoreId NullSid;


VV_Cmp_Result VV_Cmp (const ViceVersionVector *, const ViceVersionVector *);
VV_Cmp_Result VV_Cmp_IgnoreInc (const ViceVersionVector *, const ViceVersionVector *);
int VV_Check (int *, ViceVersionVector **, int);
int VV_Check_IgnoreInc (int *, ViceVersionVector **, int);
int IsRunt (ViceVersionVector *);

void AddVVs (ViceVersionVector *, ViceVersionVector *);
void SubVVs (ViceVersionVector *, ViceVersionVector *);
void InitVV (ViceVersionVector *);
void InvalidateVV (ViceVersionVector *);
void GetMaxVV (ViceVersionVector *, ViceVersionVector **, int);

void SPrintVV(char *buf, size_t len, ViceVersionVector *);
void FPrintVV(FILE *, ViceVersionVector *);

#ifdef __cplusplus
}
#endif

#endif /* _INCONSIST_H_ */

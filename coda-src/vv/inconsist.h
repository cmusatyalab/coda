/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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

#ifndef _INCON_
#define _INCON_

#include <vice.h>
#include <vcrcommon.h>

#define EINCONS  199	      /* should go into /usr/cs/include/errno.h */

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

/* Used to be in vice/codaproc2.c */
#define	SID_EQ(a, b)	((a).Host == (b).Host && (a).Uniquifier == (b).Uniquifier)
extern const ViceStoreId NullSid;


/* Unique tag for store identification; hostid + unique counter (per host) */
typedef ViceStoreId storeid_t;


/* Preliminary version vector structure */
typedef ViceVersionVector vv_t;


extern VV_Cmp_Result VV_Cmp (const vv_t *, const vv_t *);
extern VV_Cmp_Result VV_Cmp_IgnoreInc (const vv_t *, const vv_t *);
extern int VV_Check (int *, vv_t **, int);
extern int VV_Check_IgnoreInc (int *, vv_t **, int);
extern int IsRunt (vv_t *);

extern void AddVVs (vv_t *, vv_t *);
extern void SubVVs (vv_t *, vv_t *);
extern void InitVV (vv_t *);
extern void InvalidateVV (vv_t *);
extern void GetMaxVV (vv_t *, vv_t **, int);

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

extern void PrintVV (FILE *, vv_t *);

#endif /* _INCON_ */

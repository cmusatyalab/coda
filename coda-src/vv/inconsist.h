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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/








/*
 *
 * Headers for inconsistency handling in CODA.
 *
 */

#ifndef _INCON_
#define _INCON_

#include <vice.h>

/* #ifndef C_ARGS --- uncomment after rvm.h is fixed (Satya, 6/3/95) */
#if	(__cplusplus | __STDC__)
#define	C_ARGS(arglist)	arglist
#else	__cplusplus
#define	C_ARGS(arglist)	()
#endif	__cplusplus
/* #ifndef C_ARGS --- uncomment after rvm.h is fixed (Satya, 6/3/95) */

#define EINCONS  199	      /* should go into /usr/cs/include/errno.h */

/* The possible results of a two-way version vector compare. */
#define	VV_EQ	0
#define	VV_DOM	1
#define	VV_SUB	2
#define	VV_INC	3

#define VSG_MEMBERS 8	      /* Number of servers per Volume Storage Group */
			      /* Note: may have problems if increase beyond MAXHOSTS */
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
extern ViceStoreId NullSid;


/* Unique tag for store identification; hostid + unique counter (per host) */
typedef ViceStoreId storeid_t;


/* Preliminary version vector structure */
typedef ViceVersionVector vv_t;


extern int VV_Cmp C_ARGS((vv_t *, vv_t *));
extern int VV_Cmp_IgnoreInc C_ARGS((vv_t *, vv_t *));
extern int VV_Check C_ARGS((int *, vv_t **, int));
extern int VV_Check_IgnoreInc C_ARGS((int *, vv_t **, int));
extern int IsRunt C_ARGS((vv_t *));

extern void AddVVs C_ARGS((vv_t *, vv_t *));
extern void SubVVs C_ARGS((vv_t *, vv_t *));
extern void InitVV C_ARGS((vv_t *));
extern void InvalidateVV C_ARGS((vv_t *));
extern void GetMaxVV C_ARGS((vv_t *, vv_t **, int));

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

extern void PrintVV C_ARGS((FILE *, vv_t *));

#endif _INCON_

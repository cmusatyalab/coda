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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/vv/RCS/inconsist.cc,v 4.1 1997/01/08 21:52:42 rvb Exp $";
#endif /*_BLURB_*/







/*
 *
 * Routines for inconsistency handling in CODA.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include "inconsist.h"

/* From vice/codaproc2.c */
ViceStoreId NullSid = { 0, 0 };

PRIVATE int VV_BruteForceCheck(int *, vv_t **, int, int);
PRIVATE int VV_Check_Real(int *, vv_t **, int, int);

int VV_Cmp(vv_t *a, vv_t *b) {
    if (IsIncon(*a) || IsIncon(*b)) return(VV_INC);

    return(VV_Cmp_IgnoreInc(a, b));
}

int VV_Cmp_IgnoreInc(vv_t *a, vv_t *b) {

    int	res = VV_EQ;

    for (int i = 0; i < VSG_MEMBERS; i++) {
	long a_val = (&(a->Versions.Site0))[i];
	long b_val = (&(b->Versions.Site0))[i];
	if (a_val == b_val) continue;			    /* res is unchanged by equality */
	if (a_val > b_val)
	    if (res != VV_SUB) { res = VV_DOM; continue; }  /* a dominates b so far */
	    else return(VV_INC);			    /* inconsistency! */
	else	/* a_val < b_val */
	    if (res != VV_DOM) { res = VV_SUB; continue; }  /* b dominates a so far */
	    else return(VV_INC);			    /* inconsistency! */
    }

    return(res);
}



int VV_Check(int *HowMany, vv_t **vvp, int EqReq) 
    {
    return(VV_Check_Real(HowMany, vvp, EqReq, 0)); 
    }

int VV_Check_IgnoreInc(int *HowMany, vv_t **vvp, int EqReq) 
    {
    return(VV_Check_Real(HowMany, vvp, EqReq, 1)); 
    }


PRIVATE int VV_Check_Real(int *HowMany, vv_t **vvp, int EqReq, int IgnoreInc) {

/* If IgnoreInc is set, uses VV_Cmp_IgnoreInc() for comparison;
   else uses VV_Cmp()
*/
/* Version-vector check routine.  Decides whether given array of vvs are consistent, and if so, */
/* identifies those which are in the "dominant set."  The value of the function indicates whether or */
/* not the vectors are consistent: 1 -> consistent, 0 -> inconsistent.  The OUT parameter HowMany */
/* indicates the size of the dominant set (valid only if the vectors are consistent).  The IN/OUT */
/* array parameter vvp must be of VSG_MEMBERS size; on input it contains pointers to the vv */
/* objects to be tested (NULL pointers are allowed), and on output it contains pointers to those */
/* objects that are in the dominant set and NULL pointers elsewhere.  As an out parameter, the */
/* contents of the vvp array are valid only if the vectors are consistent.  The order of entries in */
/* the array is preserved. */
/* Addendum: The parameter, EqualityRequired, if true cause us to treat dominance as inconsistency. */


    int	dom_ix = -1;	/* index of first dominant vector */
    int k, result;

    /* Find the first non-Null vector and initialize dom_ix to its index. */
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (vvp[i]) { dom_ix = i; break; }
    if (dom_ix == -1) {	*HowMany = 0; return(1); }  /* empty input array */

    /* Found a vector.  Go through the array in order, comparing each non-Null vector to the current */
    /* dominant one.  If an inconsistency is detected switch to the brute-force routine to do the check */
    /* Otherwise, if the vectors are equal go on to the next one, if the candidate vector is "submissive" */
    /* zero it (i.e., remove it from the dominant set), else if the candidate vector dominates the current */
    /* dominator zero it and all vectors between it and the candidate, make the candidate the new */
    /* dominator, and continue comparing with the successor of the new dominator. */
    *HowMany = 1;
    for (int j = dom_ix + 1; j < VSG_MEMBERS; j++) {
	if (!vvp[j]) continue;
	if (IgnoreInc) result = VV_Cmp_IgnoreInc(vvp[dom_ix], vvp[j]);
	else result = VV_Cmp(vvp[dom_ix], vvp[j]);
	switch (result) {
	    case VV_EQ:
		(*HowMany)++;
		continue;

	    case VV_DOM:
		if (EqReq) return(0);
		vvp[j] = 0;
		continue;

	    case VV_SUB:
		if (EqReq) return(0);
		for (k = dom_ix; k < j; k++) vvp[k] = 0;
		*HowMany = 1;
		dom_ix = j;
		continue;

	    case VV_INC:
		if (EqReq) return(0);
		return(VV_BruteForceCheck(HowMany, vvp, EqReq, IgnoreInc));
	}
    }

    return(1);
}




/* I can't decide whether this is ever necessary! -JJK */
/* A less efficient algorithm, but it can deal with inconsistencies amongst submissive vectors. */
PRIVATE int VV_BruteForceCheck(int *HowMany, vv_t **vvp, int EqReq, int IgnoreInc) {
    /* If IgnoreInc is set, uses VV_Cmp_IgnoreInc() for comparison;
	else uses VV_Cmp()
    */

    /* Find the first non-Null vector and initialize the outer loop counter to its index. */
    int i = -1, j;
    int result;

    for (j = 0; j < VSG_MEMBERS; j++)
	if (vvp[j]) { i = j; break; }
    if (i == -1) { *HowMany = 0; return(1); }	/* empty input array */

    /* Find the first vector which is >= or == all other vectors (depending on EqReq). */
    for (; i < VSG_MEMBERS; i++) {
	if (!vvp[i]) continue;
	*HowMany = 1;
	for (j = 0; j < VSG_MEMBERS; j++) {
	    if (i == j) continue;
	    if (!vvp[j]) continue;

	    if (IgnoreInc) result = VV_Cmp_IgnoreInc(vvp[i], vvp[j]);
	    else result = VV_Cmp(vvp[i], vvp[j]);
	    switch (result) {
		case VV_EQ:
		    (*HowMany)++;
		    continue;

		case VV_DOM:
		    if (EqReq) return(0);
		    vvp[j] = 0;
		    continue;

		case VV_SUB:
		    if (EqReq) return(0);
		    vvp[i] = 0;
		    goto outer_continue;

		case VV_INC:
		    if (EqReq) return(0);
		    goto outer_continue;
	    }
	}
	return(1);

outer_continue:
	;	/* empty statement to satisfy compiler */
    }

    return(0);
}


/* v = x */
/*
void InitVV(vv_t *v, long x) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	(&(v->Versions.Site0))[i] = x;
    v->StoreId.Host = 0;
    v->StoreId.Uniquifier = 0;
    v->Flags = 0;
}
*/


/* v1 = v1 + v2 */
void AddVVs(vv_t *v1, vv_t *v2) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	(&(v1->Versions.Site0))[i] += (&(v2->Versions.Site0))[i];
}


/* v1 = v1 - v2 */
void SubVVs(vv_t *v1, vv_t*v2) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	(&(v1->Versions.Site0))[i] -= (&(v2->Versions.Site0))[i];
}


/* v = 0 */
void InitVV(vv_t *v) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	(&(v->Versions.Site0))[i] = 0;
    v->StoreId.Host = 0;
    v->StoreId.Uniquifier = 0;
    v->Flags = 0;
}

int IsRunt(vv_t *v) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	if ((&(v->Versions.Site0))[i])
	    return(0);
    if (v->StoreId.Host || v->StoreId.Uniquifier || v->Flags)
	return(0);
    return(1);
}

/* v = -1 */
void InvalidateVV(vv_t *v) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	(&(v->Versions.Site0))[i] = -1;
    v->StoreId.Host = (unsigned) -1;
    v->StoreId.Uniquifier = (unsigned) -1;
    v->Flags = 0;	    /* must be 0, otherwise IsIncon() is fooled */
}

/* newvv[i] = max(vvgroup[][i]); storeid is got from dominant index */
/* domindex == -1 --> weakly equal group; pick any storeid */
/* domindex == -2 --> don't set storeid, let it be zero */
void GetMaxVV(vv_t *newvv, vv_t **vvgroup, int domindex)
{
    int i, j;
    bzero(newvv, sizeof(ViceVersionVector));
    for (i = 0; i < VSG_MEMBERS; i++) {
	/* compute max of VV[][i] */
	long max = 0;
	for (j = 0; j < VSG_MEMBERS; j++) 
	    if (vvgroup[j]){
		long val = (long)((&(vvgroup[j]->Versions.Site0))[i]);
		if (val > max)
		    max = val;
	    }
	(&(newvv->Versions.Site0))[i] = max;
    }
    switch (domindex) {
    case -1:
	/* pick any nonzero storeid */
	for (i = 0; i < VSG_MEMBERS; i++) 
	    if (vvgroup[i]) break;
	if (i < VSG_MEMBERS) 
	    bcopy(&(vvgroup[i]->StoreId), &(newvv->StoreId),
		  sizeof(ViceStoreId));
	break;
    case -2:
	/* do nothing */
	break;
    default:
	bcopy(&(vvgroup[domindex]->StoreId), &(newvv->StoreId),
	      sizeof(ViceStoreId));
    }
}

void PrintVV(FILE *fp, vv_t *v) {
    fprintf(fp, "{[");
    for (int i = 0; i < VSG_MEMBERS; i++)
	fprintf(fp, " %d", (&(v->Versions.Site0))[i]);
    fprintf(fp, " ] [ %d %d ] [ %#x ]}\n",
	     v->StoreId.Host, v->StoreId.Uniquifier, v->Flags);
}

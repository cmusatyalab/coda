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
 * Routines for inconsistency handling in CODA.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
}
#endif

#include "inconsist.h"

/* From vice/codaproc2.c */
const ViceStoreId NullSid = { 0, 0 };

static int VV_BruteForceCheck(int *, ViceVersionVector **, int, int);
static int VV_Check_Real(int *, ViceVersionVector **, int, int);

VV_Cmp_Result VV_Cmp_IgnoreInc(const ViceVersionVector *a,
                               const ViceVersionVector *b)
{
    VV_Cmp_Result res = VV_EQ;

    for (int i = 0; i < VSG_MEMBERS; i++) {
        long a_val = (&(a->Versions.Site0))[i];
        long b_val = (&(b->Versions.Site0))[i];
        if (a_val == b_val)
            continue; /* res is unchanged by equality */
        if (a_val > b_val)
            if (res != VV_SUB) {
                res = VV_DOM;
                continue;
            } /* a dominates b so far */
            else
                return (VV_INC); /* inconsistency! */
        else /* a_val < b_val */
            if (res != VV_DOM) {
            res = VV_SUB;
            continue;
        } /* b dominates a so far */
        else
            return (VV_INC); /* inconsistency! */
    }

    return (res);
}

VV_Cmp_Result VV_Cmp(const ViceVersionVector *a, const ViceVersionVector *b)
{
    if (IsIncon(*a) || IsIncon(*b))
        return (VV_INC);
    return (VV_Cmp_IgnoreInc(a, b));
}

int VV_Check(int *HowMany, ViceVersionVector **vvp, int EqReq)
{
    return (VV_Check_Real(HowMany, vvp, EqReq, 0));
}

int VV_Check_IgnoreInc(int *HowMany, ViceVersionVector **vvp, int EqReq)
{
    return (VV_Check_Real(HowMany, vvp, EqReq, 1));
}

static int VV_Check_Real(int *HowMany, ViceVersionVector **vvp, int EqReq,
                         int IgnoreInc)
{
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

    int dom_ix = -1; /* index of first dominant vector */
    int k;
    VV_Cmp_Result result;

    /* Find the first non-Null vector and initialize dom_ix to its index. */
    for (int i = 0; i < VSG_MEMBERS; i++)
        if (vvp[i]) {
            dom_ix = i;
            break;
        }
    if (dom_ix == -1) {
        *HowMany = 0;
        return (1);
    } /* empty input array */

    /* Found a vector.  Go through the array in order, comparing each non-Null vector to the current */
    /* dominant one.  If an inconsistency is detected switch to the brute-force routine to do the check */
    /* Otherwise, if the vectors are equal go on to the next one, if the candidate vector is "submissive" */
    /* zero it (i.e., remove it from the dominant set), else if the candidate vector dominates the current */
    /* dominator zero it and all vectors between it and the candidate, make the candidate the new */
    /* dominator, and continue comparing with the successor of the new dominator. */
    *HowMany = 1;
    for (int j = dom_ix + 1; j < VSG_MEMBERS; j++) {
        if (!vvp[j])
            continue;

        if (IgnoreInc)
            result = VV_Cmp_IgnoreInc(vvp[dom_ix], vvp[j]);
        else
            result = VV_Cmp(vvp[dom_ix], vvp[j]);

        switch (result) {
        case VV_EQ:
            (*HowMany)++;
            continue;

        case VV_DOM:
            if (EqReq)
                return (0);
            vvp[j] = 0;
            continue;

        case VV_SUB:
            if (EqReq)
                return (0);
            for (k = dom_ix; k < j; k++)
                vvp[k] = 0;
            *HowMany = 1;
            dom_ix   = j;
            continue;

        case VV_INC:
            if (EqReq)
                return (0);
            return (VV_BruteForceCheck(HowMany, vvp, EqReq, IgnoreInc));
        }
    }
    return (1);
}

/* I can't decide whether this is ever necessary! -JJK */
/* A less efficient algorithm, but it can deal with inconsistencies amongst submissive vectors. */
static int VV_BruteForceCheck(int *HowMany, ViceVersionVector **vvp, int EqReq,
                              int IgnoreInc)
{
    /* If IgnoreInc is set, uses VV_Cmp_IgnoreInc() for comparison;
	else uses VV_Cmp()
    */

    /* Find the first non-Null vector and initialize the outer loop counter to its index. */
    int i = -1, j;
    int result;

    for (j = 0; j < VSG_MEMBERS; j++)
        if (vvp[j]) {
            i = j;
            break;
        }
    if (i == -1) {
        *HowMany = 0;
        return 1;
    } /* empty input array */

    /* Find the first vector which is >= or == all other vectors (depending on EqReq). */
    for (; i < VSG_MEMBERS; i++) {
        if (!vvp[i])
            continue;
        *HowMany = 1;
        for (j = 0; j < VSG_MEMBERS; j++) {
            if (i == j)
                continue;
            if (!vvp[j])
                continue;

            if (IgnoreInc)
                result = VV_Cmp_IgnoreInc(vvp[i], vvp[j]);
            else
                result = VV_Cmp(vvp[i], vvp[j]);
            switch (result) {
            case VV_EQ:
                (*HowMany)++;
                continue;

            case VV_DOM:
                if (EqReq)
                    return 0;
                vvp[j] = 0;
                continue;

            case VV_SUB:
                if (EqReq)
                    return 0;
                vvp[i] = 0;
                goto outer_continue;

            case VV_INC:
                if (EqReq)
                    return 0;
                goto outer_continue;
            }
        }
        return 1;

    outer_continue:; /* empty statement to satisfy compiler */
    }

    return 0;
}

/* v = x */
/*
void InitVV(ViceVersionVector *v, long x) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	(&(v->Versions.Site0))[i] = x;
    v->StoreId.HostId = 0;
    v->StoreId.Uniquifier = 0;
    v->Flags = 0;
}
*/

/* v1 = v1 + v2 */
void AddVVs(ViceVersionVector *v1, ViceVersionVector *v2)
{
    for (int i = 0; i < VSG_MEMBERS; i++)
        (&(v1->Versions.Site0))[i] += (&(v2->Versions.Site0))[i];
}

/* v1 = v1 - v2 */
void SubVVs(ViceVersionVector *v1, ViceVersionVector *v2)
{
    for (int i = 0; i < VSG_MEMBERS; i++)
        (&(v1->Versions.Site0))[i] -= (&(v2->Versions.Site0))[i];
}

/* v = 0 */
void InitVV(ViceVersionVector *v)
{
    for (int i = 0; i < VSG_MEMBERS; i++)
        (&(v->Versions.Site0))[i] = 0;
    v->StoreId.HostId     = 0;
    v->StoreId.Uniquifier = 0;
    v->Flags              = 0;
}

int IsRunt(ViceVersionVector *v)
{
    for (int i = 0; i < VSG_MEMBERS; i++)
        if ((&(v->Versions.Site0))[i])
            return (0);
    if (v->StoreId.HostId || v->StoreId.Uniquifier ||
        (v->Flags && !IsIncon(*v)))
        return (0);
    return (1);
}

/* v = -1 */
void InvalidateVV(ViceVersionVector *v)
{
    for (int i = 0; i < VSG_MEMBERS; i++)
        (&(v->Versions.Site0))[i] = -1;
    v->StoreId.HostId     = (unsigned)-1; /* indicates VV is undefined */
    v->StoreId.Uniquifier = (unsigned)-1;
    v->Flags              = 0; /* must be 0, otherwise IsIncon() is fooled */
}

/* newvv[i] = max(vvgroup[][i]); storeid is got from dominant index */
/* domindex == -1 --> weakly equal group; pick any storeid */
/* domindex == -2 --> don't set storeid, let it be zero */
void GetMaxVV(ViceVersionVector *newvv, ViceVersionVector **vvgroup,
              int domindex)
{
    int i, j;
    memset(newvv, 0, sizeof(ViceVersionVector));
    for (i = 0; i < VSG_MEMBERS; i++) {
        /* compute max of VV[][i] */
        long max = 0;
        for (j = 0; j < VSG_MEMBERS; j++)
            if (vvgroup[j]) {
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
            if (vvgroup[i])
                break;
        if (i < VSG_MEMBERS)
            memcpy(&newvv->StoreId, &vvgroup[i]->StoreId, sizeof(ViceStoreId));
        break;
    case -2:
        /* do nothing */
        break;
    default:
        memcpy(&newvv->StoreId, &vvgroup[domindex]->StoreId,
               sizeof(ViceStoreId));
    }
}

void SPrintVV(char *buf, size_t len, ViceVersionVector *v)
{
#if VSG_MEMBERS != 8
#error "I was expecting VSG_MEMBERS to be 8"
#endif
    int n;
    n = snprintf(buf, len, "[ %d %d %d %d %d %d %d %d ] [ %x.%x ] [ %#x ]",
                 v->Versions.Site0, v->Versions.Site1, v->Versions.Site2,
                 v->Versions.Site3, v->Versions.Site4, v->Versions.Site5,
                 v->Versions.Site6, v->Versions.Site7, v->StoreId.HostId,
                 v->StoreId.Uniquifier, v->Flags);
    assert(n >= 0 && (size_t)n < len);
    buf[len - 1] = '\0';
}

void FPrintVV(FILE *fp, ViceVersionVector *v)
{
    char buf[256];
    SPrintVV(buf, sizeof(buf), v);
    fprintf(fp, "%s\n", buf);
}

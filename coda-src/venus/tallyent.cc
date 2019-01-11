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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif

#include "coda_assert.h"
#include <dlist.h>
#include <vcrcommon.h>

#ifndef TESTING
#include "venus.private.h"
#endif

#include "tallyent.h"

#ifdef TESTING
#include <stdlib.h>
#define LOG(level, stmt) printf stmt
#endif /* TESTING */

/* List of tallyents sorted by <priority, uid> */
dlist *TallyList;

tallyent::tallyent(int p, uid_t v, int b, TallyStatus s)
{
    /* Initialize */
    priority           = p;
    uid                = v;
    available_blocks   = 0;
    available_files    = 0;
    unavailable_blocks = 0;
    unavailable_files  = 0;
    incomplete         = 0;

    switch (s) {
    case TSavailable:
        available_blocks = b;
        available_files  = 1;
        break;
    case TSunavailable:
        unavailable_blocks = b;
        unavailable_files  = 1;
        break;
    case TSunknown:
        incomplete = 1;
        break;
    default:
        CODA_ASSERT(1 == 0);
        break;
    }
}

tallyent::tallyent(tallyent &)
{
    abort();
}

int tallyent::operator=(tallyent &i)
{
    abort();
    return (0);
}

tallyent::~tallyent()
{
    CODA_ASSERT(TallyList != NULL);
    TallyList->remove(&prioq_handle);
}

int tallyentPriorityFN(dlink *d1, dlink *d2)
{
    tallyent *t1, *t2;

    CODA_ASSERT(d1 != NULL);
    CODA_ASSERT(d2 != NULL);

    t1 = strbase(tallyent, d1, prioq_handle);
    t2 = strbase(tallyent, d2, prioq_handle);

    CODA_ASSERT(t1 != NULL);
    CODA_ASSERT(t2 != NULL);

    if (t1->priority == t2->priority) {
        if (t1->uid == t2->uid)
            return (0);
        else if (t1->uid > t2->uid)
            return (1);
        else
            return (-1);

    } else if (t1->priority > t2->priority)
        return (1);
    else
        return (-1);
}

void InitTally()
{
    if (TallyList != NULL) { // If we already have a valid TallyList, delete it
        dlist_iterator next(*TallyList);
        dlink *d;

        d = next();
        while (d) {
            tallyent *te = strbase(tallyent, d, prioq_handle);
            d            = next();
            delete te;
        }

        delete TallyList;
    }

    TallyList = new dlist(tallyentPriorityFN);
}

dlink *Find(int priority, uid_t uid)
{
    dlist_iterator next(*TallyList);
    dlink *d;

    while ((d = next())) {
        tallyent *te = strbase(tallyent, d, prioq_handle);
        if ((te->priority == priority) && (te->uid == uid))
            return (d);
    }

    return (0);
}

void Tally(int priority, uid_t uid, int blocks, TallyStatus status)
{
    tallyent *te;
    dlink *d;

    LOG(100, ("Tally: priority=%d, uid=%d, blocks=%d, status=%d\n", priority,
              uid, blocks, (int)status));

    CODA_ASSERT(TallyList != NULL);
    d = Find(priority, uid);
    if (d != NULL) {
        te = strbase(tallyent, d, prioq_handle);
        CODA_ASSERT(te != NULL);
        CODA_ASSERT(te->priority == priority);
        CODA_ASSERT(te->uid == uid);
        switch (status) {
        case TSavailable:
            te->available_blocks += blocks;
            te->available_files++;
            break;
        case TSunavailable:
            te->unavailable_blocks += blocks;
            te->unavailable_files++;
            break;
        case TSunknown:
            te->incomplete = 1;
            break;
        default:
            CODA_ASSERT(1 == 0);
            break;
        }
        LOG(100, ("tallyent::tallyent: updated <priority=%d, uid=%d>\n",
                  priority, uid));
    } else {
        te = new tallyent(priority, uid, blocks, status);
        TallyList->insert(&(te->prioq_handle));
        LOG(100, ("tallyent::tallyent: inserted <priority=%d, uid=%d>\n",
                  priority, uid));
    }
}

void TallyPrint(uid_t uid)
{
    LOG(0, ("Tally for uid=%d:\n", uid));
    dlist_iterator next(*TallyList);
    dlink *d;
    while ((d = next())) {
        tallyent *te = strbase(tallyent, d, prioq_handle);
        CODA_ASSERT(te != NULL);

        if (te->uid != uid)
            continue;

        int total_blocks = te->available_blocks + te->unavailable_blocks;
        LOG(0,
            ("\tPriority=%d: Available=%d Unavailable=%d TotalSize=%d Unknown=%d\n",
             te->priority, te->available_blocks, te->unavailable_blocks,
             total_blocks, te->incomplete));
    }
}

void TallySum(int *total_blocks, int *total_files)
{
    LOG(0, ("TallySum\n"));
    *total_blocks = 0;
    *total_files  = 0;

    if (TallyList == NULL)
        return;

    {
        dlist_iterator next(*TallyList);
        dlink *d;
        while ((d = next())) {
            tallyent *te = strbase(tallyent, d, prioq_handle);
            CODA_ASSERT(te != NULL);

            *total_blocks += (te->available_blocks + te->unavailable_blocks);
            *total_files += (te->available_files + te->unavailable_files);

            LOG(0,
                ("\tPriority=%d: Available=%d Unavailable=%d TotalSize=%d Unknown=%d\n",
                 te->priority, te->available_blocks, te->unavailable_blocks,
                 total_blocks, te->incomplete));
        }
    }
}

#ifdef TESTING

int main(int argc, char *argv[])
{
    InitTally();

    Tally(1000, 3401, 400, TSunavailable);

    Tally(700, 3401, 234, TSavailable);
    Tally(700, 3401, 345, TSavailable);
    Tally(700, 3401, 567, TSunavailable);

    TallyPrint(3401);

    InitTally();

    Tally(1000, 2660, 10, TSavailable);
    Tally(1000, 2660, 20, TSavailable);
    Tally(1000, 2660, 1000, TSavailable);

    Tally(900, 2660, 20, TSunavailable);
    Tally(900, 2660, 30, TSavailable);
    Tally(900, 2660, 40, TSavailable);
    Tally(900, 2660, 20, TSunavailable);

    Tally(825, 2660, 50, TSunavailable);
    Tally(850, 2660, 50, TSavailable);
    Tally(875, 2660, 30, TSunknown);

    Tally(1000, 2208, 400, TSavailable);
    Tally(1000, 2208, 400, TSunavailable);

    Tally(700, 2208, 234, TSavailable);
    Tally(700, 2208, 345, TSavailable);
    Tally(700, 2208, 567, TSunavailable);

    TallyPrint(2660);

    TallyPrint(2208);
}

#endif /* TESTING */

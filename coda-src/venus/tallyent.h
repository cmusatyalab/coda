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

#ifndef _TALLYENT_H_
#define _TALLYENT_H_ 1

#include <dlist.h>

enum TallyStatus
{
    TSavailable,
    TSunavailable,
    TSunknown
};

class tallyent {
    friend void InitTally();
    friend int tallyentPriorityFN(dlink *, dlink *);
    friend void Tally(int, uid_t, int, TallyStatus);
    friend dlink *Find(int, uid_t);
    friend void TallyPrint(uid_t);
    friend void TallySum(int *, int *);
    friend void NotifyUsersTaskAvailability();

    dlink prioq_handle;

    int priority;
    uid_t uid;
    int available_blocks;
    int available_files;
    int unavailable_blocks;
    int unavailable_files;
    int incomplete;

public:
    tallyent(int priority, uid_t uid, int blocks, TallyStatus status);
    tallyent(tallyent &);
    int operator=(tallyent &);
    ~tallyent();
};

extern dlist *TallyList;

extern void InitTally();
extern dlink *Find(int priority, uid_t uid);
extern void Tally(int, uid_t, int, TallyStatus);
extern void TallyPrint(uid_t);
extern void TallySum(int *, int *);

#endif /* _TALLYENT_H_ */

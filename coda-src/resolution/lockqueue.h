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
 * Specification of the Volume Lock Queue facility
 *
 */

#ifndef _VOL_LOCKQUEUE_H_
#define _VOL_LOCKQUEUE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <lock.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <dlist.h>


class lqman;
class lq_iterator;
class lqent;

class lqman {
    char *name;
    struct Lock lock;
    int pid;
    dlist objects;

    int func(void *);

  public:
    lqman(char * ="anonymous lqman");
    ~lqman();
    void add(lqent *);
    void remove(lqent *);
    lqent *find(VolumeId);
    lqent *findanddeq(VolumeId);
    void print();
    void print(FILE *);
    void print(int);
};

class lq_iterator : public dlist_iterator {
  public:
    lq_iterator(dlist&);
    lqent *operator()();
};

class lqent : public dlink{
  friend class lqman;
  friend int GetVolObj(VolumeId, Volume **, int, int, int);
  friend void PutVolObj(Volume **, int, int);
  friend long ViceLockVol(RPC2_Handle, VolumeId, UserId *, RPC2_Unsigned *);
  friend long ViceUnlockVol(RPC2_Handle, VolumeId);

    VolumeId Vid;
    long Time;
    int deqing;
    
    lqent(VolumeId);
    ~lqent();

  public:
    void print();
    void print(FILE *);
    void print(int);
};

extern lqman *LockQueueMan;
extern void InitLockVolQue();
extern void InitLockQueue();
#endif not _VOL_LOCKQUEUE_H_

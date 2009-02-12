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
 * Specification of the Volume Lock Queue facility
 *
 */

#ifndef _VOL_LOCKQUEUE_H_
#define _VOL_LOCKQUEUE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

#include <vice.h>
#include <dlist.h>

#define NO_LOCK		0	/* remains for compatibility */
#define VOL_NO_LOCK	NO_LOCK
#define VOL_SHARED_LOCK	SHARED_LOCK
#define VOL_EXCL_LOCK	WRITE_LOCK

void ForceUnlockVol(VolumeId );

class lq_iterator;
class lqent;

class lqman {
    char *name;
    struct Lock lock;
    PROCESS pid;
    dlist objects;

    friend void LQman_init(void *);
    int func(void);

  public:
    lqman(const char *name = "anonymous lqman");
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
  friend int GetVolObj(VolumeId, Volume **, int, int, unsigned);
  friend void PutVolObj(Volume **, int, int);
  friend long ViceLockVol(RPC2_Handle, VolumeId, UserId *, RPC2_Unsigned *);
  friend long ViceUnlockVol(RPC2_Handle, VolumeId);

    VolumeId Vid;
    time_t Time;
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
#endif /* _VOL_LOCKQUEUE_H_ */

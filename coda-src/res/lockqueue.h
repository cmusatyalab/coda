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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/res/Attic/lockqueue.h,v 4.1 1997/01/08 21:49:57 rvb Exp $";
#endif /*_BLURB_*/






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

    void func(int);

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

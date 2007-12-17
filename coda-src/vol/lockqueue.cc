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
 * Implementation of the Volume Lock Queue
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <volume.h>
#include <srv.h>
#include <vice.h>
#include <volume.h>
#include "lockqueue.h"

const int  LQTIMEOUT = 300;	/* seconds */
const int  LQINTERVAL =	60;	/* seconds */
const int  LockQueManStkSize = 16384;
lqman *LockQueueMan;


// used by the lock queue manager to unlock expired locks 
void ForceUnlockVol(VolumeId Vid) {/* Vid is the rw id */
    Volume *volptr;
    if (GetVolObj(Vid, &volptr, VOL_NO_LOCK, 0, 0)) {
	SLog(0,  "ForceUnlockVol: GetVolObj %x error", Vid);
	return;
    }
    PutVolObj(&volptr, VOL_EXCL_LOCK, 0);
}

void InitLockQueue() {
    LockQueueMan = new lqman("LockQueue Manager");
}

void LQman_init(void *c)
{
    lqman *lqm = (lqman *)c;
    lqm->func();
}

lqman::lqman(const char *n)
{
    name = strdup(n);
    Lock_Init(&lock);
    
    /* Create the LWP process */
    printf("lqman: Creating LockQueue Manager.....");
    LWP_CreateProcess(LQman_init, LockQueManStkSize, LWP_NORMAL_PRIORITY,
		      this, name, &pid);
    printf("done\n");
}

lqman::~lqman()
{
    LWP_DestroyProcess(pid);
    free(name);
}

int lqman::func(void)
{
    ProgramType *pt;
    rvm_perthread_t rvmptt;

    printf("LockQueue Manager starting .....\n");

    rvmlib_init_threaddata(&rvmptt);
    SLog(0, "LockQueue Manager just did a rvmlib_set_thread_data()\n");

    /* tag this lwp as a volume utility */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = volumeUtility;
    CODA_ASSERT(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    for (;;) {
	LogMsg(1, SrvDebugLevel, stdout,  "LockQueue Manager woken up");
	time_t currtime = time(0);
	ObtainWriteLock(&lock);

	{
	    lq_iterator next(objects);
	    lqent *lqe, *next_lqe;

	    next_lqe = next();
	    while ((lqe = next_lqe) != NULL) {
		next_lqe = next();
		LogMsg(0, SrvDebugLevel, stdout,
		       "LockQueue Manager: found entry for volume 0x%x",
		       lqe->Vid);
		if ((lqe->Time + LQTIMEOUT <= currtime) && !lqe->deqing) {
		    printf("LQMan: Unlocking %08x\n", lqe->Vid);
		    objects.remove(lqe);
		    ForceUnlockVol(lqe->Vid);
		    delete lqe;
		}
	    }
	}
	ReleaseWriteLock(&lock);
	LogMsg(1, SrvDebugLevel, stdout,  "LockQueue Manager sleeping for %d seconds", LQINTERVAL);
	VSLEEP(LQINTERVAL);
    }
    return 0;
}

void lqman::add(lqent *lqe)
{
    LogMsg(1, SrvDebugLevel, stdout,  "lqman::add adding entry for volume 0x%x",
	    lqe->Vid);
    ObtainWriteLock(&lock);
    objects.append(lqe);
    ReleaseWriteLock(&lock);
}
void lqman::remove(lqent *lqe) {
    ObtainWriteLock(&lock);
    objects.remove(lqe);
    ReleaseWriteLock(&lock);
}
lqent *lqman::find(VolumeId vid) {
    ObtainReadLock(&lock);

    lq_iterator next(objects);
    lqent *lqe;

    while ((lqe = next()))
	if (vid == lqe->Vid)
	    break;
    ReleaseReadLock(&lock);
    return(lqe);
}
lqent *lqman::findanddeq(VolumeId vid) {
    ObtainReadLock(&lock);
    lq_iterator next(objects);
    lqent *lqe;
    while ((lqe = next())) 
	if (vid == lqe->Vid)
	    break;
    if (lqe)
	lqe->deqing = 1;
    ReleaseReadLock(&lock);
    return(lqe);
}
void lqman::print() {
    print(stdout);
}
void lqman::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}
void lqman::print(int fd) { 
    ObtainReadLock(&lock);
    char buf[80];
    sprintf(buf, "%-16s\n", name);
    write(fd, buf, (int)strlen(buf));
    
    lq_iterator	next(objects);
    lqent *lqe;
    while((lqe = next())) lqe->print(fd);
    ReleaseReadLock(&lock);
}

lq_iterator::lq_iterator(dlist& dl):dlist_iterator(dl) {
}
lqent *lq_iterator::operator()() {
    return((lqent *) dlist_iterator::operator()());
}

lqent::lqent(VolumeId vid) {
    Vid = vid;
    Time = ::time(0);
    deqing = 0;
}

lqent::~lqent(){
}

void lqent::print() {
    print(stdout);
}
void lqent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}
void lqent::print(int fd) {
    char buf[80];
    
    sprintf(buf, "VolumeId = %08x, time = %ld, deqing = %d\n",
	     Vid, Time, deqing);
    write(fd, buf, (int)strlen(buf));
}


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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/res/Attic/lockqueue.cc,v 4.2.8.1 1998/10/05 16:54:39 rvb Exp $";
#endif /*_BLURB_*/






/* 
 * 
 * Implementation of the Volume Lock Queue
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <lwp.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <srv.h>
#include <vice.h>
#include "lockqueue.h"

const int  LQTIMEOUT = 600;	/* seconds */
const int  LQINTERVAL =	60;	/* seconds */
const int  LockQueManStkSize = 8192;
lqman *LockQueueMan;

extern void ForceUnlockVol(VolumeId );

void InitLockQueue() {
    LockQueueMan = new lqman("LockQueue Manager");
}

lqman::lqman(char *n) {
    name = new char[strlen(n) + 1];
    strcpy(name, n);
    Lock_Init(&lock);
    
    /* Create the LWP process */
    printf("lqman: Creating LockQueue Manager.....");
    LWP_CreateProcess((PFIC) &(lqman::func), LockQueManStkSize, LWP_NORMAL_PRIORITY, (char*)this, name, (PROCESS *)&pid);
    printf("done\n");
}

lqman::~lqman() {
    delete name;
    LWP_DestroyProcess((PROCESS) pid);
}

void lqman::func(int parm) {
    ProgramType *pt;

    printf("LockQueue Manager starting .....\n");
    /* tag this lwp as a volume utility */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = volumeUtility;
    assert(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    for (;;) {
	LogMsg(1, SrvDebugLevel, stdout,  "LockQueue Manager woken up");
	long currtime = time(0);
	ObtainWriteLock(&lock);

	{
	    lq_iterator next(objects);
	    lqent *lqe;
	    int readahead = 0;
	    while (readahead || (lqe = next())) {
		readahead = 0;
		LogMsg(0, SrvDebugLevel, stdout,  "LockQueue Manager: found entry for volume 0x%x", 
			lqe->Vid);
		if ((lqe->Time + LQTIMEOUT <= currtime) &&
		    !lqe->deqing) {
		    printf("LQMan: Unlocking %x\n", lqe->Vid);
		    lqent *tmplqe = lqe;
		    readahead = ((lqe = next()) != 0);
		    objects.remove(tmplqe);
		    ForceUnlockVol(tmplqe->Vid);
		    delete tmplqe;
		}
	    }
	}
	ReleaseWriteLock(&lock);
	LogMsg(1, SrvDebugLevel, stdout,  "LockQueue Manager sleeping for %d seconds", LQINTERVAL);
	VSLEEP(LQINTERVAL);
    }
}

void lqman::add(lqent *lqe) {
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

    while (lqe = next())
	if (vid == lqe->Vid)
	    break;
    ReleaseReadLock(&lock);
    return(lqe);
}
lqent *lqman::findanddeq(VolumeId vid) {
    ObtainReadLock(&lock);
    lq_iterator next(objects);
    lqent *lqe;
    while (lqe = next()) 
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
    while(lqe = next()) lqe->print(fd);
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
    
    sprintf(buf, "VolumeId = 0x%x, time = %d, deqing = %d\n",
	     Vid, Time, deqing);
    write(fd, buf, (int)strlen(buf));
}


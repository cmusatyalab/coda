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


#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <util.h>
#include "coda_string.h"
#ifdef __cplusplus
}
#endif

#include <srv.h>
#include "coppend.h"

const int CPTIMEOUT = 900; 	/* seconds */
const int cpman_stacksize = 8192;

cpman *CopPendingMan;

void InitCopPendingTable() {
    CopPendingMan = new cpman("Cop Pending Manager");
}

void AddToCopPendingTable(ViceStoreId *stid, ViceFid *fids) {
    CopPendingMan->add(new cpent(stid, fids));
}

void AddPairToCopPendingTable(ViceStoreId *sid, ViceFid *fid) {
    int i;
    cpent *cpe = CopPendingMan->find(sid);
    if (cpe) {
	int found = 0;
	for (i = 0; i < MAXFIDS; i++) {
	    if (FID_EQ(&cpe->fids[i], &NullFid))
		break;
	    if (FID_EQ(&cpe->fids[i], fid)) {
		found = 1;
		break;
	    }
	}
	if (!found) {
	    CODA_ASSERT(i < MAXFIDS);
	    cpe->fids[i] = *fid;
	}
    }
    else {
	ViceFid fids[MAXFIDS];
	memset((void *)fids, 0, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = *fid;
	CopPendingMan->add(new cpent(sid, fids));
    }
}

cpent::cpent(ViceStoreId *sid, ViceFid *fidp) {
    StoreId = *sid;
    for (int i = 0; i < MAXFIDS; i++)
	fids[i] = fidp[i];
    time = ::time(0);
    deqing = 0;
    id = CPENTMAGIC;
}
cpent::~cpent() {
    if (id != CPENTMAGIC) {
	LogMsg(0, SrvDebugLevel, stdout,
	       "~cpent: id is not CPENTMAGIC but %d\n",
	       id);
	CODA_ASSERT(0);
    }
    id = 0;
}

void cpent::print() {
    print(stdout);
}

void cpent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}

void cpent::print(int fd) {
    char buf[80];

    sprintf(buf, "StoreId = (0x%lx.%lx), time = %ld, deqing = %d\n",
	     StoreId.Host, StoreId.Uniquifier, time, deqing);

    write(fd, buf, (int)strlen(buf));

}

int coppendhashfn(void *a) {
    ViceStoreId *sid = (ViceStoreId *)a;
    return(sid->Host + sid->Uniquifier);
}

int cpman_func(cpman *cpman)
{
    return cpman->func(0);
}

cpman::cpman(char *n): objects(COPHASHSIZE, coppendhashfn) {
    name = new char[strlen(n) + 1];
    strcpy(name, n);
    Lock_Init(&lock);
    LWP_CreateProcess((PFIC)&cpman_func, cpman_stacksize, LWP_NORMAL_PRIORITY,
		       (char *)this, name, (PROCESS *)&pid);
}

cpman::~cpman() {
    delete [] name;
    LWP_DestroyProcess((PROCESS) pid);
}

const int CPINTERVAL = 60; 	/* seconds */
int cpman::func(int parm) {
    /* parm is currently not used. */

    for (;;) {
	long currtime = time(0);
	ObtainWriteLock(&lock);

	ohashtab_iterator next(objects);
	cpent *cpe;
	if ((cpe = (cpent *)next()) && (cpe->time + CPTIMEOUT <= currtime) && !cpe->deqing) {
	    if (SrvDebugLevel >= 1) {
		LogMsg(1, SrvDebugLevel, stdout,  "StoreId = (%x.%x)",
			cpe->StoreId.Host, cpe->StoreId.Uniquifier);
		for (int i = 0; i < MAXFIDS; i++)
		    if (!FID_EQ(&cpe->fids[i], &NullFid))
			LogMsg(1, SrvDebugLevel, stdout,  ", fids[%d] = (0x%x.%x.%x)",
				i, cpe->fids[i].Volume, cpe->fids[i].Vnode, cpe->fids[i].Unique);
		LogMsg(1, SrvDebugLevel, stdout,  ", expired on BusyQueue");
	    }
	    
	    objects.remove((void *)&cpe->StoreId, (olink *)cpe);
	    delete cpe;
	}
    

	ReleaseWriteLock(&lock);

	if (SrvDebugLevel >= 9)
	    print();
       
	VSLEEP(CPINTERVAL);
    }
    
    return(0);  /* just to keep C++ happy; will never get here */
}

/* Insert entry into table */
void cpman::add(cpent *cpe) {
    ObtainWriteLock(&lock);
    LogMsg(9, SrvDebugLevel, stdout,  "StoreId = (0x%x.%x)", 
	    cpe->StoreId.Host, cpe->StoreId.Uniquifier);	
    for (int i = 0; i < MAXFIDS; i++) 
	    if (!FID_EQ(&cpe->fids[i], &NullFid))
		    LogMsg(9, SrvDebugLevel, stdout,  ", fids[%d] = (0x%x.%x.%x)",
			   i, cpe->fids[i].Volume, 
			   cpe->fids[i].Vnode, cpe->fids[i].Unique);
    LogMsg(9, SrvDebugLevel, stdout,  ", added to BusyQueue");
    
    objects.append(&cpe->StoreId, cpe);
    ReleaseWriteLock(&lock);
}


void cpman::remove(cpent *cpe) 
{
    ObtainWriteLock(&lock);
    objects.remove(&cpe->StoreId, cpe);
    ReleaseWriteLock(&lock);
}

cpent *cpman::find(ViceStoreId *StoreId) {
    ObtainReadLock(&lock);

    ohashtab_iterator next(objects, StoreId);
    cpent *cpe;
    while((cpe = (cpent *)next()))
	if (cpe->StoreId.Host == StoreId->Host &&
	    cpe->StoreId.Uniquifier == StoreId->Uniquifier) {
	    ReleaseReadLock(&lock);
	    return(cpe);
	}
    ReleaseReadLock(&lock);
    return(0);
}

/*
cpent *cpman::find(ViceFid *fid) {
    if (FID_EQ(fid, &NullFid))
	Die("cpman::find: fid == NullFid");

    ObtainReadLock(&lock);

    ohashtab_iterator next(objects);
    cpent *cpe;
    while(cpe = (cpent *)next())
	for (int i = 0; i < MAXFIDS; i++)
	    if (FID_EQ(cpe->fids[i], *fid)) {
		ReleaseReadLock(&lock);
		return(cpe);
	    }
    ReleaseReadLock(&lock);
    return(0);
}
*/
cpent *cpman::findanddeq(ViceStoreId *StoreId) {
    ObtainReadLock(&lock);
    ohashtab_iterator next(objects, StoreId);
    cpent *cpe;

    while ((cpe = (cpent *)next()) )
	if (cpe->StoreId.Host == StoreId->Host &&
	    cpe->StoreId.Uniquifier == StoreId->Uniquifier)
	    break;
    if (!cpe) {
	ReleaseReadLock(&lock);
	return(0);
    }
    if (cpe->deqing) {
	LogMsg(0, SrvDebugLevel, stdout, 
	       "****** WARNING entry at 0x%x already has deqing set!\n",
	       this);
	ReleaseReadLock(&lock);
	return(0);
    }
    cpe->deqing = 1;
    ReleaseReadLock(&lock);
    return(cpe);
}

void cpman::print() {
    print(stdout);
}
void cpman::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}

void cpman::print(int fd) {
    ObtainReadLock(&lock);
    char buf[40];
    sprintf(buf, "%p : %-16s\n", this, name);
    write(fd, buf, (int)strlen(buf));

    ohashtab_iterator next(objects);
    cpent *cpe;
    while ((cpe = (cpent *)next())) cpe->print(fd);

    ReleaseReadLock(&lock);
}



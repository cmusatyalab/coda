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

#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>
#include "coda_assert.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <cfs/coda.h>
#include <lwp.h>
#include <lock.h>
#include <rvmlib.h>
#include "codadir.h"
#include "dirbody.h"
#ifdef __cplusplus
}
#endif __cplusplus



/*
 * LOCK support
 */ 

void DH_LockW(PDirHandle dh)
{
	ObtainWriteLock(&dh->dh_lock);
}

void DH_LockR(PDirHandle dh)
{
	ObtainReadLock(&dh->dh_lock);
}

void DH_UnLockW(PDirHandle dh)
{
	ReleaseWriteLock(&dh->dh_lock);
}

void DH_UnLockR(PDirHandle dh)
{
	ReleaseReadLock(&dh->dh_lock);
}

void DH_Init(PDirHandle dh)
{
	CODA_ASSERT(dh);
	bzero(dh, sizeof(*dh));
	Lock_Init(&dh->dh_lock);
}


int DH_Length(PDirHandle dh)
{
	int rc;

	DH_LockW(dh);

	rc = DIR_Length(dh->dh_data);

	DH_UnLockW(dh);

	return rc;
}


/* to convert Coda dir to Unix dir: called by client */
int DH_Convert(PDirHandle dh, char *file, VolumeId vol)
{
	int rc;

	DH_LockR(dh);

	rc = DIR_Convert(dh->dh_data, file, vol);

	DH_UnLockR(dh);

	return rc;
}

/* create new entry: called by client and server */
int DH_Create (PDirHandle dh, char *entry, struct ViceFid *vfid)
{
	int rc;
	struct DirFid dfid;
	
	FID_VFid2DFid(vfid, &dfid);
	
	DH_LockW(dh);
	dh->dh_dirty = 1;

	rc = DIR_Create(&dh->dh_data, entry, &dfid);

	DH_UnLockW(dh);

	return rc;
}

/* check if the directory has entries apart from . and .. */
int DH_IsEmpty(PDirHandle dh)
{
	int rc;

	DH_LockR(dh);

	rc  = DIR_IsEmpty(dh->dh_data);

	DH_UnLockR(dh);

	return rc;

}


/* find fid given the name: called all over */
int DH_Lookup(PDirHandle dh, char *entry, struct ViceFid *vfid)
{
	int rc;
	struct DirFid dfid;

	DH_LockR(dh);

	rc  = DIR_Lookup(dh->dh_data, entry, &dfid);

	DH_UnLockR(dh);

	FID_DFid2VFid(&dfid, vfid);

	return rc;

}

int DH_LookupByFid(PDirHandle dh, char *entry, struct ViceFid *vfid)
{
	int rc;
	struct DirFid dfid;
	
	FID_VFid2DFid(vfid, &dfid);

	DH_LockR(dh);

	rc  = DIR_LookupByFid(dh->dh_data, entry, &dfid);

	DH_UnLockR(dh);

	return rc;

}

/* remove an entry from a directory */
int DH_Delete(PDirHandle dh, char *entry) 
{
	int rc;
	
	DH_LockW(dh);
	dh->dh_dirty = 1;

	rc = DIR_Delete(dh->dh_data, entry);

	DH_UnLockW(dh);

	return rc;
}


/* the end of the data */
void DH_FreeData(PDirHandle dh)
{
	DH_LockW(dh);

	if ( DIR_rvm() ) {
		rvmlib_rec_free(dh->dh_data);
	} else {
		free(dh->dh_data);
	}

	dh->dh_data = NULL;
	DH_UnLockW(dh);
}

/* alloc a directory buffer for the DH */
void DH_Alloc(PDirHandle dh, int size, int in_rvm)
{
	CODA_ASSERT(dh);
	DH_LockW(dh);
	dh->dh_dirty = 1;
	if ( in_rvm ) {
		DIR_intrans();
		RVMLIB_REC_OBJECT(*dh);
		dh->dh_data = rvmlib_rec_malloc(size);
		CODA_ASSERT(dh->dh_data);
		bzero((void *)dh->dh_data, size);
	} else {
		dh->dh_data = malloc(size);
		CODA_ASSERT(dh->dh_data);
		bzero((void *)dh->dh_data, size);
	}

	DH_UnLockW(dh);
	return;
}

PDirHeader DH_Data(PDirHandle dh)
{
	return dh->dh_data;
}

void DH_Print(PDirHandle dh)
{

	DH_LockR(dh);
	DIR_Print(dh->dh_data);
	DH_UnLockR(dh);
	return;
}


int DH_DirOK(PDirHandle dh)
{
	int rc;

	DH_LockR(dh);
	rc = DIR_DirOK(dh->dh_data);
	DH_UnLockR(dh);
	return rc;
}

int DH_MakeDir(PDirHandle dh, struct ViceFid *vme, struct ViceFid *vparent)
{
	int rc;
	struct DirFid dme;
	struct DirFid dparent;

	FID_VFid2DFid(vme, &dme);
	FID_VFid2DFid(vparent, &dparent);

	DH_LockW(dh);
	dh->dh_dirty = 1;
	if ( DIR_rvm() ) {
		DIR_intrans();
		rc = DIR_MakeDir(&dh->dh_data, &dme, &dparent);
	} else {
		rc = DIR_MakeDir(&dh->dh_data, &dme, &dparent);
	}
	DH_UnLockW(dh);
	return rc;
}

int DH_EnumerateDir(PDirHandle dh, int (*hookproc)(struct DirEntry *de, void* hook) , 
		    void *hook)
{
	int rc;
	
	DH_LockW(dh);

	rc = DIR_EnumerateDir(dh->dh_data, hookproc, hook);

	DH_UnLockW(dh);

	return rc;
}


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

#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>

#include <lwp.h>
#include <lock.h>
#include <dllist.h>
#include <util.h>
#include "codadir.h"

/* this cache of directory handles is a child of the cvnode cache.
   For this reason we do not maintain a freelist.
*/
/* directory handle cache entry */
struct DCEntry {
	struct dllist_chain   dc_hash;
	struct dllist_chain   dc_list;
	int                   dc_count;/* number of VM vnodes referencing us */
	int                   dc_refcount; /* new refcount upon commit */
	struct DirHandle      dc_dh;
	PDirInode             dc_pdi;
	PDirInode             dc_cowpdi;
};

#define DCGROWTH 8
#define DCSIZE 256
#define DHLOGCACHESIZE 8

static Lock               dlock;
static struct dllist_head dcache[DCSIZE];
static struct dllist_head dfreelist; 
static struct dllist_head dnewlist; 


static inline int  DC_Hash(PDirInode pdi) 
{
	return ((int) ((unsigned long)pdi >> 5) % DCSIZE);
}


/* get more free entries in case freelist is empty */
static void dc_Grow(int count)
{
	PDCEntry pdce;
	int i;

	for ( i = 0 ; i < count ; i++ ) {
		pdce = malloc(sizeof(*pdce));
		CODA_ASSERT(pdce);
		bzero(pdce, sizeof(*pdce));

		list_head_init(&pdce->dc_hash);
		list_head_init(&pdce->dc_list);

		ObtainWriteLock(&dlock); 
		list_add(&pdce->dc_list, &dfreelist);
		ReleaseWriteLock(&dlock);

	}	
}

/* get a free cache entry; clean it up before handing it out */
static PDCEntry dc_GetFree()
{
	PDCEntry pdce = NULL;

 again:
	ObtainWriteLock(&dlock); 
	if ( !list_empty(&dfreelist) ) {
		pdce = list_entry(dfreelist.next, struct DCEntry, dc_list);
		list_del(&pdce->dc_list);
		list_del(&pdce->dc_hash);

		/* clean it up before use */
		if ( DH_Data(&pdce->dc_dh) )
			DH_FreeData(&pdce->dc_dh);
		bzero(pdce, sizeof(*pdce));
		list_head_init(&pdce->dc_list);
		list_head_init(&pdce->dc_hash);
		DH_Init(&pdce->dc_dh);
	}
	ReleaseWriteLock(&dlock);
	if ( pdce ) 
		return pdce;

	dc_Grow(DCGROWTH);
	goto again;
}

PDCEntry DC_Get(PDirInode pdi)
{
	int hash;
	struct DCEntry *pdce;
	struct dllist_chain *lh, *tmp;
	
	CODA_ASSERT(pdi);
	hash = DC_Hash(pdi);

	/* see if it is hashed already */
	ObtainWriteLock(&dlock); 
	lh = &dcache[hash]; 
	tmp = lh;
	while ( (tmp = tmp->next) != lh ) {
		pdce = list_entry(tmp, struct DCEntry, dc_hash);
		if ( pdi == pdce->dc_pdi ) {
			pdce->dc_count++;
			/* remove from freelist if first user */
			if ( pdce->dc_count == 1 ) {
				list_del(&pdce->dc_list);
				CODA_ASSERT(!DC_Dirty(pdce));
			}

			/* if data was flushed, refresh it */
			if ( !pdce->dc_dh.dh_data) {
				pdce->dc_dh.dh_data = DI_DiToDh(pdi);
				pdce->dc_refcount = pdi->di_refcount;
			}

			ReleaseWriteLock(&dlock);
			return pdce;
		}
	}

	/* release the lock since we are out of the hash table */
	ReleaseWriteLock(&dlock);

	/* not found use a new one */
	pdce = dc_GetFree();
	pdce->dc_count = 1;
	pdce->dc_pdi = pdi;

	/* re-lock since we want to mess with the lists again */
	ObtainWriteLock(&dlock); 
	list_add(&pdce->dc_hash, &dcache[hash]);

	/* copy in the directory handle, init lock, and copy data */
	pdce->dc_dh.dh_data = DI_DiToDh(pdi);
	CODA_ASSERT(!DC_Dirty(pdce));
	pdce->dc_refcount = pdi->di_refcount;

	ReleaseWriteLock(&dlock);
	return pdce;
}



/* Commit a DirInode - if new move it from the dnewlist to the hash list */
/* called by VN_DCommit when the RVM Dir Inode for mkdir is created */
void DC_Rehash(PDCEntry pdce)
{
	int hash;

	CODA_ASSERT(pdce);

	hash = DC_Hash(pdce->dc_pdi);

	ObtainWriteLock(&dlock); 

	list_del(&pdce->dc_hash);
	list_add(&pdce->dc_hash, &dcache[hash]);
	ReleaseWriteLock(&dlock);
}

/* totally remove a cache entry & its data;
   careful - we are calling free! */
void DC_Drop(PDCEntry pdce)
{
	if ( pdce == NULL )
		return ;
	
	ObtainWriteLock(&dlock); 
	list_del(&pdce->dc_hash);
	list_del(&pdce->dc_list);
	ReleaseWriteLock(&dlock);

	DH_FreeData(&pdce->dc_dh);
	free(pdce);
}

void DC_HashInit()
{
	int i;

	Lock_Init(&dlock);

	for ( i=0 ; i < DCSIZE ; i++ ) {
		list_head_init(&dcache[i]);
	}
	list_head_init(&dfreelist);
	list_head_init(&dnewlist);
}
	

void DC_Put(PDCEntry pdce)
{

	ObtainWriteLock(&dlock); 

	if ( pdce->dc_count == 1 ) {
		list_add(&pdce->dc_list, dfreelist.prev);
		CODA_ASSERT(!DC_Dirty(pdce));
	} 
	pdce->dc_count--;

	ReleaseWriteLock(&dlock);
	return;
}

int DC_Count(PDCEntry pdce) 
{
	return pdce->dc_count;
}

void DC_SetCount(PDCEntry pdce, int count) 
{
	pdce->dc_count = count; 
}



/* called by ViceMakedir */
PDCEntry DC_New()
{
	PDCEntry pdce;
	ObtainWriteLock(&dlock); 

	pdce = dc_GetFree();
	pdce->dc_count = 1;
	pdce->dc_pdi = NULL;
	pdce->dc_refcount = 1;
	DC_SetDirty(pdce, 1);
	list_add(&pdce->dc_hash, &dnewlist);

	ReleaseWriteLock(&dlock);
	return pdce;
}


/* utility functions for setting/getting fields */
inline int DC_Refcount(PDCEntry pdc)
{
	return pdc->dc_refcount;
}

inline void DC_SetRefcount(PDCEntry pdc, int count)
{
	pdc->dc_refcount = count;
}

PDirHandle DC_DC2DH(PDCEntry pdce)
{
	CODA_ASSERT(pdce);
	return &pdce->dc_dh;
}

PDCEntry DC_DH2DC(PDirHandle pdh)
{
	CODA_ASSERT(pdh);
	return list_entry(pdh, struct DCEntry, dc_dh);
}


PDirInode DC_DC2DI(PDCEntry pdce)
{
	CODA_ASSERT(pdce);
	return pdce->dc_pdi;
}

void DC_SetDirh(PDCEntry pdce, PDirHeader pdh)
{
	CODA_ASSERT(pdce);
	pdce->dc_dh.dh_data = pdh;
}

void DC_SetDI(PDCEntry pdce, PDirInode pdi)
{
	CODA_ASSERT(pdce);
	pdce->dc_pdi = pdi;
}


void DC_SetCowpdi(PDCEntry pdce, PDirInode pdi)
{
	CODA_ASSERT(pdce);
	pdce->dc_cowpdi = pdi;
	pdce->dc_pdi = NULL;
}

PDirInode DC_Cowpdi(PDCEntry pdce)
{
	CODA_ASSERT(pdce);
	return pdce->dc_cowpdi;
}


void DC_SetDirty(PDCEntry pdce, int flag)
{
	if ( !pdce ) 
		return;
	pdce->dc_dh.dh_dirty = flag;
}

int DC_Dirty(PDCEntry pdce)
{
	CODA_ASSERT(pdce);
	return pdce->dc_dh.dh_dirty;
}
		

/* called by PutObjects to move the directory handle after copy on write */
void DC_MoveDH(PDCEntry source, PDCEntry target)
{
	target->dc_dh.dh_data = source->dc_dh.dh_data;
	source->dc_dh.dh_data = NULL;
}



#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>

#include <lwp.h>
#include <lock.h>
#include <cfs/coda.h>
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
	int                   dc_count;  /* number of vnodes referencing us */
	int                   dc_refcount; /* new refcount upon commit */
	struct DirHandle      dc_dh;
	PDirInode             dc_pdi;
	PDirInode             dc_cowpdi;
};

#define DCGROWTH 8
#define DCSIZE 256
#define DHLOGCACHESIZE 8

Lock              dlock;
static struct dllist_head dcache[DCSIZE];
static struct dllist_head dfreelist; 
static struct dllist_head dnewlist; 


static inline int  DC_Hash(PDirInode pdi) 
{
	return ((int) ((unsigned long)pdi >> 13) % DCSIZE);
}


/* get more free entries in case freelist is empty */
static void dc_Grow(int count)
{
	PDCEntry pdce;
	int i;

	for ( i = 0 ; i < count ; i++ ) {
		pdce = malloc(sizeof(*pdce));
		assert(pdce);

		INIT_LIST_HEAD(&pdce->dc_hash);
		INIT_LIST_HEAD(&pdce->dc_list);

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
		pdce->dc_count = 0;
		pdce->dc_refcount = 0;
		DH_Init(&pdce->dc_dh);
		pdce->dc_pdi = NULL;
		pdce->dc_cowpdi = NULL;
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
	
	assert(pdi);
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
			if ( pdce->dc_count == 1 ) 
				list_del(&pdce->dc_list);
			/* if data was flushed, refresh it */
			if ( !pdce->dc_dh.dh_vmdata) {
				pdce->dc_dh.dh_vmdata = DI_DiToDh(pdi);
				pdce->dc_dh.dh_refcount = pdi->di_refcount;
			}
			ReleaseWriteLock(&dlock);
			return pdce;
		}
	}

	/* not found use a new one */
	pdce = dc_GetFree();
	pdce->dc_count = 1;
	pdce->dc_pdi = pdi;
	list_add(&pdce->dc_hash, &dcache[hash]);

	/* copy in the directory handle, init lock, and copy data */
	pdce->dc_dh.dh_vmdata = DI_DiToDh(pdi);
	pdce->dc_refcount = pdi->di_refcount;
	ReleaseWriteLock(&dlock);
	return pdce;
}



/* Commit a DirInode - if new move it from the dnewlist to the hash list */
/* called by VN_DCommit when the RVM Dir Inode for mkdir is created */
void DC_Commit(PDCEntry pdce)
{
	PDirHandle dh = DC_DC2DH(pdce);
	int hash;

	assert(dh);

	pdce = list_entry(dh, struct DCEntry, dc_dh);

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
	DH_FreeData(&pdce->dc_dh);
	free(pdce);
	ReleaseWriteLock(&dlock);
}

void DC_HashInit()
{
	int i;

	Lock_Init(&dlock);

	for ( i=0 ; i < DCSIZE ; i++ ) {
		INIT_LIST_HEAD(&dcache[i]);
	}
	INIT_LIST_HEAD(&dfreelist);
	INIT_LIST_HEAD(&dnewlist);
}
	

void DC_Put(PDCEntry pdce)
{

	ObtainWriteLock(&dlock); 

	if ( pdce->dc_count == 1 ) {
		list_add(dfreelist.prev, &pdce->dc_list);
	} 
	pdce->dc_count--;

	ReleaseWriteLock(&dlock);
	return;
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
	list_add(&pdce->dc_hash, &dnewlist);

	ReleaseWriteLock(&dlock);
	return pdce;
}


/* utility functions for setting/getting fields */
inline int DC_Refcount(PDCEntry pdc)
{
	return pdc->dc_refcount;
}

PDirHandle DC_DC2DH(PDCEntry pdce)
{
	assert(pdce);
	return &pdce->dc_dh;
}

void DC_SetDirh(PDCEntry pdce, PDirHeader pdh)
{
	assert(pdce);
	pdce->dc_dh.dh_vmdata = pdh;
}


void DC_SetCowpdi(PDCEntry pdce, PDirInode pdi)
{
	assert(pdce);
	pdce->dc_cowpdi = pdi;
}

PDirInode DC_Cowpdi(PDCEntry pdce)
{
	assert(pdce);
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
	if ( !pdce ) 
		return 0;
	else
		return pdce->dc_dh.dh_dirty;
}
		

/* called by PutObjects to move the directory handle after copy on write */
void DC_MoveDH(PDCEntry source, PDCEntry target)
{
	target->dc_dh.dh_vmdata = source->dc_dh.dh_vmdata;
	source->dc_dh.dh_vmdata = NULL;
}



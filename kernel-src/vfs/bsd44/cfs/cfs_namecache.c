/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon University.
 * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
 */

/*
 * HISTORY
 * $Log: cfs_namecache.c,v $
 * Revision 1.3  1996/11/08 18:06:09  bnoble
 * Minor changes in vnode operation signature, VOP_UPDATE signature, and
 * some newly defined bits in the include files.
 *
 * Revision 1.2  1996/01/02 16:56:50  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:15  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:07:57  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:07:56  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.3  1994/10/14  09:57:54  dcs
 * Made changes 'cause sun4s have braindead compilers
 *
 * Revision 2.2  94/08/28  19:37:35  luqi
 * Add a new CFS_REPLACE call to allow venus to replace a ViceFid in the
 * mini-cache. 
 * 
 * In "cfs.h":
 * Add CFS_REPLACE decl.
 * 
 * In "cfs_namecache.c":
 * Add routine cfsnc_replace.
 * 
 * In "cfs_subr.c":
 * Add case-statement to process CFS_REPLACE.
 * 
 * In "cfsnc.h":
 * Add decl for CFSNC_REPLACE.
 * 
 * 
 * Revision 2.1  94/07/21  16:25:15  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.2  92/10/27  17:58:21  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.3  92/09/30  14:16:20  mja
 * 	call cfs_flush instead of calling inode_uncache_try directly 
 * 	(from dcs). Also...
 * 
 * 	Substituted rvb's history blurb so that we agree with Mach 2.5 sources.
 * 	[91/02/09            jjk]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * Revision 2.2  90/07/05  11:26:30  mrt
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.3  90/05/31  17:01:24  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 */

/*
 * This module contains the routines to implement the CFS name cache. The
 * purpose of this cache is to reduce the cost of translating pathnames 
 * into Vice FIDs. Each entry in the cache contains the name of the file,
 * the vnode (FID) of the parent directory, and the cred structure of the
 * user accessing the file.
 *
 * The first time a file is accessed, it is looked up by the local Venus
 * which first insures that the user has access to the file. In addition
 * we are guaranteed that Venus will invalidate any name cache entries in
 * case the user no longer should be able to access the file. For these
 * reasons we do not need to keep access list information as well as a
 * cred structure for each entry.
 *
 * The table can be accessed through the routines cnc_init(), cnc_enter(),
 * cnc_lookup(), cnc_rmfidcred(), cnc_rmfid(), cnc_rmcred(), and cnc_purge().
 * There are several other routines which aid in the implementation of the
 * hash table.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <cfs/cfs.h>
#include <cfs/cnode.h>
#include <cfs/cfsnc.h>

#ifdef __NetBSD__
#ifndef insque
#include <sys/systm.h>
#endif /* insque */
#endif /* __NetBSD__ */

/* 
 * Declaration of the name cache data structure.
 */

int 	cfsnc_use = 1;			 /* Indicate use of CFS Name Cache */

int	cfsnc_size = CFSNC_CACHESIZE;	 /* size of the cache */
int	cfsnc_hashsize = CFSNC_HASHSIZE; /* size of the primary hash */

struct 	cfscache *cfsncheap;	/* pointer to the cache entries */
struct	cfshash  *cfsnchash;	/* hash table of cfscache pointers */
struct	cfslru   cfsnc_lru;	/* head of lru chain */

struct cfsnc_statistics cfsnc_stat;	/* Keep various stats */

/* 
 * for testing purposes
 */
int cfsnc_debug = 0;


/*
 * Auxillary routines -- shouldn't be entry points
 */

static int cred_debug =0;

static struct cfscache *
cfsnc_find(dcp, name, namelen, cred, hash)
	struct cnode *dcp;
	char *name;
	int namelen;
	struct ucred *cred;
	int hash;
{
	/* 
	 * hash to find the appropriate bucket, look through the chain
	 * for the right entry (especially right cred, unless cred == 0) 
	 */
	register struct cfscache *cncp;
	int count = 1;

	CFSNC_DEBUG(CFSNC_FIND, 
		    myprintf(("cfsnc_find(dcp 0x%x, name %s, len %d, cred 0x%x, hash %d\n",
			   dcp, name, namelen, cred, hash));)

	for (cncp = cfsnchash[hash].hash_next; 
	     cncp != (struct cfscache *)&cfsnchash[hash];
	     cncp = cncp->hash_next, count++) 
	{

	    if ((CFS_NAMEMATCH(cncp, name, namelen, dcp)) &&
		((cred == 0) || (cncp->cred == cred))) 
	    { 
		/* compare cr_uid instead */
		cfsnc_stat.Search_len += count;
		return(cncp);
	    }
	}
	
	return((struct cfscache *)0);
}

static void
cfsnc_remove(cncp, dcstat)
	struct cfscache *cncp;
	enum dc_status dcstat;
{
	/* 
	 * remove an entry -- VN_RELE(cncp->dcp, cp), crfree(cred),
	 * remove it from it's hash chain, and
	 * place it at the head of the lru list.
	 */
        CFSNC_DEBUG(CFSNC_REMOVE,
		    myprintf(("cfsnc_remove %s from parent %x.%x.%x\n",
			   cncp->name, (cncp->dcp)->c_fid.Volume,
			   (cncp->dcp)->c_fid.Vnode, (cncp->dcp)->c_fid.Unique));)

  	CFSNC_HSHREM(cncp);

	CFSNC_HSHNUL(cncp);		/* have it be a null chain */
	if ((dcstat == IS_DOWNCALL) && (CNODE_COUNT(cncp->dcp) == 1)) {
		cncp->dcp->c_flags |= CN_PURGING;
	}
	VN_RELE(CTOV(cncp->dcp)); 

	if ((dcstat == IS_DOWNCALL) && (CNODE_COUNT(cncp->cp) == 1)) {
		cncp->cp->c_flags |= CN_PURGING;
	}
	VN_RELE(CTOV(cncp->cp)); 

	crfree(cncp->cred); 
	bzero(DATA_PART(cncp),DATA_SIZE);

	/* Put the null entry just after the least-recently-used entry */
	/* LRU_TOP adjusts the pointer to point to the top of the structure. */
	CFSNC_LRUREM(cncp);
	CFSNC_LRUINS(cncp, LRU_TOP(cfsnc_lru.lru_prev));
}


/*
 * Entry points for the CFS Name Cache
 */

/*  
 * Initialize the cache, the LRU structure and the Hash structure(s)
 */

#define TOTAL_CACHE_SIZE 	(sizeof(struct cfscache) * cfsnc_size)
#define TOTAL_HASH_SIZE 	(sizeof(struct cfshash)  * cfsnc_hashsize)

int cfsnc_initialized = 0;      /* Initially the cache has not been initialized */

void
cfsnc_init()
{
    register int i;

    /* zero the statistics structure */
    
    bzero(&cfsnc_stat, (sizeof(struct cfsnc_statistics)));
    
    CFS_ALLOC(cfsncheap, struct cfscache *, TOTAL_CACHE_SIZE);
    CFS_ALLOC(cfsnchash, struct cfshash *, TOTAL_HASH_SIZE);
    
    cfsnc_lru.lru_next = 
	cfsnc_lru.lru_prev = (struct cfscache *)LRU_PART(&cfsnc_lru);
    
    
    for (i=0; i < cfsnc_size; i++) {	/* initialize the heap */
	CFSNC_LRUINS(&cfsncheap[i], &cfsnc_lru);
	CFSNC_HSHNUL(&cfsncheap[i]);
	cfsncheap[i].cp = cfsncheap[i].dcp = (struct cnode *)0;
    }
    
    for (i=0; i < cfsnc_hashsize; i++) {	/* initialize the hashtable */
	CFSNC_HSHNUL((struct cfscache *)&cfsnchash[i]);
    }
    
    cfsnc_initialized++;
}

/*
 * Enter a new (dir cnode, name) pair into the cache, updating the
 * LRU and Hash as needed.
 */

void
cfsnc_enter(dcp, name, cred, cp)
    struct cnode *dcp;
    register char *name;
    struct ucred *cred;
    struct cnode *cp;
{
    register int namelen;
    register struct cfscache *cncp;
    register int hash;
    
    if (cfsnc_use == 0)			/* Cache is off */
	return;
    
    CFSNC_DEBUG(CFSNC_ENTER, 
		myprintf(("Enter: dcp 0x%x cp 0x%x name %s cred 0x%x \n",
		       dcp, cp, name, cred)); )
	
    namelen = strlen(name);		/* Don't store long file names */
    if (namelen > CFSNC_NAMELEN) {
	CFSNC_DEBUG(CFSNC_ENTER, 
		    myprintf(("long name enter %s\n",name));)
	    cfsnc_stat.long_name_enters++;	/* record stats */
	return;
    }
    
    hash = CFSNC_HASH(name, namelen, dcp);
    cncp = cfsnc_find(dcp, name, namelen, cred, hash);
    if (cncp != (struct cfscache *) 0) {	
	cfsnc_stat.dbl_enters++;		/* duplicate entry */
	return;
    }
    
    cfsnc_stat.enters++;		/* record the enters statistic */
    
    /* Grab the next element in the lru chain */
    cncp = CFSNC_LRUGET(cfsnc_lru);
    
    CFSNC_LRUREM(cncp);	/* remove it from the lists */
    
    if (CFSNC_VALID(cncp)) {
	/* Seems really ugly, but we have to decrement the appropriate
	   hash bucket length here, so we have to find the hash bucket
	   */
	cfsnchash[CFSNC_HASH(cncp->name, cncp->namelen, cncp->dcp)].length--;
	
	cfsnc_stat.lru_rm++;	/* zapped a valid entry */
	CFSNC_HSHREM(cncp);
	VN_RELE(CTOV(cncp->dcp)); 
	VN_RELE(CTOV(cncp->cp));
	crfree(cncp->cred);
    }
    
    /*
     * Put a hold on the current vnodes and fill in the cache entry.
     */
    VN_HOLD(CTOV(cp));
    VN_HOLD(CTOV(dcp));
    crhold(cred); 
    cncp->dcp = dcp;
    cncp->cp = cp;
    cncp->namelen = namelen;
    cncp->cred = cred;
    
    bcopy(name, cncp->name, (unsigned)namelen);
    
    /* Insert into the lru and hash chains. */
    
    CFSNC_LRUINS(cncp, &cfsnc_lru);
    CFSNC_HSHINS(cncp, &cfsnchash[hash]);
    cfsnchash[hash].length++;                      /* Used for tuning */
    
    CFSNC_DEBUG(CFSNC_PRINTCFSNC, print_cfsnc(); )
}

/*
 * Find the (dir cnode, name) pair in the cache, if it's cred
 * matches the input, return it, otherwise return 0
 */

struct cnode *
cfsnc_lookup(dcp, name, cred)
	struct cnode *dcp;
	register char *name;
	struct ucred *cred;
{
	register int namelen, hash;
	register struct cfscache *cncp;

	if (cfsnc_use == 0)			/* Cache is off */
		return((struct cnode *) 0);

	namelen = strlen(name);
	if (namelen > CFSNC_NAMELEN) {
	        CFSNC_DEBUG(CFSNC_LOOKUP, 
			    myprintf(("long name lookup %s\n",name));)
		cfsnc_stat.long_name_lookups++;		/* record stats */
		return((struct cnode *) 0);
	}

	/* Use the hash function to locate the starting point,
	   then the search routine to go down the list looking for
	   the correct cred.
 	 */

	hash = CFSNC_HASH(name, namelen, dcp);
	cncp = cfsnc_find(dcp, name, namelen, cred, hash);
	if (cncp == (struct cfscache *) 0) {
		cfsnc_stat.misses++;			/* record miss */
		return((struct cnode *) 0);
	}

	cfsnc_stat.hits++;

	/* put this entry at the end of the LRU */
	CFSNC_LRUREM(cncp);
	CFSNC_LRUINS(cncp, &cfsnc_lru);

	/* move it to the front of the hash chain */
	/* don't need to change the hash bucket length */
	CFSNC_HSHREM(cncp);
	CFSNC_HSHINS(cncp, &cfsnchash[hash]);

	CFSNC_DEBUG(CFSNC_LOOKUP, 
		printf("lookup: dcp 0x%x, name %s, cred 0x%x = cp 0x%x\n",
			dcp, name, cred, cncp->cp); )

	return(cncp->cp);
}

/*
 * Remove all entries with a parent which has the input fid.
 */

void
cfsnc_zapParentfid(fid, dcstat)
	ViceFid *fid;
	enum dc_status dcstat;
{
	/* To get to a specific fid, we might either have another hashing
	   function or do a sequential search through the cache for the
	   appropriate entries. The later may be acceptable since I don't
	   think callbacks or whatever Case 1 covers are frequent occurences.
	 */
	register struct cfscache *cncp, *ncncp;
	register int i;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_ZAPPFID, 
		myprintf(("ZapParent: fid 0x%x, 0x%x, 0x%x \n",
			fid->Volume, fid->Vnode, fid->Unique)); )

	cfsnc_stat.zapPfids++;

	for (i = 0; i < cfsnc_hashsize; i++) {

		/*
		 * Need to save the hash_next pointer in case we remove the
		 * entry. remove causes hash_next to point to itself.
		 */

		for (cncp = cfsnchash[i].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if ((cncp->dcp->c_fid.Volume == fid->Volume) &&
			    (cncp->dcp->c_fid.Vnode == fid->Vnode)   &&
			    (cncp->dcp->c_fid.Unique == fid->Unique)) {
			        cfsnchash[i].length--;      /* Used for tuning */
				cfsnc_remove(cncp, dcstat); 
			}
		}
	}
}

void
cfsnc_zapfid(fid, dcstat)
	ViceFid *fid;
	enum dc_status dcstat;
{
	/* See comment for zapParentfid. This routine will be used
	   if attributes are being cached. 
	 */
	register struct cfscache *cncp, *ncncp;
	register int i;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_ZAPFID, 
		myprintf(("Zapfid: fid 0x%x, 0x%x, 0x%x \n",
			fid->Volume, fid->Vnode, fid->Unique)); )

	cfsnc_stat.zapFids++;

	for (i = 0; i < cfsnc_hashsize; i++) {
		for (cncp = cfsnchash[i].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if ((cncp->cp->c_fid.Volume == fid->Volume) &&
			    (cncp->cp->c_fid.Vnode == fid->Vnode)   &&
			    (cncp->cp->c_fid.Unique == fid->Unique)) {
			        cfsnchash[i].length--;     /* Used for tuning */
				cfsnc_remove(cncp, dcstat); 
			}
		}
	}
}

/*
 * Remove all entries which have the same fid as the input
 */

/* 
 * Remove all entries which match the fid and the cred
 */

void
cfsnc_zapvnode(fid, cred, dcstat)	
	ViceFid *fid;
	struct ucred *cred;
	enum dc_status dcstat;
{
	/* See comment for zapfid. I don't think that one would ever
	   want to zap a file with a specific cred from the kernel.
	   We'll leave this one unimplemented.
	 */
	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_ZAPVNODE, 
		myprintf(("Zapvnode: fid 0x%x, 0x%x, 0x%x cred 0x%x\n",
			  fid->Volume, fid->Vnode, fid->Unique, cred)); )

}

/*
 * Remove all entries which have the (dir vnode, name) pair
 */

void
cfsnc_zapfile(dcp, name)
	struct cnode *dcp;
	register char *name;
{
	/* use the hash function to locate the file, then zap all
 	   entries of it regardless of the cred.
	 */
	register int namelen;
	register struct cfscache *cncp;
	int hash;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_ZAPFILE, 
		myprintf(("Zapfile: dcp 0x%x name %s \n",
			  dcp, name)); )

	namelen = strlen(name);
	if (namelen > CFSNC_NAMELEN) {
		cfsnc_stat.long_remove++;		/* record stats */
		return;
	}

	cfsnc_stat.zapFile++;

	hash = CFSNC_HASH(name, namelen, dcp);
	cncp = cfsnc_find(dcp, name, namelen, 0, hash);

	while (cncp) {
	  cfsnchash[hash].length--;                 /* Used for tuning */
	  cfsnc_remove(cncp);
	  cncp = cfsnc_find(dcp, name, namelen, 0, hash);
	}
}

/* 
 * Remove all the entries for a particular user. Used when tokens expire.
 * A user is determined by his/her effective user id (id_uid).
 */

void
cfsnc_purge_user(cred, dcstat)
	struct ucred    *cred;
	enum dc_status  dcstat;
{
	/* 
	 * I think the best approach is to go through the entire cache
	 * via HASH or whatever and zap all entries which match the
	 * input cred. Or just flush the whole cache.  It might be
	 * best to go through on basis of LRU since cache will almost
	 * always be full and LRU is more straightforward.  
	 */

	register struct cfscache *cncp, *ncncp;
	int hash;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_PURGEUSER, 
		myprintf(("ZapDude: uid 0x%x\n",cred->cr_uid)); )
	cfsnc_stat.zapUsers++;

	for (cncp = CFSNC_LRUGET(cfsnc_lru);
	     cncp != (struct cfscache *)(&cfsnc_lru);
	     cncp = ncncp) {
		ncncp = CFSNC_LRUGET(*cncp);

		if ((CFSNC_VALID(cncp)) &&
		   ((cncp->cred)->cr_uid == cred->cr_uid)) {
		        /* Seems really ugly, but we have to decrement the appropriate
			   hash bucket length here, so we have to find the hash bucket
			   */
		        hash = CFSNC_HASH(cncp->name, cncp->namelen, cncp->dcp);
			cfsnchash[hash].length--;     /* For performance tuning */

			cfsnc_remove(cncp, dcstat); 
		}
	}
}

/*
 * Flush the entire name cache. In response to a flush of the Venus cache.
 */

void
cfsnc_flush(dcstat)
	enum dc_status dcstat;
{
	/* One option is to deallocate the current name cache and
	   call init to start again. Or just deallocate, then rebuild.
	   Or again, we could just go through the array and zero the 
	   appropriate fields. 
	 */
	
	/* 
	 * Go through the whole lru chain and kill everything as we go.
	 * I don't use remove since that would rebuild the lru chain
	 * as it went and that seemed unneccesary.
	 */
	register struct cfscache *cncp;
	int i;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	cfsnc_stat.Flushes++;

	for (cncp = CFSNC_LRUGET(cfsnc_lru);
	     cncp != (struct cfscache *)&cfsnc_lru;
	     cncp = CFSNC_LRUGET(*cncp)) {
		if (CFSNC_VALID(cncp)) {
			CFSNC_HSHREM(cncp);	/* only zero valid nodes */
			CFSNC_HSHNUL(cncp);
			if ((dcstat == IS_DOWNCALL) 
			    && (CNODE_COUNT(cncp->dcp) == 1))
			{
				cncp->dcp->c_flags |= CN_PURGING;
			}
			VN_RELE(CTOV(cncp->dcp)); 
			if (!ISDIR(cncp->cp->c_fid) && (CTOV(cncp->cp)->v_flag & VTEXT)) {
			    if (cfs_vmflush(cncp->cp))
				CFSDEBUG(CFS_FLUSH, 
					 myprintf(("cfsnc_flush: (%x.%x.%x) busy\n", cncp->cp->c_fid.Volume, cncp->cp->c_fid.Vnode, cncp->cp->c_fid.Unique)); )
			}

			if ((dcstat == IS_DOWNCALL) 
			    && (CNODE_COUNT(cncp->cp) == 1))
			{
				cncp->cp->c_flags |= CN_PURGING;
			}
			VN_RELE(CTOV(cncp->cp));  

			crfree(cncp->cred); 
			bzero(DATA_PART(cncp),DATA_SIZE);
		}
	}

	for (i = 0; i < cfsnc_hashsize; i++)
	  cfsnchash[i].length = 0;
}

/*
 * This routine replaces a ViceFid in the name cache with another.
 * It is added to allow Venus during reintegration to replace 
 * locally allocated temp fids while disconnected with global fids 
 * even when the reference count on those fids are not zero.
 */
void
cfsnc_replace(f1, f2)
	ViceFid *f1; ViceFid *f2;
{
        /* 
	 * Replace f1 with f2 throughout the name cache
	 */
	int hash;
	register struct cfscache *cncp;
	
	CFSNC_DEBUG(CFSNC_REPLACE,
		    myprintf(("cfsnc_replace fid_1 = (%x.%x.%x) and fid_2 = (%x.%x.%x)\n",
			      f1->Volume, f1->Vnode, f1->Unique, 
			      f2->Volume, f2->Vnode, f2->Unique)););
		
	for (hash = 0; hash < cfsnc_hashsize; hash++) {
		for (cncp = cfsnchash[hash].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[hash];
		     cncp = cncp->hash_next) {
			if (!bcmp(&cncp->cp->c_fid, f1, sizeof(ViceFid))) {
				bcopy(f2, &cncp->cp->c_fid, sizeof(ViceFid));
				/* no need to check cncp->dcp now */
				continue; 	
			}
			if (!bcmp(&cncp->dcp->c_fid, f1, sizeof(ViceFid)))
				bcopy(f2, &cncp->dcp->c_fid, sizeof(ViceFid));
		}
	}
}

/*
 * Debugging routines
 */

/* 
 * This routine should print out all the hash chains to the console.
 */

void
print_cfsnc()
{
	int hash;
	register struct cfscache *cncp;

	for (hash = 0; hash < cfsnc_hashsize; hash++) {
		myprintf(("\nhash %d\n",hash));

		for (cncp = cfsnchash[hash].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[hash];
		     cncp = cncp->hash_next) {
			myprintf(("cp 0x%x dcp 0x%x cred 0x%x name %s\n",
				  (int)cncp->cp, (int)cncp->dcp,
				  (int)cncp->cred, cncp->name));
		     }
	}
}

void
cfsnc_gather_stats()
{
    int i, max = 0, sum = 0, temp, zeros = 0, ave, n;

	for (i = 0; i < cfsnc_hashsize; i++) {
	  if (cfsnchash[i].length) {
	    sum += cfsnchash[i].length;
	  } else {
	    zeros++;
	  }

	  if (cfsnchash[i].length > max)
	    max = cfsnchash[i].length;
	}

/*
 * When computing the Arithmetic mean, only count slots which 
 * are not empty in the distribution.
 */
        cfsnc_stat.Sum_bucket_len = sum;
        cfsnc_stat.Num_zero_len = zeros;
        cfsnc_stat.Max_bucket_len = max;

	if ((n = cfsnc_hashsize - zeros) > 0) 
	  ave = sum / n;
	else
	  ave = 0;

	sum = 0;
	for (i = 0; i < cfsnc_hashsize; i++) {
	  if (cfsnchash[i].length) {
	    temp = cfsnchash[i].length - ave;
	    sum += temp * temp;
	  }
	}
        cfsnc_stat.Sum2_bucket_len = sum;
}

/*
 * The purpose of this routine is to allow the hash and cache sizes to be
 * changed dynamically. This should only be used in controlled environments,
 * it makes no effort to lock other users from accessing the cache while it
 * is in an improper state (except by turning the cache off).
 */
int
cfsnc_resize(hashsize, heapsize, dcstat)
     int hashsize, heapsize;
     enum dc_status dcstat;
{
    if ((hashsize % 2) || (heapsize % 2)) { /* Illegal hash or cache sizes */
	return(EINVAL);
    }                 
    
    cfsnc_use = 0;                       /* Turn the cache off */
    
    cfsnc_flush(dcstat);                 /* free any cnodes in the cache */
    
    /* WARNING: free must happen *before* size is reset */
    CFS_FREE(cfsncheap,TOTAL_CACHE_SIZE);
    CFS_FREE(cfsnchash,TOTAL_HASH_SIZE);
    
    cfsnc_hashsize = hashsize;
    cfsnc_size = heapsize;
    
    cfsnc_init();                        /* Set up a cache with the new size */
    
    cfsnc_use = 1;                       /* Turn the cache back on */
    return(0);
}




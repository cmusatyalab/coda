/*
 * Cache operations for Coda.
 * Original version: (C) 1996 Peter Braam 
 * Rewritten for Linux 2.1: (C) 1997 Carnegie Mellon University
 *
 * Carnegie Mellon encourages users of this code to contribute improvements
 * to the Coda project. Contact Peter Braam <coda@cs.cmu.edu>.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <linux/string.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_cnode.h>
#include <linux/coda_namecache.h>

int cfsnc_use;

static struct cfscache * cfsnc_find(struct cnode *dcp, const char * name, int namelen, int hash);
static void cfsnc_remove(struct cfscache *cncp);
static inline int  nchash(const char *, int, struct cnode *);
static inline int ncmatch(struct cfscache *, const char *, int, 
                          struct cnode *);
static inline void hashins(struct cfscache *a, struct cfscache *pred);
static inline void hashrem(struct cfscache *a);
static inline void hashnull(struct cfscache *);
static inline void lrurem(struct cfscache *a);
static inline void lruins(struct cfscache *a, struct cfscache *pred);
static void cfsnc_gather_stats(void);


/* externals */
extern int coda_fideq(ViceFid *fid1, ViceFid *fid2);
extern int coda_debug;
extern int coda_print_entry;
extern struct super_block *coda_super_block;



/* 
 * Declaration of the name cache data structure.
 */

int 	cfsnc_use = 0;			 /* Indicate use of CFS Name Cache */
int	cfsnc_size = CFSNC_CACHESIZE;	 /* size of the cache */
int	cfsnc_hashsize = CFSNC_HASHSIZE; /* size of the primary hash */
int     cfsnc_flushme = 0;
int     cfsnc_procsize = 0;
static  int cfsnc_force = 0;

struct cfshash {
	struct cfscache *hash_next, *hash_prev;
	int              length;
};

struct cfslruhead {
        struct cfscache *dummy1, *dummy2;
        struct cfscache *lru_next, *lru_prev;
};

struct 	cfscache *cfsncheap;	/* pointer to the cache entries */
struct	cfshash  *cfsnchash;	/* hash table of cfscache pointers */
struct	cfslruhead  cfsnc_lru;	/* head of lru chain; prev = lru */

struct cfsnc_statistics cfsnc_stat;	/* Keep various stats */

#define TOTAL_CACHE_SIZE 	(sizeof(struct cfscache) * cfsnc_size)
#define TOTAL_HASH_SIZE 	(sizeof(struct cfshash)  * cfsnc_hashsize)
int cfsnc_initialized = 0;      /* Initially the cache has not been initialized */

/* 
 * for testing purposes
 */
int cfsnc_debug = 1;


/*
 * Auxillary routines -- shouldn't be entry points
 */


/*
 * Hash function for the primary hash.
 * First try -- (first + last letters + length + (int)cp) mod size
 * 2nd try -- same, except dir fid.vnode instead of cp
 */
static inline int  
nchash(const char *name, int namelen, struct cnode *cp)
{
    return ((name[0] + name[namelen-1] + 
             namelen + (int)(cp)) & (cfsnc_hashsize-1));   
}

/* matching function */
static inline int ncmatch(struct cfscache *cp, const char *name, int namelen,
                          struct cnode *dcp)
{
    return 	((namelen == cp->namelen) && (dcp == cp->dcp) && 
		 (memcmp(cp->name,name,namelen) == 0));
}

/* insert  a  behind  pred */
static inline void hashins(struct cfscache *a, struct cfscache *pred)
{
	a->hash_next = pred->hash_next;
	pred->hash_next->hash_prev= a;
	pred->hash_next = a;
	a->hash_prev = pred;
}

static inline void hashrem(struct cfscache *a) 
{
	a->hash_prev->hash_next = a->hash_next;
	a->hash_next->hash_prev = a->hash_prev;
}

static inline void hashnull(struct cfscache *elem) {
	elem->hash_next = elem;
	elem->hash_prev = elem;
}

static inline void lrurem(struct cfscache *a) 
{
	a->lru_prev->lru_next = a->lru_next;
	a->lru_next->lru_prev = a->lru_prev;
}

static inline void lruins(struct cfscache *a, struct cfscache *pred)
{
	pred->lru_next->lru_prev= a;
	a->lru_next = pred->lru_next;
	
	a->lru_prev = pred;
	pred->lru_next = a;
}

static struct cfscache *
cfsnc_find(struct cnode *dcp, const char * name, int namelen, int hash)
{
	/* 
	 * hash to find the appropriate bucket, look through the chain
	 * for the right entry 
	 */
	register struct cfscache *cncp;
	int count = 1;

	CDEBUG(D_CACHE, "dcp 0x%x, name %s, len %d, hash %d\n",
			   (int)dcp, name, namelen, hash);

	for (cncp  = cfsnchash[hash].hash_next; 
	     cncp != (struct cfscache *)&cfsnchash[hash];
	     cncp  = cncp->hash_next, count++) 
	{

	    if (ncmatch(cncp, name, namelen, dcp))
	    { 
		cfsnc_stat.Search_len += count;
		CDEBUG(D_CACHE, "dcp 0x%x,found.\n", (int) dcp);
		return(cncp);
			
	    }
	}
	CDEBUG(D_CACHE, "dcp 0x%x,not found.\n", (int) dcp);
	return((struct cfscache *)0);
}

static void
cfsnc_remove(struct cfscache *cncp)
{
	/* 
	 * remove an entry -- VN_RELE(cncp->dcp, cp), crfree(cred),
	 * remove it from it's hash chain, and
	 * place it at the head of the lru list.
	 */
    CDEBUG(D_CACHE, "remove %s from parent %lx.%lx.%lx\n",
           cncp->name, (cncp->dcp)->c_fid.Volume,
           (cncp->dcp)->c_fid.Vnode, (cncp->dcp)->c_fid.Unique);

  	hashrem(cncp);
	hashnull(cncp);		/* have it be a null chain */

	/* VN_RELE(CTOV(cncp->dcp));  */
	iput(CTOI(cncp->cp)); 
	/* crfree(cncp->cred);  */

	memset(DATA_PART(cncp), 0 ,DATA_SIZE);
	cncp->cp = NULL;
	cncp->dcp = (struct cnode *) 0;

	/* Put the null entry just after the least-recently-used entry */
	lrurem(cncp);
	lruins(cncp, cfsnc_lru.lru_prev);
}


/*
 * Entry points for the CFS Name Cache
 */

/*  
 * Initialize the cache, the LRU structure and the Hash structure(s)
 */
void
cfsnc_init(void)
{
    register int i;

    /* zero the statistics structure */
    cfsnc_procsize =  10000 * cfsnc_hashsize + cfsnc_size;
    memset(&cfsnc_stat, 0, (sizeof(struct cfsnc_statistics)));
    
    CODA_ALLOC(cfsncheap, struct cfscache *, TOTAL_CACHE_SIZE);
    CODA_ALLOC(cfsnchash, struct cfshash *, TOTAL_HASH_SIZE);
    
    cfsnc_lru.lru_next = cfsnc_lru.lru_prev = (struct cfscache *)&cfsnc_lru; 
    
    /* initialize the heap */
    for (i=0; i < cfsnc_size; i++) {	
	lruins(&cfsncheap[i], (struct cfscache *) &cfsnc_lru);
	hashnull(&cfsncheap[i]);
	cfsncheap[i].cp = cfsncheap[i].dcp = (struct cnode *)0;
    }
    
    for (i=0; i < cfsnc_hashsize; i++) {	/* initialize the hashtable */
	hashnull((struct cfscache *)&cfsnchash[i]);
	cfsnchash[i].length=0;  /* bucket length */
    }
    
    cfsnc_initialized = 1;
    CDEBUG(D_CACHE, "cfsnc_initialized is now 1.\n");
}

/*
 * Enter a new (dir cnode, name) pair into the cache, updating the
 * LRU and Hash as needed.
 */

void
cfsnc_enter(struct cnode *dcp, register const char *name, int namelen, struct cnode *cp)
{
    register struct cfscache *cncp;
    register int hash;
    
    if (cfsnc_use == 0)			/* Cache is off */
	return;
    
    CDEBUG(D_CACHE, "dcp 0x%x cp 0x%x name %s, ind 0x%x \n",
	   (int)dcp, (int)cp, name, (int)cp->c_vnode); 
	
    if (namelen > CFSNC_NAMELEN) {
        CDEBUG(D_CACHE, "long name enter %s\n",name);
	    cfsnc_stat.long_name_enters++;	/* record stats */
	return;
    }
    
    hash = nchash(name, namelen, dcp);
	CDEBUG(D_CACHE, "Calling find with name %s, dcp %d, hash %d\n",
	       name, (int) dcp, (int) hash);

    cncp = cfsnc_find(dcp, name, namelen, hash);
    if (cncp != (struct cfscache *) 0) {	
	printk("cfsnc_enter: Duplicate cache entry; tell Peter.\n");
	cfsnc_stat.dbl_enters++;		/* duplicate entry */
	return;
    }
    
    cfsnc_stat.enters++;		/* record the enters statistic */
    
    /* Grab the lru element in the lru chain */
    cncp = cfsnc_lru.lru_prev;
    
    lrurem(cncp);	/* remove it from the lists */
    
    /* if cncp is on hash list remove it */
    if ( cncp->dcp != (struct cnode *) 0 ) {
	/* We have to decrement the appropriate hash bucket length
	   here, so we have to find the hash bucket */
	cfsnchash[nchash(cncp->name, cncp->namelen, cncp->dcp)].length--;
	cfsnc_stat.lru_rm++;	/* zapped a valid entry */
	hashrem(cncp);
	iput(CTOI(cncp->cp));
	/* VN_RELE(CTOV(cncp->dcp));  */
	/* crfree(cncp->cred); */
    }
    /*
     * Put a hold on the current vnodes and fill in the cache entry.
     */
    iget((CTOI(cp))->i_sb, CTOI(cp)->i_ino);
    /* VN_HOLD(CTOV(dcp)); */
    /* XXXX crhold(cred); */
    cncp->dcp = dcp;
    cncp->cp = cp;
    cncp->namelen = namelen;
    /* cncp->cred = cred; */
    
    memcpy(cncp->name, name, (unsigned)namelen);
    
    /* Insert into the lru and hash chains. */
    
    lruins(cncp, (struct cfscache *) &cfsnc_lru);
    hashins(cncp, (struct cfscache *)&cfsnchash[hash]);
    cfsnchash[hash].length++;                      /* Used for tuning */
    CDEBUG(D_CACHE, "Entering:\n");
    coda_print_ce(cncp);
}

/*
 * Find the (dir cnode, name) pair in the cache, if it's cred
 * matches the input, return it, otherwise return 0
 */

struct cnode *
cfsnc_lookup(struct cnode *dcp, register const char *name, int namelen)
{
	register int hash;
	register struct cfscache *cncp;
        /* this should go into a callback funcntion for /proc/sys
           don't know how at the moment? */  
	if (cfsnc_flushme == 1) {
		cfsnc_flush();
		cfsnc_flushme = 0;
	}
	
	if (cfsnc_procsize != 10000*cfsnc_hashsize + cfsnc_size ) {
		int hsh = cfsnc_procsize/10000;
		int siz = cfsnc_procsize%10000;
		int rc;
		
		if ( (hsh > 1) && (siz > 2) ) {
			rc = cfsnc_resize(hsh, siz);
			if ( !rc ) {
				printk("Coda:cache size (hash,size) (%d,%d)\n",
					hsh, siz);
			} else {
				printk("Coda: cache resize failed\n");
			}
		}
	}

	if (cfsnc_use == 0)			/* Cache is off */
		return((struct cnode *) 0);

	if (namelen > CFSNC_NAMELEN) {
        CDEBUG(D_CACHE,"long name lookup %s\n",name);
		cfsnc_stat.long_name_lookups++;		/* record stats */
		return((struct cnode *) 0);
	}

	/* Use the hash function to locate the starting point,
	   then the search routine to go down the list looking for
	   the correct cred.
 	 */

	hash = nchash(name, namelen, dcp);
	CDEBUG(D_CACHE, "Calling find with name %s, dcp %d, hash %d\n",
	       name, (int) dcp, (int) hash);
	cncp = cfsnc_find(dcp, name, namelen, hash);
	if (cncp == (struct cfscache *) 0) {
		cfsnc_stat.misses++;			/* record miss */
		return((struct cnode *) 0);
	}

	cfsnc_stat.hits++;

	/* put this entry at the mru end of the LRU */
	lrurem(cncp);
	lruins(cncp, (struct cfscache *) &cfsnc_lru);

	/* move it to the front of the hash chain */
	/* don't need to change the hash bucket length */
	hashrem(cncp);
	hashins(cncp, (struct cfscache *) &cfsnchash[hash]);

	CDEBUG(D_CACHE, "lookup: dcp 0x%x, name %s,  cp 0x%x\n",
           (int) dcp, name,  (int) cncp->cp);

	return(cncp->cp);
}

/*
 * Remove all entries with a parent which has the input fid.
 */

void
cfsnc_zapParentfid(ViceFid *fid)
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

	CDEBUG(D_CACHE, " fid 0x%lx, 0x%lx, 0x%lx \n",
           fid->Volume, fid->Vnode, fid->Unique);

	cfsnc_stat.zapPfids++;

	for (i = 0; i < cfsnc_hashsize; i++) {

		/*
		 * Need to save the hash_next pointer in case we remove the
		 * entry. remove causes hash_next to point to itself.
		 */

		for (cncp = cfsnchash[i].hash_next; 
		     cncp != (struct cfscache *) &cfsnchash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if ( coda_fideq(&cncp->dcp->c_fid, fid) ) {
			        cfsnchash[i].length--;   /* Used for tuning */
				cfsnc_remove(cncp); 
			    }
		     }
	}
}

/*
 * Remove all entries which have the same fid as the input
 */
void
cfsnc_zapfid(ViceFid *fid)
{
	/* See comment for zapParentfid. This routine will be used
	   if attributes are being cached. 
	 */
	register struct cfscache *cncp, *ncncp;
	register int i;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CDEBUG(D_CACHE, "Zapfid: fid 0x%lx, 0x%lx, 0x%lx \n",
           fid->Volume, fid->Vnode, fid->Unique);

	cfsnc_stat.zapFids++;

	for (i = 0; i < cfsnc_hashsize; i++) {
		for (cncp = cfsnchash[i].hash_next; 
		     cncp != (struct cfscache *) &cfsnchash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if (coda_fideq(&(cncp->cp->c_fid), fid)) {
			        CDEBUG(D_CACHE, "Found cncp: name %s\n", cncp->name);
			        cfsnchash[i].length--;   /* Used for tuning */
				cfsnc_remove(cncp); 
			    }
		     }
	}
}


/*
 * Remove all entries which have the (dir vnode, name) pair
 */
void
cfsnc_zapfile(struct cnode *dcp, register const char *name, int length)
{
	/* use the hash function to locate the file, then zap all
 	   entries of it regardless of the cred.
	 */
	register struct cfscache *cncp;
	int hash;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CDEBUG(D_CACHE,"Zapfile: dcp 0x%x name %s \n",
           (int) dcp, name);

	if (length > CFSNC_NAMELEN) {
		cfsnc_stat.long_remove++;		/* record stats */
		return;
	}

	cfsnc_stat.zapFile++;

	hash = nchash(name, length,  dcp);
    /* remove entries: remember they might exist for more than a 
       single cred */
	while ( (cncp = cfsnc_find(dcp, name, length, hash)) != NULL ) {
	  cfsnchash[hash].length--;       
	  cfsnc_remove(cncp);
	}
}

/* 
 * Remove all the entries for a particular user. Used when tokens expire.
 * A user is determined by his/her effective user id (id_uid).
 */

void
cfsnc_purge_user(struct CodaCred *cred)
{
	/* I think the best approach is to go through the entire cache
	   via HASH or whatever and zap all entries which match the
	   input cred. Or just flush the whole cache.
	   It might be best to go through on basis of LRU since cache
	   will almost always be full and LRU is more straightforward.
	 */

	register struct cfscache *cncp;
	int hash;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CDEBUG(D_CACHE,"ZapDude: uid %ld\n",cred->cr_uid);
	cfsnc_stat.zapUsers++;

	for (cncp = cfsnc_lru.lru_next;
	     cncp != (struct cfscache *) &cfsnc_lru;
	     cncp = cncp->lru_next) {

		if ((CFSNC_VALID(cncp)) &&
		   ((cncp->cred)->cr_uid == cred->cr_uid)) {
		        /* Seems really ugly, but we have to decrement the appropriate
			   hash bucket length here, so we have to find the hash bucket
			   */
		        hash = nchash(cncp->name, cncp->namelen, cncp->dcp);
			cfsnchash[hash].length--;     /* For performance tuning */

			cfsnc_remove(cncp); 
		}
	}
}

/*
 * Flush the entire name cache. In response to a flush of the Venus cache.
 */

void
cfsnc_flush(void)
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

	if ((cfsnc_use == 0 || cfsnc_initialized == 0) && (cfsnc_force == 0) )
		return;

	cfsnc_stat.Flushes++;

	for (cncp = cfsnc_lru.lru_next;
	     cncp != (struct cfscache *) &cfsnc_lru;
	     cncp = cncp->lru_next) {
		if ( cncp->cp ) {
			hashrem(cncp);	/* only zero valid nodes */
			hashnull(cncp);
			iput(CTOI(cncp->cp));  
			/* crfree(cncp->cred);  */
			memset(DATA_PART(cncp), 0, DATA_SIZE);
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
cfsnc_replace(ViceFid *f1, ViceFid *f2)
{
        /* 
	 * Replace f1 with f2 throughout the name cache
	 */
	int hash;
	register struct cfscache *cncp;

	CDEBUG(D_CACHE, 
    "cfsnc_replace fid_1 = (%lx.%lx.%lx) and fid_2 = (%lx.%lx.%lx)\n",
           f1->Volume, f1->Vnode, f1->Unique, 
           f2->Volume, f2->Vnode, f2->Unique);

	for (hash = 0; hash < cfsnc_hashsize; hash++) {
		for (cncp = cfsnchash[hash].hash_next; 
		     cncp != (struct cfscache *) &cfsnchash[hash];
		     cncp = cncp->hash_next) {
		        if (!memcmp(&cncp->cp->c_fid, f1, sizeof(ViceFid))) {
			    memcpy(&cncp->cp->c_fid, f2, sizeof(ViceFid));
			    continue; 	/* no need to check cncp->dcp now */
			}
		        if (!memcmp(&cncp->dcp->c_fid, f1, sizeof(ViceFid)))
			    memcpy(&cncp->dcp->c_fid, f2, sizeof(ViceFid));
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
print_cfsnc(void)
{
	int hash;
	register struct cfscache *cncp;

	for (hash = 0; hash < cfsnc_hashsize; hash++) {
		printk("\nhash %d\n",hash);

		for (cncp = cfsnchash[hash].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[hash];
		     cncp = cncp->hash_next) {
			printk("cp 0x%x dcp 0x%x cred 0x%x name %s ino %d count %d dev %d\n",
				  (int)cncp->cp, (int)cncp->dcp,
				  (int)cncp->cred, cncp->name, CTOI(cncp->cp)->i_count, CTOI(cncp->cp)->i_count, CTOI(cncp->cp)->i_dev);
		     }
	}
}

int
cfsnc_get_info(char *buffer, char **start, off_t offset, int length, int dummy)
{
        int hash;
        int len=0;
        off_t pos=0;
        off_t begin;
	struct cfscache *cncp;
        char tmpbuf[80];

        if (offset < 80) 
              len += sprintf(buffer, "%-79s\n",
      "hash  len   volume    vnode   unique             name        ino       pino ct");
	if ( !cfsnc_initialized ) {
		*start = buffer;
		return len;
	}
        pos = 80;
	for (hash = 0; hash < cfsnc_hashsize; hash++) {
		for (cncp = cfsnchash[hash].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[hash];
		     cncp = cncp->hash_next) {
		 	pos += 80;
		 	if (pos < offset)
                                continue;
			sprintf(tmpbuf, "%4d  %3d %8x %8x %8x %16s %10ld %10ld %2d", 
				hash, cfsnchash[hash].length, (int) cncp->cp->c_fid.Volume, 
				(int) cncp->cp->c_fid.Vnode, (int) cncp->cp->c_fid.Unique , cncp->name, 
				CTOI(cncp->cp)->i_ino, 
				CTOI(cncp->dcp)->i_ino, 
				CTOI(cncp->cp)->i_count);
                     	len += sprintf(buffer+len, "%-79s\n", tmpbuf);
                     	if(len >= length)
                                break;
                	}
               	if(len>= length)
                       	break;
		}
        begin = len - (pos - offset);
        *start = buffer + begin;
        len -= begin;
        if(len>length)
                len = length;
        return len;
} 

int
cfsnc_nc_info(char *buffer, char **start, off_t offset, int length, int dummy)
{
        int len=0;
        off_t begin;
	
	cfsnc_gather_stats();

	/* this works as long as we are below 1024 characters! */    
    len += sprintf(buffer,"Coda minicache statistics\n\n");
    len += sprintf(buffer+len, "cfsnc_hits : %d\n", cfsnc_stat.hits);
    len += sprintf(buffer+len, "cfsnc_misses : %d\n", cfsnc_stat.misses);
    len += sprintf(buffer+len, "cfsnc_enters : %d\n", cfsnc_stat.enters);
    len += sprintf(buffer+len, "cfsnc_dbl_enters : %d\n", cfsnc_stat.dbl_enters);
    len += sprintf(buffer+len, "cfsnc_long_name_enters : %d\n", cfsnc_stat.long_name_enters);
    len += sprintf(buffer+len, "cfsnc_long_name_lookups : %d\n", cfsnc_stat.long_name_lookups);
    len += sprintf(buffer+len, "cfsnc_long_remove : %d\n", cfsnc_stat.long_remove);
    len += sprintf(buffer+len, "cfsnc_lru_rm : %d\n", cfsnc_stat.lru_rm);
    len += sprintf(buffer+len, "cfsnc_zapPfids : %d\n", cfsnc_stat.zapPfids);
    len += sprintf(buffer+len, "cfsnc_zapFids : %d\n", cfsnc_stat.zapFids);
    len += sprintf(buffer+len, "cfsnc_zapFile : %d\n", cfsnc_stat.zapFile);
    len += sprintf(buffer+len, "cfsnc_zapUsers : %d\n", cfsnc_stat.zapUsers);
    len += sprintf(buffer+len, "cfsnc_Flushes : %d\n", cfsnc_stat.Flushes);
    len += sprintf(buffer+len, "cfsnc_SumLen : %d\n", cfsnc_stat.Sum_bucket_len);
    len += sprintf(buffer+len, "cfsnc_Sum2Len : %d\n", cfsnc_stat.Sum2_bucket_len);
    len += sprintf(buffer+len,  "cfsnc_# 0 len : %d\n", cfsnc_stat.Num_zero_len);
    len += sprintf(buffer+len,  "cfsnc_MaxLen : %d\n", cfsnc_stat.Max_bucket_len);
    len += sprintf(buffer+len,  "cfsnc_SearchLen : %d\n", cfsnc_stat.Search_len);
       	begin =  offset;
       	*start = buffer + begin;
       	len -= begin;

        if(len>length)
                len = length;
	if (len< 0)
		len = 0;
        return len;
} 



void
coda_print_ce(struct cfscache *ce) 
{
CDEBUG(D_CACHE, "cp 0x%x, dcp 0x%x, name %s, inod 0x%x, ino %d, count %d, dev %d\n",
        (int)ce->cp, (int)ce->dcp, ce->name, (int)CTOI(ce->cp),(int)CTOI(ce->cp)->i_ino,  CTOI(ce->cp)->i_count, CTOI(ce->cp)->i_dev);
}

static void
cfsnc_gather_stats(void)
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
cfsnc_resize(int hashsize, int heapsize)
{
    if ( !cfsnc_use )
	    return 0;

    if ((hashsize % 2) || (heapsize % 2)) { /* Illegal hash or cache sizes */
	    return(EINVAL);
    }                 
    
    cfsnc_use = 0;                       /* Turn the cache off */
    cfsnc_force = 1;                     /* otherwise we can't flush */
    
    cfsnc_flush();                       /* free any cnodes in the cache */
    cfsnc_force = 0;
    
    /* WARNING: free must happen *before* size is reset */
    CODA_FREE(cfsncheap,TOTAL_CACHE_SIZE);
    CODA_FREE(cfsnchash,TOTAL_HASH_SIZE);
    
    cfsnc_hashsize = hashsize;
    cfsnc_size = heapsize;
    
    cfsnc_init();                        /* Set up a cache with the new size */
    
    cfsnc_use = 1;                       /* Turn the cache back on */
    return(0);
}




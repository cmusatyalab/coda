/* Coda filesystem -- Linux Minicache
 *
 * Copyright (C) 1989 - 1997 Carnegie Mellon University
 *
 * Carnegie Mellon University encourages users of this software to
 * contribute improvements to the Coda project. Contact Peter Braam
 * <coda@cs.cmu.edu>
 */

#ifndef _CFSNC_HEADER_
#define _CFSNC_HEADER_


/*
 * Cfs constants
 */
#define CFSNC_NAMELEN	15		/* longest name stored in cache */
#define CFSNC_CACHESIZE 256		/* Default cache size */
#define CFSNC_HASHSIZE	64		/* Must be multiple of 2 */
/*
 * Structure for an element in the Coda Credential Cache.
 */

struct coda_cache {
	struct list_head   cc_cclist;  /* list of all cache entries */
	struct list_head   cc_cnlist;  /* list of cache entries/cnode */
	int                cc_mask;
	struct coda_cred   cc_cred;
};

void coda_ccinsert(struct coda_cache *el, struct super_block *sb);
void coda_cninsert(struct coda_cache *el, struct cnode *cnp);
void coda_ccremove(struct coda_cache *el);
void coda_cnremove(struct coda_cache *el);
void coda_cache_create(struct inode *inode, int mask);
struct coda_cache *coda_cache_find(struct inode *inode);
void coda_cache_enter(struct inode *inode, int mask);
void coda_cache_clear_cnp(struct cnode *cnp);
void coda_cache_clear_all(struct super_block *sb);
void coda_cache_clear_cred(struct super_block *sb, struct coda_cred *cred);
int coda_cache_check(struct inode *inode, int mask);
void coda_dentry_delete(struct dentry *dentry);
void coda_zapfid(struct ViceFid *fid, struct super_block *sb, int flag);

/* roughly 50 bytes per entry */
struct cfscache {	
	struct cfscache	*hash_next,*hash_prev;	/* Hash list */
	struct cfscache	*lru_next, *lru_prev;	/* LRU list */
	struct cnode	*cp;			/* vnode of the file */
	struct cnode	*dcp;			/* parent's cnode */
	struct coda_cred *cred;			/* user credentials */
	char		name[CFSNC_NAMELEN];	/* segment name */
	int		namelen;		/* length of name */
};



/* exported */
void cfsnc_init(void);
void cfsnc_enter(struct cnode *dcp, register const char *name, int namelen, struct cnode *cp);
struct cnode *cfsnc_lookup(struct cnode *dcp, register const char *name, int namelen);
void cfsnc_zapParentfid(struct ViceFid *fid);
void cfsnc_zapfid(struct ViceFid *fid);
void cfsnc_zapfile(struct cnode *dcp, register const char *name, int length);
void cfsnc_purge_user(struct coda_cred *cred);
/* void cfsnc_flush(void); */
void cfsnc_replace(struct ViceFid *f1, struct ViceFid *f2);
void print_cfsnc(void);
void coda_print_ce(struct cfscache *);
int cfsnc_resize(int hashsize, int heapsize);
 
  

/* #define CFSNC_VALID(cncp)	( (cncp->dcp != (struct cnode *)0) && (cncp->cp->c_flags & C_VATTR) ) */
#define CFSNC_VALID(cncp)	    (cncp->dcp != (struct cnode *)0) 
 
#define DATA_PART(cncp)				(struct cfscache *) \
			((char *)cncp + (4*sizeof(struct cfscache *)))
#define DATA_SIZE	(sizeof(struct cfscache)-(4*sizeof(struct cfscache *)))

/*
 * Structure to contain statistics on the cache usage
 */

struct cfsnc_statistics {
	unsigned	hits;
	unsigned	misses;
	unsigned	enters;
	unsigned	dbl_enters;
	unsigned	long_name_enters;
	unsigned	long_name_lookups;
	unsigned	long_remove;
	unsigned	lru_rm;
	unsigned	zapPfids;
	unsigned	zapFids;
	unsigned	zapFile;
	unsigned	zapUsers;
	unsigned	Flushes;
	unsigned        Sum_bucket_len;
	unsigned        Sum2_bucket_len;
	unsigned        Max_bucket_len;
	unsigned        Num_zero_len;
	unsigned        Search_len;
};

/* 
 * Symbols to aid in debugging the namecache code. Assumes the existence
 * of the variable cfsnc_debug, which is defined in cfs_namecache.c
 */
extern int cfsnc_debug;
#define CFSNC_DEBUG(N, STMT)     { if (cfsnc_debug & (1 <<N)) { STMT } }

#define CFSNC_FIND		((u_long) 1)
#define CFSNC_REMOVE		((u_long) 2)
#define CFSNC_INIT		((u_long) 3)
#define CFSNC_ENTER		((u_long) 4)
#define CFSNC_LOOKUP		((u_long) 5)
#define CFSNC_ZAPPFID		((u_long) 6)
#define CFSNC_ZAPFID		((u_long) 7)
#define CFSNC_ZAPVNODE		((u_long) 8)
#define CFSNC_ZAPFILE		((u_long) 9)
#define CFSNC_PURGEUSER		((u_long) 10)
#define CFSNC_FLUSH		((u_long) 11)
#define CFSNC_PRINTCFSNC	((u_long) 12)
#define CFSNC_PRINTSTATS	((u_long) 13)
#define CFSNC_REPLACE		((u_long) 14)

#endif _CFSNC_HEADER_

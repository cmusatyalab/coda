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
 * cfsnc.h,v
 * Revision 1.2  1996/01/02 16:57:19  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:45  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:22  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:21  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.2  1994/08/28  19:37:39  luqi
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
 * Revision 2.1  94/07/21  16:25:27  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.2  92/10/27  17:58:34  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.2  90/07/05  11:27:04  mrt
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.4  90/05/31  17:02:12  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 */
#ifndef _CFSNC_HEADER_
#define _CFSNC_HEADER_

#include "coda.h"
#include "coda_cnode.h"


/*
 * Cfs constants
 */
#define CFSNC_NAMELEN	15		/* longest name stored in cache */
#define CFSNC_CACHESIZE 256		/* Default cache size */
#define CFSNC_HASHSIZE	64		/* Must be multiple of 2 */
/*
 * Structure for an element in the CFS Name Cache.
 */

/* roughly 50 bytes per entry */
struct cfscache {	
	struct cfscache	*hash_next,*hash_prev;	/* Hash list */
	struct cfscache	*lru_next, *lru_prev;	/* LRU list */
	struct cnode	*cp;			/* vnode of the file */
	struct cnode	*dcp;			/* parent's cnode */
	struct CodaCred	*cred;			/* user credentials */
	char		name[CFSNC_NAMELEN];	/* segment name */
	int		namelen;		/* length of name */
};



/* exported */
void cfsnc_init(void);
void cfsnc_enter(struct cnode *dcp, register const char *name, int namelen, struct cnode *cp);
struct cnode *cfsnc_lookup(struct cnode *dcp, register const char *name, int namelen);
void cfsnc_zapParentfid(ViceFid *fid);
void cfsnc_zapfid(ViceFid *fid);
void cfsnc_zapfile(struct cnode *dcp, register const char *name, int length);
void cfsnc_purge_user(struct CodaCred *cred);
void cfsnc_flush(void);
void cfsnc_replace(ViceFid *f1, ViceFid *f2);
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

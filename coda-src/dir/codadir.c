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
#include <assert.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include <cfs/coda.h>
#include <lwp.h>
#include <lock.h>
#include <rvmlib.h>
#include "codadir.h"
#include "dirprivate.h"
#ifdef __cplusplus
}
#endif __cplusplus

/* are we dealing with RVM memory? (yes for Venus, no for Vice)*/
int dir_data_in_rvm;

/* definitions for static functions */
static int dir_FindBlobs (struct DirHeader **dh, int nblobs);
static int dir_AddPage (struct DirHeader **dir);
static int dir_NameBlobs(char *);
static struct DirEntry *dir_FindItem (struct DirHeader *dir,char *ename, 
				       struct DirEntry **preventry, int *index);
struct DirEntry *dir_GetBlob (struct DirHeader *dir, long blobno);
static void dir_FreeBlobs(struct DirHeader *dir, register int firstblob, int nblobs);
static struct DirHeader *dir_Extend(struct DirHeader *olddirh, int in_rvm);

static int fid_FidEqNFid(struct DirFid *fid, struct DirNFid *nfid);
static void fid_Fid2NFid(struct DirFid *fid, struct DirNFid *nfid);
static void fid_NFid2Fid(struct DirNFid *nfid, struct DirFid *fid); 
static void fid_NFidV2Fid(struct DirNFid *, VolumeId, struct ViceFid *);

static void dir_SetRange(struct DirHeader *dir);
static int dir_rvm(void);

/* locking policy: DH_ routines lock.  DIR routines may
   assume directory is locked */


/* 
 * RVM support
 */

struct DirStat {
	int    get;
	int    put;
	int    flush;
} dir_stats;

inline void DIR_check_trans(char *where, char *file)
{
	if ( dir_rvm() && (! rvmlib_in_transaction()) ) {
		fprintf(stderr, "Aborting: no transaction in %s (%s)!\n", where, file);
		fflush(stderr);
		abort();
	}
}


static void dir_SetRange(struct DirHeader *dir)
{
	if (dir_rvm())
		rvmlib_set_range((void *)dir, DIR_Length(dir));
	return;
}

int dir_rvm(void) 
{
	return dir_data_in_rvm ;
}


/* 
 * DIR support
 */


/* Find out how many entries are required to store a name. */
static int dir_NameBlobs (char *name)
{
    register int i;
    i = strlen(name)+1;
    return 1+((i+15)>>LESZ);
}

/* 
   Find a bunch of contiguous entries; at least nblobs in a row.  
   return the blob number (always bigger than DHE+1)  
*/
static int dir_FindBlobs (struct DirHeader **dh, int nblobs)
{
	register int i, j, k;
	int failed;
	int grown = 0;
	struct PageHeader *pp;
	
	if ( !dh || !*dh || nblobs <= 0 ) 
		assert(0);


	for(i=0; i<DIR_MAXPAGES; i++) {
		/* if page could contain enough entries */
		if ((*dh)->dirh_allomap[i] >= nblobs)	{
			/* If there are EPP free entries, then the page is not
			   yet allocated. Add the page to the directory. */
			if ((*dh)->dirh_allomap[i] == EPP) {
				dir_AddPage(dh);
				grown = 1;
				(*dh)->dirh_allomap[i] = EPP-1;
			}
			pp = DIR_Page(*dh, i);
			for(j=0; j<=EPP-nblobs; j++) {
				failed = 0;
				for(k=0; k<nblobs; k++)
					if ((pp->freebitmap[(j+k)>>3]>>((j+k)&7)) & 1) {
						failed = 1;
						break;
					}
				if (!failed) 
					break;
				failed = 1;
			}
			/* Here we have the first index in j.  We clean up the
			   allocation maps and free up any resources we've got
			   allocated. */
			if (!failed) {
				if ( grown == 0 ) 
					dir_SetRange(*dh);
				(*dh)->dirh_allomap[i] -= nblobs;
				pp->freecount -= nblobs;
				for (k=0; k<nblobs; k++)
					pp->freebitmap[(j+k)>>3] |= (1<<((j+k) % 8));
				assert((j + i *EPP) > DHE);
				return j+i*EPP;
			}
		}
	}
	/* If we make it here, the directory is full. */
	return -1;
}

/* Add a page to a directory. */
static int dir_AddPage (struct DirHeader **dh)
{
	register int i;
	int page;
	register struct DirHeader *dirh;
	struct PageHeader *ph;

	if ( !dh ) {
		fprintf(stderr, "bogus call to addpage\n");
		assert(0);
	}


	dirh = dir_Extend(*dh, dir_rvm() );
	
	if ( !dirh )
		return ENOMEM;
	
	if ( dir_rvm() )
		RVMLIB_MODIFY(*dh, dirh);
	else
		*dh = dirh;

	page = DIR_Length(dirh) >> LOGPS; /* length in pages */
	ph = DIR_Page(dirh, page);

	dirh->dirh_allomap[page] = EPP - 1;
	ph->tag = htonl(1234);
	ph->freecount = EPP-1;   /* The first blob contains  ph*/
	ph->freebitmap[0] = 0x01;
	for (i=1;i<EPP/8;i++)	/* It's a constant */
		ph->freebitmap[i] = 0;
	return 0;

}

/* allocate a one page directory */
static struct DirHeader *dir_New(int in_rvm) 
{
	struct DirHeader *dirh;
		       
	/* get new space */
	if ( in_rvm ) {
		dirh = rvmlib_rec_malloc(DIR_PAGESIZE);
	} else {
		dirh = malloc(DIR_PAGESIZE);
	}

	if ( !dirh ) 
		return NULL;

	return dirh;
}


/* extend a dir by one page */
static struct DirHeader *dir_Extend(struct DirHeader *olddir, int in_rvm) 
{
	struct DirHeader *dirh;
	int oldsize, newsize;

	if ( !olddir )
		return NULL;

	oldsize = DIR_Length(olddir);
	newsize = oldsize + DIR_PAGESIZE;
		       
	/* get new space */
	if ( in_rvm ) {
		dirh = rvmlib_rec_malloc(newsize);
	} else {
		dirh = malloc(newsize);
	}

	if ( !dirh ) 
		return NULL;

	/* if old dirh exist copy data */
	if ( in_rvm ) 
		rvmlib_set_range((void *)dirh, newsize);
	memmove(dirh, olddir, oldsize);
	
	bzero((void *)dirh + oldsize, newsize - oldsize);

	/* if old dirh exists, free it */
	if ( in_rvm ) 
		rvmlib_rec_free(olddir);
	else
		free(olddir);

	return dirh;
}


struct PageHeader *DIR_Page(struct DirHeader *dirh, int page)
{
	assert(page >= 0);
	return (struct PageHeader *)((void *)dirh + page * DIR_PAGESIZE);

}

/* Free a whole bunch of directory entries: _within_ a single page  */
static void dir_FreeBlobs(struct DirHeader *dhp, register int firstblob, 
			   int nblobs)
{
    register int i;
    int page;
    struct PageHeader *pp;
    page = firstblob>>LEPP;
    firstblob -= EPP*page;	/* convert to page-relative entry */

    /* make sure not to go onto the next page */
    assert( firstblob + nblobs <= EPP );

    if (!dhp) 
	    return;

    dhp->dirh_allomap[page] += nblobs;
    pp = DIR_Page(dhp,page);
    pp->freecount += nblobs;
    for (i=0;i<nblobs;i++)
	    pp->freebitmap[(firstblob+i)>>3] &= ~(1<<((firstblob+i)&7));
}

/* print a bitmap of add of nbits into buff. Caller must add
   a \0 character after this routine has been called. */
int dir_PrintChar(char *addr, int nbits, char *buff)
{
	int i;
	char c;
	char bit[2];
	int isset;
	int byte, offset;
	int set = 0;

	for ( i = 0 ; i < nbits ; i++ ) {
		byte = i/8;
		c = *(addr + byte);
		offset = i % 8;
		isset = ( (c << offset) & 128 ) ? 1 : 0;
		snprintf(bit, 2, "%d", isset);
		buff[i] = bit[0];
		if ( isset ) 
			set++;
	}
	return set;
}

/* 
 * Conversion from Coda to BSD dir entry format
 */

static int dir_DirEntry2VDirent(PDirEntry ep, struct venus_dirent *vd, VolumeId vol)
{
	struct ViceFid fid;
	
	assert(ep && vd);
        fid_NFidV2Fid(&ep->fid, vol, &fid);
	
	vd->d_fileno = coda_f2i(&fid);
	vd->d_type = 0;
	vd->d_namlen = strlen(ep->name);
	strcpy(vd->d_name, ep->name);
	vd->d_reclen = DIRSIZ(vd);
	return vd->d_reclen;
}

/*
 *   Fid support routines  -- to be extracted to a Fid library
 */


static int fid_FidEqNFid(struct DirFid *fid, struct DirNFid *nfid) 
{
	if ( !fid || !nfid ) 
		assert(0);
	
	if ( (ntohl(nfid->dnf_vnode) == fid->df_vnode) &&
	     (ntohl(nfid->dnf_unique) == fid->df_unique) )
		return 1;
	else 
		return 0;

}

static void fid_Fid2NFid(struct DirFid *fid, struct DirNFid *nfid) 
{
	if ( !fid || !nfid ) 
		assert(0);
	
	nfid->dnf_vnode = htonl(fid->df_vnode);
	nfid->dnf_unique = htonl(fid->df_unique);

}

static void fid_NFid2Fid(struct DirNFid *nfid, struct DirFid *fid) 
{
	if ( !fid || !nfid ) 
		assert(0);
	
	fid->df_vnode = ntohl(nfid->dnf_vnode);
	fid->df_unique = ntohl(nfid->dnf_unique);

}



/*
 *  EXPORTED interface
 */


/* to initialize the entire dir package */
int DIR_Init(int data_loc)
{
	if ( data_loc == DIR_DATA_IN_RVM ) {
		dir_data_in_rvm = 1;
		return 0;
	} else if ( data_loc == DIR_DATA_IN_VM ) {
		dir_data_in_rvm = 0;
		return 0;
	} else
		return 1;
	bzero((char *)&dir_stats, sizeof(dir_stats));
}

void DH_PrintStats(FILE *fp)
{
	fprintf(fp, "Dirstats: get %d, put %d, flush %d\n", 
		dir_stats.get, dir_stats.put, dir_stats.flush);
}


/* Look up the first fid in directory with given name: 
   return 0 if found
   return EONOENT upon failure
*/
int DIR_LookupByFid(PDirHeader dhp, char *name, struct DirFid *fid) 
{
	int i;
	int code = 0;

	if (!dhp) {
		code = ENOENT; 
		goto Exit; 
	}

	for (i = 0; i < NHASH; i++) {
		/* For each hash chain, enumerate everyone on the list. */
		int num = ntohs(dhp->dirh_hashTable[i]);
		while (num != 0) {
			/* Walk down the hash table list. */
			struct DirEntry *ep = dir_GetBlob(dhp, num);
			if (!ep) 
				break;

			if ( fid_FidEqNFid(fid, &ep->fid) ) {
				strcpy(name, ep->name);
				goto Exit;
			}

			num = ntohs(ep->next);
		}
	}

	code = ENOENT;

 Exit:
	return(code);
}

/* dir size in bytes This is called often and inefficient. */
int DIR_Length (struct DirHeader *dir)
{
	int i, ctr;
	if (!dir) 
		return 0;

	ctr=0;
	for(i=0; i<DIR_MAXPAGES; i++)
		if (dir->dirh_allomap[i] != EPP) ctr++;
	return ctr * DIR_PAGESIZE;
}


/* the following functions (Create, MkDir, Delete, Setpages) 
   modify directory contents.
   The first two of these may alse need to increase the directory 
   size, so they are passed a pointer to the dirheader address.
     - they must be called from within a transaction (if in RVM) and we
       check for that.
     - they must lock the directory in W mode
     - the first two must have a WLock on the *dh.
*/


/* Create an entry in a directory file.  */
int DIR_Create (struct DirHeader **dh, char *entry, struct DirFid *fid)
{
	int blobs, firstblob;
	register int i;
	struct DirHeader *dir;
	register struct DirEntry *ep;

	if ( !dh || !(*dh) ||  !entry || !fid )
		return EIO;

	DIR_intrans();

	dir = *dh;

	if ( strlen(entry) > CODA_MAXNAMLEN )
		return ENAMETOOLONG;
    
	/* First check if file already exists. */
	ep = dir_FindItem(dir, entry, 0, 0);
	if (ep) {
		return EEXIST;
	}

	blobs = dir_NameBlobs(entry);	/* number of entries required */
	firstblob = dir_FindBlobs(dh, blobs);
	dir = *dh;
	if (firstblob < 0) {
		return EFBIG;	/* directory is full */
	}
	
	/* First, we fill in the directory entry. */
	ep = dir_GetBlob(dir, firstblob);
	if (ep == 0) {
		return EIO;
	}
	ep->flag = FFIRST;
	fid_Fid2NFid(fid, &ep->fid);
	strcpy(ep->name,entry);

	/* Now we just have to thread it on the hash table list.     */
	i = DIR_Hash(entry);
	ep->next = dir->dirh_hashTable[i];
	dir->dirh_hashTable[i] = htons(firstblob);

	if ( !DIR_DirOK(dir)) {
		fprintf(stderr, "Corrupt directory at %p\n", dir);
		DIR_Print(dir);
	}

	return 0;
}



/* Delete an entry from a directory, including update of all free
   entry descriptors. 
   Return 0 upon success
*/
int DIR_Delete(struct DirHeader *dir, char *entry)
{
	int nitems, index;
	register struct DirEntry *firstitem;
	struct DirEntry *preventry;
    
	assert( dir && entry );

	firstitem = dir_FindItem(dir, entry, &preventry, &index);
	if ( !firstitem ) 
		return ENOENT;

	if ( preventry ) {
		preventry->next = firstitem->next;
	} else {
		dir->dirh_hashTable[DIR_Hash(entry)] = firstitem->next;
	}
	nitems = dir_NameBlobs(firstitem->name);
	
	dir_FreeBlobs(dir, index, nitems);

	if ( !DIR_DirOK(dir)) {
		fprintf(stderr, "Corrupt directory at %p\n", dir);
		DIR_Print(dir);
	}

	return 0;
}

/* Format an empty directory properly.  Note that the first 13 entries
   in a directory header page are allocated, 1 to the page header, 4
   to the allocation map and 8 to the hash table. 
   
   We don't do this in conjunction with the parent, to flexibly create
   root directories too. Maybe MakeSubDir is a good idea? 
*/
int DIR_MakeDir (struct DirHeader **dir,struct DirFid *me, 
		 struct DirFid *parent)
{
	register int i;
	register struct DirHeader *dhp;


	if ( !dir ) {
		fprintf(stderr, "Bad call to DIR_MakeDir\n");
		assert(0);
	}

	
	dhp = dir_New(dir_rvm());
	if ( !dhp )
		return ENOMEM;

	/* cannot call dir_SetRange yet, since dir is unitialized */
	if ( dir_rvm()) {
		rvmlib_set_range(dir, sizeof(*dir));
		rvmlib_set_range(dhp, DIR_PAGESIZE);
	}
	*dir = dhp;

	bzero(dhp, DIR_PAGESIZE);

	dhp->dirh_ph.tag =  htonl(1234);
	dhp->dirh_ph.freecount = (EPP-DHE-1);
	dhp->dirh_ph.freebitmap[0] = 0xff;
	dhp->dirh_ph.freebitmap[1] = 0x1f;
	for(i=2;i<EPP/8;i++) 
		dhp->dirh_ph.freebitmap[i] = 0;
	dhp->dirh_allomap[0] = (EPP-DHE-1);

	DIR_Setpages(dhp, 1);

	for(i=0;i<NHASH;i++)
		dhp->dirh_hashTable[i] = 0;

	DIR_Create(dir, ".", me);
	DIR_Create(dir, "..", parent);

	if ( !DIR_DirOK(*dir)) {
		fprintf(stderr, "Corrupt directory at %p\n", dir);
		DIR_Print(dir);
	}

	return 0;
}

/* Set the number of pages in the allomap */
void DIR_Setpages(PDirHeader dirh, int pages)
{
	int i;

	assert(pages >= 1);
	for(i=pages; i<DIR_MAXPAGES; i++)
		dirh->dirh_allomap[i] = EPP;
}


/* will release the directory pages from memory or RVM */
void DIR_Free(struct DirHeader *dir, int in_rvm)
{
	if ( !dir ) 
		return;

	if ( dir_rvm() ) 
		rvmlib_rec_free(dir);
	else
		free(dir);
}


/* print one entry */
int DIR_PrintEntry(PDirEntry entry)
{
	struct DirFid fid;

	fid_NFid2Fid(&(entry->fid), &fid);

	fprintf(stdout, "next: %hu, flag %d fid: (%lx.%lx) %s\n",
		ntohs(entry->next), entry->flag,
		fid.df_vnode, fid.df_unique, entry->name);


	return 0;
}

void DIR_PrintChain(PDirHeader dir, int chain)
{
	short blob;
	struct DirEntry *de;

	if ( chain < 0 || chain >= NHASH ) {
		fprintf(stderr, "DIR_PrintChain: no such chain\n");
		return;
	}
	
	blob = ntohs(dir->dirh_hashTable[chain]);
		
	while ( blob ) {
		de = dir_GetBlob(dir, blob);
		fprintf(stdout, "thisblob: %hu ", blob);
		DIR_PrintEntry(de);
		blob = ntohs(de->next);
	}
	return ;
}

/* print the header and then the entries */
void DIR_Print(PDirHeader dir)
{
	int i, num;
	int setbits;
	int freecount;
	char allo;
	struct PageHeader *ph;
	char bitmap[EPP+1];

	fprintf(stdout, "DIR: %p,  LENGTH: %d\n", dir, DIR_Length(dir));

	fprintf(stdout, "\nHASH TABLE:\n");
	for ( i = 0; i < NHASH ; i++ ) {
		num = ntohs(dir->dirh_hashTable[i]);
		if ( num ) 
			fprintf(stdout, "(%d %hd) ", i, num);
	}

	fprintf(stdout, "\n\nALLOMAP:\n");
	for ( i = 0; i < DIR_MAXPAGES ; i++ ) {
		allo = dir->dirh_allomap[i];
		if ( allo != EPP )
			fprintf(stdout, "(%d %i) ", i, allo);
	}

	fprintf(stdout, "\n\nPAGEHEADERS:\n");
	for ( i = 0; i < DIR_MAXPAGES ; i++ ) {
		if ( dir->dirh_allomap[i] != EPP ) {
			ph = DIR_Page(dir, i);
			setbits = dir_PrintChar(ph->freebitmap, EPP, bitmap);
			bitmap[EPP] = '\0';
			freecount = ph->freecount;
			fprintf(stdout, 
				"page %d, tag %d, freecount %d, set %d, bitmap: \n",
				i, ntohl(ph->tag), freecount, setbits);
			fprintf(stdout, "%s\n\n", bitmap);
		}
	}
			
	fprintf(stdout, "\nCHAINS:\n");
	for ( i = 0; i < NHASH ; i++ ) {
		num = ntohs(dir->dirh_hashTable[i]);
		if ( num ) {
			printf("Chain: %d\n", i);
			DIR_PrintChain(dir, i);
		}
	}

}



static void fid_NFidV2Fid(struct DirNFid *dnfid, VolumeId vol, struct ViceFid *fid)
{
	fid->Vnode = ntohl(dnfid->dnf_vnode);
	fid->Unique = ntohl(dnfid->dnf_unique);
	fid->Volume = vol;
	return;
}


/* Look up a file name in directory.
   return 0 upon success
   return ENOENT upon failure
*/
int DIR_Lookup (struct DirHeader *dir, char *entry, struct DirFid *fid)
{
	register struct DirEntry *de;

	if ( !dir ) 
		return ENOENT;

	de = dir_FindItem(dir, entry, 0, 0);
	if (de == 0) {
		return ENOENT;
	}

	fid_NFid2Fid(&de->fid, fid);

	return 0;
}

/* an EnumerateDir hook for comparing two directories. */
int dir_HkCompare(PDirEntry de, void *hook)
{
	struct DirFid dfid;
	int rc;
	PDirHeader dh = (PDirHeader)hook;
	rc = DIR_Lookup(dh, de->name, &dfid);
	if ( rc ) 
		return rc;
	if ( fid_FidEqNFid(&dfid, &de->fid) )
		return 0; /* equal */
	return 1;
}

/* Rewrite a Coda directory in Venus' BSD format for a container file */
int DIR_Convert (PDirHeader dir, char *file, VolumeId vol)
{
	struct DirEntry *ep;
	int fd;
	int len;
	struct venus_dirent *vd;
	int i;
	int num;
	char *buf;
	int offset = 0;

	if ( !dir ) 
		return ENOENT;
	
	fd = open(file, O_RDWR | O_TRUNC, 0600);
	assert( fd >= 0 );

	len = DIR_Length(dir);

#ifndef DJGPP
	assert( ftruncate(fd, len) == 0 );
	buf = mmap(0, len, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	assert( (int) buf != -1 );
#else
	buf = malloc(len);
	assert(buf);
#endif

	bzero(buf, len);

	for (i=0; i<NHASH; i++) {
		num = ntohs(dir->dirh_hashTable[i]);
		while (num != 0) {
			ep = dir_GetBlob(dir, num);
			if (!ep) 
				break;
			vd = (struct venus_dirent *)(buf + offset);
			offset += dir_DirEntry2VDirent(ep, vd, vol);
			num = ntohs(ep->next);
		}
	}
	/* add a final entry */
	vd = (struct venus_dirent *)(buf + offset);
	vd->d_fileno = 0;
	vd->d_reclen = len - offset + 1;

#if DJGPP
	rc  = write(fd, buf, len);
	assert(rc == len);
#else 
	assert(munmap(buf, len) == 0);
#endif
	assert(close(fd) == 0);
	return 0;
}

/* Enumerate the contents of a directory. */
int DIR_EnumerateDir(struct DirHeader *dhp, 
		     int (*hookproc)(struct DirEntry *de, void *hook) , void *hook)
{
	int i;
	int num;
	int rc = 0;
	register struct DirEntry *ep;
    
	if (!dhp) 
		return ENOENT;

	for(i=0;i<NHASH;i++) {
		/* For each hash chain, enumerate everyone on the list. */
		num = ntohs(dhp->dirh_hashTable[i]);
		while (num != 0) {
			/* Walk down the hash table list. */
			ep = dir_GetBlob(dhp,num);
			if (!ep) 
				break;
			num = ntohs(ep->next);
			rc = (*hookproc) (ep, hook);
			if ( rc ) 
				return rc;
		}
	}
	return 0;
}


#if 0
/* Given fid get the name if it exists in the dir If more than 1 name
   exists for the same object, return the first one */
char *DIR_FindName(struct DirHeader *dhp, struct DirFid *fid, 
		   char *buf, int buflen) 
{
	int i;
	int entryfound = 0;

	if (!dhp) 
		return NULL;

	for (i = 0; i < NHASH && !entryfound; i++) {
		int num = ntohs(dhp->dirh_hashTable[i]);
		while (num != 0) {
			struct DirEntry *ep= dir_GetBlob(dhp, num);
			if (!ep) 
				break; /* next hash */

			if (fid_FidEqNFid(fid, &ep->fid)) {
				entryfound = 1;
				strncpy(buf, ep->name, buflen);
				break;
			}

			num = ntohs(ep->next);
		}
	}
    
	if (entryfound) 
		return(buf);
	else 
		return(NULL);
}

#endif 
/* compare two directories */
int DIR_Compare(PDirHeader d1, PDirHeader d2)
{
	int rc;

	if ( (rc = DIR_EnumerateDir(d1, dir_HkCompare, (void *) d2)) )
		return rc;
	if ( (rc = DIR_EnumerateDir(d2, dir_HkCompare, (void *) d1)) )
		return -rc;
	return 0; 
}


#define TO_RVM 1
#define TO_VM 0
void dir_Copy(PDirHeader old, PDirHeader *new, int to_rvm)
{
	int size;

	size = DIR_Length(old);
	
	if ( to_rvm == TO_RVM ) {
		*new = (PDirHeader)rvmlib_rec_malloc(size);
		assert(*new);
		rvmlib_set_range((void *) new, size);
	} else
		*new = (PDirHeader)malloc(size);
	bcopy((const char *)old, (char *) *new, size);

}

/* IsEmpty 
   returns 1 when directory is empty
   returns 0 if directory is nonempty 
*/
int DIR_IsEmpty (struct DirHeader *dhp) 
{
	/* Enumerate the contents of a directory. */
	register int i;
	int num;
	int empty = 1;
	register struct DirEntry *ep;
	
	if (!dhp) 
		return 0;
	for(i=0;i<NHASH && empty;i++) {
		/* For each hash chain, enumerate everyone on the list. */
		num = ntohs(dhp->dirh_hashTable[i]);
		while (num != 0) {
			/* Walk down the hash table list. */
			ep = dir_GetBlob(dhp, num);
			if (!ep) 
				break;
			if (strcmp(ep->name,"..") && strcmp(ep->name,".")) {
				empty = 0;
				break;
			}
			num = ntohs(ep->next);
		}
	}
	return (empty);
}



/* Return a pointer to an entry, given its number. */
struct DirEntry *dir_GetBlob (struct DirHeader *dir, long blobno)
{
	if (!dir || blobno <= 0 || blobno > (EPP * DIR_MAXPAGES)) 
		return 0;
	
	return (struct DirEntry *)
		((char *)dir + blobno * sizeof(struct DirBlob));
}

/* Hash a string to a number between 0 and NHASH. */
int DIR_Hash (char *string)
{
    register char tc;
    register int hval;
    register int tval;
    hval = 0;
    while( (tc=(*string++)) )
        {hval *= 173;
        hval  += tc;
        }
    tval = hval & (NHASH-1);
    if (tval == 0) return tval;
    else if (hval < 0) tval = NHASH-tval;
    return tval;
}

/* Find a directory entry, given its name.  This entry returns a
   pointer to DirEntry, and a pointer to the previous DirEntry (to aid
   the delete code) in the hash chain.  If no entry is found a null
   pointer is returned instead. */
static struct DirEntry *dir_FindItem (struct DirHeader *dir, char *ename, 
				       struct DirEntry **preventry, int *index)
{
	register int i;
	register struct DirEntry *tp;
	register struct DirEntry *lp;	/* page of previous entry in chain */
	register short blobno;
	
	if (!dir) 
		return 0;
	
	i = DIR_Hash(ename);
	blobno = ntohs(dir->dirh_hashTable[i]);

	if (blobno == 0)
		return 0;

	tp = dir_GetBlob(dir,blobno);
	lp = NULL;

	/* walk the chain */
	while(1) {
		if (!strcmp(ename,tp->name)) {
			if (preventry)
				*preventry = lp;
			if (index)
				*index = blobno;
			return tp;
		}
		lp = tp;

		/* The end of the line */
		blobno = ntohs(tp->next);
		if (blobno == 0) 
			return 0;
		tp = dir_GetBlob(dir,blobno);
		if (!tp) 
			return 0;
	}
}

int DIR_DirOK(PDirHeader pdh) 
{
	int i;
	int k;
	int j;
	int pages;
	short entryno;
	PDirEntry ep;
	struct PageHeader *ph;
	int fcount;
	char eaMap[EPP * DIR_MAXPAGES];

	/* see if we have data */
	if ( pdh == NULL ) {
		printf("No DirHeader, no first page?\n");
		return 0;
	}

	if ( pdh->dirh_ph.tag != htonl(1234) ) {
		printf("Bad pageheader magic number in first page.\n");
		return 0;
	}		


	/* check the allomap: _in order_ we check for the following pages:
	   first page: freecount between 0 and EPP-DHE-1
            allocated pages: freecount betwen 0 and EPP-1
            free pages: freecount must be EPP
	*/

	j = pdh->dirh_allomap[0];
	if ( j < 0 || j > EPP-DHE-1 ) {
		printf("The allomap entry for page %d is bad\n", 0);
		return 0;
	}	

	pages = DIR_Length(pdh) / DIR_PAGESIZE;
	for ( i = 1 ; i < pages ; i++ ) {
		j = pdh->dirh_allomap[i];
		if ( j < 0 || j > EPP-1 ) {
			printf("The allomap entry for page %d is bad\n", i);
			return 0;
		}
	}

	for ( i = pages ; i < DIR_MAXPAGES ; i++ ) {
		if ( pdh->dirh_allomap[i] != EPP ) {
			printf("Page %d is partially empty but appears after directory ended", i);
			return 0;
		}
	}
	
	/* check that other pages have correct magic */
	for ( i=1 ; i < pages ; i++ ) {
		ph = DIR_Page(pdh, i);
		if ( ph->tag != ntohl(1234) ) {
			printf("Magic wrong in Page i\n");
			return 0;
		}
	}


	/* check page by page if free count in alloMap matches blob bitmap */
	for ( i = 1 ; i < pages ; i++ ) {
		ph = DIR_Page(pdh, i);
		fcount = EPP; 
		for(j=0;j<EPP/8;j++) {
			k = ph->freebitmap[j];
			if (k & 0x80) fcount--;
			if (k & 0x40) fcount--;
			if (k & 0x20) fcount--;
			if (k & 0x10) fcount--;
			if (k & 0x08) fcount--;
			if (k & 0x04) fcount--;
			if (k & 0x02) fcount--;
			if (k & 0x01) fcount--;
		}

		if ( fcount != (int)(pdh->dirh_allomap[i]) ) {
			printf("Page %d has free count discrepancy bitmap/allomap\n",
			       i);
			return 0;
		}
	}

	/* build an entry allocation map for the entire directory */
	bzero(eaMap, sizeof(eaMap));
	
	/* take care of page and dir header blob usage */
	/* first page */ 
	eaMap[0] = 0xff; /* DHE == 12 and 1 entry for header */
	eaMap[1] = 0x1f;  
	for ( i = 1 ; i < pages ; i++ ) {
		eaMap[i*(EPP/8)] = 0x1;
	}
	
	/* walk down the hash table: 
          - check that the values point to valid blobs
          - get the blob
          - check the entry has the FFIRST flag set
          - 
	*/
	for ( i = 0 ; i < NHASH ; i++ ) {
		entryno =  htons(pdh->dirh_hashTable[i]);
		while (1) {
			if ( entryno == 0 ) 
				break;
			if ( entryno < 0 || entryno > pages * EPP ) {
				printf("Out-of-range hash id %d in chain %d\n", 
				       entryno, i);
				return 0;
			}
			ep = dir_GetBlob(pdh, entryno);
			if ( !ep ) {
				printf("Invalid hash id %d in chain %d\n",
				       entryno, i);
				return 0;
			}
			if ( ep->flag != FFIRST ) {
				printf("Dir entry %d at %p in chain %d bogus flag\n",
				       entryno, ep, i);
				return 0;
			}
			j = strlen(ep->name);
			if ( j > CODA_MAXNAMLEN ) {
				printf("Dir entry %p in chain %d too long name: %s\n",
				       ep, j, ep->name);
				return 0;
			}
			if ( j == 0 ) {
				printf("Dir entry in blob %d has null name\n",
				       entryno);
				return 0;
			}
			k = dir_NameBlobs(ep->name);
			for ( j = 0 ; j<k ; j++ ) {
				eaMap[(entryno+j)>>3] |= (1<<((entryno+j)&7));
			}
			j = DIR_Hash(ep->name);
			if ( j != i ) {
				printf("Dir entry %p, name %s, should be i nhash"
				       " bucket %d, but is in %d", 
				       ep, ep->name, j, i);
				return 0;
			}
			entryno = ntohs(ep->next);
		}

	}


	for ( i = 0 ; i < pages ; i++ ) {
		ph = DIR_Page(pdh, i);
		fcount = i * (EPP/8);
		for ( j = 0 ; j < EPP/8 ; j++ ) {
			if ( eaMap[fcount + j] != ph->freebitmap[j] ) {
				printf("blob bitmap error: page %d, map offset %d,"
				       "%x should be %x.\n",
				       i, j, ph->freebitmap[j], 
				       eaMap[fcount + j]);
				return 0;
			}
		}
	}
	/* dir is OK */
	return 1;

}





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
	assert(dh);
	bzero(dh, sizeof(*dh));
	Lock_Init(&dh->dh_lock);
}


#if 0
PDirHandle DH_New(int in_rvm, PDirHeader vmdata, PDirHeader rvmdata)
{
	struct DirHandle *dh;
	
	if ( in_rvm == 0 ) {
		dh = malloc(sizeof(*dh));
		assert(dh);
	} else {
		DIR_intrans();
		dh = rvmlib_rec_malloc(sizeof(*dh));
		assert( dh );
		rvmlib_set_range((char *)dh, sizeof(*dh));
	}
	Lock_Init(&dh->dh_lock);
	dh->dh_vmdata = vmdata;
	dh->dh_rvmdata = rvmdata;
	dh->dh_refcount = 0;
	return dh;
}

/* instantiate a new dirheader with same data in VM */
void DH_CopyRvmToVM(PDirHandle old, PDirHandle *new)
{
	*new = DH_New(0,0,0);
	dir_Copy(old->dh_rvmdata, &((*new)->dh_vmdata), TO_VM);
	(*new)->dh_refcount = old->dh_refcount;
	return; 
}

/* instantiate a new dirheader with same data in RVM */
void DH_CopyVMToRvm(PDirHandle old, PDirHandle *new)
{
	*new = DH_New(0,0,0);
	dir_Copy(old->dh_vmdata, &((*new)->dh_rvmdata), TO_RVM);
	RVMLIB_MODIFY((*new)->dh_refcount, old->dh_refcount);
	return;
}

#endif 
int DH_Length(PDirHandle dh)
{
	int rc;

	DH_LockW(dh);

	if ( dir_rvm() ) 
		rc = DIR_Length(dh->dh_rvmdata);
	else
		rc = DIR_Length(dh->dh_vmdata);

	DH_UnLockW(dh);

	return rc;
}


/* to convert Coda dir to Unix dir: called by client */
int DH_Convert(PDirHandle dh, char *file, VolumeId vol)
{
	int rc;

	DH_LockR(dh);

	if ( dir_rvm() ) 
		rc = DIR_Convert(dh->dh_rvmdata, file, vol);
	else
		rc = DIR_Convert(dh->dh_vmdata, file, vol);

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

	if ( dir_rvm() ) {
		DIR_intrans();
		dir_SetRange(dh->dh_rvmdata);
		rc = DIR_Create(&dh->dh_rvmdata, entry, &dfid);
	} else {
		rc = DIR_Create(&dh->dh_vmdata, entry, &dfid);
	}

	DH_UnLockW(dh);

	return rc;
}

/* check if the directory has entries apart from . and .. */
int DH_IsEmpty(PDirHandle dh)
{
	int rc;

	DH_LockR(dh);

	if ( dir_rvm() ) {
		rc  = DIR_IsEmpty(dh->dh_rvmdata);
	} else {
		rc = DIR_IsEmpty(dh->dh_vmdata);
	}

	DH_UnLockR(dh);

	return rc;

}

#if 0
/* lookup by Fid: called by Resolve */
char *DH_FindName(PDirHandle dh, struct DirFid *fid, char *name, int len)
{
	char *rc;

	DH_LockR(dh);

	if ( dir_rvm() ) {
		rc  = DIR_FindName(dh->dh_rvmdata, fid, name, len);
	} else {
		rc = DIR_FindName(dh->dh_vmdata, fid, name, len);
	}

	DH_UnLockR(dh);

	return rc;

}

#endif 

/* find fid given the name: called all over */
int DH_Lookup(PDirHandle dh, char *entry, struct ViceFid *vfid)
{
	int rc;
	struct DirFid dfid;

	DH_LockR(dh);

	if ( dir_rvm() ) {
		rc  = DIR_Lookup(dh->dh_rvmdata, entry, &dfid);
	} else {
		rc = DIR_Lookup(dh->dh_vmdata, entry, &dfid);
	}

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

	if ( dir_rvm() ) {
		rc  = DIR_LookupByFid(dh->dh_rvmdata, entry, &dfid);
	} else {
		rc = DIR_LookupByFid(dh->dh_vmdata, entry, &dfid);
	}

	DH_UnLockR(dh);

	return rc;

}

/* remove an entry from a directory */
int DH_Delete(PDirHandle dh, char *entry) 
{
	int rc;
	
	DH_LockW(dh);
	dh->dh_dirty = 1;

	if ( dir_rvm() ) {
		DIR_intrans();
		dir_SetRange(dh->dh_rvmdata);
		rc = DIR_Delete(dh->dh_rvmdata, entry);
	} else {
		rc = DIR_Delete(dh->dh_vmdata, entry);
	}

	DH_UnLockW(dh);

	return rc;
}

#if 0
/* the end of this dirhandle and the associated dir data */
void DH_Free(PDirHandle dh, int in_rvm)
{

	DH_FreeData(dh);

	if ( in_rvm ) 
		rvmlib_rec_free(dh);
	else 
		free(dh);

	return;
}
#endif

/* the end of the data */
void DH_FreeData(PDirHandle dh)
{
	DH_LockW(dh);
	dh->dh_dirty = 1;
	if ( dh->dh_rvmdata ) {
		rvmlib_rec_free(dh->dh_rvmdata);
	}
	
	if ( dh->dh_vmdata ) {
		free(dh->dh_vmdata);
	}

	dh->dh_vmdata = NULL;
	dh->dh_rvmdata = NULL;
	dh->dh_refcount = 0;
	DH_UnLockW(dh);
}

/* alloc a directory buffer for the DH */
void DH_Alloc(PDirHandle dh, int size, int in_rvm)
{
	assert(dh);
	DH_LockW(dh);
	dh->dh_dirty = 1;
	if ( in_rvm ) {
		DIR_intrans();
		RVMLIB_REC_OBJECT(*dh);
		dh->dh_rvmdata = rvmlib_rec_malloc(size);
		assert(dh->dh_rvmdata);
		bzero((void *)dh->dh_rvmdata, size);
	} else {
		dh->dh_vmdata = malloc(size);
		assert(dh->dh_vmdata);
		bzero((void *)dh->dh_vmdata, size);
	}

	DH_UnLockW(dh);
	return;
}

PDirHeader DH_Data(PDirHandle dh)
{
	if ( dir_rvm() ) 
		return dh->dh_rvmdata;
	else 
		return dh->dh_vmdata;
}

void DH_Print(PDirHandle dh)
{

	DH_LockR(dh);
	if ( dir_rvm() ) {
		DIR_Print(dh->dh_rvmdata);
	} else {
		DIR_Print(dh->dh_vmdata);
	}
	DH_UnLockR(dh);
	return;
}


int DH_DirOK(PDirHandle dh)
{
	int rc;

	DH_LockR(dh);
	if ( dir_rvm() ) {
		rc = DIR_DirOK(dh->dh_rvmdata);
	} else {
		rc = DIR_DirOK(dh->dh_vmdata);
	}
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
	if ( dir_rvm() ) {
		DIR_intrans();
		rc = DIR_MakeDir(&dh->dh_rvmdata, &dme, &dparent);
	} else {
		rc = DIR_MakeDir(&dh->dh_vmdata, &dme, &dparent);
	}
	DH_UnLockW(dh);
	return rc;
}

int DH_EnumerateDir(PDirHandle dh, int (*hookproc)(struct DirEntry *de, void* hook) , 
		    void *hook)
{
	int rc;
	
	DH_LockW(dh);

	if ( dir_rvm() ) {
		rc = DIR_EnumerateDir(dh->dh_rvmdata, hookproc, hook);
	} else {
		rc = DIR_EnumerateDir(dh->dh_vmdata, hookproc, hook);
	}

	DH_UnLockW(dh);

	return rc;
}

#if 0

/* copy a VM copy of a directory back to RVM; 
 * must be called from withing a transaction 
 */
void DH_Put(PDirHandle dh)
{
	int len;
	DH_LockW(dh);
	DIR_intrans();

	len = DIR_Length(dh->dh_vmdata);
	/* check on sizes */
	if ( dh->dh_rvmdata == NULL ) {
		dh->dh_rvmdata = rvmlib_rec_malloc(len);
		assert(dh->dh_rvmdata);
		bzero(dh->dh_rvmdata, len);
	} else if ( len != DIR_Length(dh->dh_vmdata) ) {
		rvmlib_rec_free(dh->dh_rvmdata);
		dh->dh_rvmdata = rvmlib_rec_malloc(len); 
		assert(dh->dh_rvmdata);
		bzero(dh->dh_rvmdata, len);
	}
	
	rvmlib_set_range((char *)dh->dh_rvmdata, len);
	bcopy((const void *)dh->dh_vmdata, dh->dh_rvmdata, len);
	free(dh->dh_vmdata);
	dh->dh_vmdata = NULL;
	return;
}

/* this should only be called with a lock on the Vnode */
void DH_Get(PDirHandle dh, PDirHeader data)
{
	int length;
	DH_LockW(dh);
	if ( dh->dh_refcount == 0 ) {
		dh->dh_rvmdata = data;
		length = DIR_Length(dh->dh_rvmdata);
		dh->dh_vmdata = malloc(length);
		bcopy((const void *)dh->dh_rvmdata, (void *)dh->dh_vmdata, length);
	} 
	dh->dh_refcount++;
	DH_UnLockW(dh);
}
	
void DH_Dec(PDirHandle dh)
{
   int count = dh->dh_refcount-1;
   DH_LockW(dh); 

   DIR_intrans();

   RVMLIB_MODIFY(dh->dh_refcount, count);
   if ( dh->dh_refcount == 0 ) {
	   DH_Free(dh, dir_rvm());
   }
   DH_UnLockW(dh);

}

void DH_Inc(PDirHandle dh)
{
   int count = dh->dh_refcount+1;
   DH_LockW(dh); 

   DIR_intrans();

   RVMLIB_MODIFY(dh->dh_refcount, count);
   DH_UnLockW(dh);
}

void SetDirHandle(register DirHandle *dir, register Vnode *vnode)
{
    register Volume *volume = vnode->volumePtr;
    dir->inode = vnode->disk.inodeNumber;
    dir->device = (unsigned short) volume->device;
    dir->cacheCheck = volume->cacheCheck;
    dir->volume = V_parentId(volume);
    dir->unique = vnode->disk.uniquifier;
    dir->vnode = vnode->vnodeNumber;
}

void SetSalvageDirHandle(register DirHandle *dir, int volume, int device, int inode)
{
    private SalvageCacheCheck = 1;
    bzero((char *)dir, sizeof(DirHandle));
    dir->inode = inode;
    dir->device = device;
    dir->volume = volume;
    dir->cacheCheck = SalvageCacheCheck++;  /* Always re-read for a new dirhandle */
}

#endif

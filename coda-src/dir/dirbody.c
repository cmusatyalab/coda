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
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_assert.h"
#include <sys/param.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include "coda_string.h"
#include <stdlib.h>
#ifdef __BSD44__
#include <ufs/ufs/dir.h>
#undef	DIRSIZ
#else
#define DIRBLKSIZ	0x200
#endif
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rvmlib.h>
#include <ctype.h>
#include "codadir.h"
#include "dirbody.h"
#ifdef __cplusplus
}
#endif __cplusplus

#undef MMAP_DIR_CONTENTS
#if defined(HAVE_MMAP) && !defined(DJGPP)
#define MMAP_DIR_CONTENTS 1
#endif

/* are we dealing with RVM memory? (yes for Venus, no for Vice)*/
int dir_data_in_rvm;


/* definitions for static functions */
void DIR_Print(PDirHeader, FILE *f);
static int dir_FindBlobs (struct DirHeader **dh, int nblobs);
static int dir_AddPage (struct DirHeader **dir);
static int dir_NameBlobs(char *);
static struct DirEntry *dir_FindItem (struct DirHeader *dir,char *ename, 
				       struct DirEntry **preventry, int *index, int flags);
struct DirEntry *dir_GetBlob (struct DirHeader *dir, long blobno);
static void dir_FreeBlobs(struct DirHeader *dir, register int firstblob, int nblobs);
static struct DirHeader *dir_Extend(struct DirHeader *olddirh, int in_rvm);


static int fid_FidEqNFid(struct DirFid *fid, struct DirNFid *nfid);
static void fid_Fid2NFid(struct DirFid *fid, struct DirNFid *nfid);
static void fid_NFid2Fid(struct DirNFid *nfid, struct DirFid *fid); 
static void fid_NFidV2Fid(struct DirNFid *, VolumeId, struct ViceFid *);

struct DirFind {
	char            *df_ename;
	struct DirEntry *df_tp;
	struct DirEntry *df_lp;
	int             df_index;          	
};

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
	if ( DIR_rvm() && (! rvmlib_in_transaction()) ) {
		fprintf(stderr, "Aborting: no transaction in %s (%s)!\n", where, file);
		fflush(stderr);
		abort();
	}
}


static void DIR_SetRange(struct DirHeader *dir)
{
	if (DIR_rvm())
		rvmlib_set_range((void *)dir, DIR_Length(dir));
	return;
}

int DIR_rvm(void) 
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
	int failed = 0;
	int grown = 0;
	struct PageHeader *pp;
	
	if ( !dh || !*dh || nblobs <= 0 ) 
		CODA_ASSERT(0);


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
					DIR_SetRange(*dh);
				(*dh)->dirh_allomap[i] -= nblobs;
				pp->freecount -= nblobs;
				for (k=0; k<nblobs; k++)
					pp->freebitmap[(j+k)>>3] |= (1<<((j+k) % 8));
				CODA_ASSERT((j + i *EPP) > DHE);
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
		CODA_ASSERT(0);
	}


	dirh = dir_Extend(*dh, DIR_rvm() );
	
	if ( !dirh )
		return ENOMEM;
	
	if ( DIR_rvm() )
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
	CODA_ASSERT(page >= 0);
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
    CODA_ASSERT( firstblob + nblobs <= EPP );

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
	
	CODA_ASSERT(ep && vd);
        fid_NFidV2Fid(&ep->fid, vol, &fid);
	
	vd->d_fileno = coda_f2i(&fid);
#ifdef CDT_UNKNOWN
	vd->d_type = 0;
#endif
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
		CODA_ASSERT(0);
	
	if ( (ntohl(nfid->dnf_vnode) == fid->df_vnode) &&
	     (ntohl(nfid->dnf_unique) == fid->df_unique) )
		return 1;
	else 
		return 0;

}

static void fid_Fid2NFid(struct DirFid *fid, struct DirNFid *nfid) 
{
	if ( !fid || !nfid ) 
		CODA_ASSERT(0);
	
	nfid->dnf_vnode = htonl(fid->df_vnode);
	nfid->dnf_unique = htonl(fid->df_unique);

}

static void fid_NFid2Fid(struct DirNFid *nfid, struct DirFid *fid) 
{
	if ( !fid || !nfid ) 
		CODA_ASSERT(0);
	
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

	if ( DIR_rvm() ) {
		DIR_intrans();
		DIR_SetRange(*dh);
	}

	dir = *dh;

	if ( strlen(entry) > CODA_MAXNAMLEN )
		return ENAMETOOLONG;
	if ( strlen(entry) == 0 )
		return EINVAL;
    
	/* First check if file already exists. */
	ep = dir_FindItem(dir, entry, 0, 0, CLU_CASE_SENSITIVE);
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
		DIR_Print(dir, stdout);
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
    
	CODA_ASSERT( dir && entry );

	if ( DIR_rvm() ) {
		DIR_intrans();
		DIR_SetRange(dir);
	}


	firstitem = dir_FindItem(dir, entry, &preventry, &index, CLU_CASE_SENSITIVE);
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
		DIR_Print(dir, stdout);
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
		CODA_ASSERT(0);
	}

	
	dhp = dir_New(DIR_rvm());
	if ( !dhp )
		return ENOMEM;

	/* cannot call DIR_SetRange yet, since dir is unitialized */
	if ( DIR_rvm()) {
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
		DIR_Print(*dir, stdout);
	}

	return 0;
}

/* Set the number of pages in the allomap */
void DIR_Setpages(PDirHeader dirh, int pages)
{
	int i;

	CODA_ASSERT(pages >= 1);
	for(i=pages; i<DIR_MAXPAGES; i++)
		dirh->dirh_allomap[i] = EPP;
}


/* will release the directory pages from memory or RVM */
void DIR_Free(struct DirHeader *dir, int in_rvm)
{
	if ( !dir ) 
		return;

	if ( DIR_rvm() ) 
		rvmlib_rec_free(dir);
	else
		free(dir);
}


/* print one entry */
int DIR_PrintEntry(PDirEntry entry, FILE *f)
{
	struct DirFid fid;

	fid_NFid2Fid(&(entry->fid), &fid);

	fprintf(f, "next: %hu, flag %d fid: (%lx.%lx) %s\n",
		ntohs(entry->next), entry->flag,
		fid.df_vnode, fid.df_unique, entry->name);


	return 0;
}

void DIR_PrintChain(PDirHeader dir, int chain, FILE *f)
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
		fprintf(f, "thisblob: %hu ", blob);
		DIR_PrintEntry(de, f);
		blob = ntohs(de->next);
	}
	return ;
}

/* print the header and then the entries */
void DIR_Print(PDirHeader dir, FILE *f)
{
	int i, num;
	int setbits;
	int freecount;
	char allo;
	struct PageHeader *ph;
	char bitmap[EPP+1];

	fprintf(f, "DIR: %p,  LENGTH: %d\n", dir, DIR_Length(dir));

	fprintf(f, "\nHASH TABLE:\n");
	for ( i = 0; i < NHASH ; i++ ) {
		num = ntohs(dir->dirh_hashTable[i]);
		if ( num ) 
			fprintf(f, "(%d %hd) ", i, num);
	}

	fprintf(f, "\n\nALLOMAP:\n");
	for ( i = 0; i < DIR_MAXPAGES ; i++ ) {
		allo = dir->dirh_allomap[i];
		if ( allo != EPP )
			fprintf(f, "(%d %i) ", i, allo);
	}

	fprintf(f, "\n\nPAGEHEADERS:\n");
	for ( i = 0; i < DIR_MAXPAGES ; i++ ) {
		if ( dir->dirh_allomap[i] != EPP ) {
			ph = DIR_Page(dir, i);
			setbits = dir_PrintChar(ph->freebitmap, EPP, bitmap);
			bitmap[EPP] = '\0';
			freecount = ph->freecount;
			fprintf(f, "page %d, tag %d, freecount %d, set %d, bitmap: \n",
				i, (int)ntohl(ph->tag), freecount, setbits);
			fprintf(f, "%s\n\n", bitmap);
		}
	}
			
	fprintf(f, "\nCHAINS:\n");
	for ( i = 0; i < NHASH ; i++ ) {
		num = ntohs(dir->dirh_hashTable[i]);
		if ( num ) {
			printf("Chain: %d\n", i);
			DIR_PrintChain(dir, i, f);
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
int DIR_Lookup (struct DirHeader *dir, char *entry, struct DirFid *fid, int flags)
{
	register struct DirEntry *de;

	if ( !dir ) 
		return ENOENT;

	de = dir_FindItem(dir, entry, 0, 0, flags);
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
	rc = DIR_Lookup(dh, de->name, &dfid, CLU_CASE_SENSITIVE);
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
	int oldoffset = 0;
	int direntlen;

#ifndef MMAP_DIR_CONTENTS
	int rc;
#endif

	if ( !dir ) 
		return ENOENT;
	
	fd = open(file, O_RDWR | O_TRUNC | O_BINARY, 0600);
	CODA_ASSERT( fd >= 0 );

	len = DIR_Length(dir);
#ifdef NOTE
	/* Because of a poor bsd libc readdir(), directory entries
	   cannot span a dir block boundary.  Actually, the kernel
	   also writes directories this way.  Because we can't know until
	   we write out the directory how much we will have to pad
	   things, just assume that we will have to pad one 
	   maximum-sized directory entry for each chunk of
	   directory we write.
	 */
#endif /* __BSD44__ */
	len += (len / DIRBLKSIZ + 1) * ((sizeof(struct venus_dirent) + 3) & ~3);
	len = ((len + (DIRBLKSIZ - 1)) & ~(DIRBLKSIZ - 1));

#ifdef MMAP_DIR_CONTENTS
	CODA_ASSERT( ftruncate(fd, len) == 0 );
	buf = mmap(0, len, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	CODA_ASSERT( (int) buf != -1 );
#else
	buf = malloc(len);
	CODA_ASSERT(buf);
#endif

	bzero(buf, len);

	for (i=0; i<NHASH; i++) {
		num = ntohs(dir->dirh_hashTable[i]);
		while (num != 0) {
			ep = dir_GetBlob(dir, num);
			if (!ep) 
				break;

			/* optimistically write the directory entry: */
			direntlen = dir_DirEntry2VDirent(ep, (struct venus_dirent *) (buf + offset), vol);
			/* if what we just wrote crosses a boundary: */
			if (((offset + direntlen) ^ offset) & ~(DIRBLKSIZ - 1)) {
				/* Note that offset still points to the *previous* directory 
				   entry.  Calculate how much we have to add to its d_reclen 
				   and offset for the padding: */
				int pad = ((offset + (DIRBLKSIZ - 1)) & 
						~(DIRBLKSIZ - 1)) - offset;

				((struct venus_dirent *) (buf + oldoffset))->d_reclen += pad;
				offset += pad;
				/* do it again ... shifted by pad */
				direntlen = dir_DirEntry2VDirent(ep, (struct venus_dirent *) (buf + offset), vol);
			}
			oldoffset = offset;
			offset += direntlen;
			num = ntohs(ep->next);
		}
	}
	/* add a final entry */
	vd = (struct venus_dirent *)(buf + offset);
	vd->d_fileno = 0;
	vd->d_reclen = ((offset + (DIRBLKSIZ - 1)) & ~(DIRBLKSIZ - 1)) - offset;
#define minEntry 12 /* XXX */
	if (vd->d_reclen < minEntry) 
		((struct venus_dirent *) (buf + oldoffset))->d_reclen += vd->d_reclen;
	offset += vd->d_reclen;

#ifdef MMAP_DIR_CONTENTS
	CODA_ASSERT(munmap(buf, len) == 0);
	CODA_ASSERT( ftruncate(fd, offset) == 0 );
#else 
	rc  = write(fd, buf, offset);
	free(buf);
	CODA_ASSERT(rc == offset);
#endif
	CODA_ASSERT(close(fd) == 0);
	return 0;
}

/* Enumerate the contents of a directory: hook is called
   with the direntry in NETWORK order */
int DIR_EnumerateDir(struct DirHeader *dhp, 
		     int (*hookproc)(struct DirEntry *de, void *hook) , void *hook)
{
	int i;
	int num;
	int rc = 0;
	struct DirEntry *ep;

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
		CODA_ASSERT(*new);
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

/* an EnumerateDir hook to find a DirEntry with case-insensitive match.*/
/* BUG: index is always set to 0. smarc */
int dir_FindCaseInsensitive(PDirEntry de, void *hook){
	struct DirFind *find = (struct DirFind *) hook;
	char           *p = NULL;
	char           *name = strdup(find->df_ename);
	int            length = strlen(name);
	char           *name2;
	int            length2;
	int            rc = 0;

      	/* lowercase name to look for */
	for (p = name; p < name + length; p++)
		if (isupper(*p))
	       		*p += 'a' - 'A';

	name2 = strdup(de->name);
       	length2 = strlen(name2);

      	/* lowercase name2 */       
	for (p = name2; p < name2 + length2; p++)
      	       	if (isupper(*p))
			*p += 'a' - 'A';

	if (!strcmp(name, name2)){
		find->df_tp = de;
		find->df_index = 0; /* BUG no way to find out about index here. smarc */
		strcpy(find->df_ename, de->name);
		rc = 1; /* indicate EnumerateDir to stop */
	}
	else 
		find->df_lp = de;
		
	if (name) free(name);
	if (name2) free(name2);

	return rc;
}

/* Find a directory entry, given its name.  This entry returns a
   pointer to DirEntry, and a pointer to the previous DirEntry (to aid
   the delete code) in the hash chain.  If no entry is found a null
   pointer is returned instead. 

BUG: index doesn't work with CLU_CASE_INSENSITIVE so far. smarc
*/
static struct DirEntry *dir_FindItem (struct DirHeader *dir, char *ename, 
				       struct DirEntry **preventry, int *index, int flags)
{
	int rc = 0;
	register int i;	
	register short blobno;
	register struct DirEntry *tp;
	register struct DirEntry *lp = NULL;	/* page of previous entry in chain */
	struct DirFind find;

	switch (flags) {
	case CLU_CASE_INSENSITIVE:
		if (!dir) 
			return 0;

		find.df_ename = ename;
		find.df_lp = NULL;
		find.df_tp = NULL;
		rc = DIR_EnumerateDir(dir, dir_FindCaseInsensitive, (void *) &find);
		if (rc){
			if (preventry)
		          	*preventry = find.df_lp;
		       	if (index)
		       		*index = find.df_index;	
			strcpy(ename, find.df_tp->name);	       							
			return find.df_tp;			
		}		       			

		return 0;
		break;

	case CLU_CASE_SENSITIVE:
	
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
		break;
	default:
		fprintf(stdout, "!!! You might have an unsupported Coda kernel module. Please update coda.o !!!\n");
		CODA_ASSERT(0);
	}

	return 0;
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
				printf("Dir entry %p in chain %d name too long: %s\n",
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




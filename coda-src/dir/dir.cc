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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/dir/RCS/dir.cc,v 4.1 1997/01/08 21:49:32 rvb Exp $";
#endif /*_BLURB_*/



/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#ifdef __MACH__
#include <libc.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__BSD44__)
#include <stdlib.h>
#endif /* __BSD44__ */

#ifdef __cplusplus
}
#endif __cplusplus

#include "coda_dir.h"
#include "dir.private.h"

/* definitions for PRIVATE functions */
PRIVATE int FindBlobs (long *dir, int nblobs);
PRIVATE void AddPage (long *dir, int pageno);
PRIVATE void FreeBlobs(long *dir, register int firstblob, int nblobs);
PRIVATE struct DirEntry *FindItem (long *dir,char *ename, struct PageHeader **prevpage, int *prevoffset);


int NameBlobs (char *name)
    {/* Find out how many entries are required to store a name. */
    register int i;
    i = strlen(name)+1;
    return 1+((i+15)>>LESZ);
    }

int Create (long *dir, char *entry, long *vfid){
/* Create an entry in a file.  Dir is a file representation, while entry is a string name. */
    int blobs, firstelt;
    register int i;
    register struct DirEntry *ep;
    register struct DirHeader *dhp;
    /* First check if file already exists. */
    ep = FindItem(dir,entry,0,0);
    if (ep)
        {DRelease((buffer *)ep, 0);
        return EEXIST;
        }
    blobs = NameBlobs(entry);	/* number of entries required */
    firstelt = FindBlobs(dir,blobs);
    if (firstelt < 0) return EFBIG;	/* directory is full */
    /* First, we fill in the directory entry.
     */ep = GetBlob(dir,firstelt);
    if (ep == 0) return EIO;
    ep->flag = FFIRST;
    ep->fid.mkvnode = htonl(vfid[1]);
    ep->fid.mkvunique = htonl(vfid[2]);
    strcpy(ep->name,entry);
    /* Now we just have to thread it on the hash table list.     */
    dhp = (struct DirHeader *) DRead(dir,0);
    if (!dhp) {DRelease((buffer *)DEHTOPH(ep,firstelt),1); return EIO;}
    i = DirHash(entry);
    ep->next = dhp->hashTable[i];
    dhp->hashTable[i] = htons(firstelt);
    DRelease((buffer *)dhp,1);
    DRelease((buffer *)DEHTOPH(ep,firstelt),1);
    return 0;
    }

int Length (long *dir){
    int i,ctr;
    struct DirHeader *dhp;
    dhp = (struct DirHeader *) DRead(dir,0);
    if (!dhp) return 0;
    ctr=0;
    for(i=0;i<MAXPAGES;i++)
        if (dhp->alloMap[i] != EPP) ctr++;
    DRelease((buffer *)dhp,0);
    return ctr*PAGESIZE;
    }

int Delete (long *dir, char *entry){
     /* Delete an entry from a directory, including update of all */
     /*	free entry descriptors. */
    int nitems, index;
    register struct DirEntry *firstitem;
    struct PageHeader *prevpage;
    int prevoffset;
    short *previtem;
    firstitem = FindItem(dir,entry,&prevpage,&prevoffset);
    if (firstitem == 0) return ENOENT;
    previtem = (short *)&(((char *)prevpage)[prevoffset]);
    index = ntohs(*previtem);
    *previtem = firstitem->next;
    DRelease((buffer *)prevpage,1);
    nitems = NameBlobs(firstitem->name);
    DRelease((buffer *)firstitem,0);
    FreeBlobs(dir,index,nitems);
    return 0;
    }

PRIVATE int FindBlobs (long *dir, int nblobs){
     /* Find a bunch of contiguous entries; at least nblobs in a row.
     */register int i, j, k;
    int failed;
    register struct DirHeader *dhp;
    struct PageHeader *pp;
    dhp = (struct DirHeader *) DRead(dir,0);	/* read the dir header in first. */
    if (!dhp) return -1;
    for(i=0;i<MAXPAGES;i++)
        {if (dhp->alloMap[i] >= nblobs)	/* if page could contain enough entries */
            {/* If there are EPP free entries, then the page is not even allocated. */
            if (dhp->alloMap[i] == EPP)
                {/* Add the page to the directory. */
                AddPage(dir,i);
                dhp->alloMap[i] = EPP-1;
                }
            pp = (struct PageHeader *) DRead(dir,i);  /* read the page in. */
            if (!pp) {DRelease((buffer *)dhp, 1); break;}
            for(j=0;j<=EPP-nblobs;j++)
                {failed = 0;
                for(k=0;k<nblobs;k++)
                    if ((pp->freebitmap[(j+k)>>3]>>((j+k)&7)) & 1)
                        {failed = 1;
                        break;
                        }
                if (!failed) break;
                failed = 1;
                }
            if (!failed)
                {/* Here we have the first index in j.  We clean up the allocation maps and free up any resources we've got allocated. */
                dhp->alloMap[i] -= nblobs;
                DRelease((buffer *)dhp,1);
                for (k=0;k<nblobs;k++)
                    pp->freebitmap[(j+k)>>3] |= 1<<((j+k)&7);
                DRelease((buffer *)pp,1);
                return j+i*EPP;
                }
            DRelease((buffer *)pp, 0);	/* This dir page is unchanged. */
            }
        }
    /* If we make it here, the directory is full. */
    return -1;
    }

PRIVATE void AddPage (long *dir, int pageno){
     /* Add a page to a directory.
     */register int i;
    register struct PageHeader *pp;
    if (pageno==0) Die("bogus call to addpage");
    pp = (struct PageHeader *) DNew(dir,pageno);	/* Get a new buffer labelled dir,pageno */
    pp->tag = htonl(1234);
    pp->freecount = EPP-1;	/* The first dude is already allocated */
    pp->freebitmap[0] = 0x01;
    for (i=1;i<EPP/8;i++)	/* It's a constant */
        pp->freebitmap[i] = 0;
    DRelease((buffer *)pp,1);
    }

PRIVATE void FreeBlobs(long *dir, register int firstblob, int nblobs){
     /* Free a whole bunch of directory entries.
     */register int i;
    int page;
    struct DirHeader *dhp;
    struct PageHeader *pp;
    page = firstblob>>LEPP;
    firstblob -= EPP*page;	/* convert to page-relative entry */
    dhp = (struct DirHeader *) DRead(dir,0);
    if (!dhp) return;
    dhp->alloMap[page] += nblobs;
    DRelease((buffer *)dhp,1);
    pp = (struct PageHeader *) DRead(dir,page);
    if (pp) for (i=0;i<nblobs;i++)
        pp->freebitmap[(firstblob+i)>>3] &= ~(1<<((firstblob+i)&7));
    DRelease((buffer *)pp,1);
    }

int MakeDir (long *dir,long *me, long *parent){
     /* Format an empty directory properly.  Note that the first 13 entries in a directory header page are allocated, 1 to the page header, 4 to the allocation map and 8 to the hash table. */
    register int i;
    register struct DirHeader *dhp;
    dhp = (struct DirHeader *) DNew(dir,0);
    dhp->header.tag = htonl(1234);
    dhp->header.freecount = (EPP-DHE-1);
    dhp->header.freebitmap[0] = 0xff;
    dhp->header.freebitmap[1] = 0x1f;
    for(i=2;i<EPP/8;i++) dhp->header.freebitmap[i] = 0;
    dhp->alloMap[0]=(EPP-DHE-1);
    for(i=1;i<MAXPAGES;i++)dhp->alloMap[i] = EPP;
    for(i=0;i<NHASH;i++)dhp->hashTable[i] = 0;
    DRelease((buffer *)dhp,1);
    Create(dir,".",me);
    Create(dir,"..",parent);	/* Virtue is its own .. */
    return 0;
    }

int Lookup (long *dir, char *entry, register long *fid){
    /* Look up a file name in directory. */
    register struct DirEntry *firstitem;
    firstitem = FindItem(dir,entry,0,0);
    if (firstitem == 0) return ENOENT;
    fid[1] = ntohl(firstitem->fid.mkvnode);
    fid[2] = ntohl(firstitem->fid.mkvunique);
    DRelease((buffer *)firstitem,0);
    return 0;
    }

int EnumerateDir (long *dir, int (*hookproc)(void *par1,...), long hook){
     /* Enumerate the contents of a directory. */
    int i;
    int num;
    register struct DirHeader *dhp;
    register struct DirEntry *ep;
    
    dhp = (struct DirHeader *)DRead(dir,0);
    if (dhp) for(i=0;i<NHASH;i++)
        {/* For each hash chain, enumerate everyone on the list. */
        num = ntohs(dhp->hashTable[i]);
        while (num != 0)
            {/* Walk down the hash table list. */
            ep = GetBlob(dir,num);
            if (!ep) break;
            num = ntohs(ep->next);
            (*hookproc) ((void *)hook, ep->name, ntohl(ep->fid.mkvnode), ntohl(ep->fid.mkvunique));
            DRelease((buffer *)ep,0);
            }
        }
    DRelease((buffer *)dhp,0);
    return 0;
    }

/* Given fid get the name if it exists in the dir 
 * 	If more than 1 name exists for the same object, return the first one 
 */
char *FindName(long *dir, long vnode, long unique, char *buf) {
    int i;
    int entryfound = 0;
    struct DirHeader *dhp;
    dhp = (struct DirHeader *)DRead(dir, 0);
    if (dhp) {
	for (i = 0; i < NHASH && !entryfound; i++) {
	    int num = ntohs(dhp->hashTable[i]);
	    while (num != 0) {
		struct DirEntry *ep;
		ep = GetBlob(dir, num);
		if (!ep) break;
		num = ntohs(ep->next);

		if (ntohl(ep->fid.mkvunique) == unique && 
		    ntohl(ep->fid.mkvnode) == vnode) {
		    entryfound = 1;
		    strcpy(buf, ep->name);
		    DRelease((buffer *)ep, 0);
		    break;
		}
		DRelease((buffer *)ep, 0);
	    }
	}
    }
    DRelease((buffer *)dhp, 0);
    
    if (entryfound) return(buf);
    else return(NULL);
}
/* DirToNetBuf:
   Dump sorted DirContents into a buffer in network order
   return 0 if no errors occur; -1 otherwise
 */
typedef struct SDE {
    long vnode;
    long unique;
    char *name;
}SDE;

PRIVATE int cmpsde(SDE **a, SDE **b) {
    if ((*a)->vnode < (*b)->vnode) return(-1);
    if ((*a)->vnode > (*b)->vnode) return(1);
    if ((*a)->unique < (*b)->unique) return(-1);
    if ((*a)->unique > (*b)->unique) return(1);
    return(strcmp((*a)->name, (*b)->name));
}
int DirToNetBuf(long *dir, char *sortedbuf, int maxsize, int *size) {
    int errorcode = 0;
    int sortedindex = 0;
    int i = 0;
    /* initialize the buffer of pointers for qsorting */
    int maxentries = maxsize / (2 * sizeof(long));	/* conservative maxentries */
    SDE *sde = (SDE *)malloc(maxentries * sizeof(SDE));
    SDE **sdep = (SDE **)malloc(maxentries * sizeof(SDE *));
    char *namebuf = (char *)malloc(maxsize);
    int cindex = 0;
    int nentries = 0;

    /* get all the entries into the buffers */
    struct DirHeader *dhp;

    *size = 0;

    for (i = 0; i < maxentries; i++) {
	sde[i].vnode = 0;
	sde[i].unique = 0;
	sde[i].name = 0;
	sdep[i] = 0;
    }

    dhp = (struct DirHeader *)DRead(dir, 0);
    if (dhp) {
	for (i = 0; i < NHASH; i++) {
	    int num = ntohs(dhp->hashTable[i]);
	    while (num != 0) {
		struct DirEntry *ep;
		ep = GetBlob(dir, num);
		if (!ep) break;
		num = ntohs(ep->next);
		
		/* copy the direntry */
		{
		    if ((cindex + strlen(ep->name) + 1) > maxsize) {
			errorcode = 1;
			goto cleanup;
		    }
		    sde[nentries].vnode = ep->fid.mkvnode;
		    sde[nentries].unique = ep->fid.mkvunique;
		    sde[nentries].name = &namebuf[cindex];
		    strcpy(&namebuf[cindex], ep->name);
		    cindex += strlen(ep->name) + 1;
		    sdep[nentries] = &sde[nentries];
		    nentries++;
		}
		
		DRelease((buffer *)ep, 0);
	    }
	}
    }
    DRelease((buffer *)dhp, 0);

    /* sort the entries */
    qsort(sdep, nentries, sizeof(SDE *), 
	  (int (*)(const void *, const void *)) cmpsde);
    
    /* copy sorted entries into buffer to be returned */
    sortedindex = 0;
    for (i = 0; i < nentries; i++) {
	int index = strlen(sdep[i]->name) + 1 + sortedindex +(2 * sizeof(long));
	if (index > maxsize) {
	    errorcode = 1;
	    goto cleanup;
	}
	bcopy(&(sdep[i]->vnode), &sortedbuf[sortedindex], sizeof(long));
	sortedindex += sizeof(long);
	bcopy(&(sdep[i]->unique), &sortedbuf[sortedindex], sizeof(long));
	sortedindex += sizeof(long);
	strcpy(&sortedbuf[sortedindex], sdep[i]->name);
	sortedindex += strlen(sdep[i]->name) + 1;
    }
    *size = sortedindex;

  cleanup:
    free((char *)sde);
    free((char *)sdep);
    free(namebuf);
    if (errorcode) return(-1);
    return(0);
}

/* IsEmpty returns 0 when directory is empty
   and 1 if directory is nonempty */
int IsEmpty (long *dir) {
    /* Enumerate the contents of a directory. */
    register int i;
    int num;
    int empty = 1;
    register struct DirHeader *dhp;
    register struct DirEntry *ep;
    dhp = (struct DirHeader *) DRead(dir,0);
    if (!dhp) return 0;
    for(i=0;i<NHASH && empty;i++) {
	/* For each hash chain, enumerate everyone on the list. */
        num = ntohs(dhp->hashTable[i]);
        while (num != 0) {
            /* Walk down the hash table list. */
            ep = GetBlob(dir,num);
            if (!ep) break;
            if (strcmp(ep->name,"..") && strcmp(ep->name,".")) {
		empty = 0;
		DRelease((buffer *)ep, 0);
		break;
	    }
            num = ntohs(ep->next);
            DRelease((buffer *)ep,0);
	}
    }
    DRelease((buffer *)dhp,0);
    return (!empty);
}

struct DirEntry *GetBlob (long *dir, long blobno){
    /* Return a pointer to an entry, given its number. */
    struct PageHeader *pp;
#ifdef	__linux__
    pp=(struct PageHeader *)DRead(dir,blobno>>LEPP);
#else
    pp=(PageHeader *)DRead(dir,blobno>>LEPP);
#endif
    if (!pp) return 0;
    return PHTODEH(pp, blobno);
    }

int DirHash (char *string){
    /* Hash a string to a number between 0 and NHASH. */
    register char tc;
    register int hval;
    register int tval;
    hval = 0;
    while(tc=(*string++))
        {hval *= 173;
        hval  += tc;
        }
    tval = hval & (NHASH-1);
    if (tval == 0) return tval;
    else if (hval < 0) tval = NHASH-tval;
    return tval;
    }

PRIVATE struct DirEntry *FindItem (long *dir,char *ename, struct PageHeader **prevpage, int *prevoffset){
    /* Find a directory entry, given its name.  This entry returns a pointer to a locked buffer, and a pointer to a locked buffer (in <prevpage,prevoffset>) referencing the found item (to aid the delete code).  If no entry is found, however, no items are left locked, and a null pointer is returned instead. */
    register int i;
    register struct DirHeader *dhp;
    register struct PageHeader *lp;	/* page of last entry in chain */
    register int lo;			/* offset in lp of that entry */
    register short blobno;
    register struct DirEntry *tp;
    i = DirHash(ename);
    dhp = (struct DirHeader *) DRead(dir,0);
    if (!dhp) return 0;
    blobno = ntohs(dhp->hashTable[i]);
    if (blobno == 0)
        {/* no such entry */
        DRelease((buffer *)dhp,0);
        return 0;
        }
    tp = GetBlob(dir,blobno);
    if (!tp) {DRelease((buffer *)dhp, 0); return 0;}
    lp = (struct PageHeader *)dhp;
    lo = ((char *)&(dhp->hashTable[i]) - (char *)lp);
    while(1)
        {/* Look at each hash conflict entry. */
        if (!strcmp(ename,tp->name))
            {/* Found our entry. */
	    if (prevpage) { *prevpage = lp; *prevoffset = lo; }
	    else DRelease((buffer *)lp,0);
            return tp;
            }
	DRelease((buffer *)lp,0);
	lp = DEHTOPH(tp, blobno);
	lo = ((char *)&(tp->next) - (char *)lp);
	blobno = ntohs(tp->next);
        if (blobno == 0)
            {/* The end of the line */
            DRelease((buffer *)lp,0);	/* Release all locks. */
            return 0;
            }
        tp = GetBlob(dir,blobno);
        if (!tp) {DRelease((buffer *)lp, 0); return 0;}
        }
    }

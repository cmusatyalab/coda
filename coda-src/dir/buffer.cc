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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/dir/buffer.cc,v 1.2 1997/01/07 18:40:34 rvb Exp";
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
#ifdef __MACH__
#include <libc.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__NetBSD__)
#include <stdlib.h>
#endif __NetBSD__
#ifdef __cplusplus
}
#endif __cplusplus


#include "coda_dir.h"
#include "dir.private.h"

/* page hash table size */
#define PHSIZE 32
/* the pHash macro */
#define pHash(fid) ((fid)[0] & (PHSIZE-1))

struct buffer *Buffers;
char *BufferData;

static struct buffer *phTable[PHSIZE];	/* page hash table */
static struct buffer *LastBuffer;
int nbuffers;
int timecounter;
static int calls=0, ios=0;
/* definition of routines */
PRIVATE void FixupBucket(register struct buffer *ap);
PRIVATE struct buffer *newslot (long *afid, long apage);

void DStat (int *abuffers, int *acalls, int *aios){
    *abuffers = nbuffers;
    *acalls = calls;
    *aios = ios;
    }

int DInit (int abuffers){
    /* Initialize the venus buffer system. */
    register int i;
    register struct buffer *tb;
    Buffers = (struct buffer *) malloc(abuffers * sizeof(struct buffer));
    BufferData = (char *) malloc(abuffers * PAGESIZE);
    timecounter = 0;
    LastBuffer = Buffers;
    nbuffers = abuffers;
    for(i=0;i<PHSIZE;i++) phTable[i] = 0;
    for (i=0;i<abuffers;i++)
        {/* Fill in each buffer with an empty indication. */
	tb = &Buffers[i];
        FidZap(tb->fid);
        tb->accesstime = tb->lockers = 0;
        tb->data = &BufferData[PAGESIZE*i];
	tb->hashIndex = 0;
        tb->dirty = 0;
        }
    return 0;
    }

char *DRead(register long *fid, register int page){
    /* Read a page from the disk. */
    register struct buffer *tb;
    calls++;
    if (LastBuffer->page == page && FidEq(LastBuffer->fid, fid))
	{tb = LastBuffer;
	tb->accesstime = ++timecounter;
	tb->lockers++;
	return tb->data;
	}
    for(tb=phTable[pHash(fid)]; tb; tb=tb->hashNext)
	{if (tb->page == page && FidEq(tb->fid, fid))
	    {tb->lockers++;
	    tb->accesstime = ++timecounter;
	    LastBuffer = tb;
	    return tb->data;
	    }
	}
    /* can't find it */
    tb = newslot(fid, page);
    tb->lockers++;
    if (!ReallyRead(fid,tb->page,tb->data))
        {FidZap(tb->fid);	/* disaster */
	tb->lockers--;
        return 0;
        }
    ios++;
    /* Note that findslot sets the page field in the buffer equal to what it is searching for. */
    return tb->data;
    }

PRIVATE void FixupBucket(register struct buffer *ap){
    register struct buffer **lp, *tp;
    register int i;
    /* first try to get it out of its current hash bucket, in which it might not be */
    i = ap->hashIndex;
    lp = &phTable[i];
    for(tp = *lp; tp; tp=tp->hashNext)
	{if (tp == ap)
	    {*lp = tp->hashNext;
	    break;
	    }
	lp = &tp->hashNext;
	}
    /* now figure the new hash bucket */
    i = pHash(ap->fid);
    ap->hashIndex = i;		/* remember where we are for deletion */
    ap->hashNext = phTable[i];	/* add us to the list */
    phTable[i] = ap;
    }

PRIVATE struct buffer *newslot (long *afid, long apage){
    /* Find a usable buffer slot */
    register long i;
    long lt,pt;
    register struct buffer *lp, *pp, *tp;

    lp = 0;		/* last non-pure */
    pp = 0;		/* last pure */
    lt = 999999999;
    pt = 999999999;
    tp = Buffers;
    for (i=0;i<nbuffers;i++,tp++)
	{if (tp->lockers == 0)
	    {if (tp->dirty)
		{if (tp->accesstime < lt)
		    {lp = tp;
		    lt = tp->accesstime;
		    }
		}
	    else if (tp->accesstime < pt)
		{pp = tp;
		pt = tp->accesstime;
		}
	    }
	}
    /* If we make it here, the buffer is not in memory.  Find an already-used buffer and trash it.
	If the buffer is dirty, try not to use it.  If it must be used, don't forget to write it out first. */

    if (pp == 0)
        {/* There are no unlocked buffers that don't need to be written to the disk.
	    The variable lx gives the index of the buffer to write out to the disk. */
        if (lp == 0) Die ("all buffers locked");
        if (!ReallyWrite(lp->fid,lp->page,lp->data)) Die("writing bogus buffer");
        lp->dirty = 0;
        pp = lp;		/* The buffer to use from now on. */
        }

    /* Now fill in the header. */
    FidCpy(pp->fid, afid);	/* set this */
    pp->page = apage;
    pp->accesstime = ++timecounter;

    FixupBucket(pp);		/* move to the right hash bucket */

    LastBuffer = pp;
    return pp;
    }

void DRelease (register struct buffer *bp, int flag){
    /* Release a buffer, specifying whether or not the buffer has been modified by the locker. */
    register int index;

    if (!bp) return;
    index = (((char *)bp)-((char *)BufferData))>>LOGPS;
    bp = &(Buffers[index]);
    bp->lockers--;
    if (flag) bp->dirty=1;
    }

int DVOffset (register struct buffer *ap){
    /* Return the byte within a file represented by a buffer pointer. */
    register struct buffer *bp;
    register int index;
    bp=ap;
    index = (((char *)bp) - ((char *)BufferData)) >> LOGPS;
    if (index<0 || index >= nbuffers) return -1;
    bp = &(Buffers[index]);
    return PAGESIZE*bp->page+((char *)ap)-bp->data;
    }

void DZap (register long *fid){
    /* Destroy all buffers pertaining to a particular fid. */
    register struct buffer *tb;
    for(tb=phTable[pHash(fid)]; tb; tb=tb->hashNext)
        if (FidEq(tb->fid,fid))
            {FidZap(tb->fid);
            tb->dirty = 0;
            }
    }

void DFlushEntry (register long *fid){
    /* Flush pages modified by one entry. */
    register struct buffer *tb;
    for(tb = phTable[pHash(fid)]; tb; tb=tb->hashNext)
        if (tb->dirty && FidEq(tb->fid, fid))
            {if (ReallyWrite(tb->fid, tb->page, tb->data)) tb->dirty = 0;
            }
    }

void DFlush (){
    /* Flush all the modified buffers. */
    register int i;
    register struct buffer *tb;
    tb = Buffers;
    for(i=0;i<nbuffers;i++,tb++)
        {if (tb->dirty && ReallyWrite(tb->fid, tb->page, tb->data))
            tb->dirty = 0;	/* Clear the dirty flag */
        }
    }

char *DNew (register long *fid, register int page){
    /* Same as read, only do *not* even try to read the page, since it probably doesn't exist. */
    register struct buffer *tb;
    if ((tb = newslot(fid,page)) == 0) return 0;
    tb->lockers++;
    return tb->data;
    }


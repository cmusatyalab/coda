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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
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



/* This is the directory salvager.  It consists of two routines.  The first, DirOK, checks to see if the directory looks good.  If the directory does NOT look good, the approved procedure is to then call Salvage, which copies all the good entries from the damaged dir into a new directory. */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef __MACH__
#include <libc.h>
#endif __MACH__
#ifdef __NetBSD__
#include <stdlib.h>
#endif __NetBSD__

#ifdef __cplusplus
}
#endif __cplusplus


#include "dir.h"
#include "dir.private.h"

/* This routine is called with one parameter, the id (the same thing that is passed to physio or the buffer package) of a directory to check.  It returns 1 if the directory looks good, and 0 otherwise. */


/* int DirOK (File *file){  changed *file to file */
int DirOK (long *file){
    struct DirHeader *dhp;
    struct PageHeader *pp;
    struct DirEntry *ep;
    register int i, j, k;
    int maxEntry, usedPages, count, entry;
    char eaMap[MAXPAGES*EPP/8];	/* Change eaSize initialization below, too. */
    int eaSize;

    eaSize = MAXPAGES*EPP/8;

    /* Check magic number for first page, and initialize dhp. */
    dhp = (struct DirHeader *) DRead (file, 0);
    if (!dhp)
        {printf("First page in directory does not exist.\n");
        DRelease((buffer *)dhp, 0);
        return 0;
        }
    if (dhp->header.tag != htonl(1234))
        {printf("Bad first pageheader magic number.\n");
        DRelease((buffer *)dhp, 0);
        return 0;
        }

    /* Ensure directory is contiguous, by checking dhp->alloMap.  Once the first dude is found with EPP free entries, the rest should match.  Check that alloMap entries all look in range.  Compute the largest numbered acceptable entry number as 256K / 32. */
    for(i=0; i<MAXPAGES; i++)
        {j = dhp->alloMap[i];
        if (j<0 || j > EPP)
            {printf("The dir header alloc map for page %d is bad.\n", i);
            DRelease((buffer *)dhp, 0);
            return 0;
            }
        }

    usedPages = 0;
    for(i=0; i<MAXPAGES; i++)
        {if (dhp->alloMap[i] == EPP)
            {usedPages = i;
            break;
            }
        }
    if (usedPages == 0) usedPages = MAXPAGES;
    maxEntry = usedPages << LEPP;

    for(i=usedPages; i<MAXPAGES; i++)
        {if (dhp->alloMap[i] != EPP)
            {printf("A partially-full page occurs in slot %d, after the dir end.\n", i);
            DRelease((buffer *)dhp, 0);
            return 0;
            }
        }

    /* For all pages, initialize new allocation map with the proper header entries used.  The first page, as it contains the dir header, uses more room than the others.  Also, check that alloMap has the right count for each page.  Also check the magic number in each page header. */

    /* First initialize the allocation map. */
    for(i=0; i<eaSize; i++) eaMap[i] = 0;

    for(i=0;i<usedPages;i++)
        {pp = (struct PageHeader *) DRead(file, i);
        if (!pp)
            {printf("Directory shorter than alloMap indicates (page %d)\n", i);
            DRelease((buffer *)dhp, 0);
            return 0;
            }
        if (pp->tag != htonl(1234))
            {DRelease((buffer *)pp, 0);
            printf("Directory page %d has a bad magic number.\n", i);
            DRelease((buffer *)dhp, 0);
            return 0;
            }
        if (i==0)
            {eaMap[0] = 0xff;	/* These two lines assume DHE==12. */
            eaMap[1] = 0x1f;
            }
        else eaMap[i*(EPP/8)] = 0x01;
        count = 0;
        for(j=0;j<EPP/8;j++)
            {k = pp->freebitmap[j];
            if (k & 0x80) count++;
            if (k & 0x40) count++;
            if (k & 0x20) count++;
            if (k & 0x10) count++;
            if (k & 0x08) count++;
            if (k & 0x04) count++;
            if (k & 0x02) count++;
            if (k & 0x01) count++;
            }
        count = EPP - count;
        if ((count & 0xff) != (dhp->alloMap[i] & 0xff))
            {DRelease((buffer *)pp, 0);
            printf("Header allocation map doesn't match page header for page %d.\n", i);
            DRelease((buffer *)dhp, 0);
            return 0;
            }
        DRelease((buffer *)pp, 0);
        }
     
    /* Walk down all the hash lists, ensuring that each flag field has FFIRST in it.  Mark the appropriate bits in the new allocation map.  Check that the name is in the right hash bucket. */
    for(i=0; i<NHASH; i++)
        {entry = ntohs(dhp->hashTable[i]);
        while(1)
            {if (!entry) break;
            if (entry < 0 || entry >= usedPages*EPP)
                {printf("Out-of-range hash id %d in chain %d.\n", entry, i);
                DRelease((buffer *)dhp, 0);
                return 0;
                }
            ep = GetBlob(file, entry);
            if (!ep)
                {printf("Invalid hash id %d in chain %d.\n", entry, i);
                DRelease((buffer *)dhp, 0);
                return 0;
                }
            if (ep->flag != FFIRST)
                {printf("Dir entry %x in chain %d has bogus flag field.\n", ep, i);
                DRelease((buffer *)dhp, 0);
                DRelease((buffer *)ep, 0);
                return 0;
                }
            j = strlen(ep->name);
            if (j>256)
                {printf("Dir entry %x in chain %d has too-long name.\n", ep, i);
                DRelease((buffer *)ep, 0);
                DRelease((buffer *)dhp, 0);
                return 0;
                }
            k = NameBlobs(ep->name);
            for(j=0; j<k; j++)
                eaMap[(entry+j)>>3] |= (1<<((entry+j)&7));
            if ((j=DirHash(ep->name)) != i)
                {printf("Dir entry %x should be in hash bucket %d but IS in %d.\n", ep, j, i);
                DRelease((buffer *)dhp, 0);
                DRelease((buffer *)ep, 0);
                return 0;
                }
            entry = ntohs(ep->next);
            DRelease((buffer *)ep, 0);
            }
        }

    /* Now the new allocation map has been computed.  Check that it matches the old one.  Note that if this matches, alloMap has already been checked against it. */
    for(i=0; i<usedPages; i++)
#ifdef LINUX
        {pp = (struct PageHeader *)DRead(file, i);
#else
        {pp = (PageHeader *)DRead(file, i);
#endif
        if (!pp)
            {printf("Failed on second attempt(!) to read dir page %d\n", i);
            printf("This shouldn't have happened!\n");
            DRelease((buffer *)dhp, 0);
            return 0;
            }
        count = i*(EPP/8);
        for(j=0;j<EPP/8;j++) if (eaMap[count+j] != pp->freebitmap[j])
            {printf("Entry alloc bitmap error, page %d, map offset %d, %x should be %x.\n", i, j, pp->freebitmap[j], eaMap[count+j]);
            DRelease((buffer *)pp, 0);
            DRelease((buffer *)dhp, 0);
            return 0;
            }
        DRelease((buffer *)pp, 0);
        }

    /* Finally cleanup and return. */
    DRelease((buffer *)dhp, 0);
    return 1;
    }

/* This routine is called with two parameters.  The first is the id of the original, currently suspect, directory.  The second is the file id of the place the salvager should place the new, fixed, directory. */
/*int DirSalvage (File *fromFile, File *toFile){*/
int DirSalvage (long *fromFile, long *toFile){	/* corrected referencing level (ehs 10/87) */
    /* First do a MakeDir on the target. */
    long dot[3], dotdot[3], code, usedPages;
    register int i;
    struct DirHeader *dhp;
    struct DirEntry *ep;
    int entry;

    bzero((char *)dot, sizeof(dot));
    bzero((char *)dotdot, sizeof(dotdot));
    MakeDir(toFile, dot, dotdot);	/* Returns no error code. */
    code = Delete(toFile, ".");
    if (code) printf("makedir screwup on '.', code %d.\n", code);
    code = Delete(toFile, "..");
    if (code) printf("makedir screwup on '..', code %d.\n", code);

    /* Find out how many pages are valid, using stupid heuristic since DRead never returns null. */
    dhp = (struct DirHeader *) DRead(fromFile, 0);
    if (!dhp)
        {printf("Failed to read first page of fromDir!\n");
        return 0;
        }

    usedPages = 0;
    for(i=0; i<MAXPAGES; i++)
        {if (dhp->alloMap[i] == EPP)
            {usedPages = i;
            break;
            }
        }
    if (usedPages == 0) usedPages = MAXPAGES;

    /* Finally, enumerate all the entries, doing a create on them. */
    for(i=0; i<NHASH; i++)
        {entry = ntohs(dhp->hashTable[i]);
        while(1)
            {if (!entry) break;
            if (entry < 0 || entry >= usedPages*EPP)
                {printf("Warning: bogus hash table entry encountered, ignoring.\n");
                break;
                }
            ep = GetBlob(fromFile, entry);
            if (!ep)
                {printf("Warning: bogus hash chain encountered, switching to next.\n");
                break;
                }
            entry = ntohs(ep->next);
/* XXXXX - Create expects fid to be an array of 3 longs.  
  Here we are passing just an array of 2 (3rd param) */
/*            code = Create(toFile, ep->name, (long *)(&ep->fid)); */
/* XXXXX - Subtracting 1 from the pointer to make it look 
  like an array of 3 longs - VERY BAD PRACTICE XXXXXXX */
            code = Create(toFile, ep->name, ((long *)(&ep->fid)) - 1 );
            if (code) printf("Create of %s returned code %d, continuing.\n", ep->name, code);
            DRelease((buffer *)ep, 0);
            }
        }

    /* Clean up things. */
    DRelease((buffer *)dhp, 0);
    return 0;
    }

/* the end */

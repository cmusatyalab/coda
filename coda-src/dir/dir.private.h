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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/dir/dir.private.h,v 4.1 1997/01/08 21:49:32 rvb Exp $";
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



#ifndef _DIR_PRIVATE_H_
#define _DIR_PRIVATE_H_ 1

/* use this file to declare objects and routines that are not used outside the dir module */
/* if changed also change dirvnode.h */
#define PAGESIZE 2048	/* bytes per page */
#define	LOGPS 11	/* log page size */
#define NHASH 128	/* entries in the hash tbl */
#define MAXPAGES 128	/* max pages in a dir */
#define EPP 64		/* dir entries per page */
#define LEPP 6		/* log above */
#define	ESZ 32		/* entry size (PAGESIZE / EPP) */
#define	LESZ 5		/* log above */
/* When this next field changs, it is crucial to modify MakeDir, since the latter is responsible for marking these entries as allocated.  Also change the salvager. */
#define DHE 12		/* entries in a dir header above a pages header alone. */

#define	PHTODEH(ph, blobno)\
    ((struct DirEntry *)(((char *)(ph)) + (((blobno) & (EPP - 1)) << LESZ)))
#define	DEHTOPH(deh, blobno)\
    ((struct PageHeader *)((char *)(deh) - (((blobno) & (EPP - 1)) << LESZ)))

#define FFIRST 1
#define FNEXT 2

struct MKFid
    {/* A file identifier. */
    long mkvnode;	/* file's vnode slot */
    long mkvunique;	/* the slot incarnation number */
    };

struct PageHeader
    {/* A page header entry. */
    long tag;
    char freecount;	/* unused, info in dirHeader structure */
    char freebitmap[EPP/8];
    char padding[32-(5+EPP/8)];
    };

struct DirHeader
    {/* A directory header object.
     */struct PageHeader header;
    char alloMap[MAXPAGES];    /* one byte per 2K page */
    short hashTable[NHASH];
    };

struct DirEntry
    {/* A directory entry */
    char flag;
    char length;	/* currently unused */
    short next;
    struct MKFid fid;
    char name[16];
    };

struct DirXEntry
    {/* A directory extension entry. */
    char name[32];
    };

struct DirPage0
    {/* A page in a directory. */
    struct DirHeader header;
    struct DirEntry entry[1];
    };

struct DirPage1
    {/* A page in a directory. */
    struct PageHeader header;
    struct DirEntry entry[1];
    };

/* extern definitions for physio.c */
extern int  ReallyRead(void *, long, char *);
extern int  ReallyWrite(void *, long, char *);
extern void FidZap (void *);
extern int FidEq (void *, void *);
extern void FidCpy (void *, void *);
extern void Die(char *);

#endif _DIR_PRIVATE_H_

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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/dir/dirprivate.h,v 4.1 1998/08/26 21:15:07 braam Exp $";
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
#define	LOGPS 11	/* log page size */
#define NHASH 128	/* entries in the hash tbl */
#define EPP 64		/* dir entries per page */
#define LEPP 6		/* log above */
#define	ESZ 32		/* entry size (PAGESIZE / EPP) */
#define	LESZ 5		/* log above */

/* When this next field changs, it is crucial to modify MakeDir, since
   the latter is responsible for marking these entries as allocated.
   Also change the salvager. */

#define DHE 12		/* entries in a dir header above a pages header alone. */

#define	PHTODEH(ph, blobno)\
    ((struct DirEntry *)(((char *)(ph)) + (((blobno) & (EPP - 1)) << LESZ)))
#define	DEHTOPH(deh, blobno)\
    ((struct PageHeader *)((char *)(deh) - (((blobno) & (EPP - 1)) << LESZ)))

#define FFIRST (char )1
#define FNEXT  (char )2


#define dir_assign( var, val )  do {\
    if ( dir_rvm() ) RVMLIB_MODIFY(var, val); else\
    var = val; } while (0) ;




/* A directory extension entry. */
struct DirXEntry  {
    char name[32];
};

struct DirBlob {
	char name[32];
};

/* A page header entry. */
#define PH_PADSIZE sizeof(struct DirBlob) - sizeof(long) - sizeof(char) - (EPP/8) 
struct PageHeader {
    long tag;
    char freecount;	/* duplicated info: also in allomap */
    char freebitmap[EPP/8];
    char padding[PH_PADSIZE];
};

/* A directory header object. */
struct DirHeader {
	    struct PageHeader dirh_ph;
	    char dirh_allomap[DIR_MAXPAGES];    /* one byte per 2K page */
	    short dirh_hashTable[NHASH];
};


#endif _DIR_PRIVATE_H_

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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/dir/dir.h,v 1.1 1996/11/22 19:07:08 braam Exp $";
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


#ifndef _DIR_H_
#define _DIR_H_ 1

/* extern definitions for dir.c */
extern int NameBlobs (char *);
extern int Create (long *, char *, long *);
extern int Delete (long *, char *);
extern int MakeDir (long *, long *, long *);
extern int Lookup (long *, char *, long *);
extern struct DirEntry *GetBlob (long *, long);
extern int DirHash (char *);
extern int EnumerateDir (long *, int (*)(void *par1,...), long);
extern int DirToNetBuf(long *, char *, int, int *);
extern char *FindName(long *, long, long, char *);
extern int IsEmpty (long *);
extern int Length (long *);

/* extern definitions for buffer.c */
struct buffer
    {
    long fid[5];	/* Unique cache key + i/o addressing */
    long page;
    long accesstime;
    struct buffer *hashNext;
    char *data;
    char lockers;
    char dirty;
    char hashIndex;
    };

#ifdef	__linux__
typedef struct buffer buffer;
#endif

extern void DStat (int *, int *, int *);
extern int DInit (int );
extern char *DRead(long *, int);
extern void DRelease (struct buffer *, int);
extern int DVOffset (struct buffer *);
extern void DFlush ();
extern char *DNew (long *, int);
extern void DZap (long *);
extern void DFlushEntry (long *);

/* extern definitions for salvage.c */
extern int DirOK (long *);
extern int DirSalvage (long *, long *);

#endif _DIR_H_

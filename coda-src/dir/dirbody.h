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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/dir/dirprivate.h,v 4.2 1998/09/07 15:57:20 braam Exp $";
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



#ifndef _DIR_BODY_H_
#define _DIR_BODY_H_ 1

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define	LOGPS 11	/* log page size */
#define NHASH 128	/* entries in the hash tbl */
#define EPP 64		/* dir entries per page */
#define LEPP 6		/* log above */
#define	ESZ 32		/* entry size (PAGESIZE / EPP) */
#define	LESZ 5		/* log above */
#define DHE 12		/* entries in a dir header above a page header */

#define FFIRST (char )1


/* A directory blob. */
struct DirBlob {
	char name[32];
};

/* A page header entry: padded to be a 32 byte blob. */
struct PageHeader {
    long tag;
    char freecount;	/* duplicated info: also in allomap */
    char freebitmap[EPP/8];
    char padding[32-(5+EPP/8)];
};

/* A directory header object. */
struct DirHeader {
	    struct PageHeader dirh_ph;
	    char dirh_allomap[DIR_MAXPAGES];    /* one byte per 2K page */
	    short dirh_hashTable[NHASH];
};

int DIR_rvm(void);
int DIR_IsEmpty(PDirHeader);
void DIR_Print(PDirHeader);
extern void DIR_Free(struct DirHeader *, int);
extern int DirHash (char *);
extern int DirToNetBuf(long *, char *, int, int *);
void DIR_CpyVol(struct ViceFid *target, struct ViceFid *source);
int DIR_MakeDir(struct DirHeader **dir, struct DirFid *me, struct DirFid *parent);
int DIR_LookupByFid(PDirHeader dhp, char *name, struct DirFid *fid);
int DIR_Lookup(struct DirHeader *dir, char *entry, struct DirFid *fid);
int DIR_EnumerateDir(struct DirHeader *dhp, 
		     int (*hookproc)(struct DirEntry *de, void *hook), void *hook);
int DIR_Create(struct DirHeader **dh, char *entry, struct DirFid *fid);
int DIR_Length(struct DirHeader *dir);
int DIR_Delete(struct DirHeader *dir, char *entry);
int DIR_Init(int data_loc);
void DIR_PrintChain(PDirHeader dir, int chain);
int DIR_Hash (char *string);
int DIR_DirOK (PDirHeader pdh);
int DIR_Convert (PDirHeader dir, char *file, VolumeId vol);
void DIR_Setpages(PDirHeader, int);



#endif _DIR_PRIVATE_H_

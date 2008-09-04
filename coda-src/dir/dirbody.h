/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

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
    int tag;
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
extern void DIR_Free(struct DirHeader *, int);
extern int DirHash (char *);
extern int DirToNetBuf(long *, char *, int, int *);
void DIR_CpyVol(struct ViceFid *target, struct ViceFid *source);
int DIR_MakeDir(struct DirHeader **dir, struct DirFid *me, struct DirFid *parent);
int DIR_LookupByFid(PDirHeader dhp, char *name, struct DirFid *fid);
int DIR_Lookup(struct DirHeader *dir, const char *entry, struct DirFid *fid,
	       int flags);
int DIR_EnumerateDir(struct DirHeader *dhp, 
		     int (*hookproc)(struct DirEntry *de, void *hook), void *hook);
int DIR_Create(struct DirHeader **dh, const char *entry, struct DirFid *fid);
int DIR_Length(struct DirHeader *dir);
int DIR_Delete(struct DirHeader *dir, const char *entry);
void DIR_PrintChain(PDirHeader dir, int chain, FILE *f);
int DIR_Hash (const char *string);
int DIR_DirOK (PDirHeader pdh);
int DIR_Convert (PDirHeader dir, char *file, VolumeId vol, RealmId realm);
void DIR_Setpages(PDirHeader, int);

#endif /* _DIR_PRIVATE_H_ */


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

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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/libal/al.h,v 1.1.1.1 1996/11/22 19:08:32 rvb Exp";
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




#ifndef _AL_
#define _AL_

#define AL_VERSION "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/libal/al.h,v 1.1.1.1 1996/11/22 19:08:32 rvb Exp"

typedef
    struct
        {
        int Id;         /*internally-used ID of user or group*/
        int Rights;     /*mask*/
        }
    AL_AccessEntry;
/*
The above access list entry format is used in VICE
*/


#define AL_ALISTVERSION  1 /*Identifies current format of access lists*/
typedef
    struct
        {
        int MySize;     /*size of this access list in bytes, including MySize itself*/
        int Version;	/*to deal with upward compatibility ; <= AL_ALISTVERSION*/
        int TotalNoOfEntries; /*no of slots in ActualEntries[ ]; redundant, but convenient */
        int PlusEntriesInUse; /*stored forwards from ActualEntries[0]*/
        int MinusEntriesInUse; /*stored backwards from ActualEntries[TotalNoOfEntries-1]*/
        AL_AccessEntry ActualEntries[1]; /*Actual array bound is TotalNoOfEntries*/
        }
    AL_AccessList;
/*
Used in VICE. This is how acccess lists are stored on secondary storage.
*/


extern int AL_MaxExtEntries;	/* Max number of entries in an external access list */
#define AL_MAXEXTENTRIES	20	/* Default initial value for AL_MaxExtEntries */

typedef
    char *AL_ExternalAccessList;
/*
An ASCII representation of an access list. Begins with two decimal integers in format "%d\n%d\n"
specifying the number of Plus entries and Minus entries that follow.  This is followed by the list
of plus entries and then the list of minus entries.  Each entry consists of a
username or groupname followed by a decimal number representing the rights mask for that name.  Each
entry in the list looks as if it had been produced by printf() using a format list of "%s\t%d\n".

Note that the number of Plus entries and Minus entries must be less than AL_MaxExtEntries, which has
the default of AL_MAXEXTENTRIES. You can change this limit at any time by changing AL_MaxExtEntries.
*/

extern int AL_DebugLevel;   /* set for debugging info */

/* Interface definition */
extern int AL_NewAlist(int  MinNoOfEntries,  AL_AccessList **Al);
extern int AL_FreeAlist(AL_AccessList **Al);
extern int AL_htonAlist(AL_AccessList *Al);
extern int AL_ntohAlist(AL_AccessList *Al);
extern int AL_NewExternalAlist(int MinNoOfEntries,  AL_ExternalAccessList *R);
extern int AL_FreeExternalAlist(AL_ExternalAccessList *R);
extern int AL_NewCPS(int MinNoOfEntries,  PRS_InternalCPS **ICPS);
extern int AL_FreeCPS(PRS_InternalCPS **C);
extern int AL_htonCPS(PRS_InternalCPS *C);
extern int AL_ntohCPS(PRS_InternalCPS *C);
extern int AL_NewExternalCPS(int MinNoOfEntries,  PRS_ExternalCPS *R);
extern int AL_FreeExternalCPS(PRS_ExternalCPS *R);
extern int AL_Externalize(AL_AccessList *Alist,  AL_ExternalAccessList *Elist);
extern int AL_Internalize(AL_ExternalAccessList Elist,  AL_AccessList **Alist);
extern int AL_CheckRights(AL_AccessList *Alist,  PRS_InternalCPS *CPS,  int *WhichRights);
extern int AL_Initialize(char *Version,  char *pdbFile,  char *pcfFile);
extern int AL_NameToId(char *Name,  int *Id);
extern int AL_IdToName(int Id,  char Name[]);
extern int AL_GetInternalCPS(int Id,      PRS_InternalCPS **ICPS);
extern int AL_GetExternalCPS(int Id,  PRS_ExternalCPS *ECPS);
extern int AL_IsAMember(int Id,  PRS_InternalCPS *ICPS);
extern int AL_PrintAlist(AL_AccessList *A);
extern int AL_PrintExternalAlist(AL_ExternalAccessList E);
extern int AL_DisableGroup(int gid,  PRS_InternalCPS *ICPS);
extern int AL_EnableGroup(int gid,  PRS_InternalCPS *ICPS);

#endif

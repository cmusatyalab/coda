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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/libal/Attic/alprocs.cc,v 4.1 1997/01/08 21:49:41 rvb Exp $";
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

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef __NetBSD__
#define _POSIX_SOURCE
#endif

#include <errno.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "prs.h"
#include "al.h"
#include "parsepdb.h"
#include "pcf.h"

/* ----- Shared globals from parsepdb.h  -----*/
int MyDebugFlag;
int SourceLineNumber;
char FileRoot[MAXSTRLEN+1];
char *yyinFileName;
int BytesScannedByLex;

int p_TypeOfEntry;
char p_Name[PRS_MAXNAMELEN+1];
int p_Id;
int p_Owner;

int p_List1Bound, p_List1Count, *p_List1;
int p_List2Bound, p_List2Count, *p_List2;
int p_List3Bound, p_List3Count, *p_List3;

int p_PlusBound;
int p_PlusCount;
AL_AccessEntry *p_Plus;

int p_MinusBound;
int p_MinusCount;
AL_AccessEntry *p_Minus;

/*--------- End of Shared Globals from parsepdb.h ---------------*/


/*------ Unsorted, singly-linked circular free lists ---------------*/

struct FreeListEntry
    {
    struct FreeListEntry *NextEntry;
    int SizeOfBody;
    char Body[1];	/* Variable length actual body */
    };

struct FreeList
    {
    struct FreeListEntry *Head;
    struct FreeListEntry *Tail;
	/* INVARIANT: (Head == Tail == NULL) || (Tail->NextEntry = Head) */
    };


PRIVATE struct FreeList AllFree;    /* Right now: a single free list for all entities; we may
				    choose to have separate free lists later, if it appears
				    beneficial */

/*----------------------------------------------------------------------*/


int AL_MaxExtEntries = AL_MAXEXTENTRIES; /* Checked on AL_Internalize() and AL_Externalize() */
PRIVATE char *AL_pdbFileName;	/* Name of protection database */
PRIVATE char *AL_pcfFileName; 	/* Name of protection configuration file */
PRIVATE struct stat PdbStatBuf;	/* Buffer for stat() of .pdb file at the most recent AL_Initialize() */

/* made the following non-private; used by ../vice/codaproc.c also */
/* qsort() comparison routines */
int CmpPlus(AL_AccessEntry *a, AL_AccessEntry *b);
int CmpMinus(AL_AccessEntry *a, AL_AccessEntry *b);
int CmpInt(int *a, int *b);

/* Other local routines */
PRIVATE int GetFromList(INOUT struct FreeList *flist, OUT struct FreeListEntry **elem, IN int minsize);
PRIVATE int AddToList(INOUT struct FreeList *flist, INOUT struct FreeListEntry *elem);
PRIVATE int NameSearch(IN char *Name, OUT int *Id);
PRIVATE void PrintTables();




int AL_NewAlist(IN int  MinNoOfEntries, OUT AL_AccessList **Al)
    /*
    Creates an access list capable of holding at least MinNoOfEntries entries.
    Returns 0 on success; aborts if we run out of memory.
    */
    {
    int t;
    struct FreeListEntry *e;

    t = sizeof(AL_AccessList) + (MinNoOfEntries-1)*sizeof(AL_AccessEntry);
	/* Remember: AL_AccessList already has a one-element  AL_AccessEntry array in it */

    if (GetFromList(&AllFree, &e, t) < 0)
	{
	e = (struct FreeListEntry *) malloc(t + sizeof(int)+sizeof(struct FreeListEntry *));
	if (e == NULL)
	    {
	    perror("AL_NewAlist: malloc() failed");
	    abort();
	    }
	e->SizeOfBody = t;
	*Al = (AL_AccessList *)(e->Body);
	}
    else
	*Al = (AL_AccessList *)(e->Body);

    (*Al)->MySize = t;			/* May be less than actual size of storage */
    (*Al)->Version = AL_ALISTVERSION;
    (*Al)->TotalNoOfEntries = MinNoOfEntries;
    (*Al)->PlusEntriesInUse = (*Al)->MinusEntriesInUse = 0;
    return(0);
    }


int AL_FreeAlist(INOUT AL_AccessList **Al)
    /*
    Releases the access list defined by Al.
    Returns 0 always.
    */
    {
    struct FreeListEntry *x;
    x = (struct FreeListEntry *)
	    ((char *)*Al - sizeof(struct FreeListEntry *) - sizeof(int));
    *Al = NULL;
    return(AddToList(&AllFree, x));
    }

int AL_htonAlist(INOUT AL_AccessList *Al)
    /*
    Converts the access list defined by Al to network order.
    Returns 0 always.
    */
    {
    long i;

    i = 1;
    if (htonl(i) == i) return(0); /* host order == byte order */

    /* else byte swapping needed */
    for (i = 0; i < Al->PlusEntriesInUse; i++)
	{
	Al->ActualEntries[i].Id = htonl(Al->ActualEntries[i].Id);
	Al->ActualEntries[i].Rights = htonl(Al->ActualEntries[i].Rights);
	}
    for (i = Al->TotalNoOfEntries - 1; i > Al->TotalNoOfEntries -  Al->MinusEntriesInUse - 1; i--)
	{
	Al->ActualEntries[i].Id = htonl(Al->ActualEntries[i].Id);
	Al->ActualEntries[i].Rights = htonl(Al->ActualEntries[i].Rights);
	}
    Al->MySize = htonl(Al->MySize);
    Al->Version = htonl(Al->Version);
    Al->TotalNoOfEntries = htonl(Al->TotalNoOfEntries);
    Al->PlusEntriesInUse = htonl(Al->PlusEntriesInUse);
    Al->MinusEntriesInUse = htonl(Al->MinusEntriesInUse);
    return(0);
    }

int AL_ntohAlist(INOUT AL_AccessList *Al)
    /*
    Converts the access list defined by Al to host order.
    Returns 0 always.
    */
    {
    long i;

    i = 1;
    if (htonl(i) == i) return(0); /* host order == byte order */

    /* Else byte swapping needed */
    Al->MySize = ntohl(Al->MySize);
    Al->Version = ntohl(Al->Version);
    Al->TotalNoOfEntries = ntohl(Al->TotalNoOfEntries);
    Al->PlusEntriesInUse = ntohl(Al->PlusEntriesInUse);
    Al->MinusEntriesInUse = ntohl(Al->MinusEntriesInUse);
    for (i = 0; i < Al->PlusEntriesInUse; i++)
	{
	Al->ActualEntries[i].Id = ntohl(Al->ActualEntries[i].Id);
	Al->ActualEntries[i].Rights = ntohl(Al->ActualEntries[i].Rights);
	}
    for (i = Al->TotalNoOfEntries - 1; i > Al->TotalNoOfEntries -  Al->MinusEntriesInUse - 1; i--)
	{
	Al->ActualEntries[i].Id = ntohl(Al->ActualEntries[i].Id);
	Al->ActualEntries[i].Rights = ntohl(Al->ActualEntries[i].Rights);
	}
    return(0);
    }

int AL_NewExternalAlist(IN int MinNoOfEntries, OUT AL_ExternalAccessList *R)
    /*
    On successful return, R defines an external access list big enough
	to hold MinNoOfEntries full-sized entries.
    Returns 0 on success; aborts if insufficient memory.
    */
    {
    int t;
    struct FreeListEntry *e;

    t = 20 + (MinNoOfEntries)*(PRS_MAXNAMELEN+20);
	/* Conservative estimate: enough space in each entry for longest 
		name plus decimal 2**32 (for largest rights mask) plus some formatting */

    if (GetFromList(&AllFree, &e, t))
	{
	e = (struct FreeListEntry *) malloc(t + sizeof(int)+sizeof(struct FreeListEntry *));
	if (e == NULL)
	    {
	    perror("AL_NewExternalAlist(): malloc() failed");
	    abort();
	    }
	e->SizeOfBody = t;
	}

    *R = e->Body;
    sprintf(*R, "0\n0\n");
    return(0);
    }


int AL_FreeExternalAlist(INOUT AL_ExternalAccessList *R)
    /*
    Releases the external access list defined by R.
    Returns 0 always.
    */
    {
    struct FreeListEntry *x;
    x = (struct FreeListEntry *)
	    ((char *)*R - sizeof(struct FreeListEntry *) - sizeof(int));
    *R = NULL;
    return(AddToList(&AllFree, x));
    }

int AL_NewCPS(IN int MinNoOfEntries, OUT PRS_InternalCPS **ICPS)
    /*
    On successful return, ICPS defines an internal CPS which is
	capable of holding at least MinNoOfEntries entries.
    Returns 0 on success; aborts if we run out of memory.
    */
    {
    int t;
    struct FreeListEntry *e;

    t = sizeof(PRS_InternalCPS) + (MinNoOfEntries-1)*sizeof(int);
	/* Remember: PRS_InternalCPS already has a one-element array in it */

    if (GetFromList(&AllFree, &e, t) < 0)
	{
	e = (struct FreeListEntry *) malloc(t + sizeof(int)+sizeof(struct FreeListEntry *));
	if (e == NULL)
	    {
	    perror("AL_NewClist: malloc() failed");
	    abort();
	    }
	e->SizeOfBody = t;
	*ICPS = (PRS_InternalCPS *)(e->Body);
	}
    else
	*ICPS = (PRS_InternalCPS *)(e->Body);

    (*ICPS)->InclEntries = 0;
    (*ICPS)->ExclEntries = 0;
    return(0);
    }


int AL_FreeCPS(INOUT PRS_InternalCPS **C)
    /*
    Releases the internal CPS defined by C.
    Returns 0 always.
    */
    {
    struct FreeListEntry *x;
    x = (struct FreeListEntry *)
	    ((char *)*C - sizeof(struct FreeListEntry *) - sizeof(int));
    *C = NULL;
    return(AddToList(&AllFree, x));
    }


int AL_htonCPS(INOUT PRS_InternalCPS *C)
    /*
    Converts the CPS defined by C to network byte order.
    Returns 0.
    */
    {
    long i;

    i = 1;
    if (htonl(i) == i) return(0); /* host order == byte order */

    /* else byte swap */
    for (i = 0; i < C->InclEntries+C->ExclEntries; i++)
	C->IdList[i] = htonl(C->IdList[i]);
    C->InclEntries = htonl(C->InclEntries);
    C->ExclEntries = htonl(C->ExclEntries);
    return(0);
    }

int AL_ntohCPS(INOUT PRS_InternalCPS *C)
    /*
    Converts the CPS defined by C to host byte order.
    Returns 0 always.
    */
    {
    long i;

    i = 1;
    if (htonl(i) == i) return(0); /* host order == byte order */

    /* else byte swap */
    C->InclEntries = ntohl(C->InclEntries);
    C->ExclEntries = ntohl(C->ExclEntries);
    for (i = 0; i < C->InclEntries+C->ExclEntries; i++)
	C->IdList[i] = ntohl(C->IdList[i]);
    return(0);
    }

int AL_NewExternalCPS(IN int MinNoOfEntries, OUT PRS_ExternalCPS *R)
    /*
    On successful return, R defines a newly-created external CPS  which is
	big enough to hold MinNoOfEntries full-sized entries.
    Returns 0 on success; aborts if insufficient memory.
    */
    {
    int t;
    struct FreeListEntry *e;

    t = 20 + (MinNoOfEntries)*(PRS_MAXNAMELEN+2);
	/* Conservative estimate: enough space in each entry for longest 
		name plus formatting */

    if (GetFromList(&AllFree, &e, t))
	{
	e = (struct FreeListEntry *) malloc(t + sizeof(int)+sizeof(struct FreeListEntry *));
	if (e == NULL)
	    {
	    perror("AL_NewExternalCPS(): malloc() failed");
	    abort();
	    }
	e->SizeOfBody = t;
	}

    *R = e->Body;
    sprintf(*R, "0\n");
    return(0);
    }



int AL_FreeExternalCPS(INOUT PRS_ExternalCPS *R)
    /*
    Releases the external access list defined by R.
    Returns 0 always.
    */
    {
    struct FreeListEntry *x;
    x = (struct FreeListEntry *)
	    ((char *)*R - sizeof(struct FreeListEntry *) - sizeof(int));
    *R = NULL;
    return(AddToList(&AllFree, x));
    }

int AL_Externalize(IN AL_AccessList *Alist, OUT AL_ExternalAccessList *Elist)
    /*
    Converts the access list defined by Alist into the newly-created
	external access list in ExternalRep.
    Non-translatable Ids  are coverted to their Ascii integer representations.
    Returns  0 on success. Returns -1 if total number of entries > AL_MaxExtEntries.
    */
    {
    register int i;
    register char *nextc;

    if (Alist->PlusEntriesInUse + Alist->MinusEntriesInUse > AL_MaxExtEntries) return(-1);
    AL_NewExternalAlist(Alist->PlusEntriesInUse + Alist->MinusEntriesInUse, Elist);
    nextc = *Elist;

    sprintf(nextc, "%d\n%d\n", Alist->PlusEntriesInUse, Alist->MinusEntriesInUse);
    nextc += strlen(nextc);
    for (i = 0; i < Alist->PlusEntriesInUse; i++)
	{
	if (AL_IdToName(Alist->ActualEntries[i].Id, nextc) < 0)
	    sprintf(nextc, "%d", Alist->ActualEntries[i].Id);
	nextc += strlen(nextc);
	sprintf(nextc, "\t%d\n", Alist->ActualEntries[i].Rights);
	nextc += strlen(nextc);
	}
    for (i = Alist->TotalNoOfEntries - 1; i >= Alist->TotalNoOfEntries - Alist->MinusEntriesInUse; i--)
	{
	if (AL_IdToName(Alist->ActualEntries[i].Id, nextc) < 0)
	    sprintf(nextc, "%d", Alist->ActualEntries[i].Id);
	nextc += strlen(nextc);
	sprintf(nextc, "\t%d\n", Alist->ActualEntries[i].Rights);
	nextc += strlen(nextc);
	}
    return (0);
    }

int AL_Internalize(IN AL_ExternalAccessList Elist, OUT AL_AccessList **Alist)
    /*
    On successful return, Alist will define a newly-created access list
	corresponding to  the external access list defined by Elist.
    Returns 0 on successful conversion.
    Returns -1 if ANY name in the access list is not translatable, or if
    	the total number of entries is greater than AL_MaxExtEntries.
    */
    {
    register int i;
    register char *nextc;
    int m, p;
    char tbuf[PRS_MAXNAMELEN+1];

    if (sscanf(Elist, "%d\n%d\n", &p, &m) != 2) return (-1);
    if (p + m > AL_MaxExtEntries) return(-1);
    AL_NewAlist(p + m, Alist);
    (*Alist)->PlusEntriesInUse = p;
    (*Alist)->MinusEntriesInUse = m;
    
    nextc = Elist;
    while(*nextc && *nextc != '\n') nextc++;
    nextc++;
    while(*nextc && *nextc != '\n') nextc++;
    nextc++;	/* now at the beginning of the entry list */

    for (i = 0; i < (*Alist)->PlusEntriesInUse; i++)
	{
	if (sscanf(nextc, "%s\t%d\n", tbuf, &((*Alist)->ActualEntries[i].Rights)) != 2)
	    return(-1);
	if (AL_NameToId(tbuf, &((*Alist)->ActualEntries[i].Id)) < 0)
	    return(-1);
	nextc = (char *)(1 + index(nextc, '\n'));
	}
    for (i = (*Alist)->TotalNoOfEntries - 1; i >= (*Alist)->TotalNoOfEntries -  (*Alist)->MinusEntriesInUse; i--)
	{
	if (sscanf(nextc, "%s\t%d\n", tbuf, &((*Alist)->ActualEntries[i].Rights)) != 2)
	    return(-1);
	if (AL_NameToId(tbuf, &((*Alist)->ActualEntries[i].Id)) < 0)
	    return(-1);
	nextc = (char *)(1 + index(nextc, '\n'));
	}
    /* Sort positive and negative entries */
    qsort( (char *)&((*Alist)->ActualEntries[0]),(*Alist)->PlusEntriesInUse,
    		sizeof(AL_AccessEntry), 
		(int (*)(const void *, const void *))CmpPlus); 
    qsort( (char *)&((*Alist)->ActualEntries[(*Alist)->TotalNoOfEntries - (*Alist)->MinusEntriesInUse]),
		(*Alist)->MinusEntriesInUse, sizeof(AL_AccessEntry), 
		(int (*)(const void *, const void *))CmpMinus); 
    return(0);
    }

int AL_CheckRights(IN AL_AccessList *Alist, IN PRS_InternalCPS *CPS, OUT int *WhichRights)
    /*
    Returns in WhichRights, the rights possessed by CPS on Alist
    */
    {
    int plusrights;		/* plus rights accumulated so far */
    int minusrights;		/* minus rights accumulated so far */
    int a;			/* index into next entry in Alist */
    int c;			/* index into next entry in CPS */

    if (Alist->PlusEntriesInUse <= 0 || CPS->InclEntries <= 0)
	{*WhichRights = 0; return(0);}
    if (CPS->IdList[CPS->InclEntries - 1] == PRS_SYSTEMID)
	{*WhichRights = -1; return(0);}		/* System always gets all rights; being a user, it will
						    always be the last entry in the sorted CPS list */

    /* Each iteration eats up exactly one entry from either Alist or CPS.
       Duplicate Entries in access list ==> accumulated rights are obtained.
       Duplicate Entries in CPS ==> irrelevant */
    plusrights = 0;
    c = a = 0;    
    while ( (a < Alist->PlusEntriesInUse)  && (c < CPS->InclEntries) )
	switch (CmpInt(&(Alist->ActualEntries[a].Id), &(CPS->IdList[c])))
	    {
	    case -1:
		a += 1;
		break;
		
	    case 0:
		plusrights |= Alist->ActualEntries[a].Rights;
		a += 1;
		break;
		
	    case 1:
		c += 1;
		break;	    
		
	    default:
		printf("CmpInt() returned bogus value. Aborting ...\n");
		abort();
	    }
	    
	    
    /* Now walk backwards on MinusEntries */
    minusrights = 0;
    c = 0;
    a = Alist->TotalNoOfEntries - 1 ;
    while ( (c < CPS->InclEntries) && (a > Alist->TotalNoOfEntries - Alist->MinusEntriesInUse -1))
	switch (CmpInt(&(Alist->ActualEntries[a].Id), &(CPS->IdList[c])))
	    {
	    case -1:
		a -= 1;
		break;
		
	    case 0:
		minusrights |= Alist->ActualEntries[a].Rights;
		a -= 1;
		break;
		
	    case 1:
		c += 1;
		break;	    
		
	    default:
		printf("CmpInt() returned bogus value. Aborting ...\n");
		abort();
	    }
	    
    *WhichRights = plusrights & (~minusrights);
    return(0);
    }


int AL_Initialize(IN char *Version, IN char *pdbFile, IN char *pcfFile)
    /*
    Initializes the access list package.
    Version should always be AL_VERSION.
    pdbFile is a string defining the protection database file; set to NULL for default.
    pcfFile is a string defining the protection configuration file; set to NULL for default.
    
    This routine may be called many times -- it will perform reinitialization each time.
    Synchronization code here guarantees that the .pdb and .pcf files are mutually consistent,
	provided all updaters  follow the locking discipline.
    */
    {
/*    register int i; */
    FILE *pfd;
    LogMsg(1, AL_DebugLevel, stdout, "AL_Initialize(%s, %s, %s)", Version, pdbFile, pcfFile);
    LogMsg(4, AL_DebugLevel,
	stdout, "Library version: '%s'\tHeader version: '%s'", AL_VERSION, Version);

    assert(strcmp(Version, AL_VERSION) == 0);

    /* We do not free AL_pdbFileName or AL_pcfFileName; this is because they may be aliased as
	pdbFile and pcfFile if AL_Initialize() is being called from inside the AL package itself.
	
	This may result in a tiny core  leak, but it is not worth worrying about.
    */

    if (pdbFile == NULL)	
	{
	AL_pdbFileName = (char *)malloc(1+strlen(PRS_PDBNAME));
	strcpy(AL_pdbFileName, PRS_PDBNAME);
	}
    else
	{
	AL_pdbFileName = (char *)malloc(1+strlen(pdbFile));
	strcpy(AL_pdbFileName, pdbFile);
	}

    if (pcfFile == NULL)
	{
	AL_pcfFileName = (char *)malloc(1+strlen(PRS_PCFNAME));
	strcpy(AL_pcfFileName, PRS_PCFNAME);
	}
    else
	{
	AL_pcfFileName = (char *)malloc(1+strlen(pcfFile));
	strcpy(AL_pcfFileName, pcfFile);
	}

    /* We may be re-initializing */
    if (LitPool) free(LitPool);
    if (Uoffsets) free((char *)Uoffsets);
    if (Usorted) free((char *) Usorted);
    if (Useeks) free((char *) Useeks);
    if (Goffsets) free((char *) Goffsets);
    if (Gsorted) free((char *) Gsorted);
    if (Gseeks) free((char *) Gseeks);

    /* To prevent race hazards in updating .pcf and .pdb files we use flock() on 
	the .pdb file.  For this to be useful, the updating agent must flock() the .pdb
	file exclusively.  Note that pcfRead() also flock()s the .pcf file, but that is
	probably redundant -- the .pdb file suffices as a semaphore.  We hold the
	lock on the .pdb file until the .pcf file has been read and the initialization is
	complete.  The invariants guaranteed by this procedure are:
		1. The .pcf and .pdb files are mutually consistent
		2. The global PdbStatBuf contains the status of the .pdb file as of now.

	REPEAT: all this caution is worthless unless the updater also flock()s the .pdb file.
		Just doing a "cp" will buy you no consistency guarantee.
    */

    if ((pfd = fopen(AL_pdbFileName, "r")) == 0 ||	/* changed '< 0' to '== 0' */
	(flock(fileno(pfd), LOCK_SH) < 0)	||
	(fstat(fileno(pfd), &PdbStatBuf) < 0))
	    {
	    perror("AL_Initialize");
	    fclose(pfd);
	    return(-1);
	    }

    if (pcfRead(AL_pcfFileName) != 0
    	|| (PDBCheckSum != 0 && ComputeCheckSum(pfd) != PDBCheckSum))
	{/* PDBCheckSum == 0 ==> old style .pcf file */
	fprintf(stderr, "Bad .pcf or .pdb file\n");
	flock(fileno(pfd), LOCK_UN); /* ignore return codes */
	fclose(pfd);
	return(-1);
	}
    

    LogMsg(1, AL_DebugLevel, stdout, "HighestUID = %d\tHighestGID = %d\tLitPoolSize = %d", 
	    HighestUID, HighestGID, LitPoolSize);

    if (AL_DebugLevel > 100) PrintTables();

    if (flock(fileno(pfd), LOCK_UN) < 0)
	{perror("AL_Initialize"); fclose(pfd); return(-1);}
    fclose(pfd);
    return(0);
    }

PRIVATE void PrintTables()
      {
	  register int i;
	for (i=0; i < 1+HighestUID; i++)
	    if (Uoffsets[Usorted[i]] > -1)
		printf("%d\t%s\t%d\t%d\n", i, LitPool+Uoffsets[Usorted[i]], Usorted[i], Useeks[Usorted[i]]);
	    else printf("%d\t%s\t%d\t%d\n", i, "*****", Usorted[i], Useeks[Usorted[i]]);

	for (i=0; i < 1-HighestGID; i++)
	    if (Goffsets[Gsorted[i]] > -1)
		printf("%d\t%s\t%d\t%d\n", i, LitPool+Goffsets[Gsorted[i]], Gsorted[i], Gseeks[Gsorted[i]]);
	    else printf("%d\t%s\t%d\t%d\n", i, "*****", Gsorted[i], Gseeks[Gsorted[i]]);

      }

int AL_NameToId(IN char *Name, OUT int *Id)
    /*
    Translates the username or groupname defined by Name to Id.
    Returns 0 on success, -1 if translation fails.
    */
    {
    char temp[1+PRS_MAXNAMELEN];
    if (NameSearch(Name, Id) == 0)
	return(0);
    strcpy(temp, "System:");
    SafeStrCat(temp, Name, 1+PRS_MAXNAMELEN);
    return(NameSearch(temp, Id));    
    }

PRIVATE int NameSearch(char *Name, int *Id)
    {
    int Hi; 	/* Highest element that could match */
    int Lo;	/* Lowest element that could match */
    int Mid;	/* Element currently being tried */

    if (index(Name, ':') == 0)
	{
	Lo = 0; Hi = HighestUID;
	while (Lo <= Hi)
	    {
	    Mid = (Lo + Hi)/2;
	    if (Uoffsets[Usorted[Mid]] == -1)
		Hi = Mid - 1;
	    else 
		switch(CaseFoldedCmp(Name, &LitPool[Uoffsets[Usorted[Mid]]]))
		    {
		    case 0:	*Id = Usorted[Mid]; return(0);
		    case 1:	Lo = Mid+1; break;
		    case -1:	Hi = Mid - 1; break;
		    }
	    }
	}
    else
	{
	Lo = 0; Hi = -HighestGID;
	while (Lo <= Hi)
	    {
	    Mid = (Lo + Hi)/2;
	    if (Goffsets[Gsorted[Mid]] == -1)
		Hi = Mid - 1;
	    else 
		switch(CaseFoldedCmp(Name, &LitPool[Goffsets[Gsorted[Mid]]]))
		    {
		    case 0:	*Id = -Gsorted[Mid]; return(0);
		    case 1:	Lo = Mid+1; break;
		    case -1:	Hi = Mid - 1; break;
		    }
	    }
	}
    return(-1);
    }


int AL_IdToName(IN int Id, OUT char Name[])
    /*
    Translates Id and returns the corresponding username or groupname in Name.
    Returns 0 on success, -1 if Id is not translatable.
    */
    {
    if (Id == 0 || HighestGID > Id || Id > HighestUID) return(-1);
    if (Id > 0)
	{
	if (Uoffsets[Id] == -1) return(-1);
	else return(SafeStrCpy(Name, &LitPool[Uoffsets[Id]], PRS_MAXNAMELEN));
	}
    else 
	{
	if (Goffsets[-Id] == -1) return(-1);
	else return(SafeStrCpy(Name, &LitPool[Goffsets[-Id]], PRS_MAXNAMELEN));
	}
    }

int AL_GetInternalCPS(IN int Id, OUT PRS_InternalCPS **ICPS)
    /*
    On successful return, ICPS defines a newly-created data structure,
	corresponding to the internal CPS of Id.
    Return 0  on success; -1 if Id is not a valid user or group id.
    */
    {
    int seekval;
    struct stat mystatbuf;
    LogMsg(1, AL_DebugLevel, stdout, "in AL_GetInternalCPS(%d, 0x%x)", Id, ICPS);

RetryGet:
    if (Id == 0 || HighestGID > Id || Id > HighestUID) return(-1);
    if (Id > 0)
	if (Uoffsets[Id] == -1) return(-1);
	else seekval = Useeks[Id];
    else 
	if (Goffsets[-Id] == -1) return(-1);
	else seekval = Gseeks[-Id];
    
    if ((yyin = fopen(AL_pdbFileName, "r")) == NULL ||
	    (flock(fileno(yyin), LOCK_SH) < 0) ||
	    (fstat(fileno(yyin), &mystatbuf) < 0))
	{
	perror(AL_pdbFileName);
	abort();
	}

    if (mystatbuf.st_mtime != PdbStatBuf.st_mtime)
	{
	/* .pdb file has changed since we last did AL_Initialize(). Redo. */
	assert(AL_Initialize(AL_VERSION, AL_pdbFileName, AL_pcfFileName) == 0);
	fclose(yyin);
	goto RetryGet;
	}

    yyinFileName = AL_pdbFileName;
    fseek(yyin, seekval, 0);
    yyparse();
    flock(fileno(yyin), LOCK_UN);	/* ignore return codes */
    fclose(yyin);
    if (AL_DebugLevel > 0) PrintEntry();

    AL_NewCPS(2+p_List2Count, ICPS);
    (*ICPS)->InclEntries = (Id == PRS_ANONYMOUSID)? (1 + p_List2Count) : (2+p_List2Count);
    (*ICPS)->ExclEntries = 0; /* <<<< just for now >>>> */
    bcopy((char *)p_List2, (char *)((*ICPS)->IdList), p_List2Count*sizeof(int));
    if (Id != PRS_ANONYMOUSID)
	(*ICPS)->IdList[(*ICPS)->InclEntries-2] = PRS_ANYUSERID;	/* automagically in one's CPS */
    (*ICPS)->IdList[(*ICPS)->InclEntries-1] = Id;	/* Reflexive; repetition won't hurt */
    qsort((char *)((*ICPS)->IdList), (*ICPS)->InclEntries, sizeof(int), 
	  (int (*)(const void *, const void *))CmpInt);    
    return(0);
    }



int AL_GetExternalCPS(IN int Id, OUT PRS_ExternalCPS *ECPS)
    /*
    On successful return, ECPS defines a newly-created data structure,
	corresponding to the external CPS of Id.
    Return 0  on success; -1 if Id is not a valid user or group id.
    */
    {
    PRS_InternalCPS *ICPS;
    register char *s;
    register int i;

    LogMsg(1, AL_DebugLevel, stdout, "in AL_GetExternalCPS(%d, 0x%x)", Id, ECPS);
    if (AL_GetInternalCPS(Id, &ICPS) != 0)
	return(-1);
    AL_NewExternalCPS(ICPS->InclEntries, ECPS);
    s = *ECPS;
    sprintf(s, "%d\n", ICPS->InclEntries);
    s += strlen(s);
    for (i = 0; i < ICPS->InclEntries; i++)
	{
	if (AL_IdToName(ICPS->IdList[i], s) != 0)
	    sprintf(s, "%d", ICPS->IdList[i]);
	strcat(s, " ");
	s += strlen(s);
	}
    return(0);
    }



int AL_IsAMember(int Id, PRS_InternalCPS *ICPS)
    /*
    Returns 0 iff Id is in the CPS  ICPS.
    Else returns -1.
    */
    {
    int i;
    
    /* Yes, I know we can do a binary search on IdList */
    for (i = 0; i < ICPS->InclEntries; i++)
	if (ICPS->IdList[i] == Id) return(0);
    return(-1);
    }

PRIVATE int AddToList(INOUT struct FreeList *flist, INOUT struct FreeListEntry *elem)
    /* Adds elem to the freelist flist;  returns 0 */
    {
    if (flist->Head == NULL) flist->Head = flist->Tail = elem->NextEntry = elem;
    else
	{
	flist->Tail->NextEntry = elem;
	elem->NextEntry = flist->Head;
	flist->Tail = elem;	
	}
    return(0);    
    }

PRIVATE int GetFromList(INOUT struct FreeList *flist, OUT struct FreeListEntry **elem, IN int minsize)
    /*  Looks for an element whose Body is atleast minsize bytes in the freelist flist.
	If found, unlinks it, puts its address in elem, and returns 0.
	Else returns -1.    
        A trivial first-fit algorithm is used.
    */
    {
    struct FreeListEntry *y;

    if (flist->Head != NULL)
	{
	y = flist->Tail;
	do
	    {
	    *elem = y->NextEntry;
	    if ((*elem)->SizeOfBody >= minsize)
		{
		if (y == *elem) flist->Head = flist->Tail = NULL;
		else
		    {
		    y->NextEntry = (*elem)->NextEntry;
		    flist->Tail = y;
		    flist->Head = y->NextEntry;

		    }
		return(0);
		}
	    y = y->NextEntry;
	    }
	while (y != flist->Tail);
	}
    return(-1);
    }


int CmpPlus(AL_AccessEntry *a, AL_AccessEntry *b)
    {
    if (a->Id < b->Id) return(-1);
    if (a->Id == b->Id) return(0);
    return(1);
    }

int CmpMinus(AL_AccessEntry *a, AL_AccessEntry *b)
    {
    if (a->Id > b ->Id) return(-1);
    if (a->Id == b->Id) return(0);
    return(1);
    }

int CmpInt(int *x, int *y)
    {
    if (*x < *y) return (-1);
    if (*x == *y) return (0);
    return (1);
    }


int AL_PrintAlist(IN AL_AccessList *A)
    /* Displays the access list A on stdout. Returns 0.*/
    {
    int i;
    printf("MySize = %d  Version = %d\n", A->MySize, A->Version);
    printf("TotalNoOfEntries = %d  PlusEntriesInUse = %d  MinusEntriesInUse = %d\n",
	A->TotalNoOfEntries, A->PlusEntriesInUse, A->MinusEntriesInUse);
    printf("Plus Entries:\n");
    for (i = 0; i < A->PlusEntriesInUse; i++)
	printf("\t%d\t%d\n", A->ActualEntries[i].Id, A->ActualEntries[i].Rights);
    printf("Minus Entries:\n");
    for (i = A->TotalNoOfEntries - 1; i > A->TotalNoOfEntries - A->MinusEntriesInUse -1; i--)
	printf("\t%d\t%d\n", A->ActualEntries[i].Id, A->ActualEntries[i].Rights);
    fflush(stdout);
    return(0);
    }
    

int AL_PrintExternalAlist(IN  AL_ExternalAccessList E)
    /* Displays the external access list E on stdout. Returns 0. */
    {
    printf("%s\n", E);
    return(0);
    }



int AL_DisableGroup(IN int gid, IN PRS_InternalCPS *ICPS)
    {
    register int i,x;

    for (i = 0; i < ICPS->InclEntries; i++)
	{
	if (ICPS->IdList[i] > gid) return(-1);	/* sorted, so can't be there */
	if (ICPS->IdList[i] == gid) break;
	}
    if (i >= ICPS->InclEntries) return(-1);     /* group not currently included */
    
    /* save entry and squeeze others in, thus retaining sorted order  */
    x = ICPS->IdList[i];
    bcopy((char *)(&ICPS->IdList[i+1]), (char *)(&ICPS->IdList[i]), sizeof(int)*(ICPS->InclEntries-i-1));
    ICPS->IdList[ICPS->InclEntries-1] = x;  /* insert at last position */
    ICPS->InclEntries--;
    ICPS->ExclEntries++;
    return(0);
    }

int AL_EnableGroup(IN int gid, IN PRS_InternalCPS *ICPS)
    {
    register int i, x;

    for (i = ICPS->InclEntries; i < ICPS->InclEntries+ICPS->ExclEntries; i++)
	{/* not sorted, so can't break early */
	if (ICPS->IdList[i] == gid) break;
	}
    if (i >= ICPS->InclEntries+ICPS->ExclEntries) return(-1);     /* group not currently excluded */
    
    /* swap entry with head of excluded list, enlarge included set, and then sort it */
    x = ICPS->IdList[i];
    ICPS->IdList[i] = ICPS->IdList[ICPS->InclEntries];
    ICPS->IdList[ICPS->InclEntries] = x;
    ICPS->InclEntries++;
    ICPS->ExclEntries--;
    qsort((char *)(ICPS->IdList), ICPS->InclEntries, sizeof(int), 
	  (int (*)(const void *, const void *))CmpInt);
    return(0);
    }

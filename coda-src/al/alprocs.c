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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>


#ifdef __BSD44__
#define _POSIX_SOURCE
#endif

#include <errno.h>
#include <stdarg.h>
#include <util.h>

#include <rpc2/rpc2.h>
#include "coda_assert.h"
#ifdef __cplusplus
}
#endif


#include "prs.h"
#include "pdb.h"
#include "al.h"

int AL_MaxExtEntries = AL_MAXEXTENTRIES; /* Checked on AL_Internalize()
					    and AL_Externalize() */

/* made the following non-private; used by ../vice/codaproc.c also */
/* qsort() comparison routines */
int CmpPlus(AL_AccessEntry *a, AL_AccessEntry *b);
int CmpMinus(AL_AccessEntry *a, AL_AccessEntry *b);
int CmpInt(int *a, int *b);

/* Creates an access list capable of holding at least MinNoOfEntries entries.
   Returns 0 on success; aborts if we run out of memory. */
int AL_NewAlist(IN int  MinNoOfEntries, OUT AL_AccessList **Al){
	int t;

	t = sizeof(AL_AccessList) + (MinNoOfEntries-1)*sizeof(AL_AccessEntry);
	/* Remember: AL_Accesslist has a one-element AL_AccessEntry array */
	
	if((*Al = (AL_AccessList *) malloc(t)) == NULL){
		perror("AL_NewAlist: malloc() failed");
		abort();
	}
	(*Al)->MySize = t;
	(*Al)->Version = AL_ALISTVERSION;
	(*Al)->TotalNoOfEntries = MinNoOfEntries;
	(*Al)->PlusEntriesInUse = (*Al)->MinusEntriesInUse = 0;
	return 0;
}


/* Releases the access list defined by Al. Returns 0 always. */
int AL_FreeAlist(INOUT AL_AccessList **Al){
	free(*Al);
	*Al = NULL;
	return 0;
}


/* Converts the access list defined by Al to network order. Returns 0 always.*/
int AL_htonAlist(INOUT AL_AccessList *Al){
	long i=1;
	
	if (htonl(i) == i) return(0); /* host order == byte order */
	
	/* else byte swapping needed */
	for (i = 0; i < Al->PlusEntriesInUse + Al->MinusEntriesInUse; i++){
		Al->ActualEntries[i].Id=htonl(Al->ActualEntries[i].Id);
		Al->ActualEntries[i].Rights=htonl(Al->ActualEntries[i].Rights);
	}
	Al->MySize = htonl(Al->MySize);
	Al->Version = htonl(Al->Version);
	Al->TotalNoOfEntries = htonl(Al->TotalNoOfEntries);
	Al->PlusEntriesInUse = htonl(Al->PlusEntriesInUse);
	Al->MinusEntriesInUse = htonl(Al->MinusEntriesInUse);
	return 0;
}


/* Converts the access list defined by Al to host order. Returns 0 always. */
int AL_ntohAlist(INOUT AL_AccessList *Al){
	long i=1;
	
	if (htonl(i) == i) return(0); /* host order == byte order */
	
	/* Else byte swapping needed */
	Al->MySize = ntohl(Al->MySize);
	Al->Version = ntohl(Al->Version);
	Al->TotalNoOfEntries = ntohl(Al->TotalNoOfEntries);
	Al->PlusEntriesInUse = ntohl(Al->PlusEntriesInUse);
	Al->MinusEntriesInUse = ntohl(Al->MinusEntriesInUse);
	for (i = 0; i < Al->PlusEntriesInUse + Al->MinusEntriesInUse; i++){
		Al->ActualEntries[i].Id=ntohl(Al->ActualEntries[i].Id);
		Al->ActualEntries[i].Rights=ntohl(Al->ActualEntries[i].Rights);
	}
	return 0;
}


/* On successful return, R defines an external access list big enough
   to hold MinNoOfEntries full-sized entries.
   Returns 0 on success; aborts if insufficient memory. */
int AL_NewExternalAlist(IN int MinNoOfEntries, OUT AL_ExternalAccessList *R){
	/* Conservative estimate: enough space in each entry for longest 
	   name plus decimal 2**32 (for largest rights mask) plus some
	   formatting */
	if((*R=(AL_ExternalAccessList)
	    malloc(20 + MinNoOfEntries*(PRS_MAXNAMELEN+2)))==NULL){
		perror("AL_NewExternalAlist(): malloc() failed");
		abort();
	}
	sprintf(*R, "0\n0\n");
	return 0;
}


/* Releases the external access list defined by R. Returns 0 always. */
int AL_FreeExternalAlist(INOUT AL_ExternalAccessList *R){
	free(*R);
	*R = NULL;
	return 0;
}

/* On successful return, ICPS defines an internal CPS which is
   capable of holding at least MinNoOfEntries entries.
   Returns 0 on success; aborts if we run out of memory. */
int AL_NewCPS(IN int MinNoOfEntries, OUT PRS_InternalCPS **ICPS){
	char tmp[80];
	/* Remember: PRS_InternalCPS already has a one-element array in it */
	*ICPS = (PRS_InternalCPS *)
		malloc(sizeof(PRS_InternalCPS)+(MinNoOfEntries-1)*sizeof(int));
	if(*ICPS == NULL){
		snprintf(tmp,80,"AL_NewCPS(%d): malloc() failed",MinNoOfEntries);
		perror(tmp);
		abort();
	}
	(*ICPS)->InclEntries = (*ICPS)->ExclEntries = 0;
	return 0;
}


/* Releases the internal CPS defined by C. */
int AL_FreeCPS(INOUT PRS_InternalCPS **C){
	free(*C);
	*C = NULL;
	return 0;
}


/* Converts the CPS defined by C to network byte order. Returns 0. */
int AL_htonCPS(INOUT PRS_InternalCPS *C){
	long i=1;
	
	if (htonl(i) == i) return(0); /* host order == byte order */

	/* else byte swap */	for (i = 0; i < C->InclEntries+C->ExclEntries; i++)
		C->IdList[i] = htonl(C->IdList[i]);
	C->InclEntries = htonl(C->InclEntries);
	C->ExclEntries = htonl(C->ExclEntries);
	return 0;
}


/* Converts the CPS defined by C to host byte order. Returns 0 always. */
int AL_ntohCPS(INOUT PRS_InternalCPS *C){
	long i=1;

	if (htonl(i) == i) return(0); /* host order == byte order */

	/* else byte swap */
	C->InclEntries = ntohl(C->InclEntries);
	C->ExclEntries = ntohl(C->ExclEntries);
	for (i = 0; i < C->InclEntries+C->ExclEntries; i++)
		C->IdList[i] = ntohl(C->IdList[i]);
	return 0;
}


/* On successful return, R defines a newly-created external CPS  which is
   big enough to hold MinNoOfEntries full-sized entries.
   Returns 0 on success; aborts if insufficient memory. */
int AL_NewExternalCPS(IN int MinNoOfEntries, OUT PRS_ExternalCPS *R){
	/* Conservative estimate: enough space in each entry for longest 
	   name plus formatting */
	if((*R=(PRS_ExternalCPS)
	    malloc(20 + (MinNoOfEntries)*(PRS_MAXNAMELEN+2)))==NULL){
		perror("AL_NewExternalCPS: malloc() failed");
		abort();
	}
	sprintf(*R, "0\n");
	return 0;
}



/* Releases the external access list defined by R. Returns 0 always. */
int AL_FreeExternalCPS(INOUT PRS_ExternalCPS *R){
	free(*R);
	*R = NULL;
	return 0;
}



/* Converts the access list defined by Alist into the newly-created external
   access list in ExternalRep. Non-translatable Ids are coverted to their Ascii
   integer representations. Returns  0 on success. Returns -1 if total number
   of entries > AL_MaxExtEntries. */
int AL_Externalize(IN AL_AccessList *Alist, OUT AL_ExternalAccessList *Elist){
	register int i;
	register char *nextc;

	if (Alist->PlusEntriesInUse + Alist->MinusEntriesInUse >
	    AL_MaxExtEntries) return(-1);
	AL_NewExternalAlist(Alist->PlusEntriesInUse +
			    Alist->MinusEntriesInUse, Elist);
	nextc = *Elist;

	sprintf(nextc, "%d\n%d\n", Alist->PlusEntriesInUse,
		Alist->MinusEntriesInUse);
	nextc += strlen(nextc);
	for (i = 0; i < Alist->PlusEntriesInUse + Alist->MinusEntriesInUse; i++){
		if (AL_IdToName(Alist->ActualEntries[i].Id, nextc) < 0)
			sprintf(nextc, "%d", Alist->ActualEntries[i].Id);
		nextc += strlen(nextc);
		sprintf(nextc, "\t%d\n", Alist->ActualEntries[i].Rights);
		nextc += strlen(nextc);
	}
	return (0);
}


/* On successful return, Alist will define a newly-created access list
   corresponding to  the external access list defined by Elist. Returns 0 on
   successful conversion. Returns -1 if ANY name in the access list is not
   translatable, or if the total number of entries is greater than
   AL_MaxExtEntries. */
int AL_Internalize(IN AL_ExternalAccessList Elist, OUT AL_AccessList **Alist){
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

	for (i = 0; i < (*Alist)->TotalNoOfEntries; i++){
		if (sscanf(nextc, "%s\t%d\n", tbuf,
			   &((*Alist)->ActualEntries[i].Rights)) != 2){
			AL_FreeAlist(Alist);
			return(-1);
		}
		if (AL_NameToId(tbuf, &((*Alist)->ActualEntries[i].Id)) < 0){
			AL_FreeAlist(Alist);
			return(-1);
		}
		nextc = (char *)(1 + strchr(nextc, '\n'));
	}
	/* Sort positive and negative entries */
	qsort( (char *)&((*Alist)->ActualEntries[0]),p,sizeof(AL_AccessEntry), 
	       (int (*)(const void *, const void *))CmpPlus); 
	qsort( (char *)&((*Alist)->ActualEntries[m]),m, sizeof(AL_AccessEntry),
 	       (int (*)(const void *, const void *))CmpMinus); 
	return(0);
}


/* Returns in WhichRights, the rights possessed by CPS on Alist */
int AL_CheckRights(IN AL_AccessList *Alist, IN PRS_InternalCPS *CPS,
		   OUT int *WhichRights){
	int plusrights;		/* plus rights accumulated so far */
	int minusrights;	/* minus rights accumulated so far */
	int a;			/* index into next entry in Alist */
	int c;			/* index into next entry in CPS */

	/* Absolutely no positive rights */
	if (Alist->PlusEntriesInUse <= 0 || CPS->InclEntries <= 0){
		*WhichRights = 0; return(0);
	}

	/* Each iteration eats up exactly one entry from either Alist or CPS.
	   Duplicate Entries in access list ==> accumulated rights are obtained
	   Duplicate Entries in CPS ==> irrelevant */
	plusrights = 0;
	c = a = 0;
	while ( (a < Alist->PlusEntriesInUse)  && (c < CPS->InclEntries) )
		switch (CmpInt(&(Alist->ActualEntries[a].Id),
			       &(CPS->IdList[c])))
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
			printf("CmpInt() returned bogus value. Aborting...\n");
			abort();
	    }
	    
	    
	/* Now walk on MinusEntries */
	minusrights = 0;
	c = 0;
	a = Alist->PlusEntriesInUse;
	while ( (c < CPS->InclEntries) && (a < Alist->TotalNoOfEntries))
		switch (CmpInt(&(Alist->ActualEntries[a].Id),
			       &(CPS->IdList[c])))
		{
		case -1:
			a += 1;
			break;
		
		case 0:
			minusrights |= Alist->ActualEntries[a].Rights;
			a += 1;
			break;
		
		case 1:
			c += 1;
			break;	    
		
		default:
			printf("CmpInt() returned bogus value. Aborting...\n");
			abort();
		}
	    
	*WhichRights = plusrights & (~minusrights);
	return(0);
}



/* Initializes the access list package. Version should always be AL_VERSION.
   
   This routine may be called many times -- it will perform reinitialization
   each time.  Synchronization code here guarantees that the .pdb and .pcf
   files are mutually consistent, provided all updaters follow the locking
   discipline.  */
int AL_Initialize(IN char *Version) {
	LogMsg(1,AL_DebugLevel,stdout,"AL_Initialize(%s)", Version);
	LogMsg(4,AL_DebugLevel,stdout,
	       "Library version: '%s'\tHeader version: '%s'\t",
	       AL_VERSION,Version);

	CODA_ASSERT(strcmp(Version, AL_VERSION) == 0);
	CODA_ASSERT(PDB_db_exists());

	return(0);
}


/* Translates the username or groupname defined by Name to Id.
   Returns 0 on success, -1 if translation fails. */
int AL_NameToId(IN char *Name, OUT int *Id){
	LogMsg(1, AL_DebugLevel, stdout, "in AL_NameToId(%s)", Name);
	if(Name == NULL) return -1;
	PDB_lookupByName(Name, (int32_t *) Id);
	return (*Id == 0)?-1:0;
}


/* Translates Id and returns the corresponding username or groupname in Name.
   Returns 0 on success, -1 if Id is not translatable. */
int AL_IdToName(IN int Id, OUT char *Name){
	char *tmp;
	LogMsg(1, AL_DebugLevel, stdout, "in AL_IdToName(%d)", Id);
	if(Id==0) return -1;
	PDB_lookupById((int32_t) Id, &tmp);
	if(tmp != NULL){
		strcpy(Name,tmp);
		free(tmp);
		return 0;
	}
	return -1;
}


/* On successful return, ICPS defines a newly-created data structure,
   corresponding to the internal CPS of Id.
   Return 0  on success; -1 if Id is not a valid user or group. -2 otherwise */
int AL_GetInternalCPS(IN int Id, OUT PRS_InternalCPS **ICPS){	
	PDB_HANDLE h;
	PDB_profile profile;
	
	LogMsg(1,AL_DebugLevel,stdout,"in AL_GetInternalCPS(%d, 0x%x)",
	       Id, ICPS);

	h = PDB_db_open(O_RDONLY);
	PDB_readProfile(h, Id, &profile);
	PDB_db_close(h);

	if((Id == 0) || (profile.id != Id)){
		*ICPS = NULL;
		return -1;
	}

	if(AL_NewCPS(pdb_array_size(&(profile.cps)),ICPS) != 0){
		PDB_freeProfile(&profile);
		return -2;
	}
	if(((*ICPS)->InclEntries=pdb_array_to_array((*ICPS)->IdList,
						   &(profile.cps))) < 0){
		AL_FreeCPS(ICPS);
		PDB_freeProfile(&profile);
		return -2;
	}
	(*ICPS)->ExclEntries=0;
	PDB_freeProfile(&profile);
	return 0;
}



/* On successful return, ECPS defines a newly-created data structure,
   corresponding to the external CPS of Id. Return 0  on success; -1 if Id is
   not a valid user or group id. */
int AL_GetExternalCPS(IN int Id, OUT PRS_ExternalCPS *ECPS){
	PRS_InternalCPS *ICPS;
	register char *s;
	register int i;

	LogMsg(1,AL_DebugLevel,stdout,"in AL_GetExternalCPS(%d, 0x%x)",
	       Id,ECPS);

	if (AL_GetInternalCPS(Id, &ICPS) != 0)
		return(-1);
	AL_NewExternalCPS(ICPS->InclEntries, ECPS);
	s = *ECPS;
	sprintf(s, "%d\n", ICPS->InclEntries);
	s += strlen(s);
	for (i = 0; i < ICPS->InclEntries; i++){
		if (AL_IdToName(ICPS->IdList[i], s) != 0)
			sprintf(s, "%d", ICPS->IdList[i]);
		s += strlen(s);
		if(i < ICPS->InclEntries){
			s[0] = ' ';
			s[1] = '\0';
			s++;
		}
	}
	AL_FreeCPS(&ICPS);
	return(0);
}



/* Returns 0 iff Id is in the CPS. Else returns -1. */
int AL_IsAMember(int Id, PRS_InternalCPS *CPS){
	int i;
    
	/* Yes, I know we can do a binary search on IdList */
	for (i=0;i < CPS->InclEntries;i++)
		if (CPS->IdList[i] == Id) return(0);
	return(-1);
}


int CmpPlus(AL_AccessEntry *a, AL_AccessEntry *b){
	if (a->Id < b->Id) return(-1);
	if (a->Id == b->Id) return(0);
	return(1);
}

int CmpMinus(AL_AccessEntry *a, AL_AccessEntry *b){
	if (a->Id > b ->Id) return(-1);
	if (a->Id == b->Id) return(0);
	return(1);
}

int CmpInt(int *x, int *y){
	if (*x < *y) return (-1);
	if (*x == *y) return (0);
	return (1);
}



/* Displays the access list A on stdout. Returns 0.*/
void AL_PrintAlist(IN AL_AccessList *A){
	int i;
	printf("MySize = %d  Version = %d\n", A->MySize, A->Version);
	printf("TotalNoOfEntries = %d  PlusEntriesInUse = %d  MinusEntriesInUse = %d\n",
	       A->TotalNoOfEntries, A->PlusEntriesInUse, A->MinusEntriesInUse);
	printf("Plus Entries:\n");
	for (i = 0;i < A->PlusEntriesInUse;i++)
		printf("\t%d\t%d\n", A->ActualEntries[i].Id,
		       A->ActualEntries[i].Rights);
	printf("Minus Entries:\n");
	for (;i < A->PlusEntriesInUse + A->MinusEntriesInUse;i++)
		printf("\t%d\t%d\n", A->ActualEntries[i].Id,
		       A->ActualEntries[i].Rights);
	fflush(stdout);
}
    
/* Displays the external access list E on stdout. Returns 0. */
void AL_PrintExternalAlist(IN AL_ExternalAccessList E){
	printf("%s\n", E);
}


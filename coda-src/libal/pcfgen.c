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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/libal/pcfgen.c,v 4.2 1998/05/15 16:55:02 braam Exp $";
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
#include <sys/file.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
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

#define MAXUID	10000	/* UIDs are in the range 1:MAXUID */
			/* 0 <= HighestUID <= MAXUID */		
#define MAXGID  1000   	/* GIDs are in the range -1:-MAXGID */
			/* 0 <= -HighestGID <= MAXGID */
#define POOLSIZE (MAXUID+MAXGID)*PRS_MAXNAMELEN/4	/* Average name expected to be 25% of max */
							/* LitPoolSize <= POOLSIZE  */



static int AvoidCheckSum;
static int InitGlobals();
static  int RecordEntry(IN int WhereIsIt);



int main(int argc, char *argv[])
    {
    int i;
    char *p;
    char tmp[MAXSTRLEN+1];

#define ABORT {flock(fileno(yyin), LOCK_UN); fclose(yyin); exit(-1);}	

    AvoidCheckSum = MyDebugFlag = 0;
#if YYDEBUG != 0
    yydebug = 0;
#endif

    FileRoot[0] = '\0';
    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-x") == 0)
	    {
	    MyDebugFlag++;
	    continue;
	    }

	if (strcmp(argv[i], "-c") == 0)
	    {
	    AvoidCheckSum++;
	    continue;
	    }

	if (i < argc-1)
	    {
	    printf("Usage: pcfgen [-c] [-x] file\n");
	    exit(-1);
	    }

	if (SafeStrCat(FileRoot, argv[i], MAXSTRLEN) < 0)
	    {
	    printf("String too long; increase MAXSTRLEN\n");
	    exit(-1);
	    }
	}
    /* strip off suffix */
    p = rindex (FileRoot, '.');
    if (p != NULL && strcmp (p, ".pdb") == 0)
	*p = 0;

    SourceLineNumber = 1;

    if (SafeStrCpy(tmp,FileRoot,MAXSTRLEN) < 0)
	{printf("String too long; increase MAXSTRLEN\n"); exit(-1);}
    if (SafeStrCat(tmp, ".pdb", MAXSTRLEN) < 0)
    	{printf("String too long; increase MAXSTRLEN\n"); exit(-1);}


    if ((yyin = fopen(tmp, "r")) == NULL)
	{
	perror(tmp);
	exit(-1);
	}

    if (flock(fileno(yyin), LOCK_SH) < 0)
	{
	perror(tmp);
	exit(-1);
	}
	
    if (InitGlobals() != 0)
	{
	perror("InitGlobals");
	ABORT;
	}

    /* Obtain check sum of input file */
    if (!AvoidCheckSum)
	{
	PDBCheckSum = ComputeCheckSum(yyin);
	rewind(yyin);
	}
    else PDBCheckSum = 0;
    
    /* Then parse each entry in it */
    yyinFileName = tmp;
    do
	{
	int tt = BytesScannedByLex;
	yyparse();
	if (MyDebugFlag) PrintEntry(); 
	if (p_TypeOfEntry != EMPTYDEF) RecordEntry(tt);
	}
    while (p_TypeOfEntry != EMPTYDEF);

    if (MyDebugFlag)
	printf ("HighestUID = %d    HighestGID = %d    LitPoolSize = %d\n", 
		HighestUID, HighestGID, LitPoolSize);



    qsort((char *)Usorted, 1+HighestUID, sizeof(int), 
	  (int (*)(const void *, const void *))CmpUn);
    if (MyDebugFlag)
	{
	for (i=0; i < 1+HighestUID; i++)
	    if (Uoffsets[Usorted[i]] > -1)
		printf("%d    %s    %d    %d\n", i, LitPool+Uoffsets[Usorted[i]], Usorted[i], Useeks[Usorted[i]]);
	    else printf("%d    %s    %d    %d\n", i, "*****", Usorted[i], Useeks[Usorted[i]]);
	}

    qsort((char *)Gsorted, 1-HighestGID, sizeof(int), 
	  (int (*)(const void *, const void *))CmpGn);
    if (MyDebugFlag)
	{
	for (i=0; i < 1-HighestGID; i++)
	    if (Goffsets[Gsorted[i]] > -1)
		printf("%d    %s    %d    %d\n", i, LitPool+Goffsets[Gsorted[i]], Gsorted[i], Gseeks[Gsorted[i]]);
	    else printf("%d    %s    %d    %d\n", i, "*****", Gsorted[i], Gseeks[Gsorted[i]]);
	}
    
    if (SafeStrCpy(tmp,FileRoot,MAXSTRLEN) < 0)
	{printf("String too long; increase MAXSTRLEN\n"); ABORT;}
    if (SafeStrCat(tmp, ".pcf", MAXSTRLEN) < 0)
	{printf("String too long; increase MAXSTRLEN\n"); ABORT;}
    if (pcfWrite(tmp) != 0)
	{
	perror(tmp);
	ABORT;
	}
    if (flock(fileno(yyin), LOCK_UN) < 0)
	{
	perror("pcfgen");
	exit(-1);
	}
    fclose(yyin);


    };



static int InitGlobals()
    {
    int i;
    LitPoolSize = HighestUID = HighestGID = 0;

    /* Allocate tables */
    if ( (Uoffsets = (int *)calloc(MAXUID+1, sizeof(int))) == 0)
	return(-1);
    if ( (Usorted = (int *)calloc(MAXUID+1, sizeof(int))) == 0)
	return(-1);
    if ( (Useeks = (int *)calloc(MAXUID+1, sizeof(int))) == 0)
	return(-1);

    if ( (Goffsets = (int *)calloc(MAXGID, sizeof(int))) == 0)
	return(-1);
    if ( (Gsorted = (int *)calloc(MAXGID, sizeof(int))) == 0)
	return(-1);
    if ( (Gseeks = (int *)calloc(MAXGID, sizeof(int))) == 0)
	return(-1);

    if ( (LitPool = (char *)malloc(POOLSIZE)) == 0)
	return(-1);
	
    /* Initialize tables */
    for (i=0; i < MAXUID; i++)
	{
	Useeks[i] = Uoffsets[i] = -1;
	Usorted[i] = i;
	}
    for (i=0; i < MAXGID; i++)
	{
	Gseeks[i] = Goffsets[i] = -1;
	Gsorted[i] = i;
	}
    LitPool[0] = 0;
    BytesScannedByLex = 0;
    return(0);
    }



static  int RecordEntry(IN int WhereIsIt)
    /* An user or group entry has been successfully parsed, starting at byte WhereIsIt of
	the .pdb file; fill its info into .pcf tables.
	Return 0 on success, -1 and a message on failure */
    {
#define err(x) {printf("%s", x); return(-1);}

    if (SafeStrCpy(&LitPool[LitPoolSize], p_Name, POOLSIZE-LitPoolSize) < 0)
	err("Insufficient space: increase POOLSIZE");
    if (p_TypeOfEntry == USERDEF)
	{
	if (p_Id > MAXUID)
	    err("Too large UID: increase MAXUID");
	if (HighestUID < p_Id) HighestUID = p_Id;
	Uoffsets[p_Id] = LitPoolSize;
	Useeks[p_Id] = WhereIsIt;
	}
    else
	{
	if (p_Id < -MAXGID)
	    err("Too large GID: increase MAXGID");
	Goffsets[-p_Id] = LitPoolSize;
	Gseeks[-p_Id] = WhereIsIt;
	if (HighestGID > p_Id) HighestGID = p_Id;
	}
    LitPoolSize += 1+strlen(p_Name);
    return(0);
#undef err
    }

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
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>

#include <errno.h>
#include <stdarg.h>
#include <util.h>
#ifdef __cplusplus
}
#endif

#include "prs.h"
#include "al.h"

#include <codaconf.h>
#include <vice_file.h>

static char *vicedir = NULL;

extern int AL_DebugLevel;

union GenericPtr
    {
    AL_AccessList *A;
    AL_ExternalAccessList E;
    PRS_InternalCPS *C;
    PRS_ExternalCPS EC;
    };

struct slot
    {
    int Allocated;		/* TRUE if this entry is allocated */
    union GenericPtr Gptr;
    };

#define SLOTTYPES 4
#define AVEC 0
#define EVEC 1
#define CVEC 2
#define ECVEC 3

#define SLOTMAX 10

struct slot Vec[SLOTTYPES][SLOTMAX];

static void Op_1(), Op_2(), Op_3(), Op_4(), Op_5(), Op_6();
static int AskSlot(IN int VecType, IN char *Prompt);
static int NewSlot(IN int VecType);
static int GetInputOutput(OUT FILE **infile, OUT FILE **outfile);

void
ReadConfigFile()
{
    /* Load configuration file to get vice dir. */
    codaconf_init("server.conf");

    CODACONF_STR(vicedir, "vicedir", "/vice");

    vice_dir_init(vicedir, 0);
}


int main(int argc, char *argv[])
    {
    int DoMore, MajorOp, i;

    ReadConfigFile();

    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-x") == 0 && i < argc -1)
	    {AL_DebugLevel = atoi(argv[++i]); continue;}
	printf("Usage: altest  [-x debuglevel]\n");
	exit(-1);
	}    
	    
    i = 0;
    if (AL_Initialize(AL_VERSION) < 0)
	{printf("Initialize failed\n."); exit(-1);}
    else printf("AL_Initialize-->%d\n", ++i);
    
    DoMore = TRUE;
    while(DoMore)
	{
	printf("MajorOp: (0 for help) ");
	scanf("%d", &MajorOp);
	switch(MajorOp)
	    {
	    case 0:	/* Help */
		    printf("%s%s%s",
		    		"1: Translate  2: Alist  3: ExternalAlist\n",
				"4: InternalCPS  5: ExternalCPS  6: Misc\n",
				"7: DebugLevel  8: Quit\n");
		    break;
		    
	    case 1:	/* Translate */
		    Op_1();
		    break;
	    
	    case 2:	/* Alist */
		    Op_2();
		    break;
		    
	    case 3:	/* ExternalAlist */
		    Op_3();
		    break;

	    case 4:	/* CPS */
		    Op_4();
		    break;

	    case 5:	/* ExternalCPS */
		    Op_5();
		    break;

	    case 6:	/* CheckAccess */
		    Op_6();
		    break;

	    case 7:	/* Set Debug Level */
		    printf("New debug level: ");
		    scanf("%d", &AL_DebugLevel);
		    break;

	    case 8:	/* Quit */
		    DoMore = FALSE;
		    break;
		    
	    default:
		    printf("Huh? (0 for help)\n");
		    break;
		    
	    }

	}
    return 0;
    }

static void Op_1()		/* Name-Id Translation */
    {
    int MinorOp, Id;
    char Name[PRS_MAXNAMELEN];


    while(TRUE)
	{
	printf("Translate: MinorOp? ");
	scanf("%d", &MinorOp);
	switch(MinorOp)
	    {
	    case 0:
		    printf("1: NameToId  2: IdToName  3: Quit\n");
		    break;
	    
	    case 1: 	/* Name to Id */
		    printf("Name: "); 
		    scanf("%s", Name);
		    printf("AL_NameToId() = %d\n", AL_NameToId(Name, &Id));
		    printf("Id = %d\n", Id);
		    break;
    
	    case 2:		/* Id to Name */
		    printf("Id: "); 
		    scanf("%d", &Id);
		    printf("AL_IdToName() = %d\n", AL_IdToName(Id, Name));
		    printf("Name = %s\n", Name);
		    break;
		    
	    case 3: 	/* Quit */
		    return;
    
	    default:
		    printf("Huh? (0 for help)\n");
		    break;
	    }
	}
			
    }

static void Op_2()		/* Alist: New, Free, Fill, Print, Externalize, hton, ntoh */
    {
    int MinorOp;

    while (TRUE)
	{
	printf("Alist: MinorOp? ");
	scanf("%d", &MinorOp);
	switch(MinorOp)
	    {
	    case 0:
		    printf("%s%s",
			"1: New  2: Free  3: Fill  4: Print\n",
			"5: Externalize  6: hton  7: ntoh  8: Quit\n");
		    break;
	    
	    case 1:	/* New Alist */
		    {
		    int i, MinNoOfEntries;
    
		    if ( (i = NewSlot(AVEC)) < 0)break;
		    printf("MinNoOfEntries: ");
		    scanf("%d", &MinNoOfEntries);
		    printf("AL_NewAlist() = %d\n", AL_NewAlist(MinNoOfEntries, &(Vec[AVEC][i].Gptr.A)));
		    printf("Slot %d allocated for %p\n", i, Vec[AVEC][i].Gptr.A);
		    break;
		    }
		    
	    case 2:	/* Free Alist */
		    {
		    int w;
		    if ( (w = AskSlot(AVEC, "Which Alist slot? ")) < 0) break;
		    printf("AL_FreeAlist() = %d\n", AL_FreeAlist(&(Vec[AVEC][w].Gptr.A)));
		    Vec[AVEC][w].Allocated = FALSE;
		    break;
		    }
    
    
	    case 3:	/* Fill Alist */
		    {
		    int i, t, w;
		    AL_AccessList *ThisAl;
		    FILE *myin, *myout;
    
		    if ( (w = AskSlot(AVEC, "Which Alist slot? ")) < 0) break;
		    ThisAl = Vec[AVEC][w].Gptr.A;

		    if (GetInputOutput(&myin, &myout) < 0) break;
		
		    fprintf(myout, "How many plus entries?");
		    fscanf (myin, "%d", &(ThisAl->PlusEntriesInUse));
		    fprintf(myout, "How many minus entries?");
		    fscanf (myin, "%d", &(ThisAl->MinusEntriesInUse));
		    if (ThisAl->TotalNoOfEntries  < ThisAl->PlusEntriesInUse + ThisAl->MinusEntriesInUse)
			printf("Only %d entries available\n", ThisAl->TotalNoOfEntries);
		    else
			{
			t = ThisAl->TotalNoOfEntries;
			for (i = 0; i < ThisAl->PlusEntriesInUse; i++)
			    {
			    fprintf(myout, "%d-th positive entry:   ", i);
			    fscanf(myin, "%d %d", &(ThisAl->ActualEntries[i].Id),
				    &(ThisAl->ActualEntries[i].Rights));
			    }
			for (i = 0; i < ThisAl->MinusEntriesInUse; i++)
			    {
			    fprintf(myout, "%d-th negative entry:  ", i);
			    fscanf(myin, "%d%d", &(ThisAl->ActualEntries[t-i-1].Id),
				    &(ThisAl->ActualEntries[t-i-1].Rights));
			    }
			}
		    if (myin != stdin)
			{fclose(myin); fclose(myout);}
		    break;
		    }
    
    
	    case 4:	/* Print  Alist */
		    {
		    int w;
		    AL_AccessList *A;
    
		    if ( (w = AskSlot(AVEC, "Which Alist slot? ")) < 0) break;
		    A = Vec[AVEC][w].Gptr.A;
    		    AL_PrintAlist(A);
		    break;
		    }
    
	    case 5: /* Externalize */
		    {
		    int i, w;
    
		    if ( (w = AskSlot(AVEC, "Which Alist? ")) < 0) break;
		    if ( (i = NewSlot(EVEC)) < 0) break;
		    printf("Vec[%d][%d] allocated\n", EVEC, i);
    
		    if (AL_Externalize(Vec[AVEC][w].Gptr.A, &(Vec[EVEC][i].Gptr.E)) != 0)
			{printf("AL_Externalize() failed.\n");break;}
		    break;
		    }
    
    
	    case 6:		/* hton */
		    {
		    int w;
    
		    if ( (w = AskSlot(AVEC, "Which Alist slot? ")) < 0) break;

		    printf("AL_htonAlist() = %d\n",
		    	AL_htonAlist(Vec[AVEC][w].Gptr.A));
		    break;
		    }
		    
 
	    case 7:		/* ntoh */
		    {
		    int w;
    
		    if ( (w = AskSlot(AVEC, "Which Alist slot? ")) < 0) break;

		    printf("AL_ntohAlist() = %d\n",
		    	AL_ntohAlist(Vec[AVEC][w].Gptr.A));
		    break;
		    }
		    
	    case 8:		/* Quit*/
		    return;
    
	    default:
		    printf("Huh? (0 for help)\n");
		    break;
	
	    }
	}
    }

static void Op_3()		/* ExternalAlist:   New, Free, Fill, Print, Internalize, hton, ntoh */
    {
    int MinorOp;
    
    while(TRUE)
	{
	printf("ExternalAlist: MinorOp? ");
	scanf("%d", &MinorOp);
	switch(MinorOp)
	    {
	    case  0:
		    printf("%s%s",
			"1: New  2: Free  3: Fill  4: Print\n",
			"5: Internalize 6: Quit\n");
		    break;
    
	    case 1:	/* New External Alist */
		    {
		    int i, MinNoOfEntries;
    
		    if ( (i = NewSlot(EVEC)) < 0)break;
		    printf("MinNoOfEntries: ");
		    scanf("%d", &MinNoOfEntries);
		    printf("AL_NewExternalAlist() = %d\n",
			    AL_NewExternalAlist(MinNoOfEntries, &(Vec[EVEC][i].Gptr.E)));
		    printf("Slot %d allocated for %p\n", i, Vec[EVEC][i].Gptr.E);
		    break;
		    }
		    
	    case 2:	/* Free External Alist */
		    {
		    int w;
    
		    if ( (w = AskSlot(EVEC, "Which ExternalAlist? ")) < 0) break;
		    printf("AL_FreeExternalAlist() = %d\n", AL_FreeExternalAlist(&(Vec[EVEC][w].Gptr.E)));
		    Vec[EVEC][w].Allocated = FALSE;
		    break;
		    }
    
		    
	    case 3:	/* Fill External Alist */
		    {
		    register int i;
		    register char *s;
		    int w, m, p;
		    FILE *myin, *myout;
    
		    if ( (w = AskSlot(EVEC, "Which ExternalAlist? ")) < 0) break;
		    s = Vec[EVEC][w].Gptr.E;
		    if (GetInputOutput(&myin, &myout) < 0)break;
		    
		    fprintf(myout, "How many plus entries?");
		    fscanf (myin, "%d", &p);
		    sprintf(s, "%d\n", p);
		    s += strlen(s);
		    
		    fprintf(myout, "How many minus entries?");
		    fscanf (myin, "%d", &m);
		    sprintf(s, "%d\n", m);
		    s += strlen(s);
    
    
		    for (i = 0; i < p; i++)
			{
			fprintf(myout, "%d-th positive entry:   ", i);
			fscanf(myin, "%s", s);
			strcat(s, "\t");
			s += strlen(s);
			fscanf(myin, "%s", s);
			strcat(s, "\n");
			s += strlen(s);
			}
		    for (i = 0; i < m; i++)
			{
			fprintf(myout, "%d-th negative entry:  ", i);
			fscanf(myin, "%s", s);
			strcat(s, "\t");
			s += strlen(s);
			fscanf(myin, "%s", s);
			strcat(s, "\n");
			s += strlen(s);
			}
		    if (myin != stdin)
			{fclose(myin); fclose(myout);}
		    break;
		    }
		    
    
	    case 4:	/* Print External Alist */
		    {
		    int w;
    
		    if ( (w = AskSlot(EVEC, "Which ExternalAlist? ")) < 0) break;
    		    AL_PrintExternalAlist(Vec[EVEC][w].Gptr.E);
		    break;
		    }
    
	    case 5:		 /* Internalize */
		    {
		    int i, w, rc;

		    if ( (w = AskSlot(EVEC, "Which ExternalAlist? ")) < 0) break;
		    if ( (i = NewSlot(AVEC)) < 0) break;
		    printf("Vec[%d][%d] allocated\n", AVEC, i);
    
		    rc = AL_Internalize(Vec[EVEC][w].Gptr.E, &(Vec[AVEC][i].Gptr.A));
		    if (rc)
			{printf("AL_Internalize() failed %d.\n", rc);break;}
		    break;
		    }
    
	    case 6:		/* quit */
		    return;
    
	    default:
		    printf("Huh? (0 for help)\n");
		    break;
	
	    }    
	}
    }

static void Op_4()		/* InternalCPS: New, Free, Get, Print, hton, ntoh */
    {
    int MinorOp;

    while(TRUE)
	{
	printf("InternalCPS: MinorOp? ");
	scanf("%d", &MinorOp);
	
	switch(MinorOp)
	    {
	    case  0:
		    printf("%s%s",
			"1: New  2: Free  3: Get\n",
			"4: Print  5: hton  6: ntoh 9: Quit\n");
		    break;
    
	    case 1:	/* New CPS */
		    {
		    int i, MinNoOfEntries;
    
		    if ( (i = NewSlot(CVEC)) < 0)break;
		    printf("MinNoOfEntries: ");
		    scanf("%d", &MinNoOfEntries);
		    printf("AL_NewCPS() = %d\n",
			    AL_NewCPS(MinNoOfEntries, &(Vec[CVEC][i].Gptr.C)));
		    printf("Slot %d allocated for %p\n", i, Vec[CVEC][i].Gptr.C);
		    break;
		    }
		    
	    case 2:	/* Free CPS */
		    {
		    int w;
    
		    if ( (w = AskSlot(CVEC, "Which CPS? ")) < 0) break;
		    printf("AL_FreeCPS() = %d\n", AL_FreeCPS(&(Vec[CVEC][w].Gptr.C)));
		    Vec[CVEC][w].Allocated = FALSE;
		    break;
		    }

	    case 3:		/* Get Internal CPS */
		    {
		    int i, Id;
	
		    if ( (i = NewSlot(CVEC)) < 0)break;
		    printf("Id: ");scanf("%d", &Id);
	
		    printf("AL_GetInternalCPS() = %d\n", AL_GetInternalCPS(Id, &(Vec[CVEC][i].Gptr.C)));
		    printf("Slot %d allocated for %p\n", i, Vec[CVEC][i].Gptr.C);
		    break;
		    }
	
	
	    case 4:		/* Print Internal CPS */
		    {
		    int i, w;
	
		    if ( (w = AskSlot(CVEC, "Which InternalCPS? ")) < 0) break;
		    
		    printf("InclEntries = %d\nIdList = ", (Vec[CVEC][w].Gptr.C)->InclEntries);
		    for(i=0; i < (Vec[CVEC][w].Gptr.C)->InclEntries; i++)
			printf("\t%d", (Vec[CVEC][w].Gptr.C)->IdList[i]);
		    printf("\n");		    
		    printf("ExclEntries = %d\nIdList = ", (Vec[CVEC][w].Gptr.C)->ExclEntries);
		    for(i=(Vec[CVEC][w].Gptr.C)->InclEntries; 
			i < (Vec[CVEC][w].Gptr.C)->InclEntries + (Vec[CVEC][w].Gptr.C)->ExclEntries;
			i++)
			printf("\t%d", (Vec[CVEC][w].Gptr.C)->IdList[i]);
		    printf("\n");		    
		    break;
		    }
	    
	    case 5:		/* hton */
		    {
		    int w;
    
		    if ( (w = AskSlot(CVEC, "Which InternalCPS slot? ")) < 0) break;

		    printf("AL_htonCPS() = %d\n",
		    	AL_htonCPS(Vec[CVEC][w].Gptr.C));
		    break;
		    }
    
	    case 6:		/* ntoh */
		    {
		    int w;
    
		    if ( (w = AskSlot(CVEC, "Which InternalCPS slot? ")) < 0) break;

		    printf("AL_ntohCPS() = %d\n",
		    	AL_ntohCPS(Vec[CVEC][w].Gptr.C));
		    break;
		    }
    
	    case 9:	/* quit */
		    return;
    
	    default:
		    printf("Huh? (0 for help)\n");
		    break;
    
	    }
	}

    }

static void Op_5()		/* ExternalCPS:  New, Free, Get, Print, hton, ntoh */
    {
    int MinorOp;

    while(TRUE)
	{    
	printf("ExternalCPS: MinorOp? ");
	scanf("%d", &MinorOp);
	
	switch(MinorOp)
	    {
	    case  0:
		    printf("%s%s",
			"1: New  2: Free  3: Get  4: Print\n",
			"5:  Quit\n");
		    break;

	    case 1:	/* New External CPS */
		    {
		    int i, MinNoOfEntries;
    
		    if ( (i = NewSlot(ECVEC)) < 0)break;
		    printf("MinNoOfEntries: ");
		    scanf("%d", &MinNoOfEntries);
		    printf("AL_NewExternalCPS() = %d\n",
			    AL_NewExternalCPS(MinNoOfEntries, &(Vec[ECVEC][i].Gptr.EC)));
		    printf("Slot %d allocated for %p\n", i, Vec[ECVEC][i].Gptr.EC);
		    break;
		    }
		    
	    case 2:	/* Free External CPS */
		    {
		    int w;
    
		    if ( (w = AskSlot(ECVEC, "Which ExternalCPS? ")) < 0) break;
		    printf("AL_FreeExternalCPS() = %d\n", AL_FreeExternalCPS(&(Vec[ECVEC][w].Gptr.EC)));
		    Vec[ECVEC][w].Allocated = FALSE;
		    break;
		    }

    
	    case 3:	/* Get External CPS */
		    {
		    int i, Id;
    
		    if ( (i = NewSlot(ECVEC)) < 0)break;
		    printf("Id: ");scanf("%d", &Id);
    
		    printf("AL_GetExternalCPS() = %d\n", AL_GetExternalCPS(Id, &(Vec[ECVEC][i].Gptr.EC)));
		    break;
		    }
    
    
	    case 4:	/* Print External CPS */
		    {
		    int  w;
    
		    if ( (w = AskSlot(ECVEC, "Which ExternalCPS? ")) < 0) break;
		    printf("%s\n", Vec[ECVEC][w].Gptr.EC);
		    printf("\n\n");		    
		    break;
		    }
	    
	    
	    case 5:	/* Quit */
		    return;
    
	    default:
		    printf("Huh? (0 for help)\n");
		    break;
    
	    }
	}
    }

static void Op_6()		/* Miscellaneous:  CheckRights */
    {
    int MinorOp;

    while (TRUE)
	{
	printf("Misc: MinorOp? ");
	scanf("%d", &MinorOp);
	
	switch(MinorOp)
	    {
	    case 0:
		printf("%s",
			"1: CheckRights  2: IsAMember 3: Quit\n");
		break;
		
	    case 1:	    /* Check Rights */
		{
		int w1, w2, r;
		if ( (w1 = AskSlot(AVEC, "Which Alist? ")) < 0) break;
		if ( (w2 = AskSlot(CVEC, "Which InternalCPS? ")) < 0) break;
		
		printf("AL_CheckRights() = %d\n",
		    AL_CheckRights(Vec[AVEC][w1].Gptr.A, Vec[CVEC][w2].Gptr.C, &r));
		printf("rights = 0x%x\n", r);
		}
		break;
		
	    case 2:	    /* Test membership */
		{
		int id, w;

		printf("Which Id? ");
		scanf("%d", &id);
		if ( (w = AskSlot(CVEC, "Which InternalCPS? ")) < 0) break;
		
		printf("AL_IsAMember() = %d\n",
		    AL_IsAMember(id, Vec[CVEC][w].Gptr.C));
		}
		break;

	    case 3:	/* Quit */
		    return;
    
	    default:
		printf("Huh? (0 for help)\n");
		break;
	    }
	}
    }

static int AskSlot(IN int VecType, IN char *Prompt)
    {
    int w;
    do {
      printf("%s", Prompt);
    } while (scanf ("%d", &w) < 1);
    if (w < 0 || w >= SLOTMAX)
	{printf("C'mon: 0 <= which one < %d\n", SLOTMAX); return(-1);}
    if (Vec[VecType][w].Allocated != TRUE) 
	{printf("Not allocated\n"); return(-1);}
    return(w);

    }

static int NewSlot(IN int VecType)
    {
    int i;
    for (i = 0; i < SLOTMAX; i++)
	if (Vec[VecType][i].Allocated == FALSE) break;
    if (i >= SLOTMAX)
	{printf("No free slots"); return(-1);}
    Vec[VecType][i].Allocated = TRUE;
    Vec[VecType][i].Gptr.A= NULL;
    return(i);
    }

static int GetInputOutput(OUT FILE **infile, OUT FILE **outfile)
    {
    char filename[100];
    printf("Filename: ");
    scanf("%s", filename);
    if (strcmp(filename, "-") == 0)
	{*infile = stdin; *outfile = stdout;}
    else
	{
	if ((*outfile = fopen("/dev/null", "w")) == NULL)
	    {perror("/dev/null"); return(-1);}
	if ( (*infile = fopen(filename, "r")) == NULL)
	    {perror(filename); fclose(*outfile); return(-1);}
	}
    return(0);
    }

%{
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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
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
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif __MACH__
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <errno.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "prs.h"
#include "al.h"
#include "pcf.h"
#include "parsepdb.h"

#define	YYDEBUG 0
#define DIE(x) {perror(x); fflush(stderr); abort();}

int temp_Bound;
int temp_Count;
int *temp_List;

PRIVATE void yyerror(IN char *s);
PRIVATE int BigEnough(INOUT int *b, IN int c, IN int s, INOUT char **a);
PRIVATE void SwapAndClear(INOUT int **l, INOUT int *b, INOUT int *c);

%}


	    /* YACC Declarations Section */
%start OneDef
%token IDENTIFIER	302	/* Letter followed by sequence of letters and digits  */
%token GROUPIDENTIFIER 303	/* Identifier:Identifier with embedded periods */
%token DECNUMBER	311	/* Unsigned decimal number */
%token HEXNUMBER	312	/* Unsigned hex number */

%%	    /* YACC rules section */
OneDef		:	EmptyDef
			    {
			    if (yydebug) PrintEntry();
			    }
		|	UserDef
			    {
			    if (yydebug) PrintEntry();
			    }
		|	GroupDef
			    {
			    if (yydebug) PrintEntry();
			    }
		;

EmptyDef	:	';'
			    {
			    if (MyDebugFlag) printf("EmptyDef\n");
			    p_TypeOfEntry = EMPTYDEF;
			    }
		;

UserDef		:	UserName PlusNumber UList1 UList2 UList3 AccessList ';'
			    {
			    p_TypeOfEntry = USERDEF;
			    p_Id = $2;
			    if (MyDebugFlag) printf("UserDef: UserName = %s  UID = %d\n", p_Name, $2);
			    }
		;

GroupDef	:	GroupName MinusNumber PlusNumber GList1 GList2 GList3 AccessList ';'
			    {
			    p_TypeOfEntry = GROUPDEF;
			    p_Id = $2;
			    p_Owner = $3;
			    if (MyDebugFlag) printf("GroupDef: GroupName = %s  GID = %d   Owner = %d\n", p_Name, $2, $3);
			    }
		;

UserName	:	IDENTIFIER
			    {
			    if (SafeStrCpy(p_Name, (char *)$1, PRS_MAXNAMELEN) < 0)
				yyerror("String too long: increase PRS_MAXNAMELEN");
			    /* we will soon be parsing lists */
			    temp_Count = 0;
			    p_List1Count = p_List2Count = p_List3Count = p_PlusCount = p_MinusCount = 0;
			    }
		;

GroupName	:	GROUPIDENTIFIER
			    {
			    if (SafeStrCpy(p_Name, (char *)$1, PRS_MAXNAMELEN) < 0)
				yyerror("String too long: increase PRS_MAXNAMELEN");
			    /* we will soon be parsing lists */
			    temp_Count = 0;
			    p_List1Count = p_List2Count = p_List3Count = p_PlusCount = p_MinusCount = 0;
			    }
		;

UList1		:	GroupIdList
			    {
			    SwapAndClear(&p_List1, &p_List1Bound, &p_List1Count);
			    }
		;

UList2		:	GroupIdList
			    {
			    SwapAndClear(&p_List2, &p_List2Bound, &p_List2Count);			    
			    }
		;

UList3		:	GroupIdList
			    {
			    SwapAndClear(&p_List3, &p_List3Bound, &p_List3Count);			    
			    }
		;

GList1		:	GroupIdList
			    {
			    SwapAndClear(&p_List1, &p_List1Bound, &p_List1Count);
			    }

		;

GList2		:	GroupIdList
			    {
			    SwapAndClear(&p_List2, &p_List2Bound, &p_List2Count);			    		    
			    }
		;

GList3		:	AnyIdList
			    {
			    SwapAndClear(&p_List3, &p_List3Bound, &p_List3Count);		    
			    }

		;

GroupIdList	:	'(' MinusNumberList ')'
		;

MinusNumberList	:	/* Null */
		|	MinusNumberList MinusNumber
			{
			if (BigEnough(&temp_Bound, temp_Count, sizeof(int), (char **)&temp_List) != 0)
			    {
			    }
			temp_List[temp_Count] = $2;
			temp_Count += 1;
			}
		;
		
AnyIdList	:	'(' NumberList ')'
		;

NumberList	:	/* Null */
		|	NumberList Number
			{
			if (BigEnough(&temp_Bound, temp_Count, sizeof(int), (char **)&temp_List) != 0)
			    DIE("BigEnough");
			temp_List[temp_Count] = $2;
			temp_Count += 1;
			}
		;
		
AccessList	:	'(' '+' PlusList ')'  '(' '-' MinusList ')'
		;
		

PlusList	:	/* Null */
		|	PlusList '(' Number Number ')'
			{
			if (BigEnough(&p_PlusBound, p_PlusCount, sizeof(AL_AccessList), (char **)&p_Plus) != 0)
			    DIE("BigEnough");
			p_Plus[p_PlusCount].Id = $3;
			p_Plus[p_PlusCount].Rights = $4;
			p_PlusCount += 1;
			}
		;

MinusList	:	/* Null */
		|	MinusList '(' Number Number ')'
			{
			if (BigEnough(&p_MinusBound, p_MinusCount, sizeof(AL_AccessList), (char **)&p_Minus) != 0)
			    DIE("BigEnough");
			p_Minus[p_MinusCount].Id = $3;
			p_Minus[p_MinusCount].Rights = $4;
			p_MinusCount += 1;
			}
		;

Number		:	PlusNumber
			{$$ = $1;}
		|	MinusNumber
			{$$ = $1;}
		;

PlusNumber	:	DECNUMBER
			    {
			    int x;
			    x = atoi((char *)$1);
			    $$ = x;
			    }
		|	'+' DECNUMBER
			    {
			    int x;
			    x = atoi((char *)$2);
			    $$ = x;
			    }
		|	HEXNUMBER
			    {
			    int x;
			    sscanf((char *)($1+2), "%x",&x);
			    $$ = x;
			    }
		|	'+' HEXNUMBER
			    {
			    int x;
			    sscanf((char *)($2+2), "%x",&x);
			    $$ = x;
			    }
		;
		
MinusNumber	:	'-' DECNUMBER
			    {
			    int x;
			    x = atoi((char *)$2);
			    $$ = -x;
			    }
		|	'-' HEXNUMBER
			    {
			    int x;
			    sscanf((char *)($2+2), "%x",&x);
			    $$ = -x;
			    }
		;

%%
		/* Program Section */
PRIVATE void yyerror(char *s)
    {
    char msg[100];
    sprintf(msg, "\"%s\", line %d: %s\n", yyinFileName, SourceLineNumber, s);
    DIE(msg);
    };


PRIVATE int BigEnough(INOUT int *b, IN int c, IN int s, INOUT char **a)
/*  b current bound 
    c current count 
    s size of an element
    a pointer to current array 
*/
    {
    if (c < *b) return(0);
    if (*b == 0)
	{
	*a = (char *)malloc(10*s);
	*b = 10;
	}
    else
	{
	*a = (char *)realloc(*a, 2*(*b)*s);
	*b = 2*(*b);
	}
    if (*a == NULL) return (-1);    
    else return(0);
    }

PRIVATE void SwapAndClear(INOUT int **l, INOUT int *b, INOUT int *c)
    {
    /* Swap temp list with real one; then  empty temp list  */
    int *iptr, t;
    iptr = *l; *l = temp_List; temp_List = iptr;
    t = *b; *b = temp_Bound; temp_Bound = t;
    *c = temp_Count;
    temp_Count = 0;
    }


void PrintEntry()
    /* Intended for debugging. Prints details of the recently-parsed entry */
    {
    int i;
    printf("p_TypeOfEntry: ");
    switch(p_TypeOfEntry)
	{
	case USERDEF: 	printf("USERDEF\n"); break;
	case GROUPDEF: 	printf("GROUPDEF\n"); break;
	case EMPTYDEF:	printf("EMPTYDEF\n"); return;
	default: 	printf("?????\n");break;
	}
    /* Only user and group defs get this far */
    printf("p_Name = \"%s\"    p_Id = %d", p_Name, p_Id);
    if (p_TypeOfEntry == GROUPDEF)
	printf("    p_Owner = %d\n", p_Owner);
    else printf("\n");
    
    printf("p_List1Bound = %d    p_List1Count = %d\np_List1 = (", p_List1Bound, p_List1Count);
    for (i = 0; i < p_List1Count; i++) printf(" %d ", p_List1[i]);
    printf(")\n");

    printf("p_List2Bound = %d    p_List2Count = %d\np_List2 = (", p_List2Bound, p_List2Count);
    for (i = 0; i < p_List2Count; i++) printf(" %d ", p_List2[i]);
    printf(")\n");

    printf("p_List3Bound = %d    p_List3Count = %d\np_List3 = (", p_List3Bound, p_List3Count);
    for (i = 0; i < p_List3Count; i++) printf(" %d ", p_List3[i]);
    printf(")\n");

    printf("p_PlusBound = %d    p_PlusCount = %d\np_Plus = (", p_PlusBound, p_PlusCount);
    for (i = 0; i < p_PlusCount; i++) printf(" (%d  %d) ", p_Plus[i].Id, p_Plus[i].Rights);
    printf(")\n");

    printf("p_MinusBound = %d    p_MinusCount = %d\np_Minus = (", p_MinusBound, p_MinusCount);
    for (i = 0; i < p_MinusCount; i++) printf(" (%d  %d) ", p_Minus[i].Id, p_Minus[i].Rights);
    printf(")\n\n");


    }


#include "lex.yy.c"


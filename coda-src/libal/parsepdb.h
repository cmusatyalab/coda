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


#define YYDEBUG 0

#define MAXSTRLEN 100


extern char FileRoot[];

extern int yydebug;
extern int MyDebugFlag;
extern int SourceLineNumber;

extern int yyparse();
extern FILE *yyin;	/* Defined in lex.yy.c; where Lex gets its input from  */
extern char *yyinFileName;	/* Name of file that corresponds to yyin */

extern int BytesScannedByLex;	/* Number of bytes of the input file that Lex has scanned in parsing this entry 
				    Warning: this is what LEX has scanned; the last few of these bytes 
				    may have been lookahead for YACC */


/* ------- Data Structures filled by successful parse of an entry ----------*/


extern int p_TypeOfEntry;		/* USERDEF, GROUPDEF or EMPTYDEF */
#define USERDEF 1
#define GROUPDEF 2
#define EMPTYDEF 3

extern char p_Name[PRS_MAXNAMELEN+1];	/* name of user or group */
extern int p_Id;			/* user or group id */
extern int p_Owner;			/* only filled for groups */

extern int p_List1Bound;		/* max no. of items that can fit in p_List1 */
extern int p_List1Count;		/* actual no. of items in p_List1 */
extern int *p_List1;			/* pointer to array of ids */

extern int p_List2Bound;
extern int p_List2Count;
extern int *p_List2;

extern int p_List3Bound;
extern int p_List3Count;
extern int *p_List3;

	
extern int p_PlusBound;			/* max number of entries in p_Plus */ 
extern int p_PlusCount;			/* actual number of entries in p_Plus */
extern AL_AccessEntry *p_Plus;		/* pointer to array of plus access list entries */

extern int p_MinusBound;
extern int p_MinusCount;
extern AL_AccessEntry *p_Minus;

extern void PrintEntry();
/* ------------------------------------------------------------ */

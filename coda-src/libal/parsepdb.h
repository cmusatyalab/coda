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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/libal/Attic/parsepdb.h,v 4.1 1997/01/08 21:49:43 rvb Exp $";
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

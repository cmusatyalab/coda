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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/libal/Attic/parsepdb.lex,v 4.3 1998/05/15 16:55:00 braam Exp $";
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

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "prs.h"
#include "al.h"
#include "parsepdb.h"

int yylook(); /* forward refs; lex doesn't emit defs for C++ */
int yyback(int *p, int m); /* ditto */

#if YYDEBUG != 0
# define RETURN(Arg) if (yydebug != 0) printf("Lex: token:%d\tvalue:'%s'\n",Arg,yylval);return(Arg);
#else
# define RETURN(Arg) return(Arg);
#endif
#define YYLVAL 1000 /* length of yytext */

%}

%%
\n	{SourceLineNumber += 1; BytesScannedByLex += 1; yyleng = 0; yytext[0] = 0; yymore();}

#.*			{/* Comment */
			BytesScannedByLex += yyleng;
			yyleng = 0;
			yytext[0] = 0;
			yymore();
			}

[\040\t\014]		{/* White space */
			BytesScannedByLex += 1;
			yyleng -= 1;
			yytext[yyleng] = '\0';
			yymore();
			}

[A-Za-z][A-Za-z0-9\.]*\:[A-Za-z][A-Za-z0-9\.]*	{BytesScannedByLex += yyleng; yylval = (int) yytext;  RETURN(GROUPIDENTIFIER);}

[A-Za-z][A-Za-z0-9]*	{BytesScannedByLex += yyleng; yylval = (int) yytext;  RETURN(IDENTIFIER);}

0x[0-9a-f]+		{BytesScannedByLex += yyleng; yylval = (int )yytext;  RETURN(HEXNUMBER);}

[0-9]+			{BytesScannedByLex += yyleng; yylval = (int )yytext;  RETURN(DECNUMBER);}

;			{
			static int SawSemiColon=0;
			if (SawSemiColon)
			    {
			    SawSemiColon = 0;
			    yylval = 0; RETURN(0);
			    }
			else
			    {
			    SawSemiColon = 1; unput(';');
			    BytesScannedByLex += yyleng; yylval = (int)yytext; RETURN((int) *yytext);
			    }
			}

.	{BytesScannedByLex += yyleng; yylval = (int) yytext; RETURN((int) *yytext);}

%%
yywrap()
    {
    RETURN(1);
    };


%{
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


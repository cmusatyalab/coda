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

#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "rp2.h"
#include "y.tab.h"

#define RETURN(Arg) if (yydebug != 0) printf("Lex: token:%d\n", Arg); return(Arg)

extern FILE *file, *hfile;
extern char *file_name;
int line;

static rp2_bool included();
static void include();
int yywrap();

extern int yydebug;
extern YYSTYPE yylval;

extern char *copy();

extern int cpathcnt;
extern char **cpatharray;

/* Stack of unput chars */
#define MAX_UNPUT_CHARS 10

struct {
    int		unput_top;
    char	unput_chars[MAX_UNPUT_CHARS];
} unput_stack;

/*
 * i386_nbsd1's flex defines input() as a function which uses the macro 
 * YY_INPUT rather than using a macro input().
 */
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size)                          \
do {                                                           \
    int c;                                                     \
                                                               \
    if (unput_stack.unput_top > 0) {                           \
	c = unput_stack.unput_chars[--unput_stack.unput_top];  \
    } else {                                                   \
	c = (file == NULL ? getchar() : getc(file));           \
    }                                                          \
    result = (c == EOF) ? YY_NULL : (buf[0] = c, 1);           \
} while (0)               

/* 
 * But, unput is a macro in both lex and flex.  God only knows what will
 * happen with POSIX.
 */
#undef unput
static unput(c)
    char c;
{
    if (c == '\0') return;

    if (unput_stack.unput_top >= MAX_UNPUT_CHARS) {
	printf("Too many UNPUT's: %d\n", MAX_UNPUT_CHARS);
	exit(1);
    }

    unput_stack.unput_chars[unput_stack.unput_top++] = c;
}
%}

 /* No distinction between upper & lower case in reserved words */

A		[aA]
B		[bB]
C		[cC]
D		[dD]
E		[eE]
F		[fF]
G		[gG]
H		[hH]
I		[iI]
K		[kK]
L		[lL]
M		[mM]
N		[nN]
O		[oO]
P		[pP]
Q		[qQ]
R		[rR]
S		[sS]
T		[tT]
U		[uU]
V		[vV]
W		[wW]
X		[xX]
Y		[yY]
Z		[zZ]

SPACE		[ \t\014]

%%

"%{"				{IncludeVerbatim();}

\n				{ line++; }

{C}{L}{I}{E}{N}{T}		{ RETURN(CLIENT); }

{I}{N}				{ RETURN(IN); }

{N}{E}{W}_{C}{O}{N}{N}{E}{C}{T}{I}{O}{N}	{ RETURN(NEW_CONNECTION); }

{O}{U}{T}			{ RETURN(OUT); }

{P}{R}{E}{F}{I}{X}		{ RETURN(PREFIX); }

{R}{P}{C}2_{E}{N}{U}{M}		{ RETURN(RPC2_ENUM); }

{R}{P}{C}2_{S}{T}{R}{U}{C}{T}	{ RETURN(RPC2_STRUCT); }

{S}{E}{R}{V}{E}{R}		{ RETURN(SERVER); }

{S}{U}{B}{S}{Y}{S}{T}{E}{M}	{ RETURN(SUBSYSTEM); }

{T}{I}{M}{E}{O}{U}{T}		{ RETURN(TIMEOUT);}

{T}{Y}{P}{E}{D}{E}{F}		{
				  yylval.u_bool = !included();
				  RETURN(TYPEDEF);
				}

"/*"				{ ConsumeComment(); }

\"[^"]*\"			{
				  yylval.u_string = copy(yytext);
				  RETURN(STRING);
				}

[A-Za-z_][A-Za-z0-9_$-]*		{
				  if (yytext[0] == '_') {
				      printf("RP2GEN: identifier name may not begin with '_' %s\n", yytext);
				      exit(1);
				  }
				  yylval.u_string = copy(yytext);
				  RETURN(IDENTIFIER);
				}

"-"[0-9]+			|
[0-9]+				{
				  yylval.u_string = copy(yytext);
				  RETURN(NUMBER);
				}

";"				{ RETURN(';'); }

"("				{ RETURN('('); }

")"				{ RETURN(')'); }

","				{ RETURN(','); }

"{"				{ RETURN('{'); }

"}"				{ RETURN('}'); }

"="				{ RETURN('='); }

"["				{ RETURN('['); }

"]"				{ RETURN(']'); }

":"				{ RETURN(':'); }

"#"{I}{N}{C}{L}{U}{D}{E}	{ include(); }

"#"{D}{E}{F}{I}{N}{E}		{
				  yylval.u_bool = !included();
				  return(DEFINE);
				}

{SPACE}*			;

.				{
				  printf("[Line %d] Illegal character ignored: %3o (octal)\n",
					 line, 
					 yytext[0]);
				}

%%

ConsumeComment()
{
    /* terminated only by a star slash */
    char c, d;		/* d: most recent; c: last but one */
    d = '\0';
    for (;;) {
	c = d;
	d = input();
	if (d == 0)
	    break;		/* eof */
	if ((c == '*') && (d == '/'))
	    break;
	if (d == '\n') line++;
    }
}

IncludeVerbatim()
{
    /* terminated only by a %} */
    register char c, d;		/* d: most recent; c: last but one */
    d = '\0';
    for (;;) {
	c = d;
	d = input();
	if (d == 0)    break;		/* eof */
	if (d == '\n') line++;
	if ((c == '%') && (d == '}'))   {fprintf(hfile, "\n"); break;}
	if (c != '\0') fprintf(hfile, "%c", c);
    }
}

/*************************\
* 			  *
*  Handle include files   *
* 			  *
\*************************/

/* Stack of included files */
#define MAX_INCLUDE_FILES 5

static struct {
    int 	include_top;
    FILE	*files[MAX_INCLUDE_FILES];
    int 	lines[MAX_INCLUDE_FILES];
    char	*names[MAX_INCLUDE_FILES];
} include_stack;

/**********************************************\
* 					       *
*  Tunable definitions for INCLUDE mechanism   *
* 					       *
\**********************************************/


static rp2_bool included()
{
    return include_stack.include_top > 0;
}

static FILE *inc_open(name)
    char *name;
{
    FILE *f;
    char **d;

    /* 1st try name without any qualification */
    f = fopen(name, "r");
    if (f != NIL) return f;

    return NIL;
}

rp2_bool include2(name, proc)
    char *name, *proc;
{
    FILE *f;

    /* Attempt to open file */
    f = inc_open(name);
    if (f == NIL) {
	printf("Can't find %s file: %s\n", proc, name);
	return RP2_FALSE;
    }

    /* Stack old file */
     if (file != NIL) {
	include_stack.lines[include_stack.include_top] = line;
	include_stack.files[include_stack.include_top] = file;
	include_stack.names[include_stack.include_top++] = file_name;
    }
    line = 1;
    file = f;
    file_name = name;
    return RP2_TRUE;
}

static void include()
{
    register char c;
    char name[MAXPATHLEN+1], fullname[MAXPATHLEN+1];
    struct stat statbuf;
    int i, foundit;

    /* Read input file name */

    /* Skip blanks */
    do { c = input(); } while (c == ' ' || c == '\t');
    /* Read file name */
    if (c != '"') {
	puts("Bad INCLUDE file name");
	while (c != '\n') c = input();
	return;
    }
    c = input();
    i = 0;
    while (c != '"') {
	if (c == '\n') {
	    puts("INCLUDE file name missing closing \"");
	    break;
	}
	if (i >= MAXPATHLEN) {
	    puts("ERROR: include file name too long");
	    exit(-1);
	}
	name[i++] = c;
	c = input();
    }
    c = input();
    if (c == '\n') line++;
    name[i] = '\0';

    /* See if too many include files */
    if (include_stack.include_top >= MAX_INCLUDE_FILES) {
	printf("ERROR: include file %s  nested too deep\n", name);
	exit(-1);
    }

    /* Find the included file using the search list */
    strcpy(fullname, name);
    foundit = 0;
    if (name[0] == '/')
	{/* absolute path */
	if (stat(fullname, &statbuf) == 0) foundit = 1;
	}
    else
	{/* relative path */
	for (i = 0; i <= cpathcnt; i++)
	    {
	    if (stat(fullname, &statbuf) == 0)
		{foundit = 1; break;}
	    if (i == cpathcnt) break;
	    strncpy(fullname, cpatharray[i], MAXPATHLEN);
	    strncat(fullname, "/", MAXPATHLEN);
	    strncat(fullname, name, MAXPATHLEN);
	    }
	}

    if (!foundit)
	{
	printf("ERROR: can't find include file %s\n", name);
	exit(-1);
	}

    /* Include the file now and in the generated header  */
    spit_include(name); /* should we be using fullname here? debatable! */
    include2(fullname, "INCLUDE");
}

int yywrap()
{
    /* Close old file */
    if (file != NIL) fclose(file);

    if (include_stack.include_top == 0)
	return 1;
    else {
	file = include_stack.files[--include_stack.include_top];
	file_name = include_stack.names[include_stack.include_top];
    }
    line = include_stack.lines[include_stack.include_top];
    return 0;
}

yyerror(s)
    char *s;
{
    printf("FATAL ERROR, line %d: %s\n", line, s);
    exit(1);
}

yywarn(s)
    char *s;
{
    printf("WARNING, line %d: %s\n", line, s);
    return(0);
}

init_lex()
{
    file = NIL;		/* Indicate no input file yet */
    line = 1;
    include_stack.include_top = 0;
    unput_stack.unput_top = 0;
}

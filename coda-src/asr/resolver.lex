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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/asr/Attic/resolver.lex,v 4.3 1997/02/26 16:02:29 rvb Exp $";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "y.tab.h"
#include "asr.h"
#include <stdio.h>
#ifdef __MACH__
#include <libc.h>
#endif
#if defined(__linux__) || defined(__BSD44__)
#include <stdlib.h>
#endif /* __linux__ || __BSD44__ */
extern int yylook();

#define YYERRCODE	256		/* gross hack to simulate error */
int yywrap() {
	return(1);
}
int yyback(int *p, int m);
int yyoutput(int);
static void yyunput(int, char *);

#ifdef __cplusplus
}
#endif __cplusplus
int context = FILE_NAME_CTXT;

%}
%option yylineno
integer		[0-9]+
wspace	[ \t]
filenamechar	[a-zA-Z0-9\/\*\?\.\-\#]
argnamechar	[a-zA-Z0-9\/\.\-\$\*\#\<\>\!]
depnamechar	[a-zA-Z0-9\/\.\-\_]
cmdnamechar	[a-zA-Z0-9\/\.\-\_]
%%
{wspace}+	;
":"		{ return(COLON); }
";"		{ return(SEMI_COLON); }
","		{ return(COMMA); }
"*"		{ if (context == ARG_CTXT) 
			return(ALL); 
		  else { REJECT;}
		}
\\\n		;

^(({wspace}*\n)|(!.*\n))+ {
			char c;
			c = input();
			unput(c);
			if (c != '!') {
	    	 	   DEBUG((stdout, "Debug: Returning blank_line\n"));
			   return(BLANK_LINE);
			}
		}
\n		{ DEBUG((stdout, "Debug: returning new_line\n")); return(NEW_LINE); }
"["		{ if (context == ARG_CTXT) 
			return('[');
		  else {
		     DEBUG((stderr, 
                             "error: line %d [ allowed only for args of commands\n",
			     yylineno));
		     return(YYERRCODE);
		  }
		}
"]"		{ if (context == ARG_CTXT)
			return(']');
		  else {
		     DEBUG((stderr, 
                             "error: line %d ] allowed only for args of commands\n",
			     yylineno));
		     return(YYERRCODE);
		  }
		}
[0-9]+		{return(INTEGER);}

{depnamechar}+	{if (context == DEP_CTXT) {
			DEBUG((stdout, "Debug: Lex returns dep_name(%s)\n", yytext));
			return(DEPENDENCY_NAME);
		 }
		 if (context == CMD_CTXT) {
			DEBUG((stdout, "Debug: Lex returns cmd_name(%s)\n", yytext));
			return(COMMAND_NAME);
		 }
		 if (context == ARG_CTXT) {
			DEBUG((stdout, "Debug: Lex returns arg_name(%s)\n", yytext));
			return(ARG_NAME);
		 }
		 if (context == FILE_NAME_CTXT) {
			DEBUG((stdout, "Debug: Lex returns object_name(%s)\n", yytext));
			return(OBJECT_NAME);
		 }
		 else {
			DEBUG((stdout, "In depnamechar - couldn't recognize ")); 
			ECHO;
		 }
		}
{argnamechar}+	{ if (context == ARG_CTXT) {
			return(ARG_NAME);
		  }
		  else {
			REJECT;
		  }
		}

{filenamechar}+	{ if (context == FILE_NAME_CTXT) 
			return(OBJECT_NAME);
		 else {
			DEBUG((stdout, "In filenamechar - couldn't recognize ")); 
			ECHO;
		 }
		}
%%
#ifdef notdef 
^\!.*\n		{
			/* comment lines */
			char c;
			c = input();
			unput(c);
			if (c != '!') {
	    	 	   DEBUG((stdout, "Debug: Returning blank_line\n"));
			   return(BLANK_LINE);
			}
		}
#endif notdef 

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
                           none currently

#*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "resolver_parser.h"
#include "asr.h"
#include <stdio.h>
#include <stdlib.h>
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

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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/asr/Attic/resolver.yacc,v 4.1 1997/01/08 21:49:23 rvb Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "asr.h"
#include <stdio.h>
#ifdef	__linux__
#include <stdlib.h>	
#else
#include <libc.h>	
#endif
#include <sys/param.h>
#include <sys/dir.h>
#include <strings.h>
#include <vcrcommon.h>

extern int yylineno;
extern char yytext[];
#ifndef	__linux__
extern int yylex();
#else
extern int yylex ( void );
#endif
#ifdef __cplusplus
}
#endif __cplusplus
extern int yyparse();
#include <olist.h> 
#include "ruletypes.h"

extern olist	rules;
rule_t	*crule;		// current rule 
command_t *ccmd = NULL;
int debug = 0;

/* This file defines the syntax of the rule language used to specify an ASR 
   in a ResolveFile. Some macros understood by the language:
   *, ?, [, ], are all understood as wildcards in object names.
   Further, in the args to commands, $*, $<, $# are also understood
   $* stands for the prefix corresponding to the wildcard *
   $< stands for the file name component of the inconsistent object.
   $> stands for the absolute path name of the parent of the inc object
   $# stands for the number of replicas of an object 
*/
%}
%token COMMA
%token OBJECT_NAME
%token DEPENDENCY_NAME
%token NEW_LINE
%token COLON
%token SEMI_COLON
%token COMMAND_NAME
%token WHITESPACE
%token ARG_NAME
%token ALL
%token INTEGER
%token REPLICA_COUNT
%token BLANK_LINE

%%
start		: BLANK_LINE {crule = new rule_t; }rule_list
		| rule_list
		;
rule_list	: rule_list  rule 
		{	
			context = FILE_NAME_CTXT;
			rules.append(crule);
			crule = new rule_t;
		}
		| rule 	
		{
			context = FILE_NAME_CTXT;
			rules.append(crule);
			crule = new rule_t;
		}
		;

X		: object_list 
		  {DEBUG((stdout, "Debug: Finding :\n"));} 
		  COLON  {context = DEP_CTXT;}
		dependency_list NEW_LINE {context = CMD_CTXT;} command_list
		;

rule		: X BLANK_LINE	{DEBUG((stdout, "end of rule\n"));} 
		| X {DEBUG((stdout, "end of rule\n"));} 
		;


object_list	: object_list COMMA OBJECT_NAME 
		{ 
		   DEBUG((stdout, "Debug: Adding object_name %s\n", yytext));
		   crule->addobject(yytext);
		}
		| OBJECT_NAME 
		{   DEBUG((stdout, "Debug: Found object_name %s \n", yytext));
		    crule->addobject(yytext);
		}
		;

dependency_list	: dependency_list DEPENDENCY_NAME
		{
		   DEBUG((stdout, "Debug: Adding dependency %s\n", yytext));
		   crule->adddep(yytext);
		}
		| /* empty */
		;

command_list	: command_list command terminator 
		| command terminator 
		;

terminator	: NEW_LINE {crule->addcmd(ccmd); ccmd = NULL;}
		| SEMI_COLON {crule->addcmd(ccmd); ccmd = NULL; }
		;

command		: COMMAND_NAME 
		{  
		   context = ARG_CTXT;
		   DEBUG((stdout, "Debug: Adding command name %s\n", yytext));
		   ccmd = new command_t(yytext);
		} 
		arglist {context = CMD_CTXT;}
		;

arglist		: /* empty */
		| arglist arg
		;

arg		: ARG_NAME 
		{   
		   DEBUG((stdout, "Debug: adding arg %s \n", yytext));
		   ccmd->addarg(yytext);
		}
		replica_specifier
		| REPLICA_COUNT
		;

replica_specifier : /* empty */
		| '[' 
		index 
		{
		   DEBUG((stdout, "Debug:Adding replicaid %s\n", yytext));
		   ccmd->addreplicaid(yytext);
		}
		']'
		;

index		: INTEGER
		| ALL
		;

%%

int yyerror(char *s)
{
   fprintf(stderr, "Syntax error in line %d token = %s context = %d\n",
	   yylineno, yytext, context);
   return(0);
}


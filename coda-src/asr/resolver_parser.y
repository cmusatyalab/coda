%{ /* BLURB gpl

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "asr.h"
#include <stdio.h>
#include <stdlib.h>	
#include <sys/param.h>
#include "coda_string.h"
#include <vcrcommon.h>

extern int yylineno;
extern char yytext[];
#ifndef	__linux__
extern int yylex();
#else
extern int yylex ( void );
#endif

#include <sys/types.h>
#include <time.h>
#include <cfs/coda.h>

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
int yyerror(char *s);

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


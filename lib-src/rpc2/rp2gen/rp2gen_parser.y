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
#include <stdlib.h>
#include "rp2.h"

extern int yydebug;
extern int HeaderOnlyFlag;

extern char *copy();
extern RPC2_TYPE *rpc2_enum_type(), *rpc2_struct_type();
extern ENTRY *make_entry(), *find();
extern ENUM *make_enum();
extern VAR *make_var();
extern PROC *make_proc(), *check_proc();
extern void enter();
extern char *concat();
extern struct subsystem subsystem;
extern void no_storage(char *);
extern void spit_define(char *, char *);
extern void insert(PROC *proc);

/* Structure for handling IDENTIFIER lists */

#define MAX_IDS	100

struct {
    char	*ids[MAX_IDS];
    int		counter;
} id_list;

static char **make_id_array()
{
    register char **array;
    register int i;

    array = (char **) calloc(id_list.counter+1, sizeof(char *));
    if (array == NIL) no_storage("make_id_array");
    array[id_list.counter] = NIL;
    for (i=0, id_list.counter--; id_list.counter>=0; i++, id_list.counter--)
	array[i] = id_list.ids[id_list.counter];
    return array;
}

#define MAX_ENUMS	100

struct {
    ENUM	*enums[MAX_ENUMS];
    int		counter;
} enum_list;

static ENUM **make_enum_array()
{
    register ENUM **array;
    register int i;

    array = (ENUM **) calloc(enum_list.counter+1, sizeof(ENUM *));
    if (array == NIL) no_storage("make_enum_array");
    array[enum_list.counter] = NIL;
    for (i=0, enum_list.counter--; enum_list.counter>=0; i++, enum_list.counter--)
	array[i] = enum_list.enums[enum_list.counter];
    return array;
}

#define MAX_FORMALS	100

struct {
    VAR		*formals[MAX_FORMALS];
    int		counter;
} formal_list;

static VAR **make_formal_array()
{
    register VAR **array;
    register int i;

    array = (VAR **) calloc(formal_list.counter+1, sizeof(VAR *));
    if (array == NIL) no_storage("make_formal_array");
    array[formal_list.counter] = NIL;
    for (i=0, formal_list.counter--; formal_list.counter>=0; i++, formal_list.counter--)
	array[i] = formal_list.formals[formal_list.counter];
    return array;
}

#define MAX_VARS	100

struct {
    VAR		**vars[MAX_VARS];
    int		counter;
} var_list;

static int length(p)
    char **p;
{
    register int len;

    for (len=0; *p!=NIL; p++) len++;
    return len;
}

static VAR **make_var_array()
{
    register int count, i;
    register VAR **array;

    for (i=0, count=0; i<var_list.counter; i++) count += length((char **)var_list.vars[i]);
    array = (VAR **) calloc(count+1, sizeof(VAR *));
    if (array == NIL) no_storage("make_var_array");
    array[count] = NIL;

    /* Transfer to array */
    for (i=0, var_list.counter--; var_list.counter>=0; var_list.counter--) {
	register VAR **v;
	for (v = var_list.vars[var_list.counter]; *v != NIL; v++)
	    array[i++] = *v;
    }
    return array;
}

static char *createsize(name)
    char *name;
{
    return concat(name, "_size_");
}

static char *createmaxsize(name)
    char *name;
{
    return concat(name, "_max_size_");
}

int next_opnum = 1;

%}
	    /* YACC Declarations Section */
%start file

%token IDENTIFIER	/* Letter followed by sequence of letters and digits  */
%token NUMBER 		/* arb sequence of digits*/
%token STRING		/* quoted string */
%token SUBSYSTEM
%token TIMEOUT DEFINE NEW_CONNECTION
%token TYPEDEF RPC2_STRUCT RPC2_ENUM
%token CLIENT SERVER PREFIX

%token IN
%token OUT

%union {
    rp2_bool	u_bool;
    MODE	u_mode;
    char	*u_string;
    char	**u_string_array;
    RPC2_TYPE	*u_rpc2_type;
    ENTRY	*u_entry;
    ENUM	*u_enum;
    VAR		*u_var;
    VAR		**u_var_array;
    VAR		***u_var_array_array;
    int         u_int;
}

%type <u_bool> new_connection
%type <u_bool> DEFINE TYPEDEF procedure_description

%type <u_mode> usage

%type <u_string> DefinedNumber String id_number protocol_version
%type <u_string> subsystem_name IDENTIFIER NUMBER STRING array_spec
%type <u_string> timeout_override
%type <u_int>    opcode_number

%type <u_string_array> identifier_list

%type <u_rpc2_type> rpc2_struct rpc2_enum

%type <u_entry> rpc2_type type_name

%type <u_enum> enum_val

%type <u_var> formal array_spec_var

%type <u_var_array> field

%type <u_var_array_array> field_list

%%
	    /* YACC rules section */
file			: prefixes header_line default_timeout decl_or_proc_list
			;

prefixes		: empty
			| prefix
			| prefix prefix
			;

prefix			: SERVER PREFIX String ';'
					{
					    extern char *server_prefix;
					    server_prefix = copy($3+1);
					    server_prefix[strlen(server_prefix)-1] = '\0';
					}
			| CLIENT PREFIX String ';'
					{
					    extern char *client_prefix;
					    client_prefix = copy($3+1);
					    client_prefix[strlen(client_prefix)-1] = '\0';
					}
			;

header_line		: SUBSYSTEM subsystem_name compatability_stuff ';'
					{
					    subsystem.subsystem_name = copy($2+1);
					    subsystem.subsystem_name[strlen(subsystem.subsystem_name)-1] = '\0';
					}
			| empty
			    {
			    printf("NO SUBSYSTEM SPECIFIED: only generating header file\n");
			    HeaderOnlyFlag = 1;
			    }
			;

compatability_stuff	: id_number protocol_version
					{}
			| empty
			;

subsystem_name		: String
					{ $$ = $1; }
			;

id_number		: DefinedNumber
					{ $$ = $1; }
			;

protocol_version	: DefinedNumber
					{ $$ = $1; }
			;

default_timeout		: TIMEOUT '(' DefinedNumber ')' ';'
					{ subsystem.timeout = $3; }
			| empty
					{ subsystem.timeout = NIL; }
			;

decl_or_proc_list	: decl_or_proc_list decl_or_proc
			| empty
			;

decl_or_proc		: typedef
			| define
			| procedure_description
					{}
			;

define			: DEFINE IDENTIFIER NUMBER
					{ if ($1) spit_define($2, $3); }
			| DEFINE IDENTIFIER String			
					{ if ($1) spit_define($2, $3); }		
			;

typedef			: TYPEDEF rpc2_type IDENTIFIER array_spec ';'
					{
					    extern void spit_type();
					    $2 -> name = $3;
					    $2 -> bound = $4;
					    if ($2->bound != NIL && $2->type->tag != RPC2_BYTE_TAG) {
						printf("RP2GEN: array type unimplemented: %s\n",
						       $3);
						exit(1);
					    }
					    enter($2);
					    if ($1) spit_type($2);
					}
			;

rpc2_type		: type_name
					{ $$ = make_entry($1->type, $1); }
			| rpc2_struct
					{ $$ = make_entry($1, NIL); }
			| rpc2_enum
					{ $$ = make_entry($1, NIL); }
			;

type_name		: IDENTIFIER
					{
					    $$ = find($1);
					    if ($$ == NIL) {
						printf("RP2GEN: can't find type: %s\n", $1);
						exit(1);
					    }
					}
			;

rpc2_struct		: RPC2_STRUCT '{' field_list '}'
					{ $$ = rpc2_struct_type(make_var_array()); }
			;

field_list		: field field_list
					{
					    if (var_list.counter >= MAX_VARS) {
						printf("RP2GEN: too many fields: %d\n", MAX_VARS);
						exit(1);
					    }
					    var_list.vars[var_list.counter++] = $1;
					}
			| field
					{
					    var_list.vars[0] = $1;
					    var_list.counter = 1;
					}
			;

field			: type_name identifier_list ';'
					{
					    register char **id;
					    register VAR **p;
					    $$ = (VAR **) calloc(length($2)+1, sizeof(VAR *));
					    if ($$ == NIL) no_storage("field");
					    for (id=$2, p=$$; *id!=NIL; id++, p++)
						*p = make_var(*id, NO_MODE, $1);
					    *p = NIL;
					    free($2);
					}
			;

identifier_list		: identifier_list2
					{ $$ = make_id_array(); }
			;

identifier_list2	: IDENTIFIER ',' identifier_list2
					{
					    if (id_list.counter >= MAX_IDS) {
						printf("RP2GEN: too many identifiers in list: %d\n", MAX_IDS);
						exit(1);
					    }
					    id_list.ids[id_list.counter++] = $1;
					}
			| IDENTIFIER
					{
					    id_list.ids[0] = $1;
					    id_list.counter = 1;
					}
			;

rpc2_enum		: RPC2_ENUM '{' enum_val_list '}'
					{ $$ = rpc2_enum_type(make_enum_array()); }
			;

enum_val_list		: enum_val ',' enum_val_list
					{
					    if (enum_list.counter >= MAX_ENUMS) {
						printf("RP2GEN: too many enum values: %d\n", MAX_ENUMS);
						exit(1);
					    }
					    enum_list.enums[enum_list.counter++] = $1;
					}
			| enum_val
					{
					    enum_list.enums[0] = $1;
					    enum_list.counter = 1;
					}
			;

enum_val		: IDENTIFIER '=' NUMBER
					{ $$ = make_enum($1, $3); }
			;

array_spec		: '[' DefinedNumber ']'
					{ $$ = $2; }
			| empty
					{ $$ = NIL; }
			;

array_spec_var		: '[' ']'
					{
					    $$ = make_var(NIL, NO_MODE, find("RPC2_Integer"));
					}
			| empty
					{ $$ = NIL; }
			;

procedure_description	: opcode_number IDENTIFIER '(' formal_list ')' timeout_override new_connection ';'
                                        { if ($7 == RP2_FALSE) {
                                            if ($1 == -1)
                                                $1 = next_opnum;
                                            if ($1 < next_opnum) {
                                              printf("RP2GEN: Opcode numbers must be always increasing\n");
                                              exit(1);
                                            }
                                            next_opnum = $1 + 1;
                                          }
					  insert(check_proc(make_proc($1, $2, make_formal_array(), $6, $7))); }
			;

opcode_number		: NUMBER ':'
					{ $$ = atoi($1); }

			| empty
					{ $$ = -1; }

			;

formal_list		: formal array_spec_var ',' formal_list
					{
					    register VAR *maxvarp;
					    if (formal_list.counter >= MAX_FORMALS) {
						printf("RP2GEN: too many formals: %d\n", MAX_FORMALS);
						exit(1);
					    }
					    formal_list.formals[formal_list.counter++] = $1;
					    if ($2 != NIL) {
						if (formal_list.counter >= MAX_FORMALS) {
						    printf("RP2GEN: too many formals: %d\n", MAX_FORMALS);
						    exit(1);
						}
					        if ($1->type->type->tag != RPC2_STRUCT_TAG) {
						    printf("RP2GEN: array type unimplemented: %s\n", $1->name);
						    exit(1);
					        } 
					        formal_list.formals[formal_list.counter++] = $2;
						$1->array = $2->name = createsize($1->name);
						$2->mode = $1->mode;
						if ($1->mode != IN_MODE) { 
						    maxvarp = make_var(NIL, NO_MODE, find("RPC2_Integer"));
						    $1->arraymax = maxvarp->name = createmaxsize($1->name);
						    maxvarp->mode = IN_MODE;
						    formal_list.formals[formal_list.counter++] = maxvarp;
						}
					    }
					}
			| formal array_spec_var
					{
					    register VAR *maxvarp;
					    formal_list.formals[0] = $1;
					    formal_list.counter = 1;
					    if ($2 != NIL) {
					        if ($1->type->type->tag != RPC2_STRUCT_TAG) {
						    printf("RP2GEN: array type unimplemented: %s\n",
						           $1 /* Is this a char *?*/);
						    exit(1);
					        } 
					        formal_list.formals[formal_list.counter++] = $2;
						$1->array = $2->name = createsize($1->name);
						$2->mode = $1->mode;
						if ($1->mode != IN_MODE) { 
						    maxvarp = make_var(NIL, NO_MODE, find("RPC2_Integer"));
						    $1->arraymax = maxvarp->name = createmaxsize($1->name);
						    maxvarp->mode = IN_MODE;
						    formal_list.formals[formal_list.counter++] = maxvarp;
						}
					    }

					}
			| empty
					{ formal_list.counter = 0; }
			;

formal			: usage type_name IDENTIFIER
					{
					  $$ = make_var($3, $1, $2);
					}
			;

usage			: IN
					{ $$ = IN_MODE; }
			| OUT
					{ $$ = OUT_MODE; }
			| IN OUT
					{ $$ = IN_OUT_MODE; }
			;

timeout_override	: TIMEOUT '(' DefinedNumber ')'
					{ $$ = $3; }
			| empty
					{ $$ = NIL; }
			;

new_connection		: NEW_CONNECTION
					{ $$ = RP2_TRUE; }
			| empty
					{ $$ = RP2_FALSE; }
			;

DefinedNumber		: NUMBER
					{ $$ = $1; }
			| IDENTIFIER
					{ $$ = $1; }
			;

String			: STRING
					{ $$ = $1; }
/*
			| IDENTIFIER
					{ $$ = $1; }
*/
			;

empty			:
			;
%%

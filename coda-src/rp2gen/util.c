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

static char *rcsid = "$Header: util.c,v 4.1 97/01/08 21:50:18 rvb Exp $";
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

/*******************************************************************\
* 								    *
*	Utility routines for RP2GEN				    *
* 								    *
\*******************************************************************/

#include <stdio.h>
#include "rp2.h"

no_storage(proc)
    char *proc;
{
    printf("[RP2GEN: Out of storage in routine %s]\n", proc);
    exit(1);
}

char *copy(s)
    char *s;
{
    register char *new;
    register int len;

    len = strlen(s) + 1;
    new = (char *) malloc(len);
    if (new == NIL) no_storage("copy");
    bcopy(s, new, len);
    return new;
}

/* Stick proc on END of procedures list */

static struct {
    PROC	*head;
    PROC	*tail;
} procedures = { NIL, NIL };

insert(proc)
    PROC *proc;
{
    proc -> thread = NIL;
    if (procedures.tail == NIL)
	procedures.head = proc;
    else
	procedures.tail -> thread = proc;
    procedures.tail = proc;	
}

PROC *get_head()
{
    return procedures.head;
}

RPC2_TYPE *rpc2_enum_type(values)
    ENUM **values;
{
    register RPC2_TYPE *type;

    type = (RPC2_TYPE *) malloc(sizeof(RPC2_TYPE));
    if (type == NIL) no_storage("rpc2_enum_type");
    type -> tag = RPC2_ENUM_TAG;
    type -> fields.values = values;
    return type;
}

RPC2_TYPE *rpc2_struct_type(struct_fields)
    VAR **struct_fields;
{
    register RPC2_TYPE *type;

    type = (RPC2_TYPE *) malloc(sizeof(RPC2_TYPE));
    if (type == NIL) no_storage("rpc2_struct_type");
    type -> tag = RPC2_STRUCT_TAG;
    type -> fields.struct_fields = struct_fields;
    return type;
}

RPC2_TYPE *rpc2_simple_type(tag)
    TYPE_TAG tag;
{
    register RPC2_TYPE *type;

    type = (RPC2_TYPE *) malloc(sizeof(RPC2_TYPE));
    if (type == NIL) no_storage("rpc2_simple_type");
    type -> tag = tag;
    return type;
}

print_var(v)
    register VAR *v;
{
    switch (v->mode) {
	case NO_MODE:		break;
	case IN_MODE:		printf("IN ");
				break;
	case OUT_MODE:		printf("OUT ");
				break;
	case IN_OUT_MODE:	printf("IN OUT ");
				break;
	default:		printf("RP2GEN [can't happen]: impossible mode for variable: %d\n", v->mode);
				abort();
    }
    printf("%s %s", v->type->name, v->name);
}

VAR *make_var(name, mode, type)
    char *name;
    MODE mode;
    ENTRY *type;
{
    VAR *var;

    var = (VAR *) malloc(sizeof(VAR));
    if (var == NIL) no_storage("make_var");
    var -> name = name;
    var -> mode = mode;
    var -> type = type;
    var -> array = NIL;
    return var;
}

ENTRY *make_entry(type, defined)
    RPC2_TYPE *type;
    ENTRY *defined;
{
    ENTRY *e;

    e = (ENTRY *) malloc(sizeof(ENTRY));
    if (e == NIL) no_storage("make_type");
    e -> thread = NIL;
    e -> type = type;
    e -> defined = defined;
    return e;
}

ENUM *make_enum(name, rep)
    char *name, *rep;
{
    register ENUM *e;

    e = (ENUM *) malloc(sizeof(ENUM));
    if (e == NIL) no_storage("make_enum");
    e -> name = name;
    e -> rep = rep;
    return e;
}

PROC *make_proc(name, formals, timeout, new_connection)
    char *name;
    VAR **formals;
    char *timeout;
    rp2_bool new_connection;
{
    PROC *proc;

    proc = (PROC *) malloc(sizeof(PROC));
    if (proc == NIL) no_storage("make_proc");
    proc -> name = name;
    proc -> formals = formals;
    proc -> timeout = timeout;
    proc -> bd = NIL;
    proc -> op_code = NIL;
    proc -> new_connection = new_connection;
    return proc;
}

PROC *check_proc(proc)
    PROC *proc;
{
    register VAR **formals;

    /* Look for <= 1 RPC2_BulkDescriptor parameter */
    for (formals=proc->formals; *formals!=NIL; formals++)
	if ((*formals)->type->type->tag == RPC2_BULKDESCRIPTOR_TAG)
	    if (proc->bd != NIL) {
		printf("RP2GEN: too many bulk descriptors to proc: %s\n", proc->name);
		exit(1);
	    } else {
		if ((*formals)->mode != IN_OUT_MODE)
		    printf("RP2GEN: usage for RPC2_BulkDescriptor must be IN OUT: %s\n", (*formals)->name);
		proc -> bd = *formals;
	    }
    return proc;
}

char *concat(s1, s2)
    char *s1, *s2;
{
    register char *new;
    register int len1, len2;

    len1 = strlen(s1);
    len2 = strlen(s2);
    new = malloc(len1+len2+1);
    if (new == NIL) no_storage("concat");
    bcopy(s1, new, len1);
    bcopy(s2, new+len1, len2);
    new[len1+len2] = '\0';
    return new;
}

char *concat3elem(s1, s2, s3)
    char *s1, *s2, *s3;
{
    register char *new, *temp;

    temp = concat(s1, s2);
    new = concat(temp, s3);
    free(temp);
    return new;
}

char *basename(name)
    char *name;
{
    register char *p, *l, *r;
    register int len;
    register char *base;

    /* Save pointer to left end  -- i.e., last '/' */
    l = name - 1;
    r = name + strlen(name);
    for (p=name; *p!='\0'; p++)
	switch (*p) {
	    case '/':	l = p;
			break;
	    case '.':	r = p;
			break;
	    default:	;
	}

    len = r - l - 1;
    if (len <= 0) {
	printf("RP2GEN: illegal filename: \"%s\"\n", name);
	exit(1);
    }
    base = malloc(len+1);
    if (base == NIL) no_storage("basename");
    bcopy(l+1, base, len);
    base[len] = '\0';
    return base;
}

char *date()
{
    extern long time();
    extern char *ctime();
    long clock;

    clock = time(0);
    return ctime(&clock);
}

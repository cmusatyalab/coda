/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2016 Carnegie Mellon University
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

/*******************************************************************\
* 								    *
*	Utility routines for RP2GEN				    *
* 								    *
\*******************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rp2.h"

void no_storage(char *proc)
{
    printf("[RP2GEN: Out of storage in routine %s]\n", proc);
    exit(EXIT_FAILURE);
}

char *copy(char *s)
{
    char *new;
    int32_t len;

    len = strlen(s) + 1;
    new = (char *)malloc(len);
    if (new == NIL)
        no_storage("copy");
    memcpy(new, s, len);
    return new;
}

/* Stick proc on END of procedures list */

static struct {
    PROC *head;
    PROC *tail;
} procedures = { NIL, NIL };

void insert(PROC *proc)
{
    proc->thread = NIL;
    if (procedures.tail == NIL)
        procedures.head = proc;
    else
        procedures.tail->thread = proc;
    procedures.tail = proc;
}

PROC *get_head()
{
    return procedures.head;
}

RPC2_TYPE *rpc2_enum_type(ENUM **values)
{
    RPC2_TYPE *type;

    type = (RPC2_TYPE *)malloc(sizeof(RPC2_TYPE));
    if (type == NIL)
        no_storage("rpc2_enum_type");
    type->tag           = RPC2_ENUM_TAG;
    type->fields.values = values;
    return type;
}

RPC2_TYPE *rpc2_struct_type(VAR **struct_fields)
{
    RPC2_TYPE *type;

    type = (RPC2_TYPE *)malloc(sizeof(RPC2_TYPE));
    if (type == NIL)
        no_storage("rpc2_struct_type");
    type->tag                  = RPC2_STRUCT_TAG;
    type->fields.struct_fields = struct_fields;
    return type;
}

RPC2_TYPE *rpc2_simple_type(TYPE_TAG tag)
{
    RPC2_TYPE *type;

    type = (RPC2_TYPE *)malloc(sizeof(RPC2_TYPE));
    if (type == NIL)
        no_storage("rpc2_simple_type");
    type->tag = tag;
    return type;
}

VAR *make_var(char *name, MODE mode, ENTRY *type)
{
    VAR *var;

    var = (VAR *)malloc(sizeof(VAR));
    if (var == NIL)
        no_storage("make_var");
    var->name  = name;
    var->mode  = mode;
    var->type  = type;
    var->array = NIL;
    return var;
}

ENTRY *make_entry(RPC2_TYPE *type, ENTRY *defined)
{
    ENTRY *e;

    e = (ENTRY *)malloc(sizeof(ENTRY));
    if (e == NIL)
        no_storage("make_type");
    e->thread  = NIL;
    e->type    = type;
    e->defined = defined;
    return e;
}

ENUM *make_enum(char *name, char *rep)
{
    ENUM *e;

    e = (ENUM *)malloc(sizeof(ENUM));
    if (e == NIL)
        no_storage("make_enum");
    e->name = name;
    e->rep  = rep;
    return e;
}

PROC *make_proc(int opnum, char *name, VAR **formals, char *timeout,
                rp2_bool new_connection)
{
    PROC *proc;

    proc = (PROC *)malloc(sizeof(PROC));
    if (proc == NIL)
        no_storage("make_proc");
    proc->name           = name;
    proc->formals        = formals;
    proc->timeout        = timeout;
    proc->bd             = NIL;
    proc->op_code        = NIL;
    proc->op_number      = opnum;
    proc->new_connection = new_connection;
    proc->linenum        = line;
    return proc;
}

PROC *check_proc(PROC *proc)
{
    VAR **formals;

    /* Look for <= 1 RPC2_BulkDescriptor parameter */
    for (formals = proc->formals; *formals != NIL; formals++)
        if ((*formals)->type->type->tag == RPC2_BULKDESCRIPTOR_TAG) {
            if (proc->bd != NIL) {
                printf("RP2GEN: too many bulk descriptors to proc: %s\n",
                       proc->name);
                exit(EXIT_FAILURE);
            } else {
                if ((*formals)->mode != IN_OUT_MODE)
                    printf(
                        "RP2GEN: usage for RPC2_BulkDescriptor must be IN OUT: %s\n",
                        (*formals)->name);
                proc->bd = *formals;
            }
        }
    return proc;
}

char *concat(char *s1, char *s2)
{
    char *new;
    int32_t len1, len2;

    len1 = strlen(s1);
    len2 = strlen(s2);
    new  = malloc(len1 + len2 + 1);
    if (new == NIL)
        no_storage("concat");
    memcpy(new, s1, len1);
    memcpy(new + len1, s2, len2);
    new[len1 + len2] = '\0';
    return new;
}

char *concat3elem(char *s1, char *s2, char *s3)
{
    char *new, *temp;

    temp = concat(s1, s2);
    new  = concat(temp, s3);
    free(temp);
    return new;
}

char *coda_rp2_basename(char *name)
{
    char *p, *l, *r;
    int32_t len;
    char *base;

    /* Save pointer to left end  -- i.e., last '/' */
    l = name - 1;
    r = name + strlen(name);
    for (p = name; *p != '\0'; p++)
        switch (*p) {
        case '/':
            l = p;
            break;
        case '.':
            r = p;
            break;
        default:;
        }

    len = r - l - 1;
    if (len <= 0) {
        printf("RP2GEN: illegal filename: \"%s\"\n", name);
        exit(EXIT_FAILURE);
    }
    base = malloc(len + 1);
    if (base == NIL)
        no_storage("basename");
    memcpy(base, l + 1, len);
    base[len] = '\0';
    return base;
}

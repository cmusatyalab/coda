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


/* ************************************************************ *\

	Symbol Table Module for RP2GEN

\* ************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include "rp2.h"

extern ENTRY *make_entry();
extern RPC2_TYPE *rpc2_simple_type();
extern VAR *make_var();
ENTRY *find();

/*
 * If this is not prototyped its return value defaults to int which results
 * in bogus results on systems where sizeof(void*) > sizeof(int).
 */
extern RPC2_TYPE *rpc2_struct_type(VAR **);

/* This module uses external chaining */

#define SYMTAB_SIZE	50

int SizeofStubPredefined;

static ENTRY *table[SYMTAB_SIZE];

static struct {
    char	*name;
    TYPE_TAG	tag;
} predefined[] = {

	{ "RPC2_Integer",		RPC2_INTEGER_TAG },
	{ "RPC2_Unsigned",		RPC2_UNSIGNED_TAG },
	{ "RPC2_Byte",			RPC2_BYTE_TAG },
	{ "RPC2_String",		RPC2_STRING_TAG },
	{ "RPC2_CountedBS",		RPC2_COUNTEDBS_TAG },
	{ "RPC2_BoundedBS",		RPC2_BOUNDEDBS_TAG },
/*
 	{ "RPC2_BulkDescriptor",	RPC2_BULKDESCRIPTOR_TAG },
*/
	{ "SE_Descriptor",		RPC2_BULKDESCRIPTOR_TAG },
	{ "RPC2_EncryptionKey",		RPC2_ENCRYPTIONKEY_TAG },
	{ "RPC2_Double",		RPC2_DOUBLE_TAG },
};

STUBELEM stub_predefined[] = {

        { "RPC2_String", "name" },
	{ "RPC2_Integer", "countent" },
	{ "RPC2_Integer", "countexit" },
	{ "RPC2_Integer", "tsec" },
	{ "RPC2_Integer", "tusec" },
	{ "RPC2_Integer", "counttime" },
	{ NIL, "CallCountEntry" },

        { "RPC2_String", "name" },
	{ "RPC2_Integer", "countent" },
	{ "RPC2_Integer", "countexit" },
	{ "RPC2_Integer", "tsec" },
	{ "RPC2_Integer", "tusec" },
	{ "RPC2_Integer", "counttime" },
	{ "RPC2_Integer", "counthost" },
	{ NIL, "MultiCallEntry" },

	{ "RPC2_Integer", "opengate" },
	{ "RPC2_Integer", "tsec" },
	{ "RPC2_Integer", "tusec" },
	{ NIL, "MultiStubWork" }

};

static int hash(char *name)
{
    unsigned int value;

    for (value=0; *name!='\0'; name++)
	value += *name;
    return value % SYMTAB_SIZE;
}

extern int strcmp();
#define equal(s1, s2) (strcmp(s1, s2) == 0)

ENTRY *find(char *name)
{
    int index;
    ENTRY *p;

    index = hash(name);
    for (p=table[index]; p!=NIL; p=p->thread)
	if (equal(p->name, name)) return p;
    return NIL;
}

void enter(ENTRY *e)
{
    int index;

    if (find(e->name) != NIL) {
	printf("RP2GEN: duplicate identifier: %s, ignoring\n", e->name);
	return;
    }
    index = hash(e->name);
    /* Insert at this bucket */
    e -> thread = table[index];
    table[index] = e;
}


static void init_stubspecial()
{
    ENTRY *entryp;
    ENTRY *typep;
    VAR **vartable;
    VAR **p;
    STUBELEM *ep;
    int n, elemcount;

    SizeofStubPredefined = n = sizeof stub_predefined/sizeof(stub_predefined[0]);
    vartable = p = (VAR **)calloc(n+1, sizeof(VAR *));
    ep = stub_predefined;
    elemcount = 0;

    for (; n > 0; n--, ep++) {
        if (ep->type != NIL && ep->name != NIL) {
	    if ((typep = find(ep->type)) == NIL) {
	        free(vartable);
		return;
	    }
	    *p = make_var(ep->name, NO_MODE, typep);
	    p++;
	    elemcount++;
        } else if (ep->type == NIL && ep->name != NIL && elemcount != 0) {
	    *p = NIL;
	    entryp = make_entry(rpc2_struct_type(vartable), NIL);
	    entryp->name = ep->name;
	    entryp->bound = NIL;
	    enter(entryp);
	    vartable = p = (VAR **)calloc(n+1, sizeof(VAR *));
	    elemcount = 0;
        } else {
	    free(vartable);
	    return;
        }
    }
}

void init_table(void)
{
    int i;

    for (i=0; i<SYMTAB_SIZE; i++) table[i] = NIL;

    /* Enter predefined types */
    for (i=0; i<sizeof predefined/sizeof(predefined[0]); i++) {
	ENTRY *e;
	e = make_entry(rpc2_simple_type(predefined[i].tag), NIL);
	e -> name = predefined[i].name;
	e -> bound = NIL;
	enter(e);
    }
    init_stubspecial();
}


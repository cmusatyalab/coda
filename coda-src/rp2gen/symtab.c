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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/rp2gen/symtab.c,v 1.1.1.1 1996/11/22 19:08:54 rvb Exp";
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


/* ************************************************************ *\

	Symbol Table Module for RP2GEN

\* ************************************************************ */

#include <stdio.h>
#include "rp2.h"

extern ENTRY *make_entry();
extern RPC2_TYPE *rpc2_simple_type();
extern VAR *make_var();
ENTRY *find();

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
	{ "RPC2_EncryptionKey",		RPC2_ENCRYPTIONKEY_TAG }
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

init_table()
{
    register int i;

    for (i=0; i<SYMTAB_SIZE; i++) table[i] = NIL;

    /* Enter predefined types */
    for (i=0; i<sizeof predefined/sizeof(predefined[0]); i++) {
	register ENTRY *e;
	e = make_entry(rpc2_simple_type(predefined[i].tag), NIL);
	e -> name = predefined[i].name;
	e -> bound = NIL;
	enter(e);
    }
    init_stubspecial();
}

static int hash(name)
    register char *name;
{
    register unsigned int value;

    for (value=0; *name!='\0'; name++)
	value += *name;
    return value % SYMTAB_SIZE;
}

extern int strcmp();
#define equal(s1, s2) (strcmp(s1, s2) == 0)

ENTRY *find(name)
    char *name;
{
    register int index;
    register ENTRY *p;

    index = hash(name);
    for (p=table[index]; p!=NIL; p=p->thread)
	if (equal(p->name, name)) return p;
    return NIL;
}

enter(e)
    register ENTRY *e;
{
    register int index;

    if (find(e->name) != NIL) {
	printf("RP2GEN: duplicate identifier: %s, ignoring\n", e->name);
	return;
    }
    index = hash(e->name);
    /* Insert at this bucket */
    e -> thread = table[index];
    table[index] = e;
}

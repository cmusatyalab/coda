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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rp2gen/rp2.h,v 4.2 1997/01/23 14:18:11 lily Exp $";
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


#include "rpc2.h"

/* Satya (7/31/96): changed bool, FALSE, TRUE to rp2_bool, RP2_{FALSE,TRUE}
to avoid name clash with builtin bool on some versions of gcc; similar to
fix in rvmh.h */
typedef unsigned char rp2_bool;

#define RP2_FALSE	0
#define RP2_TRUE	1

#define NIL	0

/* Subsystem information structure */

struct subsystem {
    char	*subsystem_name;
    char	*timeout;
};

/* This structure is used for typed variables (this includes structure fields) */


typedef struct {
    char		*name;
    MODE		mode;	/* Must be NO_MODE for structure fields */
    struct entry	*type;
    char		*array; /* If array structure, array suffix name */
    char                *arraymax; /* if array && !IN_MODE, array_max_size */
} VAR;

/* RPC2 types */

typedef struct {
    char	*name;
    char	*rep;
} ENUM;

typedef struct {
    TYPE_TAG	tag;

    union {

	/* when RPC2_STRUCT_TAG => */
		VAR	**struct_fields;

	/* when RPC2_ENUM_TAG => */
		ENUM	**values;

    }		fields;
} RPC2_TYPE;

/* Symbol table entry */

typedef struct entry {
    struct entry	*thread;	/* For symbol table */
    char		*name;
    char		*bound;		/* NIL => not array, ELSE => bound */
    RPC2_TYPE		*type;		/* Pointer to underlying RPC2_TYPE */
    struct entry	*defined;	/* Pointer to type that this was defined in terms of
					   (or NIL) */
} ENTRY;

typedef struct proc {
    struct proc	*thread;	/* For chaining proc's together */
    char	*name;
    VAR		**formals;
    char	*timeout;	/* NIL => no timeout override */
    VAR		*bd;		/* Pointer to bulk descriptor parameter */
    rp2_bool	new_connection;	/* TRUE if this is the unique new connection procedure */
    char	*op_code;	/* Name of op code for this procedure */
    int		op_number;	/* Opcode number for this proc */
} PROC;

/* Language values are specified for use in array */
typedef enum{ NONE=0, C=1, PASCAL=2, F77=3 } LANGUAGE;


#if defined(CLIENT) || defined(SERVER)
#undef CLIENT
#undef SERVER
#endif
typedef enum{ CLIENT=0, SERVER=1, MULTI=2 } WHO;

typedef struct stubelem {
    char        *type;
    char        *name;
}  STUBELEM;

/* 
 * crout needs to know whether to spit out ansi paste tokens, or
 * traditional ones.
 */

extern rp2_bool ansi;

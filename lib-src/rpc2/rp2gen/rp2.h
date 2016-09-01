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


#include <rpc2/rpc2.h>
#include <stdlib.h>

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
    int         linenum;        /* Line number where this proc was defined */
} PROC;

/* Language values are specified for use in array */
typedef enum{ NONE=0, C=1, PASCAL=2, F77=3 } LANGUAGE;


typedef struct stubelem {
    char        *type;
    char        *name;
}  STUBELEM;

/*
 * crout needs to know whether to spit out ansi paste tokens, or
 * traditional ones.
 */

extern rp2_bool ansi;


/* make line number an externally-accessible variable so it can be
   set on semantic errors for yyerror() and yywarn() */

extern int line;



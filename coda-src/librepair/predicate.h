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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/







typedef int (*PtrFuncInt)(int, resreplica *, resdir_entry **, int);
extern PtrFuncInt Predicates[];

extern int nPredicates;
/* the conflict types order must match the array Predicates order */
#define	STRONGLY_EQUAL	0
#define	WEAKLY_EQUAL	1
#define	ALL_PRESENT	2
#define SUBSET_RENAME	3
#define	SUBSET_CREATE	4
#define	SUBSET_REMOVE	5
#define MAYBESUBSET_REMOVE 6
#define	UNKNOWN_CONFLICT    -1

int ObjectOK C_ARGS((int , resreplica *, resdir_entry **, int nDirEntries));
int SubsetRemove C_ARGS((int , resreplica *, resdir_entry **, int ));
int MaybeSubsetRemove C_ARGS((int , resreplica *, resdir_entry **, int ));
int SubsetCreate C_ARGS((int , resreplica *, resdir_entry **, int ));
int AllPresent C_ARGS((int , resreplica *, resdir_entry **, int ));
int Renamed C_ARGS((int , resreplica *, resdir_entry **, int ));
int WeaklyEqual C_ARGS((int , resreplica *, resdir_entry **, int ));


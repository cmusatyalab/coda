/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


              Copyright (c) 2002-2003 Intel Corporation

#*/


#ifndef _LKA_PRIVATE_H_INCLUDED_
#define _LKA_PRIVATE_H_INCLUDED_ 1

#include <rwcdb.h>
#include <dllist.h>
#include "lka.h"

/* lkdb is an entry defining a lookaside database;
   Venus has a doubly linked list of these entries  */
struct lkdb {
    struct dllist_head chain;
    struct rwcdb dbh;  /* database handle initialized by rwcdb_init() */
    char *dbname;  /* malloc'ed string with name of this db */
    char *dblocation; /* malloc'ed string of getwd() for this db */
    int entrycount; /* number of entries in database */

    /* various statistics */
    int attempts; /* number of lookaside attempts */
    int hits;     /* how many successful */
    int shafails; /* how many hits failed sha verification */
};
  
int lkdb_BindDB(struct lkdb *dbp, char *dbpathame, char *emgs, int emsglen);
int lkdb_GetFilenameFromSHA(struct lkdb *dbp,
			    unsigned char sha[SHA_DIGEST_LENGTH],
			    char *hitname, int hitnamelen,
			    char *emsg, int emsglen);

/* Constructor and destructor */
struct lkdb *new_lkdb(void);
void delete_lkdb(struct lkdb *dbp);

/* First part of descriptor record is always the version string;
   Update the version number as code evolves */
#define LKA_VERSION_STRING "Coda LookAside Database, Version 1.1"

#endif /*_LKA_PRIVATE_H_INCLUDED_ */

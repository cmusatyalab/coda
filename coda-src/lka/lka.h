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


#ifndef _LKA_H_INCLUDED_
#define _LKA_H_INCLUDED_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/param.h>
#include <coda_db.h>


  /* "helper" routines in shaprocs.cc */
extern void ViceSHAtoHex (RPC2_BoundedBS *, char *, int); 

extern int ComputeViceSHA(int, RPC2_BoundedBS *);

extern int IsZeroSHA(RPC2_BoundedBS *);

  /* "core" routines in lka.cc called by venus */
extern int LookAsideAndFillContainer (RPC2_BoundedBS *, char *, int, char *, char *, int);

extern int LKParseAndExecute(char *, char *, int);


#ifdef __cplusplus
}
#endif

#include <dlist.h>



/* lkdb is an entry defining a lookaside database;
   Venus has a doubly linked list of these entries  */

class lkdb: public dlink {
  DB *dbh;  /* database handle returned by dbopen() */

 public:
  char *dbname;  /* malloc'ed string with name of this db */
  char *dblocation; /* malloc'ed string of getwd() for this db */
  int entrycount; /* number of entries in database */

  /* various statistics */
  int attempts; /* number of lookaside attempts */
  int hits;     /* how many successful */
  int shafails; /* how many hits failed sha verification */

  /* various flags */
  unsigned pathnames_are_relative:1; /* TRUE iff all pathnames are relative to dblocation */
  

  int BindDB(char *, char *, int);
  int GetFilenameFromSHA(RPC2_BoundedBS *, char *, int, char *, int);

  /* Constructor and destructor */
  lkdb();
  ~lkdb();
};

/* First part of descriptor record is always the version string;
   Update the version number as code evolves */
#define LKA_VERSION_STRING "Coda LookAside Database, Version 1.0"

#define LKA_ABSPATH_STRING "  Paths = Absolute"

#define LKA_RELPATH_STRING "  Paths = Relative"

#define LKA_NUMENTRIES_STRING "  Entries = "

#define LKA_NUMENTRIES_FIELD_WIDTH 10 /* very generous! */

#define LKA_END_STRING "  End_LookAside"

#endif /*_LKA_H_INCLUDED_ */

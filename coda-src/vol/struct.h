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
                           none currently

#*/

/*      struct.h        4.1     83/05/03        */

/*
 * access to information relating to the fields of a structure
 */

#define fldoff(str, fld)        ((int)&(((struct str *)0)->fld))
#define fldsiz(str, fld)        (sizeof(((struct str *)0)->fld))
#define strbase(str, ptr, fld)  ((struct str *)((char *)(ptr)-fldoff(str, fld)))

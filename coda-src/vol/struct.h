/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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

#include <coda_offsetof.h>

#define fldsiz(type, member)       (sizeof(((type *)0)->member))
#define strbase(type, p, member)   ((type *)((char *)(p) - coda_offsetof(type,member)))


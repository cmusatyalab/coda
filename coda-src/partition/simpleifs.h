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

/* definitions specifically needed for the simple inode operations */ 

#ifndef SIMPLEIFS_INCLUDED
#define SIMPLEIFS_INCLUDED

#include <voltypes.h>
#include <partition.h>

#define	FNAMESIZE	256
#define MAX_NODES	99999
#define	MAX_LINKS	999
#define	FILEDATA	36

#define VICEMAGIC   47114711

struct part_simple_opts {
    Inode next;
};

#endif

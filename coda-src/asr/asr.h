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





/* context declarations */
#define FILE_NAME_CTXT	1001
#define DEP_CTXT	1002
#define CMD_CTXT	1003
#define ARG_CTXT	1004

extern int context;
extern int debug;

#define DEBUG(a)	if (debug) fprintf a

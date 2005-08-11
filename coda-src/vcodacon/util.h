/* 	$Id$	*/

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
*/

// Utility routines ... headers for gui.c

extern char *XferLabel[3];

void MainInit (int *argcp, char ***argvp);

void do_clog(const char *, const char *);

int do_findRealm (const char *);

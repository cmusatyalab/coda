/* 	$Id: util.h,v 1.6 2006-05-24 20:04:40 jaharkes Exp $	*/

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

void do_clog(void);

void do_cunlog(void);

void menu_clog (void);

void menu_ctokens (void);

void menu_cunlog (void);

int do_findRealm (const char *);

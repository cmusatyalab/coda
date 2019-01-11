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

#ifndef _RVMTESTING_H
#define _RVMTESTING_H 1

extern void protect_page(int x);
extern void unprotect_page(int x);
extern void my_sigBus(int sig, int code, struct sigcontext *scp);
#endif _RVMTESTING_H

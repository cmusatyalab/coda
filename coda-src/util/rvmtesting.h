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





#ifndef _RVMTESTING_H
#define _RVMTESTING_H 1
#ifndef C_ARGS
#if (__cplusplus | __STDC__)
#define C_ARGS(arglist) arglist
#else   __cplusplus
#define C_ARGS(arglist) ()
#endif  __cplusplus
#endif  C_ARGS

#if __cplusplus
extern void protect_page C_ARGS((int x));
extern void unprotect_page C_ARGS((int x));
extern void my_sigBus(int sig, int code, struct sigcontext *scp);
#else __cplusplus
/* total hack; this is dependent on C++'s name mangling algorithm */
extern void protect_page__Fi C_ARGS((int x));
extern void unprotect_page__Fi C_ARGS((int x));
extern void my_sigBus__FiT1P10sigcontext(int sig, int code, struct sigcontext *scp);
#endif __cplusplus
#endif  _RVMTESTING_H 

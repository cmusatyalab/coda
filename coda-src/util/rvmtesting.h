#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/





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

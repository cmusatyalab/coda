#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1998 Carnegie Mellon University
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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/lib-src/base/coda_assert.h,v 1.1 1998/11/02 16:47:26 rvb Exp $";
#endif /*_BLURB_*/

#ifndef _CODA_ASSERT_H_
#define _CODA_ASSERT_H_ 1

#define CODA_ASSERT(pred) do { if (!(pred)) coda_assert(#pred, __FILE__, __LINE__) ; } while (0)
#define CODA_NOTE(pred)   do { if (!(pred)) coda_note  (#pred, __FILE__, __LINE__) ; } while (0)

#define CODA_ASSERT_SLEEP	1
#define CODA_ASSERT_EXIT	2
#define CODA_ASSERT_ABORT	3
#define CODA_ASSERT_CORE	3

extern int (*coda_assert_cleanup)(void);
extern int   coda_assert_action;

extern void coda_assert(char *pred, char *file, int line);
extern void coda_note(char *pred, char *file, int line);


#endif  /* CODA_ASSERT_H_ */


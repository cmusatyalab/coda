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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/lib-src/mlwp/Attic/timer.h,v 4.2 1998/08/26 15:39:08 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/

#ifndef _LWPTIMER_
#define _LWPTIMER_

#include "cargs.h"

struct TM_Elem {
    struct TM_Elem	*Next;		/* filled by package */
    struct TM_Elem	*Prev;		/* filled by package */
    struct timeval	TotalTime;	/* filled in by caller; 
					   changed to expiration by package */
    struct timeval	TimeLeft;	/* filled by package */
    char		*BackPointer;	/* filled by caller, not interpreted by package */
};

#ifndef _TIMER_IMPL_
extern void TM_Insert C_ARGS((struct TM_Elem *tlistPtr, struct TM_Elem *elem));
extern void TM_Remove C_ARGS((struct TM_Elem *tlistPtr, struct TM_Elem *elem));
extern int  TM_Rescan C_ARGS((struct TM_Elem *tlist));
extern struct TM_Elem *TM_GetExpired C_ARGS((struct TM_Elem *tlist));
extern struct TM_Elem *TM_GetEarliest C_ARGS((struct TM_Elem *tlist));
#endif

#define FOR_ALL_ELTS(var, list, body)\
	{\
	    register struct TM_Elem *_LIST_, *var, *_NEXT_;\
	    _LIST_ = (list);\
	    for (var = _LIST_ -> Next; var != _LIST_; var = _NEXT_) {\
		_NEXT_ = var -> Next;\
		body\
	    }\
	}

/* extern definitions of timer routines */
extern int  TM_eql C_ARGS((register struct timeval *t1, register struct timeval *t2));
extern int  TM_Init C_ARGS((register struct TM_Elem **list));
extern int  TM_Final C_ARGS((register struct TM_Elem **list));
extern void TM_Insert C_ARGS((struct TM_Elem *tlistPtr, struct TM_Elem *elem));
extern int  TM_Rescan C_ARGS((struct TM_Elem *tlist));
extern struct TM_Elem *TM_GetExpired C_ARGS((struct TM_Elem *tlist));
extern struct TM_Elem *TM_GetEarliest C_ARGS((struct TM_Elem *tlist));

#endif _LWPTIMER_

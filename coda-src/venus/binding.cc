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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/binding.cc,v 4.3 1998/11/02 16:45:56 rvb Exp $";
#endif /*_BLURB_*/






/* from venus */
#include "binding.h"
#include "venus.private.h"


#ifdef VENUSDEBUG
int binding::allocs = 0;
int binding::deallocs = 0;
#endif VENUSDEBUG

binding::binding() {

    binder = 0;
    bindee = 0;
    referenceCount = 0;

#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}

binding::~binding() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG
    if (referenceCount != 0)
      LOG(0, ("binding::~binding:  somebody forgot to decrement before delete\n"));

    if (binder != 0 || bindee != 0)
	{ print(logFile); CHOKE("binding::~binding: something bogus");}
}


void binding::print(int fd) {
    fdprint(fd, "binder = %x, bindee = %x, refCount = %d\n", binder, bindee, referenceCount);
}

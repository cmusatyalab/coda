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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/lib-src/mlwp/preempt.c,v 4.3 1997/12/18 23:44:55 braam Exp $";
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

#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include "lwp.h"
#include "lwp.private.h"
#include "preempt.h"

char PRE_Block = 0;		/* used in lwp.c and process.s */

#ifdef __CYGWIN32__
#define ITIMER_REAL 0
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};
#endif

/* run the scheduler unless we are in a critical region */
PRIVATE void AlarmHandler(int sig)
{
    if (PRE_Block == 0 && lwp_cpptr->level == 0) {
	PRE_BeginCritical();

	LWP_DispatchProcess();
	PRE_EndCritical();
    }
}

int PRE_InitPreempt(struct timeval *slice)
{
    struct itimerval itv;
    struct sigaction vec;

    if (lwp_cpptr == 0) return (LWP_EINIT);
    
    if (slice == 0) {
	itv.it_interval.tv_sec = DEFAULTSLICE;
	itv.it_value.tv_sec = DEFAULTSLICE;
	itv.it_interval.tv_usec = itv.it_value.tv_usec = 0;
    }  else	{
	itv.it_interval = *slice;
	itv.it_value = *slice;
    }

    vec.sa_handler = AlarmHandler;
    sigemptyset(&vec.sa_mask);
    vec.sa_flags = 0;

    if ((sigaction(SIGALRM, &vec, (struct sigaction *)0) == -1) ||
	(setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0) == -1))
	return(LWP_ESYSTEM);

    return(LWP_SUCCESS);
}

int PRE_EndPreempt()
{

    struct itimerval itv;
    struct sigaction vec;

    if (lwp_cpptr == 0) return (LWP_EINIT);
    
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 0;

    vec.sa_handler = SIG_DFL;;
    sigemptyset(&vec.sa_mask);
    vec.sa_flags = 0;

    if ((setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0) == -1) ||
	(sigaction(SIGALRM, &vec, NULL) == -1))
	return(LWP_ESYSTEM);

    return(LWP_SUCCESS);
}

void PRE_PreemptMe()
{
    lwp_cpptr->level = 0;

}

void PRE_BeginCritical()
{
    lwp_cpptr->level++;
}

void PRE_EndCritical()
{
    lwp_cpptr->level--;
}

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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rpc2/Attic/ct.c,v 4.3 1998/05/07 17:23:51 braam Exp $";
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
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/time.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"
#include "se.h"
#include "trace.h"
#include "cbuf.h"

#ifdef RPC2DEBUG
/* this surrounds the entire file */

/*
  Clock tick generator for traces.
*/


#define TICKINTERVAL 60		/* in seconds */

void rpc2_ClockTick()
    {/* Non terminating LWP */
    struct SL_Entry *sl;
    struct timeval tval;
    register long timenow;
    
    sl = rpc2_AllocSle(OTHER, NULL);
    tval.tv_sec = TICKINTERVAL;
    tval.tv_usec = 0;

    while (TRUE)
	{
	/* ask for SocketListener to wake me up after TICKINTERVAL seconds */
	rpc2_ActivateSle(sl, &tval);

	LWP_WaitProcess((char *)&sl);
	timenow = rpc2_time();
	say(0, RPC2_DebugLevel, "Clock Tick at %ld\n",  timenow);

	if (RPC2_Trace && rpc2_TraceBuffHeader)
	    {
	    register struct TraceElem *te;
	    register struct te_CLOCKTICK *tea;
	    te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);
	    tea = &te->Args.ClockTickEntry;
	    te->CallCode = CLOCKTICK;
	    strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);
	    tea->TimeNow = timenow;	/* structure assignment */
	    }
	}
    }


#endif RPC2DEBUG

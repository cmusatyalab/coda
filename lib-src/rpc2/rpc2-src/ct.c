/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <rpc2/se.h>

#include "cbuf.h"
#include "rpc2.private.h"
#include "trace.h"

/* this surrounds the entire file */

/*
  Clock tick generator for traces.
*/

#define TICKINTERVAL 5 /* in seconds */

int RPC2_enableReaping = 0;

void rpc2_ClockTick(void *dummy)
{ /* Non terminating LWP */
    struct SL_Entry *sl;
    struct timeval tval;
    long timenow;
    int ticks = 0;

    sl           = rpc2_AllocSle(OTHER, NULL);
    tval.tv_sec  = TICKINTERVAL;
    tval.tv_usec = 0;

    while (TRUE) {
        /* ask for SocketListener to wake me up after TICKINTERVAL seconds */
        rpc2_ActivateSle(sl, &tval);
        LWP_WaitProcess((char *)sl);

        LUA_clocktick();

        /* only reap connections once a minute */
        if ((ticks++ % 12) != 0)
            continue;

        timenow = rpc2_time();
        say(1, RPC2_DebugLevel, "Clock Tick at %ld\n", timenow);

#ifdef RPC2DEBUG
        if (RPC2_Trace && rpc2_TraceBuffHeader) {
            struct TraceElem *te;
            struct te_CLOCKTICK *tea;
            te  = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);
            tea = &te->Args.ClockTickEntry;
            te->CallCode = CLOCKTICK;
            strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP) - 1);
            tea->TimeNow = timenow; /* structure assignment */
        }
#endif

        /* and free up `dead' connections */
        if (RPC2_enableReaping)
            rpc2_ReapDeadConns();
    }
}

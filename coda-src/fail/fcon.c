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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/fail/Attic/fcon.c,v 4.3.8.1 1998/10/08 11:26:30 jaharkes Exp $";
#endif /*_BLURB_*/









/*
  Network failure simulation package
  
  Walter Smith

  RPC2 interface

 */
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <lwp.h>
#include <rpc2.h>
#include "fail.h"


int Fcon_LWP();
static PrintRPCError(int, RPC2_Handle); /* forward decl to pacify gcc */


int Fcon_Init()
{
    PROCESS mypid;
    RPC2_SubsysIdent subsysid;

    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = FCONSUBSYSID;
    assert(RPC2_Export(&subsysid) == RPC2_SUCCESS);
    LWP_CreateProcess((PFIC) Fcon_LWP, 4096, LWP_NORMAL_PRIORITY,
		      "Fcon_LWP", NULL, &mypid);
    return 0;
}

int Fcon_LWP()
{
    RPC2_RequestFilter reqfilter;
    RPC2_PacketBuffer *reqbuffer;
    RPC2_Handle cid;
    int err;

    /* Accept requests on new or existing connections */
    reqfilter.FromWhom = ONESUBSYS;
    reqfilter.OldOrNew = OLDORNEW;
    reqfilter.ConnOrSubsys.SubsysId = FCONSUBSYSID;

    while (1) {
	cid = 0;
	if ((err = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, NULL,
				   NULL, NULL, NULL)) < RPC2_WLIMIT)
	    PrintRPCError(err, cid);
	RPC2_SetColor(cid, FAIL_IMMUNECOLOR);
	if ((err = Fcon_ExecuteRequest(cid, reqbuffer, (SE_Descriptor *) 0)) < RPC2_WLIMIT)
	    PrintRPCError(err, cid);
    }
}

Fcon_NewConn(cid, seType, secLevel, encType, cIdent)
RPC2_Handle cid;
RPC2_Integer seType, secLevel, encType;
RPC2_CountedBS *cIdent;
{
}


long Fcon_GetInfo(cid, name)
RPC2_Handle cid;
RPC2_BoundedBS *name;
{
    char namebuf[MAXNAMELEN];

    Fail_GetInfo(namebuf);
    name->SeqLen = strlen(namebuf) + 1;
    if (name->MaxSeqLen > name->SeqLen) {
	name->SeqLen = name->MaxSeqLen;
	namebuf[name->SeqLen - 1] = '\0';
    }
    strcpy(name->SeqBody, namebuf);
    return 0;
}

long Fcon_InsertFilter(cid, side, which, filter)
RPC2_Handle cid;
FailFilterSide side;
RPC2_Integer which;
FailFilter *filter;
{
    return Fail_InsertFilter(side, which, filter);
}

long Fcon_RemoveFilter(cid, side, id)
RPC2_Handle cid;
FailFilterSide side;
RPC2_Integer id;
{
    return Fail_RemoveFilter(side, id);
}

long Fcon_ReplaceFilter(cid, side, id, filter)
RPC2_Handle cid;
FailFilterSide side;
RPC2_Integer id;
FailFilter *filter;
{
    return Fail_ReplaceFilter(side, id, filter);
}

long Fcon_GetFilters(cid, side, filters)
RPC2_Handle cid;
FailFilterSide side;
RPC2_BoundedBS *filters;
{
    return Fail_GetFilters(side, filters);
}

long Fcon_CountFilters(cid, side)
RPC2_Handle cid;
FailFilterSide side;
{
    return Fail_CountFilters(side);
}

long Fcon_PurgeFilters(cid, side)
RPC2_Handle cid;
FailFilterSide side;
{
    return Fail_PurgeFilters(side);
}

static PrintRPCError(rCode, connId)
int rCode;
RPC2_Handle connId;
{
    fprintf(stderr, "fcon: %s\n", RPC2_ErrorMsg(rCode));
    if (rCode < RPC2_FLIMIT && connId != 0) RPC2_Unbind(connId);
}


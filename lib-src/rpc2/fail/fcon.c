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

/*
  Network failure simulation package
  
  Walter Smith

  RPC2 interface

 */
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include "fail.h"


int Fcon_LWP();
static void PrintRPCError(int, RPC2_Handle); /* forward decl to pacify gcc */


int Fcon_Init()
{
    PROCESS mypid;
    RPC2_SubsysIdent subsysid;

    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = FCONSUBSYSID;
    assert(RPC2_Export(&subsysid) == RPC2_SUCCESS);
    LWP_CreateProcess((PFIC) Fcon_LWP, 4096, LWP_NORMAL_PRIORITY,
		      NULL, "Fcon_LWP", &mypid);
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
				   NULL, 0, NULL)) < RPC2_WLIMIT)
	    PrintRPCError(err, cid);
	RPC2_SetColor(cid, FAIL_IMMUNECOLOR);
	if ((err = Fcon_ExecuteRequest(cid, reqbuffer, (SE_Descriptor *) 0)) < RPC2_WLIMIT)
	    PrintRPCError(err, cid);
    }
}

void Fcon_NewConn(cid, seType, secLevel, encType, cIdent)
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

static void PrintRPCError(int rCode, RPC2_Handle connId)
{
    fprintf(stderr, "fcon: %s\n", RPC2_ErrorMsg(rCode));
    if (rCode < RPC2_FLIMIT && connId != 0) RPC2_Unbind(connId);
}


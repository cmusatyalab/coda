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


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"
#include "se.h"


/* Contains the storage for all globals used in rpc2; see
   rpc2.private.h for descriptions */

long RPC2_Perror=1, RPC2_DebugLevel=0, RPC2_Trace = 0; /* see rpc2.h */

long rpc2_RequestSocket;
RPC2_HostIdent rpc2_LocalHost;
RPC2_PortIdent rpc2_LocalPort;

struct TM_Elem *rpc2_TimerQueue;
struct CBUF_Header *rpc2_TraceBuffHeader = NULL;
PROCESS rpc2_SocketListenerPID=NULL;

struct timeval rpc2_InitTime;

long Retry_N;			/* total number of retries -- see packet.c */
struct timeval *Retry_Beta;	/* array of timeout intervals */
struct timeval SaveResponse;    /* 2*Beta0: lifetime of saved response packet */
long rpc2_Bandwidth = 10485760; /* bandwidth hint supplied externally */

/* Doubly-linked lists and counts */
struct CEntry *rpc2_ConnFreeList, *rpc2_ConnList;
long rpc2_ConnFreeCount, rpc2_ConnCount, rpc2_ConnCreationCount;

struct MEntry *rpc2_MgrpFreeList;
long rpc2_MgrpFreeCount, rpc2_MgrpCreationCount;

struct SL_Entry *rpc2_SLFreeList, *rpc2_SLReqList, *rpc2_SLList;
long rpc2_SLFreeCount, rpc2_SLReqCount, rpc2_SLCount, rpc2_SLCreationCount;

RPC2_PacketBuffer *rpc2_PBSmallFreeList, *rpc2_PBMediumFreeList,
                *rpc2_PBLargeFreeList, *rpc2_PBList, *rpc2_PBHoldList;
long rpc2_PBSmallFreeCount, rpc2_PBSmallCreationCount, rpc2_PBMediumFreeCount,
	rpc2_PBMediumCreationCount, rpc2_PBLargeFreeCount, rpc2_PBLargeCreationCount;
long  rpc2_PBCount, rpc2_PBHoldCount, rpc2_PBFreezeCount;


struct SubsysEntry *rpc2_SSFreeList, *rpc2_SSList;
long rpc2_SSFreeCount, rpc2_SSCount, rpc2_SSCreationCount;

struct HEntry *rpc2_HostFreeList, *rpc2_HostList;
long rpc2_HostFreeCount, rpc2_HostCount, rpc2_HostCreationCount;


/* Packet transmission statistics */
struct SStats rpc2_Sent;
struct RStats rpc2_Recvd;
struct SStats rpc2_MSent;
struct RStats rpc2_MRecvd;

unsigned long rpc2_LamportClock;


/* Other miscellaneous globals */
long rpc2_BindLimit = -1;   /* At most how many can be in the request queue; -1 ==> infinite */
long rpc2_BindsInQueue;

long rpc2_Unbinds, rpc2_FreeConns, rpc2_AllocConns, rpc2_GCConns;

long rpc2_AllocMgrps, rpc2_FreeMgrps;

long rpc2_HoldHWMark, rpc2_FreezeHWMark;

char *rpc2_LastEdit = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rpc2/Attic/globals.c,v 4.5 1999/05/31 20:01:54 jaharkes Exp $";

long rpc2_errno;


/* Obsolete: purely for compatibility with /vice/file */
long rpc2_TimeCount, rpc2_CallCount, rpc2_ReqCount, rpc2_AckCount, rpc2_MaxConn;


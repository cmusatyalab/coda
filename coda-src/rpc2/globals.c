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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rpc2/Attic/globals.c,v 4.1 1997/01/08 21:50:22 rvb Exp $";
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


/* Contains the storage for all globals used in rpc2; see rpc2.private.h for descriptions */

long RPC2_Perror=1, RPC2_DebugLevel=0, RPC2_Trace = 0; /* see rpc2.h */

long rpc2_RequestSocket;
RPC2_HostIdent rpc2_LocalHost;
RPC2_PortalIdent rpc2_LocalPortal;

struct TM_Elem *rpc2_TimerQueue;
struct CBUF_Header *rpc2_TraceBuffHeader = NULL;
PROCESS rpc2_SocketListenerPID=NULL;

struct timeval rpc2_InitTime;

long Retry_N;	                /* total number of retries -- see packet.c */
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

char *rpc2_LastEdit = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rpc2/Attic/globals.c,v 4.1 1997/01/08 21:50:22 rvb Exp $";

long rpc2_errno;


/* Obsolete: purely for compatibility with /vice/file */
long rpc2_TimeCount, rpc2_CallCount, rpc2_ReqCount, rpc2_AckCount, rpc2_MaxConn;

/* 1 if system is 4.3, 0 if not (4.2) */
int rpc2_43bsd;


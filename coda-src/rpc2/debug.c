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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rpc2/Attic/debug.c,v 4.2.6.1 1998/05/02 21:40:42 braam Exp $";
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
#include <sys/time.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "se.h"
#include "rpc2.private.h"
#include "trace.h"

#ifdef RPC2DEBUG

/*----- Routines to aid in debugging -----*/

PRIVATE char *WhichMagic(x)
    {
    PRIVATE char buf[20];
    switch(x)
	{
	case OBJ_PACKETBUFFER:	return("OBJ_PACKETBUFFER");
	case OBJ_CENTRY:	return("OBJ_CENTRY");
	case OBJ_SLENTRY:	return("OBJ_SLENTRY");
	case OBJ_SSENTRY:	return("OBJ_SSENTRY");
	case OBJ_HENTRY:	return("OBJ_HENTRY");
	default:		(void) sprintf(buf, "%d", x); return(buf);
	}
    }

void rpc2_PrintTMElem(tPtr, tFile)
    register struct TM_Elem *tPtr;
    FILE *tFile;
    {
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */
    fprintf(tFile, "MyAddr = 0x%lx Next = 0x%lx  Prev = 0x%lx  TotalTime = %ld:%ld  TimeLeft = %ld:%ld  BackPointer = 0x%lx\n",
    	(long)tPtr, (long)tPtr->Next, (long)tPtr->Prev, tPtr->TotalTime.tv_sec, tPtr->TotalTime.tv_usec,
	tPtr->TimeLeft.tv_sec, tPtr->TimeLeft.tv_usec, tPtr->BackPointer);
    (void) fflush(tFile);
    }

void rpc2_PrintFilter(fPtr, tFile)
    register RPC2_RequestFilter *fPtr;
    FILE *tFile;
    {
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */
    fprintf(tFile, "FromWhom = %s  OldOrNew = %s  ", 
	fPtr->FromWhom == ANY ? "ANY" : (fPtr->FromWhom == ONECONN ? "ONECONN" : (fPtr->FromWhom == ONESUBSYS ? "ONESUBSYS" : "??????")),
	fPtr->OldOrNew == OLD ? "OLD" : (fPtr->OldOrNew == NEW ? "NEW" : (fPtr->OldOrNew == OLDORNEW ? "OLDORNEW" : "??????")));
    switch(fPtr->FromWhom)
	{
	case ONECONN:	fprintf(tFile, "WhichConn = 0x%lx", fPtr->ConnOrSubsys.WhichConn);
	case ONESUBSYS: fprintf(tFile, "SubsysId = %ld", fPtr->ConnOrSubsys.SubsysId);
	}
    fprintf(tFile, "\n");
    (void) fflush(tFile);
    }

void rpc2_PrintSLEntry(slPtr, tFile)
    register struct SL_Entry *slPtr;
    FILE *tFile;
    {
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */
    fprintf(tFile, "MyAddr: 0x%lx\n\tNextEntry = 0x%lx PrevEntry = 0x%lx  MagicNumber = %s  ReturnCode = %s\n\tTElem==>  ", (long)slPtr, (long)slPtr->NextEntry, (long)slPtr->PrevEntry, WhichMagic(slPtr->MagicNumber), 
	    slPtr->ReturnCode == WAITING ? "WAITING" : slPtr->ReturnCode == ARRIVED ? "ARRIVED" : slPtr->ReturnCode == TIMEOUT ? "TIMEOUT" : slPtr->ReturnCode == NAKED ? "NAKED" : "??????");
    rpc2_PrintTMElem(&slPtr->TElem, tFile);

    switch(slPtr->Type)
	{
	case REPLY:
		    fprintf(tFile, "\tType = REPLY  Conn = 0x%lx\n",
			    slPtr->Conn);
		    break;
	
	case REQ:
		    fprintf(tFile, "\tElementType = REQ  Packet = 0x%lx  Filter==>  ",
			    (long)slPtr->Packet);
		    rpc2_PrintFilter(&slPtr->Filter, tFile);
		    break;
	
	case OTHER:
		    fprintf(tFile, "\tElementType = OTHER  Conn = 0x%lx  Packet = 0x%lx\n",
			    slPtr->Conn, (long)slPtr->Packet);
		    break;

	default:
		    fprintf(tFile, "\tElementType = ???????\n");
		    break;
	}
    fprintf(tFile, "\n");
    (void) fflush(tFile);
    }


void rpc2_PrintHEntry(hPtr, tFile)
    register struct HEntry *hPtr;
    FILE *tFile;
    {
    register long a = ntohl(hPtr->Host);
    int head, ix;

    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */

    fprintf(tFile, "\nHost 0x%lx state is...\n\tNextEntry = 0x%lx  PrevEntry = 0x%lx  MagicNumber = %s\n",
	(long)hPtr, (long)hPtr->Next, (long)hPtr->Prev, WhichMagic(hPtr->MagicNumber));
    fprintf(tFile, "\tHost entry type = ");
    switch ((int) hPtr->Type) 
        {
	case RPC2_HE: fprintf(tFile, "RPC2\n"); break;
	case SMARTFTP_HE: fprintf(tFile, "SMART FTP\n"); break;
	case UNSET_HE: 
	default:  fprintf(tFile, "????\n");
	}

    fprintf(tFile, "\tHost.InetAddress = %lu.%lu.%lu.%lu, Portal.InetPortNumber = %u\n",
	    ((unsigned long)(a & 0xff000000))>>24, 
	    (unsigned long) (a & 0x00ff0000)>>16, 
	    (unsigned long) (a & 0x0000ff00)>>8, 
	    (unsigned long) a & 0x000000ff, ntohs(hPtr->Portal));
    fprintf(tFile, "\tLastWord = %ld.%06ld\n", hPtr->LastWord.tv_sec, hPtr->LastWord.tv_usec);
    fprintf(tFile, "\tObservation Log Entries = %d (%d kept)\n", 
	    hPtr->NumEntries, RPC2_MAXLOGLENGTH);
    
    if (hPtr->NumEntries < RPC2_MAXLOGLENGTH) head = 0;
    else head = hPtr->NumEntries - RPC2_MAXLOGLENGTH;
    while (head < hPtr->NumEntries) {
	ix = head & (RPC2_MAXLOGLENGTH-1);
	switch(hPtr->Log[ix].Tag) 
	    {
	    case RPC2_MEASURED_NLE:
		fprintf(tFile, "\t\tentry %d: %ld.%06ld, conn %d, %d bytes, %d msec\n",
			ix, hPtr->Log[ix].TimeStamp.tv_sec, 
			hPtr->Log[ix].TimeStamp.tv_usec,
			hPtr->Log[ix].Value.Measured.Conn,
			hPtr->Log[ix].Value.Measured.Bytes, 
			hPtr->Log[ix].Value.Measured.ElapsedTime);
		break;
	    case RPC2_STATIC_NLE:
		fprintf(tFile, "\t\tentry %d: %ld.%06ld, static bandwidth %ld bytes/sec\n",
			ix, hPtr->Log[ix].TimeStamp.tv_sec, 
			hPtr->Log[ix].TimeStamp.tv_usec,
			hPtr->Log[ix].Value.Static.Bandwidth);
		break;
	    default:
		break;
	    }		
	head++;
    }
    (void) fflush(tFile);
    }


void rpc2_PrintCEntry(cPtr, tFile)
    register struct CEntry *cPtr;
    FILE *tFile;
    {
    long i;
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */
    fprintf(tFile, "MyAddr: 0x%lx\n\tNextEntry = 0x%lx  PrevEntry = 0x%lx  MagicNumber = %s  Role = %s  State = ",
	(long)cPtr, (long)cPtr->NextEntry, (long)cPtr->PrevEntry,
	WhichMagic(cPtr->MagicNumber),
	TestRole(cPtr,FREE) ? "FREE" :(TestRole(cPtr, CLIENT) ? "CLIENT" : (TestRole(cPtr, SERVER) ? "SERVER" : "?????") ));
    if (TestRole(cPtr,CLIENT))
	switch((int) (cPtr->State & 0x0000ffff))
	    {
	    case C_THINK: fprintf(tFile, "C_THINK");break;
	    case C_AWAITREPLY: fprintf(tFile, "C_AWAITREPLY");break;
	    case C_AWAITINIT2: fprintf(tFile, "C_AWAITINIT2");break;
	    case C_AWAITINIT4: fprintf(tFile, "C_AWAITINIT4");break;
	    case C_HARDERROR: fprintf(tFile, "C_HARDERROR");break;
	    default: fprintf(tFile, "???????"); break;
	    }
    if (TestRole(cPtr,SERVER))
	switch((int) (cPtr->State & 0x0000ffff))
	    {
	    case S_AWAITREQUEST: fprintf(tFile, "S_AWAITREQUEST");break;	    
	    case S_PROCESS: fprintf(tFile, "S_PROCESS");break;	    
	    case S_STARTBIND: fprintf(tFile, "S_STARTBIND");break;	    
	    case S_FINISHBIND: fprintf(tFile, "S_FINISHBIND");break;
	    case S_AWAITINIT3: fprintf(tFile, "S_AWAITINIT3");break;
	    case S_REQINQUEUE: fprintf(tFile, "S_REQINQUEUE");break;
	    case S_HARDERROR: fprintf(tFile, "S_HARDERROR");break;
	    case S_INSE: fprintf(tFile, "S_INSE");break;
	    case S_AWAITENABLE: fprintf(tFile, "S_AWAITENABLE");break;
	    default: fprintf(tFile, "??????"); break;
	    }

    fprintf(tFile, "\n\tSecurityLevel = %s", cPtr->SecurityLevel == RPC2_OPENKIMONO ? "RPC2_OPENKIMONO" : 
	(cPtr->SecurityLevel == RPC2_AUTHONLY ? "RPC2_AUTHONLY" : (cPtr->SecurityLevel == RPC2_SECURE ? "RPC2_SECURE" : 
	(cPtr->SecurityLevel == RPC2_HEADERSONLY ? "RPC2_HEADERSONLY" :"??????"))));
    fprintf(tFile, "  EncryptionType = %ld  SessionKey = 0x", cPtr->EncryptionType);
    for(i = 0; i < RPC2_KEYSIZE; i++)fprintf(tFile, "%lx", (long)cPtr->SessionKey[i]);
	
    fprintf(tFile, "\n\tUniqueCID = 0x%lx  NextSeqNumber = %ld  PeerHandle = 0x%lx\n\tPrivatePtr = 0x%lx  SideEffectPtr = 0x%lx\n",
    	cPtr->UniqueCID, cPtr->NextSeqNumber, cPtr->PeerHandle, (long)cPtr->PrivatePtr, (long)cPtr->SideEffectPtr);
	
    fprintf(tFile, "\tLowerLimit = %lu usec  %s = %ld  %s = %ld  Retries = %ld\n",
	    cPtr->LowerLimit,
	    TestRole(cPtr, CLIENT) ? "RTT" : (TestRole(cPtr, SERVER) ? "TimeEcho" : "?????"),
	    cPtr->RTT, 
	    TestRole(cPtr, CLIENT) ? "RTTVar" : (TestRole(cPtr, SERVER) ? "RequestTime" : "?????"),
	     cPtr->RTTVar,  cPtr->Retry_N);

    fprintf(tFile, "\tRetry_Beta[0] = %ld.%0ld  (timeout)\n",
	    cPtr->Retry_Beta[0].tv_sec, cPtr->Retry_Beta[0].tv_usec);
    for (i = 1; i < cPtr->Retry_N+2; i++) 
	    fprintf(tFile, "\tRetry_Beta[%ld] = %ld.%0ld\n",
		    i, cPtr->Retry_Beta[i].tv_sec, cPtr->Retry_Beta[i].tv_usec);

    fprintf(tFile, "\tHeldPacket = 0x%lx  PeerUnique = %ld\n",
    	(long)cPtr->HeldPacket, cPtr->PeerUnique);
    fprintf(tFile, "Peer==> ");    
    rpc2_PrintHostIdent(&cPtr->PeerHost, tFile);
    fprintf(tFile, "    ");
    rpc2_PrintPortalIdent(&cPtr->PeerPortal, tFile);
    if (cPtr->HostInfo)
	rpc2_PrintHEntry(cPtr->HostInfo, tFile);

    fprintf(tFile, "\n");
    (void) fflush(tFile);
    }

void rpc2_PrintMEntry(mPtr, tFile)
    register struct MEntry *mPtr;
    FILE *tFile;
    {
    long i;
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */
    fprintf(tFile, "MyAddr: 0x%lx\n\tNextEntry = 0x%lx  PrevEntry = 0x%lx  MagicNumber = %s  Role = %s  State = ",
	(long)mPtr, (long)mPtr->Next, (long)mPtr->Prev,
	WhichMagic(mPtr->MagicNumber),
	TestRole(mPtr,FREE) ? "FREE" :(TestRole(mPtr, CLIENT) ? "CLIENT" : (TestRole(mPtr, SERVER) ? "SERVER" : "?????") ));
    if (TestRole(mPtr,CLIENT))
	switch((int) (mPtr->State & 0x0000ffff))
	    {
	    case C_THINK: fprintf(tFile, "C_THINK");break;
	    case C_AWAITREPLY: fprintf(tFile, "C_AWAITREPLY");break;
	    case C_HARDERROR: fprintf(tFile, "C_HARDERROR");break;
	    default: fprintf(tFile, "???????"); break;
	    }
    if (TestRole(mPtr,SERVER))
	switch((int) (mPtr->State & 0x0000ffff))
	    {
	    case S_AWAITREQUEST: fprintf(tFile, "S_AWAITREQUEST");break;	    
	    case S_PROCESS: fprintf(tFile, "S_PROCESS");break;	    
	    case S_REQINQUEUE: fprintf(tFile, "S_REQINQUEUE");break;
	    case S_HARDERROR: fprintf(tFile, "S_HARDERROR");break;
	    case S_INSE: fprintf(tFile, "S_INSE");break;
	    case S_AWAITENABLE: fprintf(tFile, "S_AWAITENABLE");break;
	    default: fprintf(tFile, "??????"); break;
	    }

    fprintf(tFile, "\n\tSecurityLevel = %s", mPtr->SecurityLevel == RPC2_OPENKIMONO ? "RPC2_OPENKIMONO" : 
	(mPtr->SecurityLevel == RPC2_AUTHONLY ? "RPC2_AUTHONLY" : (mPtr->SecurityLevel == RPC2_SECURE ? "RPC2_SECURE" : 
	(mPtr->SecurityLevel == RPC2_HEADERSONLY ? "RPC2_HEADERSONLY" :"??????"))));
    fprintf(tFile, "  EncryptionType = %ld  SessionKey = 0x", mPtr->EncryptionType);
    for(i = 0; i < RPC2_KEYSIZE; i++)fprintf(tFile, "%lx", (long)mPtr->SessionKey[i]);

    fprintf(tFile, "\n\tMgrpID = %ld  NextSeqNumber = %ld  SubsysID = %ld\n",
    	mPtr->MgroupID, mPtr->NextSeqNumber, mPtr->SubsysId);
	
    fprintf(tFile, "Client Host Ident:\n");
    rpc2_PrintHostIdent(&mPtr->ClientHost, tFile);
    fprintf(tFile, "Client PortalIdent:\n");
    rpc2_PrintPortalIdent(&mPtr->ClientPortal, tFile);

    if (TestRole(mPtr,CLIENT)) {
	fprintf(tFile, "\n\tMaxlisteners = %ld  Listeners = %ld\n",
	    mPtr->me_conns.me_client.mec_maxlisteners, mPtr->me_conns.me_client.mec_howmanylisteners);
	fprintf(tFile, "IP Multicast Host Address:\n");
	rpc2_PrintHostIdent(&mPtr->IPMHost, tFile);
	fprintf(tFile, "IP Multicast Portal Number:\n");
	rpc2_PrintPortalIdent(&mPtr->IPMPortal, tFile);
	fprintf(tFile, "Current multicast packet:\n");
	rpc2_PrintPacketHeader(mPtr->CurrentPacket, tFile);
    }
    else {
	fprintf(tFile, "Client CEntry:\n");
	rpc2_PrintCEntry(mPtr->me_conns.mes_conn, tFile);
    }
	
    fprintf(tFile, "\n");
    (void) fflush(tFile);
    }


void rpc2_PrintHostIdent(hPtr, tFile)
    register RPC2_HostIdent *hPtr;
    FILE *tFile;
    {
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */
    switch (hPtr->Tag)
	{
	case RPC2_HOSTBYINETADDR:
	case RPC2_MGRPBYINETADDR:
		{
		register long a = ntohl(hPtr->Value.InetAddress);
		fprintf(tFile, "Host.InetAddress = %lu.%lu.%lu.%lu",
			((unsigned long)(a & 0xff000000))>>24, 
			(unsigned long) (a & 0x00ff0000)>>16, 
			(unsigned long) (a & 0x0000ff00)>>8, 
			(unsigned long) a & 0x000000ff);
		break;	
		}
	
	case RPC2_MGRPBYNAME:
	case RPC2_HOSTBYNAME:
		fprintf(tFile, "Host.Name = \"%s\"", hPtr->Value.Name);
		break;
	
	default:	fprintf(tFile, "Host = ??????\n");
	}

    (void) fflush(tFile);
    }

void rpc2_PrintPortalIdent(pPtr, tFile)
    register RPC2_PortalIdent *pPtr;
    FILE *tFile;
    {
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */
    switch (pPtr->Tag)
	{
	case RPC2_PORTALBYINETNUMBER:
		fprintf(tFile, "Portal.InetPortNumber = %u", ntohs(pPtr->Value.InetPortNumber));
		break;	
	
	case RPC2_PORTALBYNAME:
		fprintf(tFile, "Portal.Name = \"%s\"", pPtr->Value.Name);
		break;
	
	default:	fprintf(tFile, "Portal = ??????");
	}


    (void) fflush(tFile);
    }


void rpc2_PrintSubsysIdent(Subsys, tFile)
    register RPC2_SubsysIdent *Subsys;
    FILE *tFile;
    {
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */
    switch(Subsys->Tag) {
	case RPC2_SUBSYSBYNAME:
		fprintf(tFile, "Subsys:    Tag = RPC2_SUBSYSBYNAME    Name = \"%s\"\n", Subsys->Value.Name);
		break;
			
	case RPC2_SUBSYSBYID:
		fprintf(tFile, "Subsys:    Tag = RPC2_SUBSYSBYID    Name = %ld\n", Subsys->Value.SubsysId);
		break;
			
	default:
		fprintf(tFile, "Subsys:    ??????\n");
    }
    }

/*
 *	The packet should be in *network* byte order.
 */

void rpc2_PrintPacketHeader(pb, tFile)
    register RPC2_PacketBuffer *pb;
    FILE *tFile;
{
    if (tFile == NULL) tFile = stdout;	/* it's ok, call-by-value */

    fprintf(tFile, "\tPrefix: BufferSize = %ld  LengthOfPacket = %ld  ",
	    pb->Prefix.BufferSize, pb->Prefix.LengthOfPacket);
    fprintf(tFile, "MagicNumber = %ld, Q = %lu\n",
	    (long) pb->Prefix.MagicNumber, pb->Prefix.Qname);
    fprintf(tFile, "\tHeader: ProtoVersion = 0x%lx  RemoteHandle = 0x%lx  ",
	    ntohl(pb->Header.ProtoVersion), ntohl(pb->Header.RemoteHandle));
    fprintf(tFile, "LocalHandle = 0x%lx  BodyLength = %lu  SeqNumber = %lu\n",
	    ntohl(pb->Header.LocalHandle), ntohl(pb->Header.BodyLength),
	    ntohl(pb->Header.SeqNumber));

    switch((int) ntohl(pb->Header.Opcode)) {
	case RPC2_INIT1OPENKIMONO:
		fprintf(tFile, "\t\tOpcode = RPC2_INIT1OPENKIMONO");
		break;

	case RPC2_INIT1AUTHONLY:
		fprintf(tFile, "\t\tOpcode = RPC2_INIT1AUTHONLY");
		break;

	case RPC2_INIT1SECURE:
		fprintf(tFile, "\t\tOpcode = RPC2_INIT1SECURE");
		break;

	case RPC2_INIT1HEADERSONLY:
		fprintf(tFile, "\t\tOpcode = RPC2_INIT1HEADERSONLY");
		break;

	case RPC2_LASTACK:
		fprintf(tFile, "\t\tOpcode = RPC2_LASTACK");
		break;

	case RPC2_REPLY:
		fprintf(tFile, "\t\tOpcode = RPC2_REPLY");
		break;

	case RPC2_BUSY:
		fprintf(tFile, "\t\tOpcode = RPC2_BUSY");
		break;

	case RPC2_INIT2:
		fprintf(tFile, "\t\tOpcode = RPC2_INIT2");
		break;

	case RPC2_INIT3:
		fprintf(tFile, "\t\tOpcode = RPC2_INIT3");
		break;

	case RPC2_INIT4:
		fprintf(tFile, "\t\tOpcode = RPC2_INIT4");
		break;

	case RPC2_NEWCONNECTION:
		fprintf(tFile, "\t\tOpcode = RPC2_NEWCONNECTION");
		break;

	default:
		fprintf(tFile, "\t\tOpcode = %lu", ntohl(pb->Header.Opcode));
		break;
	}

    fprintf(tFile, "  SEFlags = 0x%lx  SEDataOffset = %lu  ",
	    ntohl(pb->Header.SEFlags), ntohl(pb->Header.SEDataOffset));
    fprintf(tFile, "SubsysId = %lu  ReturnCode = %lu\n",
	    ntohl(pb->Header.SubsysId), ntohl(pb->Header.ReturnCode));
    fprintf(tFile, "\t\tFlags = 0x%lx  Uniquefier = %lu  Lamport = %lu\n",
	    ntohl(pb->Header.Flags), ntohl(pb->Header.Uniquefier),
	    ntohl(pb->Header.Lamport));
    fprintf(tFile, "\t\tTimeStamp = %lu  BindTime = %lu\n",
	    ntohl(pb->Header.TimeStamp), ntohl(pb->Header.BindTime));
    fprintf(tFile, "\n");

    (void) fflush(tFile);
}






PRIVATE char *CallName(x)
    {
    switch(x)
	{
	case INIT:		return("RPC2_Init");
	case EXPORT:		return("RPC2_Export");
	case DEEXPORT:		return("RPC2_DeExport");
	case ALLOCBUFFER:	return("RPC2_AllocBuffer");
	case FREEBUFFER:	return("RPC2_FreeBuffer");
	case SENDRESPONSE:	return("RPC2_SendResponse");
	case GETREQUEST:	return("RPC2_GetRequest");
	case MAKERPC:		return("RPC2_MakeRPC");
	case BIND:		return("RPC2_NewBinding");
	case INITSIDEEFFECT:	return("RPC2_InitSideEffect");
	case CHECKSIDEEFFECT:	return("RPC2_CheckSideEffect");
	case UNBIND:		return("RPC2_Unbind");
	case GETPRIVATEPOINTER:	return("RPC2_GetPrivatePointer");
	case SETPRIVATEPOINTER:	return("RPC2_SetPrivatePointer");
	case GETSEPOINTER:	return("RPC2_GetSEPointer");
	case SETSEPOINTER:	return("RPC2_SetSEPointer");
	case GETPEERINFO:	return("RPC2_GetPeerInfo");
	case SLNEWPACKET:	return("Packet Received");
	case SENDRELIABLY:      return("rpc2_SendReliably");
	case XMITPACKET:	return("rpc2_XmitPacket");
	case CLOCKTICK:		return("Clock Tick");
	case MULTIRPC:		return("RPC2_MultiRPC");
	case MSENDPACKETSRELIABLY: return("rpc2_MSendPacketsReliably");
	case ADDTOMGRP:		return("RPC2_AddToMgrp");
	case CREATEMGRP:	return("RPC2_CreateMgrp");
	case REMOVEFROMMGRP:	return("rpc2_RemoveFromMgrp");
	case XLATEMCASTPACKET:	return("XlateMcastPacket");
	}
    return("?????");
    }


void rpc2_PrintTraceElem(whichTE, whichIndex, outFile)
    register struct TraceElem *whichTE;
    long whichIndex;
    register FILE *outFile;
    {
    long i;
    fprintf(outFile, "\nTrace Entry %ld:	<<<<<< %s: %s", whichIndex, whichTE->ActiveLWP, CallName(whichTE->CallCode));
    switch(whichTE->CallCode)
	{
	case SLNEWPACKET:
	case CLOCKTICK:
	    fprintf(outFile, " >>>>>>\n");
	    break;
	    
	default:
	    fprintf(outFile, "() >>>>>>\n");
	    break;
	}

    switch(whichTE->CallCode)
	{
	case INIT: break;

	case EXPORT:
		{
		register struct te_EXPORT *tea;
		tea = &whichTE->Args.ExportEntry;
		if (tea->Subsys.Tag == RPC2_SUBSYSBYID)
		    fprintf(outFile, "Subsys:	Tag = RPC2_SUBSYSBYID    SubsysId = %ld\n", tea->Subsys.Value.SubsysId);
		else
		    fprintf(outFile, "Subsys:	Tag = RPC2_SUBSYSBYNAME  Name = \"%s\"\n", tea->Subsys.Value.Name);
		break;	/* switch */
		}

	case DEEXPORT:
		{
		register struct te_DEEXPORT *tea;
		tea = &whichTE->Args.DeExportEntry;
		if (tea->Subsys.Tag == RPC2_SUBSYSBYID)
		    fprintf(outFile, "Subsys:	Tag = RPC2_SUBSYSBYID    SubsysId = %ld\n", tea->Subsys.Value.SubsysId);
		else
		    fprintf(outFile, "Subsys:	Tag = RPC2_SUBSYSBYNAME  Name = \"%s\"\n", tea->Subsys.Value.Name);
		break;	/* switch */
		}

	case ALLOCBUFFER:
		{
		register struct te_ALLOCBUFFER *tea;
		tea = &whichTE->Args.AllocBufferEntry;
		fprintf(outFile, "MinBodySize:  %d\n", tea->MinBodySize);
		break;	/* switch */
		}

	case FREEBUFFER:
		{
		register struct te_FREEBUFFER *tea;
		tea = &whichTE->Args.FreeBufferEntry;
		fprintf(outFile, "*BuffPtr:  0x%lx\n", (long)tea->BuffPtr);
		break;	/* switch */
		}

	case SENDRESPONSE:
		{
		register struct te_SENDRESPONSE *tea;
		tea = &whichTE->Args.SendResponseEntry;
		fprintf(outFile, "ConnHandle: 0x%lx\n", tea->ConnHandle);
		break;	/* switch */
		}

	case GETREQUEST:
		{
		register struct te_GETREQUEST *tea;
		tea = &whichTE->Args.GetRequestEntry;
		fprintf(outFile, "Filter: "); rpc2_PrintFilter(&tea->Filter, outFile);
		if (tea->IsNullBreathOfLife) fprintf(outFile, "BreathOfLife:  NULL\n");
		else fprintf(outFile, "BreathOfLife:	%ld.%ld\n", tea->BreathOfLife.tv_sec, tea->BreathOfLife.tv_usec);
		fprintf(outFile, "GetKeys: 0x%lx    EncryptionTypeMask: 0x%x\n", (long)tea->GetKeys, tea->EncryptionTypeMask);
		break;	/* switch */
		}

	case MAKERPC:
		{
		register struct te_MAKERPC *tea;
		tea = &whichTE->Args.MakeRPCEntry;
		fprintf(outFile, "Conn: 0x%lx  ", tea->ConnHandle);
		fprintf(outFile, "Enqueue: %d  ", tea->EnqueueRequest);
		if (tea->IsNullBreathOfLife) fprintf(outFile, "BreathOfLife: NULL  ");
		else fprintf(outFile, "BreathOfLife: %ld.%ld  ", tea->BreathOfLife.tv_sec,
			tea->BreathOfLife.tv_usec);
		if (tea->IsNullSDesc) fprintf(outFile, "SDesc: NULL\n");
		else {fprintf(outFile, "\nSDesc: "); rpc2_PrintSEDesc(&tea->SDesc, outFile);}
		break;	/* switch */
		}

	case MULTIRPC:
		{
		register struct te_MULTIRPC *tea;
		tea = &whichTE->Args.MultiRPCEntry;
		fprintf(outFile, "ConnHandle: 0x%lx\n", tea->ConnHandle);
		fprintf(outFile, "Request:    OriginalAddress = 0x%lx    ", (long)tea->Request_Address);
		rpc2_PrintPacketHeader(&tea->Request, outFile);
		if (tea->IsNullSDesc) fprintf(outFile, "SDesc:    NULL\n");
		else {fprintf(outFile, "SDesc: "); rpc2_PrintSEDesc(&tea->SDesc, outFile);}
		fprintf(outFile, "HandleResult: 0x%lx\n", tea->HandleResult);
		if (tea->IsNullBreathOfLife) fprintf(outFile, "BreathOfLife:  NULL\n");
		else fprintf(outFile, "BreathOfLife:	%ld.%ld\n", tea->BreathOfLife.tv_sec, tea->BreathOfLife.tv_usec);
		break;	/* switch */
		}

	case BIND:
		{
		register struct te_BIND *tea;
		tea = &whichTE->Args.BindEntry;
		fprintf(outFile, "SecurityLevel:   %s    EncryptionType: %d\n", (tea->SecurityLevel == RPC2_OPENKIMONO) ? "RPC2_OPENKIMONO" :
			(tea->SecurityLevel == RPC2_SECURE) ? "RPC2_SECURE" : (tea->SecurityLevel == RPC2_AUTHONLY) ?
			"RPC2_ONLYAUTHENTICATE" : (tea->SecurityLevel == RPC2_HEADERSONLY) ? "RPC2_HEADERSONLY" : "????????", 
			tea->EncryptionType);
		switch (tea->Host.Tag)
		    {
		    case RPC2_HOSTBYNAME:
			fprintf(outFile, "Host:	Tag = RPC2_HOSTBYNAME    Name = \"%s\"\n", tea->Host.Value.Name);
			break;
		    
		    case RPC2_HOSTBYINETADDR:
			fprintf(outFile, "Host:     Tag = RPC2_HOSTBYINETADDR	InetAddress = %lu.%lu.%lu.%lu\n",
			 	(unsigned long) tea->Host.Value.InetAddress & 0xff000000, 
				(unsigned long) tea->Host.Value.InetAddress & 0x00ff0000,
				(unsigned long) tea->Host.Value.InetAddress & 0x0000ff00, 
				(unsigned long) tea->Host.Value.InetAddress & 0x000000ff);
			break;

		    default:
			fprintf(outFile, "Host:   ?????????\n");
			break;
		    }

		switch (tea->Portal.Tag)
		    {
		    case RPC2_PORTALBYNAME:
			fprintf(outFile, "Portal:    Tag = RPC2_PORTALBYNAME    Name = \"%s\"\n", tea->Portal.Value.Name);
			break;
			
		    case RPC2_PORTALBYINETNUMBER:
			fprintf(outFile, "Portal:    Tag = RPC2_PORTALBYINETNUMBER    InetNumber = \"%u\"\n", (unsigned) tea->Portal.Value.InetPortNumber);		    
			break;
			
		    default:
			fprintf(outFile, "Portal:    ??????\n");
			break;
		    }


		switch(tea->Subsys.Tag)
		    {
		    case RPC2_SUBSYSBYNAME:
			fprintf(outFile, "Subsys:    Tag = RPC2_SUBSYSBYNAME    Name = \"%s\"\n", tea->Subsys.Value.Name);
			break;
			
		    case RPC2_SUBSYSBYID:
			fprintf(outFile, "Subsys:    Tag = RPC2_SUBSYSBYID    Name = %ld\n", tea->Subsys.Value.SubsysId);
			break;
			
		    default:
			fprintf(outFile, "Subsys:    ??????\n");
		    }
		    
		fprintf(outFile, "SideEffectType = %d\n", tea->SideEffectType);
		if (tea->IsNullClientIdent) fprintf(outFile, "ClientIdent:    NULL\n");
		else
		    {
		    long max;
		    fprintf(outFile, "ClientIdent:    SeqLen = %ld   SeqBody\"", tea->ClientIdent.SeqLen);
		    max = (tea->ClientIdent.SeqLen < sizeof(tea->ClientIdent_Value)) ? tea->ClientIdent.SeqLen :
		    	sizeof(tea->ClientIdent_Value);
		    for (i = 0; i < max; i++) fprintf(outFile, "%c", (tea->ClientIdent_Value)[i]);
		    if (max < tea->ClientIdent.SeqLen) fprintf(outFile, ".....");
		    fprintf(outFile, "\"\n");
		    }
		if (tea->IsNullSharedSecret) fprintf(outFile, "SharedSecret:    NULL\n");		
		else
		    {
		    fprintf(outFile, "SharedSecret:    0x");
			for (i = 0; i < sizeof(RPC2_EncryptionKey); i++) fprintf(outFile, "%lx", (long)(tea->SharedSecret)[i]);
		    fprintf(outFile, "\n");
		    }
		
		break;	/* switch */
		}

	case INITSIDEEFFECT:
		{
		register struct te_INITSIDEEFFECT *tea;
		tea = &whichTE->Args.InitSideEffectEntry;
		fprintf(outFile, "ConnHandle:    0x%lx\n", tea->ConnHandle);
		if (tea->IsNullSDesc) fprintf(outFile, "SDesc:    NULL\n");
		else  {fprintf(outFile, "SDesc:    "); rpc2_PrintSEDesc(&tea->SDesc, outFile); }
		break;	/* switch */
		}

	case CHECKSIDEEFFECT:
		{
		register struct te_CHECKSIDEEFFECT *tea;
		tea = &whichTE->Args.CheckSideEffectEntry;
		fprintf(outFile, "ConnHandle:    0x%lx\n", tea->ConnHandle);
		if (tea->IsNullSDesc) fprintf(outFile, "SDesc:    NULL\n");
		else  {fprintf(outFile, "SDesc:    ");  rpc2_PrintSEDesc(&tea->SDesc, outFile);}
		fprintf(outFile, "Flags:  { ");
		if (tea->Flags & SE_AWAITLOCALSTATUS) fprintf(outFile, "SE_AWAITLOCALSTATUS  ");
		if (tea->Flags & SE_AWAITREMOTESTATUS) fprintf(outFile, "SE_AWAITREMOTESTATUS  ");
		fprintf(outFile, "}\n");
		break;	/* switch */
		}

	case UNBIND:
		{
		register struct te_UNBIND *tea;
		tea = &whichTE->Args.UnbindEntry;
		fprintf(outFile, "whichConn:    0x%lx\n", tea->whichConn);
		break;	/* switch */
		}

	case GETPRIVATEPOINTER:
		{
		register struct te_GETPRIVATEPOINTER *tea;
		tea = &whichTE->Args.GetPrivatePointerEntry;
		fprintf(outFile, "ConnHandle:    0x%lx\n", tea->ConnHandle);
		break;	/* switch */
		}

	case SETPRIVATEPOINTER:
		{
		register struct te_SETPRIVATEPOINTER *tea;
		tea = &whichTE->Args.SetPrivatePointerEntry;
		fprintf(outFile, "ConnHandle:    0x%lx\n", tea->ConnHandle);
		fprintf(outFile, "PrivatePtr:    0x%lx\n", (long)tea->PrivatePtr);
		break;	/* switch */
		}

	case GETSEPOINTER:
		{
		register struct te_GETSEPOINTER *tea;
		tea = &whichTE->Args.GetSEPointerEntry;
		fprintf(outFile, "ConnHandle:    0x%lx\n", tea->ConnHandle);
		break;	/* switch */
		}

	case SETSEPOINTER:
		{
		register struct te_SETSEPOINTER *tea;
		tea = &whichTE->Args.SetSEPointerEntry;
		fprintf(outFile, "ConnHandle:    0x%lx\n", tea->ConnHandle);
		fprintf(outFile, "SEPtr:    0x%lx\n", (long)tea->SEPtr);
		break;	/* switch */
		}

	case GETPEERINFO:
		{
		register struct te_GETPEERINFO *tea;
		tea = &whichTE->Args.GetPeerInfoEntry;
		fprintf(outFile, "ConnHandle:    0x%lx\n", (long)tea->ConnHandle);
		break;	/* switch */
		}

	case SLNEWPACKET:
		{
		register struct te_SLNEWPACKET *tea;
		tea = &whichTE->Args.SLNewPacketEntry;
		rpc2_PrintPacketHeader(&tea->pb, outFile);
		break;	/* switch */
		}

	case SENDRELIABLY:
		{
		register struct te_SENDRELIABLY *tea;
		tea = &whichTE->Args.SendReliablyEntry;
		fprintf(outFile, "Conn.UniqueCID = 0x%x    ",tea->Conn_UniqueCID);
		if (tea->IsNullTimeout) fprintf(outFile, "TimeOut:    NULL\n");
		else fprintf(outFile, "TimeOut:	%ld.%ld\n", tea->Timeout.tv_sec, tea->Timeout.tv_usec);
		break;	/* switch */
		}

	case MSENDPACKETSRELIABLY:
		{
		register struct te_MSENDPACKETSRELIABLY *tea;
		tea = &whichTE->Args.MSendPacketsReliablyEntry;
		fprintf(outFile, "HowMany:    %d    ConnArray[0]:    0x%x    ConnArray[0].UniqueCID = 0x%x\n",
			tea->HowMany, tea->ConnArray0, tea->ConnArray0_UniqueCID);
		fprintf(outFile, "PacketArray[0]:    OriginalAddress = 0x%lx    ", (long)tea->PacketArray0_Address);
		rpc2_PrintPacketHeader(&tea->PacketArray0, outFile);
		if (tea->IsNullTimeout) fprintf(outFile, "TimeOut:    NULL\n");
		else fprintf(outFile, "TimeOut:	%ld.%ld\n", tea->Timeout.tv_sec, tea->Timeout.tv_usec);
		break;	/* switch */
		}

	case XMITPACKET:
		{
		register struct te_XMITPACKET *tea;
		tea = &whichTE->Args.XmitPacketEntry;
		fprintf(outFile, "whichSocket = %ld\n", tea->whichSocket);
		fprintf(outFile, "whichHost:    "); rpc2_PrintHostIdent(&tea->whichHost, outFile);
		fprintf(outFile, "    ");
		fprintf(outFile, "whichPortal:    "); rpc2_PrintPortalIdent(&tea->whichPortal, outFile);
		fprintf(outFile,"\n");
		rpc2_PrintPacketHeader(&tea->whichPB, outFile);
		break;	/* switch */
		}

	case CLOCKTICK:
		{
		register struct te_CLOCKTICK *tea;
		tea = &whichTE->Args.ClockTickEntry;
		fprintf(outFile, "TimeNow:    %d\n", tea->TimeNow);
		break;	/* switch */
		}

	case CREATEMGRP:
		{
		register struct te_CREATEMGRP *tea;
		tea = &whichTE->Args.CreateMgrpEntry;
		fprintf(outFile, "MgroupHandle: %ld\n", tea->MgroupHandle);
		fprintf(outFile, "McastHost:      ");
		rpc2_PrintHostIdent((RPC2_HostIdent *)&(tea->McastHost), outFile);
		fprintf(outFile, "           ");
		fprintf(outFile, "McastPortal:      ");
		rpc2_PrintPortalIdent(&(tea->Port), outFile);
		fprintf(outFile, "           ");
		fprintf(outFile, "Subsystem:        ");
		rpc2_PrintSubsysIdent(&(tea->Subsys), outFile);
		fprintf(outFile, "           ");
		fprintf(outFile, "SecurityLevel = %s", tea->SecurityLevel == RPC2_OPENKIMONO ? "RPC2_OPENKIMONO" : (tea->SecurityLevel == RPC2_AUTHONLY ? "RPC2_AUTHONLY" : (tea->SecurityLevel == RPC2_SECURE ? "RPC2_SECURE" : (tea->SecurityLevel == RPC2_HEADERSONLY ? "RPC2_HEADERSONLY" :"??????"))));
		fprintf(outFile, "  IsEncrypted = %s  ", (tea->IsEncrypted) ? "TRUE" : "FALSE");
		fprintf(outFile, "  EncryptionType = %ld  SessionKey = 0x", tea->EncryptionType);
		for(i = 0; i < RPC2_KEYSIZE; i++)fprintf(outFile, "%lx", (long)tea->SessionKey[i]);
		fprintf(outFile, "\n");
		break; /* switch */
		}

	case ADDTOMGRP:
		{
		register struct te_ADDTOMGRP *tea;
		tea = &whichTE->Args.AddToMgrpEntry;
		fprintf(outFile, "MgroupHandle:   %ld     ConnHandle:   %ld\n", tea->MgroupHandle,
			tea->ConnHandle);
		break; /* switch */
		}

	case REMOVEFROMMGRP:
		{
		register struct te_REMOVEFROMMGRP *tea;
		tea = &whichTE->Args.RemoveFromMgrpEntry;
		fprintf(outFile, "MEntry:      "); rpc2_PrintMEntry(&tea->me, outFile);
		fprintf(outFile, "        ");
		fprintf(outFile, "CEntry:      "); rpc2_PrintCEntry(&tea->ce, outFile);
		fprintf(outFile, "\n");
		break; /* switch */
		}

	case XLATEMCASTPACKET:
		{
		register struct te_XLATEMCASTPACKET *tea;
		tea = &whichTE->Args.XlateMcastPacketEntry;
		fprintf(outFile, "PacketBuffer Address:  0x%lx      PacketHeader:     ",
			tea->pb_address);
		rpc2_PrintPacketHeader(&tea->pb, outFile);
		fprintf(outFile, "         ClientHost:      ");
		rpc2_PrintHostIdent((RPC2_HostIdent *)&tea->ThisHost, outFile);
		fprintf(outFile, "         ClientPortal:     ");
		rpc2_PrintPortalIdent(&tea->ThisPortal, outFile);
		fprintf(outFile, "\n");
		break; /* switch */
		}

	}
    
    }


void rpc2_PrintSEDesc(whichSDesc, whichFile)
    register SE_Descriptor *whichSDesc;
    register FILE *whichFile;
    {
    register long i;
    if (whichFile == NULL) whichFile = stdout;	/* it's ok, call by value */
    for (i = 0; i < SE_DefCount; i++)
	if (SE_DefSpecs[i].SideEffectType == whichSDesc->Tag) break;
    if (i >= SE_DefCount) return; /* Bogus side effect */
    (*SE_DefSpecs[i].SE_PrintSEDescriptor)(whichSDesc, whichFile);
    }
#endif RPC2DEBUG

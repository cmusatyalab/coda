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

#include <rpc2/rpc2_addrinfo.h>

/*------------ For tracing calls -------------*/
/* Call codes */
#define TRACEBASE	198732
#define INIT		TRACEBASE+1	/* not actually traced */
#define EXPORT 		TRACEBASE+2
#define DEEXPORT	TRACEBASE+3
#define ALLOCBUFFER	TRACEBASE+4
#define FREEBUFFER	TRACEBASE+5
#define SENDRESPONSE	TRACEBASE+6
#define GETREQUEST	TRACEBASE+7
#define MAKERPC		TRACEBASE+8
#define BIND		TRACEBASE+9
#define INITSIDEEFFECT	TRACEBASE+10
#define CHECKSIDEEFFECT	TRACEBASE+11
#define UNBIND		TRACEBASE+12
#define GETPRIVATEPOINTER	TRACEBASE+13
#define SETPRIVATEPOINTER	TRACEBASE+14
#define GETSEPOINTER	TRACEBASE+15
#define SETSEPOINTER	TRACEBASE+16
#define GETPEERINFO	TRACEBASE+17
#define SLNEWPACKET	TRACEBASE+18
#define SENDRELIABLY    TRACEBASE+19
#define XMITPACKET 	TRACEBASE+20
#define CLOCKTICK	TRACEBASE+21
#define MULTIRPC	TRACEBASE+22
#define MSENDPACKETSRELIABLY	TRACEBASE+23
#define CREATEMGRP	TRACEBASE+24
#define ADDTOMGRP	TRACEBASE+25
#define REMOVEFROMMGRP	TRACEBASE+26
#define XLATEMCASTPACKET TRACEBASE+27

struct TraceElem
    {
    int  CallCode;
    char ActiveLWP[20];
    union
	{
	struct te_EXPORT
	    {
	    RPC2_SubsysIdent Subsys;
	    }
	    ExportEntry;
    
	struct te_DEEXPORT
	    {
	    RPC2_SubsysIdent Subsys;
	    }
	    DeExportEntry;

	struct te_ALLOCBUFFER
	    {
	    int MinBodySize;
	    }
	    AllocBufferEntry;

	struct te_FREEBUFFER
	    {
	    RPC2_PacketBuffer *BuffPtr;
	    }
	    FreeBufferEntry;

	struct te_SENDRESPONSE
	    {
	    RPC2_Handle ConnHandle;
	    RPC2_PacketBuffer *Reply_Address;
	    RPC2_PacketBuffer Reply;
	    int IsNullSDesc;
	    SE_Descriptor SDesc;
	    }
	    SendResponseEntry;

	struct te_GETREQUEST
	    {
	    RPC2_RequestFilter Filter;
	    int IsNullBreathOfLife;
	    struct timeval BreathOfLife;
	    long (*GetKeys)();
	    int EncryptionTypeMask;
	    }
	    GetRequestEntry;

	struct te_MAKERPC
	    {
	    RPC2_Handle ConnHandle;
	    RPC2_PacketBuffer *Request_Address;
	    RPC2_PacketBuffer Request;
	    int IsNullSDesc;
	    SE_Descriptor SDesc;
	    int IsNullBreathOfLife;
	    struct timeval BreathOfLife;
	    int EnqueueRequest;
	    }
	    MakeRPCEntry;

	struct te_MULTIRPC
	    {
	    RPC2_Handle *ConnHandle;
	    RPC2_Integer HowMany;
	    RPC2_PacketBuffer *Request_Address;
	    RPC2_PacketBuffer Request;
	    int IsNullSDesc;
	    SE_Descriptor SDesc;
	    RPC2_HandleResult_func *HandleResult;
	    int IsNullBreathOfLife;
	    struct timeval BreathOfLife;
/*	    int EnqueueRequest; */
	    }
	    MultiRPCEntry;

	struct te_BIND
	    {
	    int SecurityLevel;
	    int EncryptionType;
	    RPC2_HostIdent Host;
	    RPC2_PortIdent Port;
	    RPC2_SubsysIdent Subsys;
	    int SideEffectType;
	    int IsNullClientIdent;
	    RPC2_CountedBS ClientIdent;
	    char ClientIdent_Value[20];
	    int IsNullSharedSecret;
	    RPC2_EncryptionKey SharedSecret;
	    }
	    BindEntry;

	struct te_INITSIDEEFFECT
	    {
	    RPC2_Handle ConnHandle;
	    int IsNullSDesc;
	    SE_Descriptor SDesc;
	    }
	    InitSideEffectEntry;

	struct te_CHECKSIDEEFFECT
	    {
	    RPC2_Handle ConnHandle;
	    int IsNullSDesc;
	    SE_Descriptor SDesc;
	    int Flags;
	    }
	    CheckSideEffectEntry;

	struct te_UNBIND
	    {
	    RPC2_Handle whichConn;
	    }
	    UnbindEntry;

	struct te_GETPRIVATEPOINTER
	    {
	    RPC2_Handle ConnHandle;
	    }
	    GetPrivatePointerEntry;

	struct te_SETPRIVATEPOINTER
	    {
	    RPC2_Handle ConnHandle;
	    char *PrivatePtr;
	    }
	    SetPrivatePointerEntry;

	struct te_GETSEPOINTER
	    {
	    RPC2_Handle ConnHandle;
	    }
	    GetSEPointerEntry;

	struct te_SETSEPOINTER
	    {
	    RPC2_Handle ConnHandle;
	    char *SEPtr;
	    }SetSEPointerEntry;

	struct te_GETPEERINFO
	    {
	    RPC2_Handle ConnHandle;
	    }
	    GetPeerInfoEntry;

	struct te_SLNEWPACKET
	    {
	    RPC2_PacketBuffer *pb_Address;
	    RPC2_PacketBuffer pb;
	    }
	    SLNewPacketEntry;


	struct te_SENDRELIABLY
	    {
	    struct CEntry *Conn;
	    int Conn_UniqueCID;
	    RPC2_PacketBuffer *Packet_Address;
	    RPC2_PacketBuffer Packet;
	    int IsNullTimeout;
	    struct timeval Timeout;
	    }
	    SendReliablyEntry;

	struct te_MSENDPACKETSRELIABLY
	    {
	    int HowMany;
	    struct CEntry *ConnArray0;	/* only first element */
	    int ConnArray0_UniqueCID;
	    RPC2_PacketBuffer *PacketArray0_Address;	/* of first packet */
	    RPC2_PacketBuffer PacketArray0;	/* first packet */
	    long (*HandleResult)();
	    int IsNullTimeout;
	    struct timeval Timeout;
	    }
	    MSendPacketsReliablyEntry;

	struct te_XMITPACKET
	    {
	    RPC2_PacketBuffer *whichPB_Address;
	    RPC2_PacketBuffer whichPB;
	    long whichSocket;
	    struct RPC2_addrinfo whichAddr;
	    }
	    XmitPacketEntry;
	    
	struct te_CLOCKTICK
	    {
	    int TimeNow;
	    }
	    ClockTickEntry;
	    
	struct te_CREATEMGRP
	    {
	    RPC2_Handle MgroupHandle;
	    RPC2_McastIdent McastHost;
	    RPC2_PortIdent Port;
	    RPC2_SubsysIdent Subsys;
	    RPC2_Integer SecurityLevel;
	    int IsEncrypted;
	    RPC2_EncryptionKey SessionKey;
	    RPC2_Integer EncryptionType;
	    }
	    CreateMgrpEntry;

	struct te_ADDTOMGRP
	    {
	    RPC2_Handle MgroupHandle;
	    RPC2_Handle ConnHandle;
	    }
	    AddToMgrpEntry;

	struct te_REMOVEFROMMGRP
	    {
	    struct MEntry me;
	    struct CEntry ce;
	    }
	    RemoveFromMgrpEntry;

	struct te_XLATEMCASTPACKET
	    {
	    RPC2_PacketBuffer pb;
	    long pb_address;
	    struct RPC2_addrinfo ThisAddr;
	    }
	    XlateMcastPacketEntry;

	}
	Args;
    };


/* Macros to actually do the tracing follows.  Each of these is used only once,
    but is placed here to avoid cluttering up all the other files */
#ifndef RPC2DEBUG
#define TR_SENDRESPONSE()
#define TR_GETREQUEST()
#define TR_MAKERPC()
#define TR_BIND()
#define TR_INITSE()
#define TR_CHECKSE()
#define TR_UNBIND()
#define TR_MULTI()
#define TR_MSENDRELIABLY()
#define TR_XMIT()
#define TR_RECV()
#define TR_SENDRELIABLY()
#define TR_CREATEMGRP()
#define TR_ADDTOMGRP()
#define TR_REMOVEFROMMGRP()
#define TR_XLATEMCASTPACKET()
#else
#define TR_SENDRESPONSE() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_SENDRESPONSE *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.SendResponseEntry;\
	te->CallCode = SENDRESPONSE;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->ConnHandle = ConnHandle;\
	tea->Reply_Address = Reply;\
	tea->Reply = *Reply;	/* structure assignment */\
	} } while (0)


#define TR_GETREQUEST() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_GETREQUEST *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.GetRequestEntry;\
	te->CallCode = GETREQUEST;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->Filter = *Filter;	/* structure assignment */\
	if (BreathOfLife == NULL) tea->IsNullBreathOfLife = TRUE;\
	else {tea->IsNullBreathOfLife = FALSE; tea->BreathOfLife = *BreathOfLife; /* structure assignment */}\
	tea->GetKeys = GetKeys;\
	tea->EncryptionTypeMask = EncryptionTypeMask;\
	} } while(0)

#define TR_MAKERPC() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_MAKERPC *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.MakeRPCEntry;\
	te->CallCode = MAKERPC;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->ConnHandle = ConnHandle;\
	tea->Request_Address = Request;\
	tea->Request = *Request;	/* structure assignment */\
	if (SDesc == NULL) tea->IsNullSDesc = TRUE;\
	else {tea->IsNullSDesc = FALSE; tea->SDesc = *SDesc; /* structure assignment */}\
	if (BreathOfLife == NULL) tea->IsNullBreathOfLife = TRUE;\
	else {tea->IsNullBreathOfLife = FALSE; tea->BreathOfLife = *BreathOfLife; /* structure assignment */}\
	tea->EnqueueRequest = EnqueueRequest;\
	} } while (0)


#define TR_BIND() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_BIND *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.BindEntry;\
	te->CallCode = BIND;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->SecurityLevel = Bparms->SecurityLevel;\
	tea->EncryptionType = Bparms->EncryptionType;\
	tea->Host = *Host;	/* structure assignment */\
	tea->Port = *Port;	/* structure assignment */\
	tea->Subsys = *Subsys;	/* structure assignment */\
	tea->SideEffectType = Bparms->SideEffectType;\
	if (Bparms->ClientIdent == NULL) tea->IsNullClientIdent = TRUE;\
	else\
	    {\
	    tea->IsNullClientIdent = FALSE;\
	    tea->ClientIdent.SeqLen = Bparms->ClientIdent->SeqLen;	/* not actual length, could be truncated */\
	    if (Bparms->ClientIdent->SeqLen < sizeof(tea->ClientIdent_Value))\
		memcpy(tea->ClientIdent_Value, Bparms->ClientIdent->SeqBody, Bparms->ClientIdent->SeqLen);\
	    else memcpy(tea->ClientIdent_Value, Bparms->ClientIdent->SeqBody, sizeof(tea->ClientIdent_Value));\
	    }\
	if (Bparms->SharedSecret == NULL) tea->IsNullSharedSecret = TRUE;\
	else {tea->IsNullSharedSecret = FALSE; memcpy(tea->SharedSecret, Bparms->SharedSecret, sizeof(RPC2_EncryptionKey));}\
	} } while (0)

#define TR_INITSE()do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_INITSIDEEFFECT *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.InitSideEffectEntry;\
	te->CallCode = INITSIDEEFFECT;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->ConnHandle = ConnHandle;\
	if (SDesc == NULL) tea->IsNullSDesc = TRUE;\
	else {tea->IsNullSDesc = FALSE; tea->SDesc = *SDesc; /* structure assignment */ }\
	} } while(0)


#define TR_CHECKSE()do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_CHECKSIDEEFFECT *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.CheckSideEffectEntry;\
	te->CallCode = CHECKSIDEEFFECT;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->ConnHandle = ConnHandle;\
	if (SDesc == NULL) tea->IsNullSDesc = TRUE;\
	else {tea->IsNullSDesc = FALSE; tea->SDesc = *SDesc;	/* structure assignment */}\
	tea->Flags = Flags;\
	} } while (0)

#define TR_UNBIND() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_UNBIND  *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.UnbindEntry;\
	te->CallCode = UNBIND;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->whichConn = whichConn;\
	}} while (0)

#define TR_MULTI() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_MULTIRPC *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.MultiRPCEntry;\
	te->CallCode = MULTIRPC;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->ConnHandle = ConnHandleList;\
	tea->Request_Address = Request;\
	tea->Request = *Request;	/* structure assignment */\
	if (SDescList == NULL) tea->IsNullSDesc = TRUE;\
	else {tea->IsNullSDesc = FALSE; tea->SDesc = SDescList[0]; /* structure assignment */}\
	tea->HandleResult = ArgInfo->HandleResult;\
	if (BreathOfLife == NULL) tea->IsNullBreathOfLife = TRUE;\
	else {tea->IsNullBreathOfLife = FALSE; tea->BreathOfLife = *BreathOfLife; /* structure assignment */}\
	} } while (0)


#define TR_MSENDRELIABLY() do { \
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_MSENDPACKETSRELIABLY *tea;\
	int idx; \
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.MSendPacketsReliablyEntry;\
	te->CallCode = MSENDPACKETSRELIABLY;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->HowMany = HowMany;\
	for(idx = 0; !ConnHandleList[idx] && idx < HowMany; idx++)/*loop*/; \
	tea->ConnArray0 = ConnArray[idx];\
	tea->ConnArray0_UniqueCID = (ConnArray[idx])->UniqueCID;\
	tea->PacketArray0_Address = PacketArray[idx];\
	tea->PacketArray0 = *(PacketArray[idx]);  /* structure assignment */\
	if (TimeOut == NULL) tea->IsNullTimeout = 1;\
	else\
	    {\
	    tea->IsNullTimeout = 0;\
	    tea->Timeout = *TimeOut;	/* structure assignment */\
	    }\
	} } while(0)

#define TR_XMIT() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_XMITPACKET *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.XmitPacketEntry;\
	te->CallCode = XMITPACKET;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->whichSocket = whichSocket;\
	tea->whichPB_Address = whichPB;\
	tea->whichPB = *whichPB;	/* structure assignment */\
	rpc2_htonp(&tea->whichPB);\
	tea->whichAddr = *addr;	/* BAD! structure assignment */\
	tea->whichAddr.ai_next = NULL; \
	} }while(0)

#define TR_RECV() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_SLNEWPACKET *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.SLNewPacketEntry;\
	te->CallCode = SLNEWPACKET;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->pb_Address = whichBuff;\
	tea->pb = *whichBuff;	/* Structure assignment */\
	rpc2_ntohp(&tea->pb);\
	} } while(0)

#define TR_SENDRELIABLY() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_SENDRELIABLY *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.SendReliablyEntry;\
	te->CallCode = SENDRELIABLY;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->Conn = Conn;\
	tea->Conn_UniqueCID = Conn->UniqueCID;\
	tea->Packet_Address = Packet;\
	tea->Packet = *Packet;  /* structure assignment */\
	if (TimeOut == NULL) tea->IsNullTimeout = 1;\
	else\
	    {\
	    tea->IsNullTimeout = 0;\
	    tea->Timeout = *TimeOut;	/* structure assignment */\
	    }\
	} } while(0)


#define TR_CREATEMGRP() do { \
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_CREATEMGRP *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.CreateMgrpEntry;\
	te->CallCode = CREATEMGRP;\
	strncpy(te->ActiveLWP, LWP_Name(), sizeof(te->ActiveLWP)-1);\
	tea->MgroupHandle = *MgroupHandle;\
	tea->McastHost = *MulticastHost;    /* structure assignment */\
	tea->Subsys = *Subsys;		    /* structure assignment */\
	tea->SecurityLevel = SecurityLevel;\
	tea->IsEncrypted = ((SessionKey == NULL) ? 0 : 1);\
	if (tea->IsEncrypted) memcpy(tea->SessionKey, SessionKey, sizeof(RPC2_EncryptionKey));\
	tea->EncryptionType = EncryptionType;\
	} } while(0)

#define TR_ADDTOMGRP() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_ADDTOMGRP *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.AddToMgrpEntry;\
	te->CallCode = ADDTOMGRP;\
	tea->MgroupHandle = MgroupHandle;\
	tea->ConnHandle = ConnHandle;\
	} }while(0)


#define TR_REMOVEFROMMGRP() do { \
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_REMOVEFROMMGRP *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.RemoveFromMgrpEntry;\
	te->CallCode = REMOVEFROMMGRP;\
	tea->me = *me;	    /* structure assignment */\
	tea->ce = *ce;	    /* structure assignment */\
	} } while (0)


#define TR_XLATEMCASTPACKET() do {\
    if (RPC2_Trace && rpc2_TraceBuffHeader)\
	{\
	struct TraceElem *te;\
	struct te_XLATEMCASTPACKET *tea;\
	te = (struct TraceElem *)CBUF_NextSlot(rpc2_TraceBuffHeader);\
	tea = &te->Args.XlateMcastPacketEntry;\
	te->CallCode = XLATEMCASTPACKET;\
	tea->pb = *pb;			/* structure assignment */\
	tea->pb_address = (long) pb;\
	tea->ThisAddr = *pb->Prefix.PeerAddr;	/* BAD! structure assignment */\
	tea->ThisAddr.ai_next = NULL; \
	} } while (0)
#endif


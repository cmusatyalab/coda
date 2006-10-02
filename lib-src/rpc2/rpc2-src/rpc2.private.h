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

#ifndef _RPC2_PRIVATE_H_
#define _RPC2_PRIVATE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <netinet/in.h>
#include <signal.h>
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <string.h>
#include <dllist.h>

#include <rpc2/rpc2_addrinfo.h>
#include <rpc2/secure.h>

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
/* this should be large enough to fit 'any' socket address. */
struct sockaddr_storage {
    struct sockaddr __ss_sa;
    char _ss_padding[128 - sizeof(struct sockaddr)];
};
#endif

#ifndef HAVE_STRUCT_SOCKADDR_IN6
struct in6_addr {
    u_int8_t u6_addr[16];
};
struct sockaddr_in6 {
    u_int16_t sin6_family;
    u_int16_t sin6_port;
    u_int32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
};
#endif

#ifndef HAVE_SOCKLEN_T
#define socklen_t unsigned int
#endif


/*
Magic Number assignments for runtime system objects.
Truly random values to allow easy detection of storage corruption.

OBJ_PACKETBUFFER = 3247517
OBJ_CENTRY = 868
OBJ_MENTRY = 69743
OBJ_SLENTRY = 107
OBJ_SSENTRY = 34079
OBJ_HENTRY = 48127

*/

/* Role and state determination:
   Top 2 bytes (0x0088 or 0x0044) determine client or server
   Bottom 2 bytes determine state within client or server.
   This allows rapid testing using bit masks.
*/

/* Server or Client? */
#define FREE 0x0
#define CLIENT 0x00880000
#define SERVER 0x00440000

/* Client States */
#define C_THINK 0x1
#define C_AWAITREPLY 0x2
#define C_HARDERROR 0x4
#define C_AWAITINIT2 0x8
#define C_AWAITINIT4 0x10

/* Server States */
#define	S_AWAITREQUEST 0x1
#define S_REQINQUEUE 0x2
#define S_PROCESS 0x4
#define	S_INSE 0x8
#define S_HARDERROR 0x10
#define S_STARTBIND 0x20
#define	S_AWAITINIT3 0x40
#define S_FINISHBIND 0x80
#define S_AWAITENABLE 0x0100


#define SetRole(e, role) (e->State = role)
#define SetState(e, new) (e->State = (e->State & 0xffff0000) | (new))
#define TestRole(e, role)  ((e->State & 0xffff0000) == role)
#define TestState(e, role, smask)\
	(TestRole(e, role) && ((e->State & 0x0000ffff) & (smask)))

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* MAXRTO is used to avoid unbounded timeouts */
#define RPC2_MAXRTO     30000000   /* max rto (rtt + variance) is 30 seconds */
#define RPC2_DELACK_DELAY 100000   /* delay for server to send an ack that it
				      received a request. This provides an
				      upper bound on server response time. */

/* Definitions for Flags field of connections */
#define CE_OLDV  0x1  /* old version detected during bind */

/*---------------- Data Structures ----------------*/
struct LinkEntry	/* form of entries in doubly-linked lists */
    {
    struct LinkEntry *NextEntry;	/* in accordance with insque(3) */
    struct LinkEntry *PrevEntry;
    long MagicNumber;			/* unique for object type; NEVER altered */
    struct LinkEntry **Qname;	/* head pointer of queue on which this element should be */
    /* body comes here, but is irrelevant for list manipulation and object validation */
    };

struct CEntry		/* describes a single RPC connection */
    {
    /* Link Entry Fields */
    struct dllist_head connlist;
    enum {OBJ_CENTRY = 868, OBJ_FREE_CENTRY = 686} MagicNumber;
    struct CEntry *Qname;

    struct dllist_head Chain;

    /* State, identity  and sequencing */
    long State;
    RPC2_Handle  UniqueCID;
    RPC2_Unsigned NextSeqNumber;
    RPC2_Integer SubsysId;
    RPC2_Integer Flags;	    /* CE_OLDV ? */
    time_t LastRef;    			/* when CEntry was last looked up */

    /* Security */
    RPC2_Integer SecurityLevel;
    RPC2_EncryptionKey SessionKey;
    RPC2_Integer EncryptionType;

    /* PeerInfo */
    RPC2_Handle      PeerHandle; /* peer's connection ID */
    RPC2_Integer     PeerUnique; /* Unique integer used in Init1 bind request
				    from peer */
    struct HEntry   *HostInfo;	 /* Link to host table and liveness
				    information */

    /* Auxiliary stuff */
    struct SE_Definition *SEProcs;	/* pointer to  side effect routines */
    long sebroken;			/* flag set on hard se errors */
    struct MEntry *Mgrp;		/* Multicast group of this conn */
    char *PrivatePtr;		        /* rock for pointer to user data */
    char *SideEffectPtr;		/* rock for  per-connection side-effect data */
    RPC2_Integer Color;                 /* Packets sent out get this color.
					    See documentation for libfail(3).
					    Only lowest byte is useful. */
    /* Stuff for SocketListener */
    struct SL_Entry *MySl;	/* SL entry pertaining to this conn, or NULL */
    RPC2_PacketBuffer *HeldPacket;	/* NULL or pointer to packet held in readiness
					    for retransmission (reply or last packet
					    of Bind handshake */
    unsigned long reqsize;              /* size of request, for RTT calcs */
    unsigned long respsize;              /* size of response, for RTT calcs */

    /* Retransmission info - client */
#define LOWERLIMIT 300000               /* floor on lower limit, usec. */
    unsigned long LowerLimit;           /* minimum retry interval, usec */

    long RTT;                           /* Smoothed RTT estimate, 1msec units. */
    long RTTVar;                        /* Variance of RTT, 1msec units. */
    unsigned int TimeStampEcho;
    unsigned int RequestTime;
    long Retry_N;                       /* Number of retries for this connection. */
    struct timeval *Retry_Beta;         /* Retry parameters for this connection. */
    struct timeval SaveResponse;        /* 2*Beta0, lifetime of saved response packet. */
    RPC2_RequestFilter Filter;		/* Set on the server during binding,
					   filter incoming requests so that the
					   SubsysID/Connection matches that of
					   the handler we authenticated with */
    struct security_association sa;
    };


struct MEntry			/* describes an RPC multicast connection */
    {
    /* Link Entry Pointers */
    struct MEntry	    *Next;
    struct MEntry	    *Prev;
    enum {OBJ_MENTRY = 69743}
			    MagicNumber;
    struct MEntry	    *Qname;

    /* Multicast Group Connection info */
    RPC2_Integer	    State;	    /* eg {C,S}_AWAITREQUEST */
    struct RPC2_addrinfo    *ClientAddr;    /* |		*/
    RPC2_Handle		    MgroupID;	    /* |		*/
    RPC2_Integer	    NextSeqNumber;  /* for mgrp connection */

    /* Auxiliary stuff */
    struct SE_Definition *SEProcs;	/* pointer to side effect routines */
    char *SideEffectPtr;		/* rock for per-mgrp side-effect data */

    /* Connection entries associated with this Mgrp. Clients have an
       array of these, servers only one. */
    union
	{
	struct
	    {
	    struct CEntry   **mec_listeners;
	    long	    mec_howmanylisteners;
	    long	    mec_maxlisteners;
	    } me_client;
	struct CEntry	    *mes_conn;
	} me_conns;
#define	listeners	    me_conns.me_client.mec_listeners
#define	howmanylisteners    me_conns.me_client.mec_howmanylisteners
#define	maxlisteners	    me_conns.me_client.mec_maxlisteners
#define	conn		    me_conns.mes_conn
	};


/* SL entries are the data structures used for comm between user LWPs and
  the SocketListener.  The user LWP creates an entry, sets it up, sets a 
  timeout on it and goes to sleep.  SocketListener eventually sets a
  return code on the entry and wakes up the LWP.
  
  SL Entries are typed:
          REPLY        associated with a specific connection
	               created by user LWP in SendResponse
	               creating LWP does not wait for timeout to expire
		       destroyed by SocketListener

	  REQ          not associated with a specific connection
                       created and destroyed by user LWP in GetRequest

          DELACK       delayed ack response to receiving a request
	               created by SocketListener in HandleRequest
		       destroyed by SocketListener on timeout
	               destroyed by user LWP when reply is sent.

          OTHER        associated with a specific connection
	               created and destroyed by user LWP

Entries of type REQ are on a separate list to minimize list searching in 
SocketListener.  Other types of entries can be directly accessed via the
connection they are associated with.

*/

/* NOTE:  enum definitions  have to be non-anonymous: else a dbx bug is triggered */
enum SL_Type {REPLY=1421, REQ=1422, OTHER=1423, DELACK=20010911};
enum RetVal {WAITING=38358230, ARRIVED=38358231, TIMEOUT=38358232,
	KEPTALIVE=38358233, KILLED=38358234, NAKED=38358235};

/* data structure for communication with SocketListener */
struct SL_Entry
    {
    /* LinkEntry fields */
    struct SL_Entry *NextEntry;
    struct SL_Entry *PrevEntry;
    enum {OBJ_SLENTRY = 107} MagicNumber;
    struct SL_Entry *Qname;

    enum SL_Type Type;

    /* Timeout-related fields */
    struct TM_Elem TElem;	/* element  to be inserted into  timer chain;
				    The BackPointer field of TElem will
				    point to this SL_Entry */
    enum RetVal ReturnCode;     /* SocketListener changes this from WAITING */

    /* Other fields */
    RPC2_Handle Conn;		/* NULL or conn corr to this SL Entry */
    RPC2_PacketBuffer *Packet;  /* NULL,  awaiting retransmission or just arrived */
    RPC2_RequestFilter Filter;  /* useful only in GetRequest */
    long RetryIndex;		/* useful only in MakeRPC */
    };


struct SubsysEntry	/* Defines a subsystem being actively serviced by a server */
    {			/* Created by RPC2_InitSubsys(); destroyed by RPC2_EndSubsys() */
    struct SubsysEntry	*Next;	/* LinkEntry field */
    struct SubsysEntry	*Prev;	/* LinkEntry field */
    enum {OBJ_SSENTRY = 34079} MagicNumber;
    struct SubsysEntry *Qname;	/* LinkEntry field */
    long Id;	/* using a struct is a little excessive, but it makes things uniform */
    };


/* extend with other side effect types */
typedef enum {UNSET_HE = 0, RPC2_HE = 1, SMARTFTP_HE = 2} HEType;

struct HEntry {
    /* Link Entry Pointers */
    struct HEntry    *Next;	/* LinkEntry field */
    struct HEntry    *Prev;	/* LinkEntry field */
    enum {OBJ_HENTRY = 48127}
		     MagicNumber;
    struct HEntry    *Qname;	/* LinkEntry field */
    struct HEntry    *HLink;	/* for host hash */
    int		      RefCount; /* # connections that have a reference */
    struct RPC2_addrinfo *Addr; /* network address */
    struct timeval   LastWord;	/* Most recent time we've heard from this host*/
    unsigned RPC2_NumEntries;	/* number of observations recorded */
    RPC2_NetLogEntry RPC2_Log[RPC2_MAXLOGLENGTH];
				/* circular buffer for recent observations on
				 * round trip times and packet sizes */
    unsigned SE_NumEntries;	/* number of sideeffect observations recorded */
    RPC2_NetLogEntry SE_Log[RPC2_MAXLOGLENGTH];

#define RPC2_RTT_SHIFT    3     /* Bits to right of binary point of RTT */
#define RPC2_RTTVAR_SHIFT 2     /* Bits to right of binary point of RTTVar */
    unsigned long   RTT;	/* RTT          (us<<RPC2_RTT_SHIFT) */
    unsigned long   RTTVar;	/* RTT variance (us<<RPC2_RTTVAR_SHIFT) */

#define RPC2_BR_SHIFT    3
#define RPC2_BRVAR_SHIFT 2
    unsigned long   BR;		/* Byterate          (ns/B<<RPC2_BW_SHIFT) */
    unsigned long   BRVar;	/* Byterate variance (ns/B<<RPC2_BWVAR_SHIFT) */
};


/*-------------- Format of special packets ----------------*/

/* WARNING: if you port RPC2, make sure the structure sizes work out to be the same on your target machine */

struct Init1Body			/* Client to Server: format of packets with opcode of RPC2_INIT1xxx */
{
/* body of fake packet from RPC2_GetRequest() */
    RPC2_Integer FakeBody_SideEffectType;
    RPC2_Integer FakeBody_SecurityLevel;
    RPC2_Integer FakeBody_EncryptionType;
    RPC2_Integer FakeBody_AuthenticationType;
    RPC2_Unsigned FakeBody_ClientIdent_SeqLen;
    RPC2_Unsigned FakeBody_ClientIdent_SeqBody;
/* end of body of fake packet from RPC2_GetRequest() */
    /* When MakeFake is called we'll clobber the remaining data in this packet
     * because we don't really send a pointer to the ClientIdent.SeqBody, but
     * we move it there from the tail of the packet so that the stub generator
     * can unpack it as a valid RPC2_NEWCONNECTION rpc call */

    RPC2_Integer XRandom;		/* encrypted random number */
    char usedtobehostport[92];		/* XXX not used anymore but old rpc2
					   servers need the alignment */
    RPC2_Integer Uniquefier;		/* to allow detection of retransmissions */
    RPC2_Unsigned RPC2SEC_version;	/* supported handshake version */
    RPC2_Unsigned Preferred_Keysize;	/* preferred encryption key length */
    RPC2_Integer Spare3;
    RPC2_Byte Version[96];		/* set to RPC2_VERSION */
    RPC2_Byte Text[4];			/* Storage for the ClientIdent. Moved
					   to FakeBody_ClientIdent_SeqBody in
					   rpc2a.c:MakeFake(). It can of course
					   be more than 4 bytes, this is just
					   for alignment purposes. */
    };

struct Init2Body		/* Server to Client */
    {
    RPC2_Integer XRandomPlusOne;
    RPC2_Integer YRandom;
    RPC2_Integer Spare1;
    RPC2_Integer Spare2;
    RPC2_Integer Spare3;
    };

struct Init3Body		/* Client to Server */
    {
    RPC2_Integer YRandomPlusOne;
    RPC2_Integer Spare1;
    RPC2_Integer Spare2;
    RPC2_Integer Spare3;
    };

struct Init4Body		/* Server to Client */
    {
    RPC2_Integer InitialSeqNumber;	/* Seq number of first expected packet from client */
    RPC2_EncryptionKey	SessionKey;	/* for use from now on */
    RPC2_Integer XRandomPlusTwo;	/* prevent replays of this packet -rnw 2/7/98 */
    RPC2_Integer Spare1;
    RPC2_Integer Spare2;
    };

struct InitMulticastBody	/* Client to Server */
    {
    RPC2_Handle		MgroupHandle;
    RPC2_Integer	InitialSeqNumber;   /* Seq number of next packet from client */
    RPC2_EncryptionKey	SessionKey;	    /* for use on the multicast channel */
    RPC2_Integer	Spare1;
    RPC2_Integer	Spare2;
    RPC2_Integer	Spare3;
    };

/* Macros for manipulating color field of packets */
#define htonPktColor(p) (p->Header.Flags = htonl(p->Header.Flags))
#define ntohPktColor(p) (p->Header.Flags = ntohl(p->Header.Flags))
#define SetPktColor(p,c)\
   (p->Header.Flags = (p->Header.Flags & 0xff00ffff) | ((c & 0xff) << 16))
#define GetPktColor(p)  ((p->Header.Flags >> 16) & 0x000000ff)




/*------------- List headers and counts -------------*/

/* NOTE: all these lists are doubly-linked and circular */

/* The multicast group abstraction */
extern struct MEntry *rpc2_MgrpFreeList;	/* free mgrp blocks */
extern long rpc2_MgrpFreeCount, rpc2_MgrpCreationCount;

/* Items for SocketListener */

extern struct SL_Entry *rpc2_SLFreeList,	/* free entries */
        	*rpc2_SLReqList,		/* in use, of type REQ */
		*rpc2_SLList;     /* in use, of types REPLY or OTHER */
extern long rpc2_SLReqCount, rpc2_SLCount;

/* Packet buffers */
extern RPC2_PacketBuffer *rpc2_PBList, *rpc2_PBHoldList;
extern RPC2_PacketBuffer *rpc2_PBSmallFreeList, *rpc2_PBMediumFreeList, *rpc2_PBLargeFreeList;

/* Subsystem definitions */
extern struct SubsysEntry *rpc2_SSFreeList,	/* free entries */
			*rpc2_SSList;		/* subsystems in active use */
	
/* Host info definitions */
extern struct HEntry *rpc2_HostFreeList, *rpc2_HostList;
extern long rpc2_HostFreeCount, rpc2_HostCount, rpc2_HostCreationCount;


/*------------- Miscellaneous  global data  ------------*/
extern int rpc2_ipv6ready; /* can userspace handle IPv6 addresses */
extern int rpc2_v4RequestSocket; /* fd of RPC socket  */
extern int rpc2_v6RequestSocket; /* fd of RPC socket  */
				/* we may need more when we deal with many domains */
extern struct TM_Elem *rpc2_TimerQueue;
extern struct CBUF_Header *rpc2_TraceBuffHeader;
extern PROCESS rpc2_SocketListenerPID;	/* used in IOMGR_Cancel() calls */
extern unsigned long rpc2_LamportClock;
extern long Retry_N;
extern struct timeval *Retry_Beta;
#define MaxRetryInterval Retry_Beta[0]
extern struct timeval SaveResponse;

/* List manipulation routines */
void rpc2_Replenish();
struct LinkEntry *rpc2_MoveEntry();
struct SL_Entry *rpc2_AllocSle();
void rpc2_FreeSle(struct SL_Entry **sl);
void rpc2_ActivateSle(), rpc2_DeactivateSle();
struct SubsysEntry *rpc2_AllocSubsys();
void rpc2_FreeSubsys();

void FreeHeld(struct SL_Entry *sle);

/* Socket creation */

long rpc2_CreateIPSocket(int af, int *svar, struct RPC2_addrinfo *addr, short *Port);

/* Packet  routines */
long rpc2_SendReliably(), rpc2_MSendPacketsReliably();
void rpc2_XmitPacket(RPC2_PacketBuffer *pb, struct RPC2_addrinfo *addr,
		     int confirm);
void rpc2_InitPacket();
int rpc2_MorePackets(void);
long rpc2_RecvPacket(long whichSocket, RPC2_PacketBuffer *whichBuff);
void rpc2_htonp(RPC2_PacketBuffer *p);
void rpc2_ntohp(RPC2_PacketBuffer *p);
long rpc2_SetRetry(), rpc2_CancelRetry();
void rpc2_ResetLowerLimit();
void rpc2_UpdateRTT(RPC2_PacketBuffer *pb, struct CEntry *ceaddr);
void rpc2_ResetObs();
void rpc2_ExpireEvents();

/* Connection manipulation routines  */
int rpc2_InitConn(void);
void rpc2_FreeConn(), rpc2_SetConnError();
struct CEntry *rpc2_AllocConn();
struct CEntry *rpc2_ConnFromBindInfo(struct RPC2_addrinfo *peeraddr,
				     RPC2_Handle RemoteHandle,
				     RPC2_Integer whichUnique);
struct CEntry *__rpc2_GetConn(RPC2_Handle handle); /* doesn't bump lastref */
struct CEntry *rpc2_GetConn(RPC2_Handle handle);
void rpc2_ReapDeadConns(void);
void rpc2_IncrementSeqNumber(struct CEntry *);

/* Host manipulation routines */
void rpc2_InitHost(void);
struct HEntry *rpc2_GetHost(struct RPC2_addrinfo *addr);
void rpc2_FreeHost(struct HEntry **whichHost);
void rpc2_GetHostLog(struct HEntry *whichHost, RPC2_NetLog *log,
		     NetLogEntryType type);
int rpc2_AppendHostLog(struct HEntry *whichHost, RPC2_NetLogEntry *entry,
		       NetLogEntryType type);
void rpc2_ClearHostLog(struct HEntry *whichHost, NetLogEntryType type);

void RPC2_UpdateEstimates(struct HEntry *whichHost, RPC2_Unsigned ElapsedTime,
			  RPC2_Unsigned InBytes, RPC2_Unsigned OutBytes);
void rpc2_RetryInterval(RPC2_Handle whichConn, RPC2_Unsigned InBytes,
			RPC2_Unsigned OutBytes, int *retry, int maxretry,
			struct timeval *tv);

/* Multicast group manipulation routines */
void rpc2_InitMgrp(), rpc2_FreeMgrp(), rpc2_RemoveFromMgrp(), rpc2_DeleteMgrp();
struct MEntry *rpc2_AllocMgrp(struct RPC2_addrinfo *addr, RPC2_Handle handle);
struct MEntry *rpc2_GetMgrp(struct RPC2_addrinfo *addr, RPC2_Handle handle,
			    long role);

/* Hold queue routines */
void rpc2_HoldPacket(), rpc2_UnholdPacket();

/* RPC2_GetRequest() filter matching function */
int rpc2_FilterMatch();

/* Autonomous LWPs */
void rpc2_SocketListener(void *);
void rpc2_ClockTick(void *);

/* rpc2_SocketListener packet handlers */
void SL_RegisterHandler(unsigned int proto, void (*func)(RPC2_PacketBuffer *));
void rpc2_HandlePacket(RPC2_PacketBuffer *pb);

/* Packet timestamp creation */
unsigned int rpc2_TVTOTS(const struct timeval *tv);
void         rpc2_TSTOTV(const unsigned int ts, struct timeval *tv);
unsigned int rpc2_MakeTimeStamp();

/* Debugging routines */
void rpc2_PrintTMElem(), rpc2_PrintFilter(), rpc2_PrintSLEntry(),
	rpc2_PrintCEntry(), rpc2_PrintTraceElem(), rpc2_PrintPacketHeader(),
	rpc2_PrintHostIdent(), rpc2_PrintPortIdent(), rpc2_PrintSEDesc();
extern FILE *ErrorLogFile;

/* encryption */
struct security_association *rpc2_GetSA(uint32_t spi);

void rpc2_ApplyD(RPC2_PacketBuffer *pb, struct CEntry *ce);
void rpc2_ApplyE(RPC2_PacketBuffer *pb, struct CEntry *ce);

time_t rpc2_time();
long rpc2_InitRetry(long HowManyRetries, struct timeval *Beta0);

void rpc2_NoteBinding(struct RPC2_addrinfo *peeraddr,
		      RPC2_Handle RemoteHandle, RPC2_Integer whichUnique,
		      RPC2_Handle whichConn);

int mkcall(RPC2_HandleResult_func *ClientHandler, int ArgCount, int HowMany,
	   RPC2_Handle ConnList[], long offset, long rpcval, int *args);


/*-----------  Macros ---------- */

/* Test if more than one bit is set; WARNING: make sure at least one bit is set first! */
#define MORETHANONEBITSET(x) (x != (1 << (ffs((long)x)-1)))

/* Macros to work with preemption package */
/* #define rpc2_Enter() (PRE_BeginCritical()) */
#define rpc2_Enter() do { } while (0)
/*#define rpc2_Quit(rc) return(PRE_EndCritical(), (long)rc) */
#define rpc2_Quit(rc) return((long)rc)

/* Macros to check if host and portal ident structures are equal */
/* The lengths of the names really should be #defined. */
#define rpc2_HostIdentEqual(_hi1p_,_hi2p_) \
(((_hi1p_)->Tag == RPC2_HOSTBYINETADDR && (_hi2p_)->Tag == RPC2_HOSTBYINETADDR)? \
 ((_hi1p_)->Value.AddrInfo->ai_family == (_hi2p_)->Value.AddrInfo->ai_family && \
 (_hi1p_)->Value.AddrInfo->ai_addrlen == (_hi2p_)->Value.AddrInfo->ai_addrlen && !memcmp((_hi1p_)->Value.AddrInfo->ai_addr,(_hi2p_)->Value.AddrInfo->ai_addr,(_hi2p_)->Value.AddrInfo->ai_addrlen)): \
 (((_hi1p_)->Tag == RPC2_HOSTBYNAME && (_hi2p_)->Tag == RPC2_HOSTBYNAME)? \
  (strncmp((_hi1p_)->Value.Name, (_hi2p_)->Value.Name, 64) == 0):0))

/*------ Other definitions ------*/

/* Allocation constants */

#define SMALLPACKET	350
#define MEDIUMPACKET	1500
#define LARGEPACKET	RPC2_MAXPACKETSIZE


/* Packets sent  */
extern unsigned long rpc2_NoNaks;
extern long rpc2_BindLimit, rpc2_BindsInQueue;
extern long rpc2_FreeMgrps, rpc2_AllocMgrps;

/* RPC2_addrinfo helper routines */
struct RPC2_addrinfo *rpc2_resolve(RPC2_HostIdent *Host, RPC2_PortIdent *Port);
void rpc2_printaddrinfo(const struct RPC2_addrinfo *ai, FILE *f);
void rpc2_splitaddrinfo(RPC2_HostIdent *Host, RPC2_PortIdent *Port,
			const struct RPC2_addrinfo *ai);
void rpc2_simplifyHost(RPC2_HostIdent *Host, RPC2_PortIdent *Port);


/*--------------- Useful definitions that used to be in potpourri.h or util.h ---------------*/
/*                 Now included here to avoid including either of those files                */

/* Parameter usage */
#define	IN	/* Input parameter */
#define OUT	/* Output parameter */
#define INOUT	/* Obvious */


/* Conditional debugging output macros: no side effect in these! */
char *rpc2_timestring();
#ifdef RPC2DEBUG
#define say(when, what, how...)\
    do { \
	if (when < what){fprintf(rpc2_logfile, "[%s]%s: \"%s\", line %d:    ",\
		rpc2_timestring(), LWP_Name(), __FILE__, __LINE__);\
			fprintf(rpc2_logfile, ## how);(void) fflush(rpc2_logfile);}\
    } while(0);
#else 
#define say(when, what, how)	
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif


/*--------- End stuff from util.h ------------*/


/* macro to subtract one timeval from another */
#define SUBTIME(fromp, subp)\
do {\
    if ((subp)->tv_usec > (fromp)->tv_usec) \
      { (fromp)->tv_sec--; (fromp)->tv_usec += 1000000; }\
    (fromp)->tv_sec -= (subp)->tv_sec;\
    (fromp)->tv_usec -= (subp)->tv_usec;\
} while(0);

/* add one timeval to another */
#define ADDTIME(top, fromp)\
do {\
    (top)->tv_sec += (fromp)->tv_sec;\
    (top)->tv_usec += (fromp)->tv_usec;\
    if ((top)->tv_usec >= 1000000)\
      { (top)->tv_sec++; (top)->tv_usec -= 1000000; }\
} while(0);

#define CMPTIME(a, b, CMP)\
  (((a)->tv_sec == (b)->tv_sec) ?\
   ((a)->tv_usec CMP (b)->tv_usec) :\
   ((a)->tv_sec CMP (b)->tv_sec))

#define CLRTIME(tm) ((tm)->tv_sec = 0, (tm)->tv_usec = 0)
#define TIMERISSET(tm) ((tm)->tv_sec || (tm)->tv_usec)

/* macros to convert between timeval and timestamp */
#define TVTOTS(_tvp_, _ts_)\
do {\
    _ts_ = ((_tvp_)->tv_sec * 1000000 + (_tvp_)->tv_usec);\
} while(0);

#define TSTOTV(_tvp_, _ts_)\
do {\
    (_tvp_)->tv_sec = (_ts_) / 1000000;\
    (_tvp_)->tv_usec = (_ts_) % 1000000;\
} while(0);

#define TSDELTA(_ts1_, _ts2_) ((int)(_ts1_) - (int)(_ts2_))

#endif

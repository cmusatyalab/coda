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


#include <netinet/in.h>
#include <signal.h>
#include "coda_assert.h"
#include "dllist.h"

#define bool long


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
#define        S_REQINQUEUE 0x2
#define        S_PROCESS 0x4
#define	S_INSE 0x8
#define        S_HARDERROR 0x10
#define        S_STARTBIND 0x20
#define	S_AWAITINIT3 0x40
#define        S_FINISHBIND 0x80
#define        S_AWAITENABLE 0x0100


#define SetRole(e, role) (e->State = role)
#define SetState(e, new) (e->State = (e->State & 0xffff0000) | (new))
#define TestRole(e, role)  ((e->State & 0xffff0000) == role)
#define TestState(e, role, smask)\
	(TestRole(e, role) && ((e->State & 0x0000ffff) & (smask)))

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* MINRTO/MAXRTO are used to avoid unbounded timeouts */
#define RPC2_MINRTO   10000        /* min rto (rtt + variance) is 10 msec */
#define RPC2_MAXRTO   30000000     /* max rto (rtt + variance) is 30 seconds */
#define UNSET_BW ((unsigned long)-1)

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
    struct CEntry *NextEntry;
    struct CEntry *PrevEntry;
    enum {OBJ_CENTRY = 868} MagicNumber;
    struct CEntry *Qname;

    struct dllist_head Chain;

    /* State, identity  and sequencing */
    long State;
    RPC2_Handle UniqueCID;
    RPC2_Integer NextSeqNumber;	
    RPC2_Integer SubsysId;
    RPC2_Integer Flags;	    /* CE_OLDV ? */

    /* Security */
    RPC2_Integer SecurityLevel;
    RPC2_EncryptionKey SessionKey;
    RPC2_Integer EncryptionType;

    /* PeerInfo */
    RPC2_Handle      PeerHandle; /* peer's connection ID */
    RPC2_HostIdent   PeerHost;	 /* Internet or other address */
    RPC2_PortIdent   PeerPort; /* Internet port or other portal */
    RPC2_Integer     PeerUnique; /* Unique integer used in Init1 bind request
				    from peer */
    struct HEntry *HostInfo;	 /* Link to host table and liveness
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
    unsigned long reqsize;              /* size of request (+response), for RTT calcs */

    /* Retransmission info - client */
#define LOWERLIMIT 300000               /* floor on lower limit, usec. */
    unsigned long LowerLimit;           /* minimum retry interval, usec */

    long RTT;                           /* Smoothed RTT estimate, 1msec units. */
#define TimeStampEcho RTT           	/* If Role == SERVER, cannabalize for 
					   rpc timing */
    long RTTVar;                        /* Variance of RTT, 1msec units. */
#define RequestTime RTTVar              /* If Role == SERVER, cannabalize for
					   rpc timing */
    long Retry_N;                       /* Number of retries for this connection. */
    struct timeval *Retry_Beta;         /* Retry parameters for this connection. */
    struct timeval SaveResponse;        /* 2*Beta0, lifetime of saved response packet. */
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
    RPC2_HostIdent	    ClientHost;	    /* |		*/
    RPC2_PortIdent	    ClientPort;     /* | Unique ID	*/
    RPC2_Handle		    MgroupID;	    /* |		*/
    RPC2_Integer	    NextSeqNumber;  /* for mgrp connection */
    RPC2_Integer	    SubsysId;
    RPC2_Integer	    Flags;

    /* Security */
    RPC2_Integer	    SecurityLevel;
    RPC2_EncryptionKey	    SessionKey;
    RPC2_Integer	    EncryptionType;

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

    /* Other information - Only needed by client */
    RPC2_HostIdent	    IPMHost;	    /* IP Multicast Host Address */
    RPC2_PortIdent	    IPMPort;	    /* IP Multicast Port Number */
    RPC2_PacketBuffer	    *CurrentPacket; /* current multicast packet */
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

          OTHER        associated with a specific connection
	               created and destroyed by user LWP

Entries of type REQ are on a separate list to minimize list searching in 
SocketListener.  Other types of entries can be directly accessed via the
connection they are associated with.

*/

/* NOTE:  enum definitions  have to be non-anonymous: else a dbx bug is triggered */
enum SL_Type {REPLY=1421, REQ=1422, OTHER=1423};
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
    struct in_addr   Host;      /* IP address */
    struct timeval   LastWord;	/* Most recent time we've heard from this host*/

    RPC2_NetLogEntry Log[RPC2_MAXLOGLENGTH];
				/* circular buffer for recent observations on
				   round trip times and packet sizes */
    unsigned NumEntries;	/* number of observations recorded */

#define RPC2_RTT_SHIFT    3     /* Bits to right of binary point of RTT */
#define RPC2_RTTVAR_SHIFT 2     /* Bits to right of binary point of RTTVar */
    unsigned long   RTT;	/* RTT          (us<<RPC2_RTT_SHIFT) */
    unsigned long   RTTVar;	/* RTT variance (us<<RPC2_RTTVAR_SHIFT) */

#define RPC2_BW_SHIFT    3
#define RPC2_BWVAR_SHIFT 2
    unsigned long   BW;		/* BW          (B/s<<RPC2_BW_SHIFT) */
    unsigned long   BWVar;	/* BW variance (B/s<<RPC2_BWVAR_SHIFT) */

    unsigned long   LastBytes;	/* last packet size */
};


/*-------------- Format of special packets ----------------*/

/* WARNING: if you port RPC2, make sure the structure sizes work out to be the same on your target machine */

struct Init1Body			/* Client to Server: format of packets with opcode of RPC2_INIT1xxx */
    {
    RPC2_NewConnectionBody FakeBody;	/* body of fake packet from RPC2_GetRequest() */
    RPC2_Integer XRandom;		/* encrypted random number */
    RPC2_HostIdent   SenderHost;   /* XXX not used anymore */
    RPC2_PortIdent   SenderPort;   /* XXX not used anymore */
    RPC2_Integer Uniquefier;		/* to allow detection of retransmissions */
    RPC2_Integer Spare1;
    RPC2_Integer Spare2;
    RPC2_Integer Spare3;
    RPC2_Byte Version[96];		/* set to RPC2_VERSION */
    RPC2_Byte Text[4];	    /* Storage pointed to by FakeBody.ClientIdent.SeqBody;
				4 bytes is a fake to cause proper longword alignment
				on all interesting machines */
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

/* The basic connection abstraction */
extern struct CEntry *rpc2_ConnFreeList,	/* free connection blocks */
			*rpc2_ConnList;		/* active connections  */
extern long rpc2_ConnFreeCount, rpc2_ConnCount, rpc2_ConnCreationCount;	

/* The multicast group abstraction */
extern struct MEntry *rpc2_MgrpFreeList;	/* free mgrp blocks */
extern long rpc2_MgrpFreeCount, rpc2_MgrpCreationCount;

/* Items for SocketListener */

extern struct SL_Entry *rpc2_SLFreeList,	/* free entries */
        	*rpc2_SLReqList,		/* in use, of type REQ */
		*rpc2_SLList;     /* in use, of types REPLY or OTHER */
extern long rpc2_SLFreeCount, rpc2_SLReqCount, rpc2_SLCount,
            rpc2_SLCreationCount;

/* Packet buffers */
extern RPC2_PacketBuffer *rpc2_PBList, *rpc2_PBHoldList;
extern long rpc2_PBCount, rpc2_PBHoldCount, rpc2_PBFreezeCount;
extern RPC2_PacketBuffer *rpc2_PBSmallFreeList, *rpc2_PBMediumFreeList, *rpc2_PBLargeFreeList;
extern long rpc2_PBSmallFreeCount, rpc2_PBMediumFreeCount, rpc2_PBLargeFreeCount;
extern long rpc2_PBSmallCreationCount, rpc2_PBMediumCreationCount, rpc2_PBLargeCreationCount;

/* Subsystem definitions */
extern struct SubsysEntry *rpc2_SSFreeList,	/* free entries */
			*rpc2_SSList;		/* subsystems in active use */
extern long rpc2_SSFreeCount, rpc2_SSCount, rpc2_SSCreationCount;
	
/* Host info definitions */
extern struct HEntry *rpc2_HostFreeList, *rpc2_HostList;
extern long rpc2_HostFreeCount, rpc2_HostCount, rpc2_HostCreationCount;


/*------------- Miscellaneous  global data  ------------*/
extern long rpc2_RequestSocket;	/* fd of RPC socket  */
				/* we may need more when we deal with many domains */
extern RPC2_PortIdent   rpc2_LocalPort;	/* of rpc2_RequestSocket */
extern RPC2_HostIdent   rpc2_LocalHost;	/* of rpc2_RequestSocket */
extern struct TM_Elem *rpc2_TimerQueue;
extern struct CBUF_Header *rpc2_TraceBuffHeader;
extern PROCESS rpc2_SocketListenerPID;	/* used in IOMGR_Cancel() calls */
extern unsigned long rpc2_LamportClock;
extern struct timeval rpc2_InitTime;    /* base for timestamps */
extern long Retry_N;
extern struct timeval *Retry_Beta;
#define MaxRetryInterval Retry_Beta[0]
extern struct timeval SaveResponse;

/* List manipulation routines */
void rpc2_Replenish();
struct LinkEntry *rpc2_MoveEntry();
struct SL_Entry *rpc2_AllocSle();
void rpc2_FreeSle(), rpc2_ActivateSle(), rpc2_DeactivateSle();
struct SubsysEntry *rpc2_AllocSubsys();
void rpc2_FreeSubsys();

/* Socket creation */
long rpc2_CreateIPSocket();

/* Packet  routines */
long rpc2_SendReliably(), rpc2_MSendPacketsReliably();
void rpc2_XmitPacket(long whichSocket, RPC2_PacketBuffer *whichPB,
		     RPC2_HostIdent *whichHost, RPC2_PortIdent *whichPort);
void rpc2_InitPacket();
long rpc2_RecvPacket(long whichSocket, RPC2_PacketBuffer *whichBuff);
void rpc2_htonp(RPC2_PacketBuffer *p);
void rpc2_ntohp(RPC2_PacketBuffer *p);
long rpc2_SetRetry(), rpc2_CancelRetry();
void rpc2_ResetLowerLimit();
void rpc2_UpdateRTT(RPC2_PacketBuffer *pb, struct CEntry *ceaddr);
void rpc2_ResetObs();
void rpc2_ProcessPackets(), rpc2_ExpireEvents();

/* Connection manipulation routines  */
void rpc2_InitConn(), rpc2_FreeConn(), rpc2_SetConnError();
struct CEntry *rpc2_AllocConn();
struct CEntry *rpc2_FindCEAddr(RPC2_Handle whichHandle);
struct CEntry *rpc2_ConnFromBindInfo(RPC2_HostIdent *whichHost, RPC2_PortIdent *whichPort, RPC2_Integer whichUnique);
struct CEntry *rpc2_GetConn(RPC2_Handle handle);
void rpc2_IncrementSeqNumber(struct CEntry *);
/*  XXX where is this baby extern bool rpc2_TestState(); */

/* Host manipulation routines */
void rpc2_InitHost(void);
struct HEntry *rpc2_FindHEAddr(struct in_addr *whichHost);
struct HEntry *rpc2_GetHost(RPC2_HostIdent *host);
struct HEntry *rpc2_AllocHost(RPC2_HostIdent *host);
void rpc2_FreeHost(struct HEntry **whichHost);
void rpc2_GetHostLog(struct HEntry *whichHost, RPC2_NetLog *log);
int rpc2_AppendHostLog(struct HEntry *whichHost, RPC2_NetLogEntry *entry);
void rpc2_ClearHostLog(struct HEntry *whichHost);
void RPC2_UpdateEstimates(struct HEntry *whichHost, RPC2_Unsigned ElapsedTime,
			  RPC2_Unsigned Bytes);
void rpc2_UpdateEstimates(struct HEntry *whichHost, struct timeval *elapsed,
			  RPC2_Unsigned Bytes);
void rpc2_RetryInterval(struct HEntry *host, RPC2_Unsigned Bytes, int retry,
			struct timeval *tv);

/* Multicast group manipulation routines */
void rpc2_InitMgrp(), rpc2_FreeMgrp(), rpc2_RemoveFromMgrp(), rpc2_DeleteMgrp();
struct MEntry *rpc2_AllocMgrp(), *rpc2_GetMgrp();

/* Hold queue routines */
void rpc2_HoldPacket(), rpc2_UnholdPacket();

/* Host manipulation routine */
long rpc2_GetLocalHost();

/* RPC2_GetRequest() filter matching function */
bool rpc2_FilterMatch();

/* Autonomous LWPs */
void rpc2_SocketListener(), rpc2_ClockTick();

/* Packet timestamp creation */
unsigned long rpc2_TVTOTS(const struct timeval *tv);
void          rpc2_TSTOTV(const long ts, struct timeval *tv);
unsigned long rpc2_MakeTimeStamp();

/* Debugging routines */
void rpc2_PrintTMElem(), rpc2_PrintFilter(), rpc2_PrintSLEntry(),
	rpc2_PrintCEntry(), rpc2_PrintTraceElem(), rpc2_PrintPacketHeader(),
	rpc2_PrintHostIdent(), rpc2_PrintPortIdent(), rpc2_PrintSEDesc();
extern FILE *ErrorLogFile;

void rpc2_InitRandom();
long rpc2_TrueRandom();

/* encryption */
void rpc2_ApplyD(RPC2_PacketBuffer *pb, struct CEntry *ce);
void rpc2_ApplyE(RPC2_PacketBuffer *pb, struct CEntry *ce);

int rpc2_time();
long rpc2_InitRetry(long HowManyRetries, struct timeval *Beta0);

void rpc2_NoteBinding(RPC2_HostIdent *whichHost, RPC2_PortIdent *whichPort, 
		 RPC2_Integer whichUnique, RPC2_Handle whichConn);




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
 ((_hi1p_)->Value.InetAddress.s_addr == (_hi2p_)->Value.InetAddress.s_addr): \
 (((_hi1p_)->Tag == RPC2_HOSTBYNAME && (_hi2p_)->Tag == RPC2_HOSTBYNAME)? \
  (strncmp((_hi1p_)->Value.Name, (_hi2p_)->Value.Name, 64) == 0):0))

#define rpc2_PortIdentEqual(_pi1p_,_pi2p_) \
(((_pi1p_)->Tag == RPC2_PORTBYINETNUMBER && (_pi2p_)->Tag == RPC2_PORTBYINETNUMBER)? \
 ((_pi1p_)->Value.InetPortNumber == (_pi2p_)->Value.InetPortNumber): \
 (((_pi1p_)->Tag == RPC2_PORTBYNAME && (_pi2p_)->Tag == RPC2_PORTBYNAME)? \
  (strncmp((_pi1p_)->Value.Name, (_pi2p_)->Value.Name, 20) == 0):0))


/*------ Other definitions ------*/


/* Allocation constants */

#define SMALLPACKET	350
#define MEDIUMPACKET	3000
#define LARGEPACKET	RPC2_MAXPACKETSIZE


/* Packets sent  */
extern unsigned long rpc2_NoNaks;
extern long rpc2_BindLimit, rpc2_BindsInQueue;
extern long rpc2_Unbinds, rpc2_FreeConns, rpc2_AllocConns, rpc2_GCConns;
extern long rpc2_FreeMgrps, rpc2_AllocMgrps;
extern long rpc2_FreezeHWMark, rpc2_HoldHWMark;

/*--------------- Useful definitions that used to be in potpourri.h or util.h ---------------*/
/*                 Now included here to avoid including either of those files                */

/* Parameter usage */
#define	IN	/* Input parameter */
#define OUT	/* Output parameter */
#define INOUT	/* Obvious */


/* Conditional debugging output macros: no side effect in these! */
extern FILE *rpc2_logfile;
extern FILE *rpc2_tracefile;
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
#endif RPC2DEBUG

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif TRUE


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

/* macros to convert from timeval to timestamp */
#define TVTOTS(_tvp_, _ts_)\
do {\
    _ts_ = ((_tvp_)->tv_sec * 1000 + (_tvp_)->tv_usec / 1000);\
} while(0);

#define TSTOTV(_tvp_, _ts_)\
do {\
    (_tvp_)->tv_sec = (_ts_) / 1000;\
    (_tvp_)->tv_usec = ((_ts_) % 1000) * 1000;\
} while(0);


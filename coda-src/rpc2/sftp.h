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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/rpc2/sftp.h,v 1.1 1996/11/22 19:07:51 braam Exp $";
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


#ifndef _SFTP
#define _SFTP

#ifndef	C_ARGS
#if	(__cplusplus | __STDC__)
#define	C_ARGS(arglist)	arglist
#else	__cplusplus
#define	C_ARGS(arglist)	()
#endif	__cplusplus
#endif	C_ARGS

/*	    
    Features:
	    1. Windowing with bit masks to avoid unnecessary retransmissions
	    2. Adaptive choice of transmission parameters	(not yet implemented)
	    3. Piggybacking of small files with request/response
*/


#define SFTPVERSION	2	/* Changed from 1 on 7 Jan 1988 by Satya (ThisRPCCall check added) */

#define SFTP_MAXPACKETSIZE	2900 /* (prefix+header+body) of largest sftp packet (2 IP fragments on Ether) */
#define SFTP_MAXBODYSIZE	SFTP_MAXPACKETSIZE - sizeof(RPC2_PacketBuffer)

#define SFTP_DEFPACKETSIZE 2800
#define SFTP_DEFWINDOWSIZE 32
#define SFTP_DEFSENDAHEAD 8

#define SFTP_MINPACKETSIZE      240  /* as above (1 IP fragment on SLIP) */
#define SFTP_MINBODYSIZE        SFTP_MINPACKETSIZE - sizeof(RPC2_PacketBuffer)
#define SFTP_MINWINDOWSIZE 2
#define SFTP_MINSENDAHEAD 1

#define	SFTP_DebugLevel	RPC2_DebugLevel

/* Packet Format:
   =============

	Smart FTP is NOT built on top of RPC, but it does use RPC format packets for convenience

	The interpretation of header fields is similar to that in RPC:

		ProtoVersion	Set to SFTPVERSION; must match value at destination
		RemoteHandle	RPC Handle, at the packet destination, of the connection
					on whose behalf this file transfer is being performed
		LocalHandle	RPC Handle, at the packet source, of the connection
					on whose behalf this file transfer is being performed
		Flags		RPC2_RETRY and RPC2_ENCRYPTED have the usual meanings.
				SFTP_ACKME is also indicated here on data packets.

		<<< Fields below here encrypted on secure connections>>>
		BodyLength	Number of bytes in packet body
		SeqNumber	SFTP Sequence number with the following properties:
				    1. Data packets (Opcode SFTP_DATA) and control
					packets (all other opcodes) form different sequences.
					Each starts at 1 and monotonically increases by 1.
				    2. Every data packet is eventually acknowledged, but control
					packets need not always be acknowledged.
				    3. The client end and server ends each have their own, independent
					sequences.
				    4. There are thus 4 sequences in a connection:
					Client to Server, Data
					Client to Server, Control
					Server to Client, Data
					Server to Client, Control
				    5. The flow of packets is effectively viewed as a series of file
					transfers from client to server and vice versa, with control
					packets being merely annotations.  The boundary between file
					transfers is demarcated by a single data packet with the MOREDATA flag
					off.  The use of a separate, non-sparse sequence numbers for
					data is what allows the use of bitmasks.


		Opcode		SFTP Op code
		SEFlags		Flags meaningful to SFTP
		SEDataOffSet	#defined to GotEmAll;
					Makes sense only with opcode SFTP_ACK
					Highest seq number up to and including which all preceding
					data packets have been seen. Says nothing about control packets.
		SubsysId	Set to SMARTFTP always
		ReturnCode	#defined to BitMask0	(only sensible with SFTP_ACK)
					BitMask0..BitMask1 together form a bit string.
					1 bits indicate received packets with seq numbers greater than GotEmAll;
					Read left to right, bits correspond to GotEmAll+1, GotEmAll+2 ....
					Leftmost bit must be 0, by definition of GotEmAll
		Lamport		#defined to BitMask1    (only sensible with SFTP_ACK)
		Uniquefier	#defined to ThisCall    (RPC call sequence number at sending side
				    of the RPC pertaining to this side effect)
		TimeStamp
		BindTime        #defined to TimeEcho
		Body		Contains actual BodyLength bytes of file data;
				Used with SFTP_DATA and SFTP_START (for conveying SFTP address)
*/


/* Renaming of RPC packet header fields */
#define GotEmAll	SEDataOffset
#define	BitMask0	ReturnCode
#define BitMask1	Lamport
#define TimeEcho	BindTime
#define ThisRPCCall	Uniquefier

/* Values of Flags field in header */
#define	SFTP_ACKME	0x80000000
				/* on data packets: acknowledge this packet.
				Located in Flags rather than SEFlags so that retransmits
				can turn off this bit without decryption and re-encryption */
				

/* Values of SEFlags field in header */
#define SFTP_MOREDATA	0x1	/* on data packets, indicates more data to come */
#define SFTP_PIGGY	0x2	/* on RPC packets: piggybacked info present on this packet */
#define SFTP_ALLOVER	0x4	/* on RPC reply packets: indicates server got all data packets */
#define SFTP_TRIGGER    0x8     /* on ack packets: distinguishes a server "triggered" ack from a real one */
                                /* necessary only for compatibility, triggers now send null timestamp echos */
#define SFTP_FIRST      0x10	/* on data packets, indicates first of group sent by source */
#define SFTP_COUNTED	0x20	/* on data packets: arrived or acked before last round */

/* SFTP Opcodes */
#define SFTP_START	1	/* Control: start sending data (flow is from RPC client to server)*/
#define SFTP_ACK	2	/* Control: acknowledgement you had requested */
#define SFTP_DATA	3	/* Data: next chunk; MOREDATA flag indicates whether EOF has been seen */
#define SFTP_NAK	4	/* Control: got a bogus packet from you */
#define SFTP_RESET	5	/* Control: reset transmission parameters */
#define SFTP_BUSY	6	/* Control: momentarily busy; reset your timeout counter */


/* Per-connection information: accessible via RPC2_GetSEPointer() and RPC2_SetSEPointer() */
#define SFTPMAGIC	4902057
#define MAXOPACKETS	64	/* Maximum no of outstanding packets; multiple of 32 */
#define BITMASKWIDTH	(MAXOPACKETS / 32)	/* No of elements in integer array */
#define MINDELTASS	10	/* minimum milliseconds between invocations of SendStrategy due to an ACK */

struct SFTP_Parms
    {/* sent in SFTP_START packets, and piggy-backed on very first RPC call on a connection */
    RPC2_PortalIdent Portal;
    long WindowSize;
    long SendAhead;
    long AckPoint;
    long PacketSize;
    long DupThreshold;
    };

struct SFTP_MCParms	/* Multicast parameters */
    {
    long PeerSendLastContig;
    };

enum  SFState {SFSERVER, SFCLIENT, ERROR, DISKERROR};

struct SFTP_Entry		/* per-connection data structure */
    {
    long  Magic;		/* SFTPMAGIC */
    enum  SFState WhoAmI;
    RPC2_Handle LocalHandle;	/* which RPC2 conn on this side do I correspond to? */
    RPC2_PeerInfo PInfo;	/* all the RPC info  about the other side */
    RPC2_PortalIdent  PeerPortal;	/* SFTP portal on other side */
    struct timeval LastWord; 	/* Most recent time we've heard from our peer */
    struct HEntry *HostInfo;	/* Connection-independent host/portal info.
				    set by ExaminePacket on client side (if !GotParms), and
				    sftp_ExtractParmsFromPacket on server side */
    long ThisRPCCall;		/* Client-side RPC sequence number of the call in progress.
				    Used to reject outdated SFTP packets that may be floating
				    around after the next RPC has begun. Set on client side
				    in SFTP_MakeRPC1() and on server side on SFTP_GetRequest() */
    long GotParms;		/* FALSE initially; TRUE after I have discovered my peer's parms */
    long SentParms;		/* FALSE initially; TRUE after I have sent my parms to peer */
    SE_Descriptor *SDesc; 	/* set by SFTP_MakeRPC1 on client side,
				    by SFTP_InitSE and SFTP_CheckSE on server side */
    long openfd;		/* file descriptor: valid during actual transfer */
    struct SLSlot *Sleeper;	/* SLSlot of LWP sleeping on this connection, or NULL */
    long PacketSize;		/* Amount of  data in each packet */
    long WindowSize;		/* Max Number of outstanding packets without acknowledgement <= MAXOPACKETS */
    long SendAhead;		/* How many more packets to send after demanding an ack. Equal to read-ahead  */
    long AckPoint;		/* After how many send ahead packets should an ack be demanded? */
    long DupThreshold;		/* How many duplicate data packets can I see before sending Ack spontaneously? */
    long RetryCount;		/* How many times to retry Ack request */
    long ReadAheadCount;	/* How many packets have been read by read strategy routine */
    long CtrlSeqNumber;		/* Seq number of last control packet sent out */
    struct timeval RInterval;	/* retransmission interval; initially SFTP_RetryInterval milliseconds */
    long Retransmitting;        /* FALSE initially; TRUE prevents RTT update */

#define SFTP_RTT_SCALE 8        /* scale of stored RTT. (uses TCP alpha = .875) */
#define SFTP_RTT_SHIFT 3        /* number of bits to the right of the binary point of RTT */
    long RTT;                   /* Current RTT estimate (like TCP's "smooth RTT") in 10 msec units */

#define SFTP_RTTVAR_SCALE 4     /* scale of stored RTTvar. (uses TCP alpha = .75) */
#define SFTP_RTTVAR_SHIFT 2     /* number of bits to the right of the binary point of RTTvar */
    long RTTVar;                /* Variance of above in 10 msec units */
    unsigned long TimeEcho;     /* Timestamp to send on next packet */
    struct timeval LastSS;	/* time SendStrategy was last invoked by an Ack on this connection */
    SE_Descriptor *PiggySDesc;	/* malloc()ed copy of SDesc; held on until SendResponse, if piggybacking
					might take place */

#define SFTP_MINRTT   10        /* min rtt is 100 msec */
#define SFTP_MAXRTT   30000     /* max rtt is 300 seconds */

/*  Transmission Parameters:

	INVARIANTs (when XferState = XferInProgress):
	==========
 	1. SendLastContig <= SendWorriedLimit <= SendAckLimit <= SendMostRecent 
	2. (SendMostRecent - SendLastContig) <= WindowSize
	3. (SendMostRecent - SendAckLimit) <= SendAhead
	4. RecvLastContig <= RecvMostRecent
	5. (RecvMostRecent - RecvLastContig) <= WindowSize

	INVARIANTs (when XferState = {XferNotStarted,XferAborted,XferCompleted}):
	=========
	1. SendLastContig (at source) = SendMostRecent (at source)
	2. RecvLastContig (at sink) = RecvMostRecent (at sink)
	3. SendLastContig (at source) = RecvLastContig (at sink)
*/
#define XferNotStarted 0
#define XferInProgress 1
#define XferCompleted  2
    long XferState;		/* {XferNotStarted,XferInProgress,XferAborted,XferCompleted} */

    /* Next block is multicast specific */
    long UseMulticast;		/* TRUE iff multicast was requested in SFTP_MultiRPC1 call */
    long RepliedSinceLastSS;	/* TRUE iff {ACK,NAK,START} received since last invocation of SendStrategy */
    long McastersStarted;	/* number of individual conns participating */
    long McastersFinished;	/* number of participating conns which have finished */
    long FirstSeqNo;
#define	SendFirst FirstSeqNo	/* Value of SendLastContig (+1) when Multicast MRPC call is initiated */
#define	RecvFirst FirstSeqNo	/* Value of RecvMostRecent (+1) when Multicast MRPC call is initiated */

    long HitEOF;		/* source side: EOF has been seen by read strategy routine
    				   sink side: last packet for this transfer has been received */
    long SendLastContig;	/* Seq no. of packet before (and including) which NO state is maintained.
				   This is the most recent packet such that it and all earlier packets
				   are known by me to have been received by the other side */
    long SendMostRecent;	/* SendMostRecent is the latest data packet we have sent out  */
    unsigned long SendTheseBits[BITMASKWIDTH];	/* Bit pattern of packets in the range SendLastContig+1..SendMostRecent
				   that have successfully been sent by me AND are known by me to have
				   been received by other side */
    long SendAckLimit;          /* Highest data packet for which an ack has been requested. */
    long SendWorriedLimit;	/* Highest data packet about which we are worried. */
    long RecvLastContig;	/* Most recent data packet up to which I no longer maintain state */
    long RecvMostRecent;	/* Highest numbered data packet seen so far */
    long DupsSinceAck;		/* Duplicates seen since the last ack I sent */

    unsigned long RecvTheseBits[BITMASKWIDTH];	/* Packets in RecvLastContig+1..RecvMostRecent that I have received */
    RPC2_PacketBuffer *ThesePackets[MAXOPACKETS];
    					/* Packets being currently dealt with.
    					There can be at most MAXOPACKETS outstanding, in the range
					LastContig+1..LastContig+WindowSize.
					The index of the i'th packet is given by (i % MAXOPACKETS).
					
					Some of these pointers, may be NULL for the following reasons:
						Receiving side:  The packets have not been received, or have
								been received and written to disk already.
						Sending side:  The packets have been sent, and an ACK for them
								has been received.

					*/
    };


/* Per-LWP SFTP status information: accessed via LWP_SetRock() and LWP_GetRock() 

    Disclaimer (made in total disgust)
    =================================
	All this stuff dealing with SLSlots etc. is needed because the LWP
    package does not provide me with a very fundamental primitive: the ability to wait for
    an arbitrary event or a timeout.  This is apparently impossible to add in an efficient
    manner to the current LWP package.  So I have to fake this using the same strategy used in
    the base RPC2 (SLEntries for communication between SocketListener and the other LWPs).  The
    difference here is that the number of SLSlots is at most one per LWP, since an LWP can be
    waiting for at most one packet at a time.  The creation of an SLSlot for an LWP is 
    deferred until the first use of this mechanism, since we have no  way to gain control when 
    a LWP_CreateProcess() is done.
*/

#define SSLMAGIC	2305988
enum SLState {S_WAITING, S_ARRIVED, S_TIMEOUT, S_INACTIVE};
struct SLSlot		/* pointed to by rock in current LWP */
    {
    long Magic;		/* SSLMAGIC */
    PROCESS Owner;	/* redundant, but useful backpointer */
    enum SLState State;	
    struct TM_Elem Te;		/* if State is WAITING, will be in linked list of timer elements */
    struct TM_Elem *TChain;	/* NULL or chain in which Te is in at present */
    RPC2_PacketBuffer *Packet;	/* newly arrived packet */
    };


#define REMOVETIMER(x)	/* Removes an SLSlot x from a timer chain */\
	    if (x->TChain != NULL)\
		{\
		TM_Remove(x->TChain, &x->Te);\
		x->TChain = NULL;\
		}

extern PROCESS sftp_ListenerPID;	/* pid of listener */
extern struct TM_Elem *sftp_Chain;	/* head of linked list of all sleeping LWPs waiting for a packet or a timeout */
extern long sftp_Socket;		/* for all SFTP traffic */
extern RPC2_HostIdent sftp_Host;
extern RPC2_PortalIdent sftp_Portal;
extern long SFTP_DebugLevel;



extern sftp_Listener();

#define IsSource(sfe)\
    ((sfe->WhoAmI == SFCLIENT && sfe->SDesc && sfe->SDesc->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER) ||\
	(sfe->WhoAmI == SFSERVER && sfe->SDesc && sfe->SDesc->Value.SmartFTPD.TransmissionDirection == SERVERTOCLIENT))

#define IsSink(sfe)\
    ((sfe->WhoAmI == SFCLIENT && sfe->SDesc  && sfe->SDesc->Value.SmartFTPD.TransmissionDirection == SERVERTOCLIENT) ||\
	(sfe->WhoAmI == SFSERVER && sfe->SDesc && sfe->SDesc->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER))


/* Operations on integer array bitmask; leftmost position is 1, rightmost is 32*BITMASKWIDTH
    Choice of 1 rather than 0 is deliberate: {Send,Recv}LastContig+1 corresponds to leftmost bit */
#define WORDOFFSET(pos) (((pos)-1) >> 5)	/* avoid / operator */
#define BITOFFSET(pos)  ((((pos)-1) & 31)+1)	/* avoid % operator */
#define PM(pos) (1L << (32 - (BITOFFSET(pos))))
#define SETBIT(mask, pos)   ((mask)[WORDOFFSET(pos)] |= PM(pos))
#define TESTBIT(mask, pos)  ((mask)[WORDOFFSET(pos)] & PM(pos))
#define CLEARBIT(mask, pos) ((mask)[WORDOFFSET(pos)] &= (~PM(pos)))

/* Packet buffer position */
#define PBUFF(x)	((x) & (MAXOPACKETS-1))	/* effectively modulo operator */


/* The transmission parameters below are initial values; actual ones are per-connection */
extern long SFTP_PacketSize;	
extern long SFTP_WindowSize;	
extern long SFTP_RetryCount;
extern long SFTP_RetryInterval;
extern long SFTP_EnforceQuota;	/* Nonzero to activate ByteQuota in SE_Descriptors */
extern long SFTP_SendAhead;
extern long SFTP_AckPoint;
extern long SFTP_DupThreshold;
extern long SFTP_DoPiggy;	/* FALSE to suppress piggybacking */

/* SE routines invoked by base RPC2 code */
extern long SFTP_Init();
extern long SFTP_Bind1 C_ARGS((RPC2_Handle ConnHandle, RPC2_CountedBS *ClientIdent));
extern long SFTP_Bind2 C_ARGS((RPC2_Handle ConnHandle, RPC2_Unsigned BindTime));
extern long SFTP_Unbind C_ARGS((RPC2_Handle ConnHandle));
extern long SFTP_NewConn C_ARGS((RPC2_Handle ConnHandle, RPC2_CountedBS *ClientIdent));
extern long SFTP_MakeRPC1 C_ARGS((RPC2_Handle ConnHandle, SE_Descriptor *SDesc, RPC2_PacketBuffer **RequestPtr));
extern long SFTP_MakeRPC2 C_ARGS((RPC2_Handle ConnHandle, SE_Descriptor *SDesc, RPC2_PacketBuffer *Reply));
extern long SFTP_MultiRPC1();
extern long SFTP_MultiRPC2();
extern long SFTP_CreateMgrp();
extern long SFTP_AddToMgrp();
extern long SFTP_InitMulticast();
extern long SFTP_DeleteMgrp();
extern long SFTP_GetRequest C_ARGS((RPC2_Handle ConnHandle, RPC2_PacketBuffer *Request));
extern long SFTP_InitSE C_ARGS((RPC2_Handle ConnHandle, SE_Descriptor *SDesc));
extern long SFTP_CheckSE C_ARGS((RPC2_Handle ConnHandle, SE_Descriptor *SDesc, long Flags));
extern long SFTP_SendResponse C_ARGS((RPC2_Handle ConnHandle, RPC2_PacketBuffer **Reply));
extern long SFTP_PrintSED C_ARGS((SE_Descriptor *SDesc, FILE *outFile));
extern void SFTP_SetDefaults C_ARGS((SFTP_Initializer *initPtr));
extern void SFTP_Activate C_ARGS((SFTP_Initializer *initPtr));
extern long SFTP_GetTime C_ARGS((RPC2_Handle ConnHandle, struct timeval *Time));
extern long SFTP_GetHostInfo C_ARGS((RPC2_Handle ConnHandle, struct HEntry **hPtr));

/* Internal SFTP routines */
extern sftp_InitIO();
extern sftp_DataArrived();
extern sftp_WriteStrategy();
extern sftp_AckArrived();
extern sftp_SendStrategy();
extern sftp_ReadStrategy();
extern sftp_SendStart();
extern sftp_StartArrived();
extern sftp_SendTrigger();
extern void sftp_InitPacket();
extern void sftp_InitRTT();
extern void sftp_UpdateRTT();
extern void sftp_Backoff();
extern SFXlateMcastPacket();
extern MC_CheckAckorNak();
extern MC_CheckStart();

extern struct SFTP_Entry *sftp_AllocSEntry();
extern void sftp_FreeSEntry();
extern void sftp_AllocPiggySDesc();
extern void sftp_FreePiggySDesc();
extern sftp_AppendParmsToPacket();
extern sftp_ExtractParmsFromPacket();
extern sftp_AppendFileToPacket();
extern sftp_ExtractFileFromPacket();
extern sftp_AddPiggy();
extern void sftp_SetError();


extern long sftp_datas, sftp_datar, sftp_acks, sftp_ackr, sftp_busy,
	sftp_triggers, sftp_starts, sftp_retries, sftp_timeouts,
	sftp_windowfulls, sftp_duplicates, sftp_bogus, sftp_ackslost, sftp_didpiggy,
	sftp_starved, sftp_rttupdates;

extern long sftp_PacketsInUse;
extern long SFTP_MaxPackets;

#define CLOSE(sfe) (sftp_vfclose(sfe->SDesc, sfe->openfd), (sfe->openfd  = -1))

/* SFTP's version of RPC2_AllocBuffer and RPC2_FreeBuffer */

#define SFTP_AllocBuffer(x, y)\
    (sftp_PacketsInUse++, RPC2_AllocBuffer(x, y))
    
#define SFTP_FreeBuffer(x)\
    (sftp_PacketsInUse--, RPC2_FreeBuffer(x))


/* For encryption and decryption */
#define sftp_Encrypt(pb, sfe)\
    rpc2_Encrypt((char *)&pb->Header.BodyLength, (char *)&pb->Header.BodyLength,\
	    pb->Prefix.LengthOfPacket-4*sizeof(RPC2_Integer),\
	    sfe->PInfo.SessionKey, sfe->PInfo.EncryptionType)

#define sftp_Decrypt(pb, sfe)\
    rpc2_Decrypt((char *)&pb->Header.BodyLength, (char *)&pb->Header.BodyLength,\
	    pb->Prefix.LengthOfPacket-4*sizeof(RPC2_Integer),\
	    sfe->PInfo.SessionKey, sfe->PInfo.EncryptionType)
#endif _SFTP


/* Predicate to test if file is in vm */
#define MEMFILE(s) (s->Value.SmartFTPD.Tag == FILEINVM)

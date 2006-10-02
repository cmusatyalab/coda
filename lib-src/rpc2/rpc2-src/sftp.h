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


#ifndef _SFTP
#define _SFTP

#include <unistd.h>
#include <stdlib.h>
/*	    
    Features:
	    1. Windowing with bit masks to avoid unnecessary retransmissions
	    2. Adaptive choice of transmission parameters	(not yet implemented)
	    3. Piggybacking of small files with request/response
*/


#define SFTPVERSION	3	/* Changed from 1 on 7 Jan 1988 by Satya (ThisRPCCall check added) */
                                /* Changed from 2 on 27 Feb 1997 by bnoble */

/* (header+body) of largest sftp packet (2 IP fragments on Ether) */
#define SFTP_MAXPACKETSIZE	2900
#define SFTP_MAXBODYSIZE	(SFTP_MAXPACKETSIZE - \
				 sizeof(struct RPC2_PacketHeader))

#define SFTP_DEFPACKETSIZE	(1024 + sizeof(struct RPC2_PacketHeader))
#define SFTP_DEFWINDOWSIZE	32
#define SFTP_DEFSENDAHEAD	8

/* as above (1 IP fragment on SLIP) */
#define SFTP_MINPACKETSIZE      240
#define SFTP_MINBODYSIZE        (SFTP_MINPACKETSIZE - \
				 sizeof(struct RPC2_PacketHeader))
#define SFTP_MINWINDOWSIZE	2
#define SFTP_MINSENDAHEAD	1

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

struct SFTP_Parms
{/* sent in SFTP_START packets, and piggy-backed on very first RPC call on a connection */
    RPC2_PortIdent Port;
    int32_t WindowSize;
    int32_t SendAhead;
    int32_t AckPoint;
    int32_t PacketSize;
    int32_t DupThreshold;
};

struct SFTP_MCParms	/* Multicast parameters */
{
    uint32_t PeerSendLastContig;
};

enum  SFState {SFSERVER, SFCLIENT, ERROR, DISKERROR};

struct SFTP_Entry		/* per-connection data structure */
{
    long  Magic;		/* SFTPMAGIC */
    enum  SFState WhoAmI;
    RPC2_Handle LocalHandle;	/* which RPC2 conn on this side do I
				   correspond to? */
    RPC2_PeerInfo  PInfo;	/* all the RPC info  about the other side */
    struct timeval LastWord;	/* Last time we received something on this SE */
    struct HEntry *HostInfo;	/* Connection-independent host info. set by
				   ExaminePacket on client side (if
				   !GotParms), and sftp_ExtractParmsFromPacket
				   on server side */
    uint32_t ThisRPCCall;	/* Client-side RPC sequence number of the call
				   in progress. Used to reject outdated SFTP
				   packets that may be floating around after
				   the next RPC has begun. Set on client side
				   in SFTP_MakeRPC1() and on server side on
				   SFTP_GetRequest() */
    uint32_t GotParms;		/* FALSE initially; TRUE after I have
				   discovered my peer's parms */
    uint32_t SentParms;		/* FALSE initially; TRUE after I have sent my
				   parms to peer */
    SE_Descriptor *SDesc; 	/* set by SFTP_MakeRPC1 on client side, by
				   SFTP_InitSE and SFTP_CheckSE on server side
				 */
    long openfd;		/* file descriptor: valid during actual
				   transfer */
    off_t fd_offset;		/* For FILEBYFD transfers, we save the offset
				   within the file after each read/write */
    struct SL_Entry *Sleeper;	/* SL_Entry of LWP sleeping on this connection,
				   or NULL */
    uint32_t PacketSize;	/* Amount of  data in each packet */
    uint32_t WindowSize;	/* Max Number of outstanding packets without
				   acknowledgement <= MAXOPACKETS */
    uint32_t SendAhead;		/* How many more packets to send after
				   demanding an ack. Equal to read-ahead  */
    uint32_t AckPoint;		/* After how many send ahead packets should an
				   ack be demanded? */
    uint32_t DupThreshold;	/* How many duplicate data packets can I see
				   before sending Ack spontaneously? */
    uint32_t RetryCount;	/* How many times to retry Ack request */
    uint32_t ReadAheadCount;	/* How many packets have been read by read
				   strategy routine */
    uint32_t CtrlSeqNumber;	/* Seq number of last control packet sent out */
    uint32_t RetryInterval;	/* retransmission interval; initially
				   SFTP_RetryInterval milliseconds */
    uint32_t Retransmitting;	/* FALSE initially; TRUE prevents RTT update */
    uint32_t TimeEcho;		/* Timestamp to send on next packet (valid
				   when not retransmitting) */
    struct timeval LastSS;	/* time SendStrategy was last invoked by an
				   Ack on this connection */
    SE_Descriptor *PiggySDesc;	/* malloc()ed copy of SDesc; held on until
				   SendResponse, if piggybacking might take
				   place */

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
    enum {
	XferNotStarted = 0,
	XferInProgress = 1,
	XferCompleted  = 2
    } XferState;

    /* Next block is multicast specific */
    uint32_t RepliedSinceLastSS; /* TRUE iff {ACK,NAK,START} received since last invocation of SendStrategy */
    uint32_t McastersStarted;	/* number of individual conns participating */
    uint32_t McastersFinished;	/* number of participating conns which have finished */
    uint32_t FirstSeqNo;
#define	SendFirst FirstSeqNo	/* Value of SendLastContig (+1) when Multicast MRPC call is initiated */
#define	RecvFirst FirstSeqNo	/* Value of RecvMostRecent (+1) when Multicast MRPC call is initiated */

    uint32_t HitEOF;		/* source side: EOF has been seen by read strategy routine
    				   sink side: last packet for this transfer has been received */
    uint32_t SendLastContig;	/* Seq no. of packet before (and including) which NO state is maintained.
				   This is the most recent packet such that it and all earlier packets
				   are known by me to have been received by the other side */
    uint32_t SendMostRecent;	/* SendMostRecent is the latest data packet we have sent out  */
    unsigned int SendTheseBits[BITMASKWIDTH];	/* Bit pattern of packets in the range SendLastContig+1..SendMostRecent
				   that have successfully been sent by me AND are known by me to have
				   been received by other side */
    uint32_t SendAckLimit;          /* Highest data packet for which an ack has been requested. */
    uint32_t SendWorriedLimit;	/* Highest data packet about which we are worried. */
    uint32_t RecvLastContig;	/* Most recent data packet up to which I no longer maintain state */
    uint32_t RecvMostRecent;	/* Highest numbered data packet seen so far */
    uint32_t DupsSinceAck;		/* Duplicates seen since the last ack I sent */
    uint32_t RecvSinceAck;		/* Packets received since the last ack I sent */

    uint32_t RequestTime;  /* arrival time of packet, to correct RTT
			      estimates for processing time */

    unsigned int RecvTheseBits[BITMASKWIDTH];	/* Packets in RecvLastContig+1..RecvMostRecent that I have received */
    RPC2_PacketBuffer *ThesePackets[MAXOPACKETS];
    /* Packets being currently dealt with. There can be at most MAXOPACKETS
     * outstanding, in the range LastContig+1..LastContig+WindowSize. The
     * index of the i'th packet is given by (i % MAXOPACKETS).
     *					
     * Some of these pointers, may be NULL for the following reasons:
     * Receiving side:  The packets have not been received, or have
     *			been received and written to disk already.
     * Sending side: The packets have been sent, and an ACK for them has been
     * received.
     */
    struct security_association *sa;
};


extern long SFTP_DebugLevel;

int sftp_XmitPacket(struct SFTP_Entry *sentry, RPC2_PacketBuffer *pb,
		    int confirm);
void sftp_Timer(void);
void sftp_ExaminePacket(RPC2_PacketBuffer *pb);

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
extern long SFTP_RetryInterval; /* In what units? */
extern long SFTP_EnforceQuota;	/* Nonzero to activate ByteQuota in SE_Descriptors */
extern long SFTP_SendAhead;
extern long SFTP_AckPoint;
extern long SFTP_DupThreshold;
extern long SFTP_DoPiggy;	/* FALSE to suppress piggybacking */

/* SE routines invoked by base RPC2 code */
long SFTP_Init();
long SFTP_Bind1 (RPC2_Handle ConnHandle, RPC2_CountedBS *ClientIdent);
long SFTP_Bind2 (RPC2_Handle ConnHandle, RPC2_Unsigned BindTime);
long SFTP_Unbind (RPC2_Handle ConnHandle);
long SFTP_NewConn (RPC2_Handle ConnHandle, RPC2_CountedBS *ClientIdent);
long SFTP_MakeRPC1 (RPC2_Handle ConnHandle, SE_Descriptor *SDesc, RPC2_PacketBuffer **RequestPtr);
long SFTP_MakeRPC2 (RPC2_Handle ConnHandle, SE_Descriptor *SDesc, RPC2_PacketBuffer *Reply);
long SFTP_MultiRPC1();
long SFTP_MultiRPC2();
long SFTP_CreateMgrp();
long SFTP_AddToMgrp();
long SFTP_InitMulticast();
long SFTP_DeleteMgrp();
long SFTP_GetRequest (RPC2_Handle ConnHandle, RPC2_PacketBuffer *Request);
long SFTP_InitSE (RPC2_Handle ConnHandle, SE_Descriptor *SDesc);
long SFTP_CheckSE (RPC2_Handle ConnHandle, SE_Descriptor *SDesc, long Flags);
long SFTP_SendResponse (RPC2_Handle ConnHandle, RPC2_PacketBuffer **Reply);
long SFTP_PrintSED (SE_Descriptor *SDesc, FILE *outFile);
void SFTP_SetDefaults (SFTP_Initializer *initPtr);
void SFTP_Activate (SFTP_Initializer *initPtr);
long SFTP_GetTime (RPC2_Handle ConnHandle, struct timeval *Time);
long SFTP_GetHostInfo (RPC2_Handle ConnHandle, struct HEntry **hPtr);

/* Internal SFTP routines */
int sftp_InitIO(struct SFTP_Entry *sEntry);
int sftp_DataArrived(RPC2_PacketBuffer *pBuff, struct SFTP_Entry *sEntry);
int sftp_WriteStrategy(struct SFTP_Entry *sEntry);
int sftp_AckArrived(RPC2_PacketBuffer *pBuff, struct SFTP_Entry *sEntry);
int sftp_SendStrategy(struct SFTP_Entry *sEntry);
int sftp_ReadStrategy(struct SFTP_Entry *sEntry);
int sftp_SendStart(struct SFTP_Entry *sEntry);
int sftp_StartArrived(RPC2_PacketBuffer *pBuff, struct SFTP_Entry *sEntry);
int sftp_SendTrigger(struct SFTP_Entry *sEntry);
void sftp_InitPacket(RPC2_PacketBuffer *pb, struct SFTP_Entry *sfe, long bodylen);
void sftp_InitTrace(void);
int sftp_vfwritefile(struct SFTP_Entry *se, char *buf, int nbytes);
void sftp_vfclose(struct SFTP_Entry *se);
int sftp_piggybackfileread(struct SFTP_Entry *se, char *buf);
off_t sftp_piggybackfilesize(struct SFTP_Entry *se);
void sftp_TraceBogus(long filenum, long linenum);
void sftp_TraceStatus(struct SFTP_Entry *sEntry, int filenum, int linenum);
void sftp_DumpTrace(char *fName);
void sftp_Progress(SE_Descriptor *sdesc, off_t BytesTransferred);

void sftp_UpdateRTT(RPC2_PacketBuffer *pb, struct SFTP_Entry *sEntry,
		    unsigned long inbytes, unsigned long outbytes);

struct SFTP_Entry *sftp_AllocSEntry(void);
void sftp_FreeSEntry(struct SFTP_Entry *se);
void sftp_AllocPiggySDesc(struct SFTP_Entry *se, off_t len, enum WhichWay direction);
void sftp_FreePiggySDesc(struct SFTP_Entry *se);
int sftp_AppendParmsToPacket(struct SFTP_Entry *sEntry, RPC2_PacketBuffer **whichP);
int sftp_ExtractParmsFromPacket(struct SFTP_Entry *sEntry, RPC2_PacketBuffer *whichP);
off_t sftp_AppendFileToPacket(struct SFTP_Entry *sEntry, RPC2_PacketBuffer **whichP);
off_t sftp_ExtractFileFromPacket(struct SFTP_Entry *sEntry, RPC2_PacketBuffer *whichP);
int sftp_AddPiggy(RPC2_PacketBuffer **whichP, char *dPtr, off_t dSize, unsigned int maxSize);
void sftp_SetError(struct SFTP_Entry *s, enum SFState e);
int sftp_MorePackets(void);


extern long sftp_datas, sftp_datar, sftp_acks, sftp_ackr, sftp_busy,
	sftp_triggers, sftp_starts, sftp_retries, sftp_timeouts,
	sftp_windowfulls, sftp_duplicates, sftp_bogus, sftp_ackslost, sftp_didpiggy,
	sftp_starved, sftp_rttupdates;

extern long sftp_PacketsInUse;
extern long SFTP_MaxPackets;

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
#endif /* _SFTP */


/* Predicate to test if file is in vm */
#define MEMFILE(s) (s->Value.SmartFTPD.Tag == FILEINVM)
#define BYFDFILE(s) (s->Value.SmartFTPD.Tag == FILEBYFD)

/* test if we can send an TimeEcho reponse */
#define VALID_TIMEECHO(se) (!(se)->Retransmitting && \
			     (se)->TimeEcho != 0 && \
			     (se)->RequestTime != 0)

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


#ifndef _RPC2_
#define _RPC2_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/time.h>  
#include <netinet/in.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifndef HAVE_INET_ATON
int inet_aton(const char *str, struct in_addr *out);
#endif

#ifndef HAVE_INET_NTOA
char *inet_ntoa(struct in_addr ip);
#endif


/* This string is used in RPC initialization calls to ensure that the
runtime system and the header files are mutually consistent.  Also
passed across on RPC2_NewBinding for advisory information to other
side.  Changes to this string may cause RPC2_OLDVERSION to be returned
on RPC2_NewBinding()s.  For really minor changes alter rpc2_LastEdit
in globals.c.  */
/* #define RPC2_VERSION "Version 14.0: Satya, 6 May 1988, 10:00" */
#define RPC2_VERSION "Version 15.0: JH, 10 Dec 1998, 12:00"


/* Found as the first 4 bytes of EVERY packet.  Change this if you
change any aspect of the protocol sequence, or if you change the
packet header, or the body formats of the initialization packets.
Used in inital packet exchange to verify that the client and server
speak exactly the same protocol.  Orthogonal to RPC2_VERSION.  We need
this in the header at the very beginning, else we cannot change packet
formats in a detectable manner.  */
#define RPC2_PROTOVERSION 8


/*
The following constants are used to indicate the security-level of RPC connections.
*/
#define RPC2_OPENKIMONO	98	/* Neither authenticated nor encrypted */
#define RPC2_AUTHONLY		12	/* Authenticated but not encrypted */
#define RPC2_HEADERSONLY	73	/* Authenticated but only headers encrypted */
#define RPC2_SECURE 		66	/* Authenticated and fully encrypted */

/* RPC2 supports multiple encryption types; the key length is fixed,
and you must always supply a field of RPC2_KEYSIZE bytes wherever an
encryption key is called for.  However, individual algorithms can
choose to ignore excess bytes in the keys.

The encryption types are specified as integer bit positions so that
the EncryptionTypesMask field of RPC2_GetRequest() can be a mask of
these types.  The required type must also be specified in
RPC2_NewBinding().

To add support for other encryption types only the constants below and
the internal runtime procedures rpc2_Encrypt() and rpc2_Decrypt() have
to be modified.  */
#define RPC2_DES 1
#define RPC2_XOR 2
#define RPC2_ENCRYPTIONTYPES (RPC2_DES | RPC2_XOR)
				/* union of all supported types */
#define RPC2_KEYSIZE 8   /*Size in bytes of the encryption keys */ 

/*
RPC procedure return codes:

These may also  occur in the RPC2_ReturnCode field of reply headers: 
Values of 0 and below in those fields are reserved for RPC stub use.
Codes greater than 0  are  assigned  and managed by subsystems.

There are three levels of errors: Warning, Error, and Fatal Error.
RPC2_SUCCESS > RPC2_WLIMIT > warning codes > RPC2_ELIMIT > error codes
> RPC2_FLIMIT > fatal error codes

The semantics of these codes are:

RPC2_SUCCESS:	Everything was perfect.

Warning: 	         Advisory information.

Error:		Something went wrong, but the connection (if any)  is still usable.

Fatal:		The connection (if any) has been marked unusable.

Note that the routine RPC2_ErrorMsg() will translate return codes into
printable strings.  */

#define RPC2_SUCCESS	0

#define RPC2_WLIMIT	-1
#define RPC2_ELIMIT	-1000
#define RPC2_FLIMIT	-2000

/*
Warnings
*/
#define RPC2_OLDVERSION 	RPC2_WLIMIT-1
#define RPC2_INVALIDOPCODE	RPC2_WLIMIT-2
			/* Never returned by RPC2 itself; Used by higher levels, such as rp2gen */
#define RPC2_BADDATA		RPC2_WLIMIT-3
			/* Never used by RPC2 itself; used by rp2gen or higher levels to indicate bogus data */
#define RPC2_NOGREEDY		RPC2_WLIMIT-4
		        /* ioctl to allocate plenty of socket buffer space
			    failed; packet losses may be high especially on
			    bulk transfers */
#define	RPC2_ABANDONED		RPC2_WLIMIT-5

/*
Errors
*/
#define RPC2_CONNBUSY 		RPC2_ELIMIT-1
#define RPC2_SEFAIL1 		RPC2_ELIMIT-2
#define RPC2_TOOLONG 		RPC2_ELIMIT-3
#define	RPC2_NOMGROUP		RPC2_ELIMIT-4
#define RPC2_MGRPBUSY		RPC2_ELIMIT-5
#define	RPC2_NOTGROUPMEMBER	RPC2_ELIMIT-6
#define	RPC2_DUPLICATEMEMBER	RPC2_ELIMIT-7
#define	RPC2_BADMGROUP		RPC2_ELIMIT-8

/*
Fatal Errors
*/
#define RPC2_FAIL      		RPC2_FLIMIT-1
#define RPC2_NOCONNECTION 	RPC2_FLIMIT-2
#define RPC2_TIMEOUT   		RPC2_FLIMIT-3
#define RPC2_NOBINDING 		RPC2_FLIMIT-4
#define RPC2_DUPLICATESERVER 	RPC2_FLIMIT-5
#define RPC2_NOTWORKER 		RPC2_FLIMIT-6
#define RPC2_NOTCLIENT 		RPC2_FLIMIT-7
#define RPC2_WRONGVERSION 	RPC2_FLIMIT-8
#define RPC2_NOTAUTHENTICATED 	RPC2_FLIMIT-9
#define RPC2_CLOSECONNECTION 	RPC2_FLIMIT-10
#define RPC2_BADFILTER 		RPC2_FLIMIT-11
#define RPC2_LWPNOTINIT		RPC2_FLIMIT-12
#define RPC2_BADSERVER		RPC2_FLIMIT-13
#define RPC2_SEFAIL2		RPC2_FLIMIT-14
#define RPC2_DEAD		RPC2_FLIMIT-15
#define RPC2_NAKED		RPC2_FLIMIT-16
#define RPC2_SEFAIL3		RPC2_FLIMIT-17	/* More error codes for side effects */
#define RPC2_SEFAIL4		RPC2_FLIMIT-18	/* More error codes for side effects */

#define	MGRPERROR(code)\
    (code == RPC2_NOMGROUP ||\
     code == RPC2_MGRPBUSY ||\
     code == RPC2_NOTGROUPMEMBER ||\
     code == RPC2_DUPLICATEMEMBER ||\
     code == RPC2_BADMGROUP)


/*
Universal opcode values:  opcode values equal to or less than 0 are reserved.  Values greater than 0
are  usable by mutual agreement between clients and servers.  
*/
#define RPC2_INIT1OPENKIMONO	-2	/* Begin a new connection with security level RPC2_OPENKIMONO */
#define RPC2_INIT1AUTHONLY 	-3	/* Begin a new connection with security level RPC2_AUTHONLY */
#define RPC2_INIT1HEADERSONLY	-4	/* Begin a new connection with security level RPC2_HEADERSONLY */
#define RPC2_INIT1SECURE	-5	/* Begin a new connection with security level RPC2_SECURE */
#define RPC2_LASTACK		-6	/* Packet that acknowledges a reply */
#define RPC2_REPLY		-8	/* Reply packet */
#define RPC2_INIT2              -10	/* Phase 2 of bind handshake */
#define RPC2_INIT3		-11	/* Phase 3 of bind handshake */
#define RPC2_INIT4		-12	/* Phase 4 of bind handshake */
#define RPC2_NEWCONNECTION	-13	/* opcode of fake request generated by RPC2_GetRequest() on new connection */
#define RPC2_BUSY		-14	/* keep alive packet */
#define	RPC2_INITMULTICAST	-15	/* Establish a multicast connection */


/*
System Limits
*/
#define RPC2_MAXPACKETSIZE	4500    /* size of the largest acceptable packet buffer in bytes (includes prefix and header) */



/* Host, Mgrp, Port and Subsys Representations */

typedef	enum {RPC2_HOSTBYNAME = 39, RPC2_HOSTBYINETADDR = 17, 
	      RPC2_DUMMYHOST=88888} HostTag;
typedef	enum {RPC2_PORTBYINETNUMBER = 53, RPC2_PORTBYNAME = 64, 
	      RPC2_DUMMYPORT = 99999} PortTag;
typedef enum {RPC2_SUBSYSBYID = 71, RPC2_SUBSYSBYNAME = 84} SubsysTag;
typedef	enum {RPC2_MGRPBYINETADDR = 111, RPC2_MGRPBYNAME = 137} MgrpTag;

/*
Global variables for debugging:

RPC2_DebugLevel controls the level of debugging output produced on stdout.
A value of 0  turns  off the  output  altogether;  values  of  1, 10, and 100 are currently meaningful.
The default value of this variable is 0.

RPC2_Perror controls  the  printing  of  Unix error  messages on stdout.
A value of 1 turns on the printing, while 0 turns it off.  The default value for this variable is 1.

RPC2_Trace controls the tracing of RPC calls, packet transmissions and packet reception.  Set it to 1
for tracing.  Set to zero for stopping tracing.  The internal circular trace  buffer can be printed out
by calling RPC2_DumpTrace().

*/

extern long RPC2_DebugLevel;
extern long RPC2_Perror;
extern long RPC2_Trace;

/* Misc. global variables
 *
 * RPC2_strict_ip_matching enables stricter matching of incoming packets on
 * sender ip/port addresses. When this is enabled, multihomed or masqueraded
 * hosts will get rejected when they send their packet from the wrong IP
 * address. But connections are less likely to get spoofed.
 */
extern long RPC2_strict_ip_matching;


/*
************************* Data Types known to RPGen ***********************
*/
typedef
    long RPC2_Integer;     /*32-bit,  2's  complement representation.  On other machines, an explicit
                        conversion may be needed.*/
typedef
    unsigned long RPC2_Unsigned;     /* 32-bits.*/

typedef
    unsigned char RPC2_Byte;      /*A single 8-bit byte.*/

typedef
    double RPC2_Double;      /*A single 8-bit byte.*/

typedef
    RPC2_Byte *RPC2_ByteSeq;
/* A contiguous sequence of bytes. In the C implementation this is a
pointer.  RPC2Gen knows how to allocate and transform the pointer
values on transmission.  Beware if you are not dealing via RPC2Gen.
May be differently represented in other languages.  */

typedef
    RPC2_ByteSeq RPC2_String; /*no nulls except last byte*/
/* A null-terminated sequence of characters.  Identical to the C
language string definition.  */


typedef
    struct
        {
        RPC2_Integer SeqLen; /*length of SeqBody*/
        RPC2_ByteSeq SeqBody; /*no restrictions on contents*/
        }
    RPC2_CountedBS;
/*
A means of transmitting binary data.
*/

typedef
    struct
         {
         RPC2_Integer MaxSeqLen; /*max size of buffer represented by SeqBody*/
         RPC2_Integer SeqLen; /*number of interesting bytes in SeqBody*/
         RPC2_ByteSeq SeqBody; /*No restrictions on contents*/
         }
    RPC2_BoundedBS;

/* RPC2_BoundedBS is intended to allow you to remotely play the game
that C programmers play all the time: allocate a large buffer, fill in
some bytes, then call a procedure which takes this buffer as a
parameter and replaces its contents by a possibly longer sequence of
bytes.  Example: strcat().  */

typedef
    RPC2_Byte RPC2_EncryptionKey[RPC2_KEYSIZE];
/*
Keys used for encryption are fixed length byte sequences
*/


/*
************************* Data Types used only in runtime calls ********************************
*/

typedef RPC2_Integer RPC2_Handle;	/* NOT a small integer!!! */

/* Values for the Tag field of the following structures are defined above */
typedef
    struct 
    	{
	HostTag Tag;
	union
	    {
	    struct in_addr InetAddress;	/* NOTE: in network order, not host order */
	    char Name[64];	/* minimum length for use with domain names */
	    }
	    Value;
	}
    RPC2_HostIdent;

typedef
    struct 
    	{
	PortTag Tag;
	union
	    {
	    unsigned short InetPortNumber; /* NOTE: in network order, not host order */
	    char Name[20];	/* this is a pretty arbitrary length */
	    }
	    Value;
	}
    RPC2_PortIdent;

typedef
    struct 
    	{
	SubsysTag Tag;
	union
	    {
	    long  SubsysId;
	    char Name[20];	/* this is a pretty arbitrary length */
	    }
	    Value;
	}
    RPC2_SubsysIdent;


typedef
    struct
	{
	MgrpTag Tag;
	union
	    {
	    struct in_addr  InetAddress;    /* NOTE: in network order, not host order */
	    char	    Name[64];	    /* minimum length for use with domain names */
	    }
	    Value;
	}
    RPC2_McastIdent;


typedef
    struct		/* data structure filled by RPC2_GetPeerInfo() call */
	{
	RPC2_HostIdent 	 RemoteHost;
	RPC2_PortIdent   RemotePort;
	RPC2_SubsysIdent RemoteSubsys;
	RPC2_Handle	 RemoteHandle;
	RPC2_Integer	 SecurityLevel;
	RPC2_Integer	 EncryptionType;
	RPC2_Integer	 Uniquefier;
	RPC2_EncryptionKey	SessionKey;
	}
    RPC2_PeerInfo;

/* The RPC2_PacketBuffer definition below deals with both requests and
replies.  The runtime system provides efficient buffer storage
management routines --- use them!  */

typedef struct RPC2_PacketBuffer {
    struct RPC2_PacketBufferPrefix {
/* 
 * NOTE: The Prefix is only used by the runtime system on the local machine.
 *	 Neither clients nor servers ever deal with it.
 *	 It is never transmitted.
 */
	/* these four elements are used by the list routines */
	struct RPC2_PacketBuffer *Next;   /* next element in buffer chain */
	struct RPC2_PacketBuffer *Prev;	  /* prev element in buffer chain */
	enum {OBJ_PACKETBUFFER = 3247517} MagicNumber;/* to detect corruption */
	struct RPC2_PacketBuffer **Qname; /* pointer to the queue this packet
					     is on */

	long  BufferSize;		  /* Set at malloc() time; size of
					     entire packet, including prefix. */
	long  LengthOfPacket;		  /* size of data actually
					     transmitted, header+body */
	long File[3];
	long Line;

	/* these fields are set when we receive the packet. */
	RPC2_HostIdent		PeerHost;
	RPC2_PortIdent		PeerPort;
	struct timeval		RecvStamp;
    } Prefix;

/*
	The transmitted packet begins here.
*/
    struct RPC2_PacketHeader {
	/* The first four fields are never encrypted */
	RPC2_Integer  ProtoVersion;	/* Set by runtime system */
	RPC2_Integer  RemoteHandle;	/* Set by runtime system; -1 indicates
					   unencrypted error packet */
	RPC2_Integer  LocalHandle;	/* Set by runtime system */
	RPC2_Integer  Flags;	/* Used by runtime system only.  First byte
				   reserved for side effect use. Second byte
				   reserved for indicating color (see libfail
				   documentation). Last two bytes reserved for
				   RPC2 use. */

	/* Everything below here can be encrypted */
	RPC2_Unsigned  BodyLength;   /* of the portion after the header. Set
					by client.*/
	RPC2_Unsigned  SeqNumber;    /* unique identifier for this message on
					this connection; set by runtime
					system; odd on packets from client to
					server; even on packets from server to
					client */
	RPC2_Integer  Opcode;        /* Values  greater than 0 are
					subsystem-specific: set by client.
					Values less than 0 reserved: set by
					runtime system. Type of packet
					determined by Opcode value: > 0 ==>
					request packet. Values of RPC2_REPLY
					==> reply packet, RPC2_ACK ==> ack
					packet, and so on */
	RPC2_Unsigned SEFlags;	    /* Bits for use by side effect routines */
	RPC2_Unsigned SEDataOffset; /* Offset of piggy-backed side effect
				       data, from the start of Body */
	RPC2_Unsigned SubsysId;     /* Subsystem identifier. Filled by runtime
				       system. */
	RPC2_Integer  ReturnCode;   /* Set by server on replies; meaningless
				       on request packets*/
	RPC2_Unsigned Lamport;	    /* For distributed clock mechanism */
	RPC2_Integer  Uniquefier;   /* Used only in Init1 packets; truly
				       unique random number */
	RPC2_Unsigned TimeStamp;    /* Used for rpc timing. */
	RPC2_Integer  BindTime;     /* Used to send the bind time to server.
				       Temporary, i hope. */
    } Header;
    
    RPC2_Byte Body[1]; /* Arbitrary length body. For requests: IN and INOUT
			  parameters; For replies: OUT and INOUT parameters;
			  Header.BodyLength gives the length of this field */
} RPC2_PacketBuffer; /* The second and third fields actually get sent over
			  the wire */


/* Meaning of Flags field in RPC2 packet header.
 * First (leftmost) byte of Flags field is reserved for use by side effect
 * routines. This is in addition to the SEFlags field. Flags is not encrypted,
 * but SEFLAGS is. Second byte of Flags field is reserved for indicating
 * packet color by libfail. Third and fourth bytes are used as genuine RPC2
 * flags */
#define RPC2_RETRY	0x1	/* set by runtime system */
#define RPC2_ENCRYPTED	0x2	/* set by runtime system */
#define	RPC2_MULTICAST	0x4	/* set by runtime system */

#define	IsMulticast(pb)	((pb)->Header.Flags & RPC2_MULTICAST)

 /* Format of filter used in RPC2_GetRequest */

enum E1 {ANY=12, ONECONN=37, ONESUBSYS=43};
enum E2 {OLD=27, NEW=38, OLDORNEW=69};

typedef
    struct
	{
        enum E1 FromWhom;
	enum E2 OldOrNew;
	union
	    {
	    RPC2_Handle WhichConn;	/* if FromWhom  == ONECONN */
	    long SubsysId;		/* if FromWhom == ONESUBSYS */
	    }
	    ConnOrSubsys;
	}
    RPC2_RequestFilter;		/* Type of Filter parameter in RPC2_GetRequest() */
    

/*
The following data structure is the body of the packet synthesised by the runtime system on a 
new connection, and returned as the result of an RPC2_GetRequest().
*/
typedef
    struct
	{
	RPC2_Integer SideEffectType;
	RPC2_Integer SecurityLevel;
	RPC2_Integer EncryptionType;
	RPC2_Integer AuthenticationType;
	RPC2_CountedBS ClientIdent;
	}
    RPC2_NewConnectionBody;


/* Structure for passing various initialization options to RPC2_Init() */
typedef 
    struct 
        {
	    RPC2_Byte	ScaleTimeouts;
	}
    RPC2_Options;


/* Structure for passing parameters to RPC2_NewBinding() and its multi clone */

typedef
    struct
	{
	long SecurityLevel;
	long EncryptionType;
	RPC2_EncryptionKey *SharedSecret;
	RPC2_Integer AuthenticationType;
	RPC2_CountedBS *ClientIdent;
	long SideEffectType;
	RPC2_Integer Color;
	}
    RPC2_BindParms;

 /* enums used both in original RPC and for MultiRPC (was in rp2.h) */

typedef enum{ NO_MODE=0, IN_MODE=1, OUT_MODE=2, IN_OUT_MODE=3, C_END=4 } MODE;

typedef enum{ RPC2_INTEGER_TAG=0,	RPC2_UNSIGNED_TAG=1,	RPC2_BYTE_TAG=2,
	      RPC2_STRING_TAG=3,	RPC2_COUNTEDBS_TAG=4,	RPC2_BOUNDEDBS_TAG=5,
	      RPC2_BULKDESCRIPTOR_TAG=6,			RPC2_ENCRYPTIONKEY_TAG=7,
	      RPC2_STRUCT_TAG=8,	RPC2_ENUM_TAG=9, RPC2_DOUBLE_TAG=10 } TYPE_TAG;


 /* struct for MakeMulti argument packing and unpacking */

typedef
    struct arg
	{
	MODE		mode;  /* IN, IN_OUT, OUT, NO_MODE */
	TYPE_TAG	type;  /* RPC2 type of argument */
	int		size;  /* NOTE: for structures, this is */
			    /* REAL size, not packed size */
	struct arg	*field;/* nested for structures only */
	int		bound; /* used for byte arrays only */
	void            (*startlog) (long); /* used for stub logging */
	void            (*endlog) (long, RPC2_Integer, RPC2_Handle *, RPC2_Integer *); /* used for stub logging */
	}
    ARG;


/* Structure for passing multicast information */
typedef
    struct
	{
	RPC2_Handle Mgroup;	/*  Multicast group handle*/
	RPC2_Integer ExpandHandle;  /* flag indicating whether Mgroup handle should be
				             expanded or not */
	RPC2_Integer StraySeen; /*  on output, total number of stray responses */
	RPC2_Integer StrayLen;  /*  size of array StraySites[]; */
	RPC2_HostIdent *StraySites; /* on output, stray hosts that responded;
				       some hosts are lost if StraySeen > StrayLen */
	}
    RPC2_Multicast;


#define RPC2_MAXLOGLENGTH  32
#define RPC2_MAXQUANTUM  ((unsigned)-1)

/* 
 * Information is exported via a log consisting of timestamped
 * variable-length records.  The log entries reflect either
 * static estimates or actual measurements of the network.  All 
 * connections to a service are coalesced in one log, but the
 * entries contain the connection ID so they may be demultiplexed
 * if desired.
 */
typedef enum {RPC2_UNSET_NLE = 0, RPC2_MEASURED_NLE = 1, 
		  RPC2_STATIC_NLE = 2} NetLogTag;

typedef enum {RPC2_MEASUREMENT = 0, SE_MEASUREMENT = 1} NetLogEntryType;

typedef
    struct
        {
	struct timeval TimeStamp;	/* time of log entry */
	NetLogTag Tag;			/* what kind of entry am I? */    
	union 			       
	    {
	    struct Measured_NLE			/* a measurement */
	        {
		RPC2_Handle Conn;		/* connection measured */
		RPC2_Unsigned Bytes;		/* data bytes involved */    
		RPC2_Unsigned ElapsedTime;	/* in msec */
		}
	        Measured;			/* RPC and SFTP use this */

	    struct Static_NLE			/* a static estimate */
	        {
		RPC2_Unsigned Bandwidth;	/* in Bytes/second */
	        }				/* latency, cost, ... */
	        Static;
            }
	    Value;
        }
    RPC2_NetLogEntry;

typedef
    struct
        {
	RPC2_Unsigned Quantum;		/* length of measurements, in msec */
	RPC2_Unsigned NumEntries;	/* number of entries requested */
	RPC2_Unsigned ValidEntries;	/* number of entries returned */
	RPC2_NetLogEntry *Entries;	/* preallocated storage for entries */
        }
    RPC2_NetLog;


/*
RPC2 runtime routines:
*/
#include "se.h"
#include "multi.h"


extern long RPC2_Init (char *VersionId, RPC2_Options *Options, RPC2_PortIdent *PortList, long RetryCount, struct timeval *KeepAliveInterval);
void RPC2_SetLog(FILE *, int);
extern long RPC2_Export (RPC2_SubsysIdent *Subsys);
extern long RPC2_DeExport (RPC2_SubsysIdent *Subsys);
#ifndef NONDEBUG
#define RPC2_AllocBuffer(x, y)  (rpc2_AllocBuffer((long) (x), y, __FILE__, (long) __LINE__))
#else
#define  RPC2_AllocBuffer(x, y)  (rpc2_AllocBuffer((long) (x), y, 0, (long) 0))
#endif /* NONDEBUG */
extern long rpc2_AllocBuffer (long MinBodySize, RPC2_PacketBuffer **BufferPtr, char *SrcFile, long SrcLine);
extern long RPC2_FreeBuffer (RPC2_PacketBuffer **Buffer);
extern long RPC2_SendResponse (RPC2_Handle ConnHandle, RPC2_PacketBuffer *Reply);

typedef long RPC2_GetKeys_func(RPC2_Integer *AuthenticationType,
			       RPC2_CountedBS *cident,
			       RPC2_EncryptionKey SharedSecret,
			       RPC2_EncryptionKey sessionkey);
typedef long RPC2_AuthFail_func(RPC2_Integer AuthenticationType,
				RPC2_CountedBS *cident,
				RPC2_Integer EncryptionType,
				RPC2_HostIdent *PeerHost,
				RPC2_PortIdent *PeerPort);

extern long RPC2_GetRequest (RPC2_RequestFilter *Filter,
			     RPC2_Handle *ConnHandle,
			     RPC2_PacketBuffer **Request,
			     struct timeval *Patience, RPC2_GetKeys_func *,
			     long EncryptionTypeMask, RPC2_AuthFail_func *);

extern long RPC2_MakeRPC (RPC2_Handle ConnHandle, RPC2_PacketBuffer *Request,
			  SE_Descriptor *SDesc, RPC2_PacketBuffer **Reply,
			  struct timeval *Patience, long EnqueueRequest);

typedef long RPC2_UnpackMulti_func(int HowMany,
				   RPC2_Handle ConnHandleList[],
				   ARG_INFO *ArgInfo,
				   RPC2_PacketBuffer *Reply,
				   long errcode, long idx);

extern long RPC2_MultiRPC (int HowMany, RPC2_Handle ConnHandleList[],
			   RPC2_Integer RCList[], RPC2_Multicast *MCast,
			   RPC2_PacketBuffer *Request,
			   SE_Descriptor SDescList[], RPC2_UnpackMulti_func *, 
			   ARG_INFO *ArgInfo, struct timeval *BreathOfLife);

extern long RPC2_NewBinding (RPC2_HostIdent *Host, RPC2_PortIdent *Port,
	 RPC2_SubsysIdent *Subsys, RPC2_BindParms *BParms, RPC2_Handle *ConnHandle);
extern long RPC2_InitSideEffect (RPC2_Handle ConnHandle, SE_Descriptor *SDesc);
extern long RPC2_CheckSideEffect (RPC2_Handle ConnHandle, SE_Descriptor *SDesc, long Flags);
extern long RPC2_Unbind (RPC2_Handle ConnHandle);
extern long RPC2_GetPrivatePointer (RPC2_Handle WhichConn, char **PrivatePtr);
extern long RPC2_SetPrivatePointer (RPC2_Handle WhichConn, char *PrivatePtr);
struct SFTP_Entry;
extern long RPC2_GetSEPointer (RPC2_Handle WhichConn, struct SFTP_Entry **SEPtr);
extern long RPC2_SetSEPointer (RPC2_Handle WhichConn, struct SFTP_Entry *SEPtr);
extern long RPC2_GetPeerInfo (RPC2_Handle WhichConn, RPC2_PeerInfo *PeerInfo);
extern char *RPC2_ErrorMsg (long rc);
extern long RPC2_DumpTrace (FILE *OutFile, long HowMany);
extern long RPC2_DumpState (FILE *OutFile, long Verbosity);
extern long RPC2_InitTraceBuffer (long HowMany);
extern long RPC2_LamportTime ();
extern long RPC2_Enable (RPC2_Handle ConnHandle);
extern long RPC2_CreateMgrp (RPC2_Handle *MgroupHandle, RPC2_McastIdent *MulticastHost, RPC2_PortIdent *MulticastPort, RPC2_SubsysIdent *Subsys, RPC2_Integer SecurityLevel, RPC2_EncryptionKey SessionKey, RPC2_Integer EncryptionType, long SideEffectType);
extern long RPC2_AddToMgrp (RPC2_Handle MgroupHandle, RPC2_Handle ConnHandle);
extern long RPC2_RemoveFromMgrp (RPC2_Handle MgroupHandle, RPC2_Handle ConnHandle);
extern long RPC2_DeleteMgrp (RPC2_Handle MgroupHandle);
extern long MRPC_MakeMulti (int ServerOp, ARG ArgTypes[], RPC2_Integer HowMany,
	RPC2_Handle CIDList[], RPC2_Integer RCList[], RPC2_Multicast *MCast,
	RPC2_HandleResult_func *, struct timeval *Timeout,  ...);

extern long RPC2_SetColor (RPC2_Handle ConnHandle, RPC2_Integer Color);
extern long RPC2_GetColor (RPC2_Handle ConnHandle, RPC2_Integer *Color);
extern long RPC2_GetPeerLiveness (RPC2_Handle ConnHandle, struct timeval *Time, struct timeval *SETime);
extern long RPC2_GetNetInfo (RPC2_Handle ConnHandle, RPC2_NetLog *RPCLog, RPC2_NetLog *SELog);
extern long RPC2_PutNetInfo (RPC2_Handle ConnHandle, RPC2_NetLog *RPCLog, RPC2_NetLog *SELog);
extern long RPC2_ClearNetInfo (RPC2_Handle ConnHandle);
extern long getsubsysbyname (char *subsysName);
extern int RPC2_R2SError (int error);
extern int RPC2_S2RError (int error);

int RPC2_GetRTT(RPC2_Handle handle, unsigned long *RTT, unsigned long *RTTvar);
int RPC2_GetBandwidth(RPC2_Handle handle, unsigned long *BWlow,
		      unsigned long *BWavg, unsigned long *BWhigh);
int RPC2_GetLastObs(RPC2_Handle handle, struct timeval *tv);

int RPC2_SetTimeout(RPC2_Handle whichConn, struct timeval timeout);


int struct_len(ARG **a_types, PARM **args);

/* These shouldn't really be here: they are internal RPC2 routines
   But some applications (e.g. Coda auth server) use them */

extern void rpc2_Encrypt (char *FromBuffer, char *ToBuffer,
		long HowManyBytes, char *WhichKey, long EncryptionType);

void rpc2_Decrypt (char *FromBuffer, char *ToBuffer, long  HowManyBytes,
    RPC2_EncryptionKey WhichKey, int EncryptionType);
extern long rpc2_NextRandom (char *StatePtr);

/* hack until we can do something more sophisticated. */
extern long rpc2_Bandwidth;

/* for multihomed servers */
struct in_addr RPC2_setip(struct in_addr *ip);


/*------- Transmission Statistics -------------*/
struct SStats
    {
    unsigned long
	Total,     /* PacketsSent */
    	Retries,   /* PacketRetries */
	Cancelled, /* Packet Retries Cancelled (heard from side effect) */
	Multicasts,/* MulticastsSent */
	Busies,    /* BusiesSent */
    	Naks,      /* NaksSent */
	Bytes;     /* BytesSent */
    };


struct RStats
    {
    unsigned long
	Total,	       /* PacketsRecvd */
	Giant,	       /* GiantPacketsRecvd */
	Replies,       /* Replies */
	Requests,      /* Requests */
	GoodReplies,  /* GoodReplies */
	GoodRequests, /* GoodRequests */
	Multicasts,   /* MulticastRequests */
	GoodMulticasts, /* GoodMulticastRequests */
	Busies,       /* BusiesReceived */
	GoodBusies,   /* GoodBusies */
	Bogus,         /* BogusPackets */
	Naks,         /* NaksReceived */
	Bytes;	       /* BytesReceived */

    };

extern struct SStats rpc2_Sent;
extern struct RStats rpc2_Recvd;
extern struct SStats rpc2_MSent;
extern struct RStats rpc2_MRecvd;

extern int rpc2_43bsd;	/* TRUE  on 4.3BSD, FALSE on 4.2BSD */

/* For debugging */
extern FILE *rpc2_logfile;
extern FILE *rpc2_tracefile;
extern int RPC2_enableReaping;

/* What port are we listening on. */
extern RPC2_HostIdent   rpc2_LocalHost;
extern RPC2_PortIdent   rpc2_LocalPort;

/* Allocation/destruction counters */
extern long rpc2_PBSmallCreationCount,  rpc2_PBSmallFreeCount;
extern long rpc2_PBMediumCreationCount, rpc2_PBMediumFreeCount;
extern long rpc2_PBLargeCreationCount,  rpc2_PBLargeFreeCount;
extern long rpc2_SLCreationCount,       rpc2_SLFreeCount;
extern long rpc2_ConnCreationCount,     rpc2_ConnCount,	rpc2_ConnFreeCount;
extern long rpc2_SSCreationCount,	rpc2_SSCount,   rpc2_SSFreeCount;
extern long rpc2_Unbinds, rpc2_FreeConns, rpc2_AllocConns, rpc2_GCConns;
extern long rpc2_PBCount, rpc2_PBHoldCount, rpc2_PBFreezeCount;
extern long rpc2_FreezeHWMark, rpc2_HoldHWMark;

#endif /* _RPC2_ */

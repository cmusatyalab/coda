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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
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
#include <netinet/in.h>
#include <sys/time.h>
#include "lwp.h"
#include "timer.h"
#include "preempt.h"
#include "rpc2.h"
#include "rpc2.private.h"

/* NOTE:
    Versions of RPC2 prior to May 28 1986 used random() for random number generation since it
    was supposed to generated more random sequences and allowed multiple sequence to coexist in
    non-interfering ways.  Unfortunately there seems to be a bug in random() that causes arbitrary
    bytes of memory to be clobbered.  This appeared in conjunction with Guardian and the bug fix
    for it caused other arbitrary bytes in RPC2 to be clobbered.
    
    Till someone can figure out what is wrong with random, we will use the old-fashioned but safer
    rand().  The code with NEWRANDOM ifdefs around it documents the changes.
*/

#ifdef NEWRANDOM
static char RNState[256];	/* state for random() number generator;
				can coexist with other states elsewhere  */
#endif

int rpc2_XDebug;

void rpc2_Encrypt(IN FromBuffer, OUT ToBuffer, IN HowManyBytes, IN WhichKey, IN EncryptionType)
    char *FromBuffer;		/* The string of bytes to be encrypted.*/
    char *ToBuffer;		/* Where to put the encrypted string.
				    Equal to FromBuffer ==> inplace. */
    long  HowManyBytes;		/* The number of bytes in FromBuffer. */
    char *WhichKey;		/* The encryption key to be used. */
    long EncryptionType;	/* one of the supported types */

    /* Does a trivial Exclusive-OR of FromBuffer and puts result in ToBuffer */
    
    /* NOTE: the assembler fast xor routine fxor has a bug somewhere; I have no time
             to go into it; am removing its invocation here, and just using the slower
	     C version below  --- Satya 3/7/1990 */

    {
    register unsigned char *p, *q, *r, *s;
    register long i;
    
    PRE_BeginCritical();
    assert(EncryptionType == RPC2_XOR);	/* for now */
    
    p = (unsigned char *)FromBuffer;		/* ptr to next input char */
    q = (unsigned char *)WhichKey;		/* ptr to next key char */
    r = q + RPC2_KEYSIZE;			/* right limit of q */
    s = (unsigned char *)ToBuffer;		/* ptr to next output char */
    for (i = HowManyBytes; i > 0; i--)
	{
	*s++ = (*p++) ^ (*q++);
	if (q >= r) q = (unsigned char *)WhichKey;
	}
    PRE_EndCritical();
    }


void rpc2_Decrypt(IN FromBuffer, OUT ToBuffer,  IN HowManyBytes, IN WhichKey, IN EncryptionType)
    char *FromBuffer;		/* The string of bytes to be decrypted. */
    char *ToBuffer;		/* Where to put the decrypted bytes. Equal to FromBuffer for inplace encryption */
    long  HowManyBytes;		/* The number of bytes in Buffer */
    RPC2_EncryptionKey WhichKey;	/* The decryption key to be used */
    int EncryptionType;

    {
    PRE_BeginCritical();
    assert(EncryptionType == RPC2_XOR);
    rpc2_Encrypt(FromBuffer, ToBuffer, HowManyBytes, WhichKey, EncryptionType);
    PRE_EndCritical();
    }

#ifdef NEWRANDOM
void rpc2_InitRandom()
    {
    PRE_BeginCritical();
    initstate(rpc2_TrueRandom(), RNState, sizeof(RNState));    
    setstate(RNState);		/* default for rpc2_NextRandom() */
    PRE_EndCritical();
    }
#else
void rpc2_InitRandom()
    {
    long seed;

    PRE_BeginCritical();
    seed = rpc2_TrueRandom();
    srand(seed);
    PRE_EndCritical();
    }
#endif

long rpc2_TrueRandom()
    /*
    Returns a non-zero random number which may be used as the seed of a pseudo-random number generator.
    Obtained by looking at the microseconds part of the time of day.
    How truly random this is depends on the hardware.  On the VAX and SUNs, it seems reasonable when the lowest
    byte of gettimeofday is thrown away.
    */
    {
    struct timeval tp;
    register long x=0;

    PRE_BeginCritical();
    while (x == 0)
	{
	TM_GetTimeOfDay(&tp, NULL);
	x = tp.tv_usec >> 8;	/* No sign problems 'cause tv_usec never has high bit set */
	}    
    PRE_EndCritical();
    return(x);
    }


#ifdef NEWRANDOM
long rpc2_NextRandom(StatePtr)
    char *StatePtr;
    /*  Generates the next random number from the sequence corr to StatePtr->state.
	Restores the generator to use the state it was using upon entry.
	Isolates multiple random number sequences.
	Handy when a server is also a client, or when higher-level software also need random numbers.

	If StatePtr is NULL, the static variable RNState is used as default.

	NOTE: The numbers are less than 2**30 to make overflow problems unlikely.
    */
    {
    register char *s;
    register long x;

    PRE_BeginCritical();
    if (StatePtr == NULL) StatePtr = RNState;	/* it's ok, call by value */
    s = (char *) setstate(StatePtr);
    while ((x = random()) > 1073741824);	/* 2**30 */
    setstate(s);
    PRE_EndCritical();
    return(x);
    }

#else
long rpc2_NextRandom(StatePtr)
    char *StatePtr;
    /*  Generates the next random number.
	NOTE: The numbers are less than 2**30 to make overflow problems unlikely.
	
	Uses the old-fashioned rand() rather than the buggy random().
	StatePtr is ignored and is present only for compatibility with version that
	uses random().
    */
    {
    register long x;

    PRE_BeginCritical();
    while ((x = rand()) > 1073741824);	/* 2**30 */
    PRE_EndCritical();
    return(x);
    }

#endif


rpc2_ApplyE(pb, ce)
    register RPC2_PacketBuffer *pb;
    register struct CEntry *ce;
    {
    switch((int)ce->SecurityLevel)
	{
	case RPC2_OPENKIMONO:
	case RPC2_AUTHONLY:
		    return;

	case RPC2_HEADERSONLY:
	    rpc2_Encrypt((char *)&pb->Header.BodyLength, (char *)&pb->Header.BodyLength, 
		    sizeof(struct RPC2_PacketHeader)-4*sizeof(RPC2_Integer),
		    ce->SessionKey, ce->EncryptionType);
	    break;
	
	case RPC2_SECURE:
	    rpc2_Encrypt((char *)&pb->Header.BodyLength, (char *)&pb->Header.BodyLength, 
		    pb->Prefix.LengthOfPacket-4*sizeof(RPC2_Integer),
		    ce->SessionKey, ce->EncryptionType);
	    break;
	}
	
    pb->Header.Flags = htonl(RPC2_ENCRYPTED | ntohl(pb->Header.Flags));
    }

rpc2_ApplyD(pb, ce)
    register RPC2_PacketBuffer *pb;
    register struct CEntry *ce;
    {
    if (!(ntohl(pb->Header.Flags) & RPC2_ENCRYPTED)) return;

    switch((int)ce->SecurityLevel)
	{
	case RPC2_HEADERSONLY:
	    rpc2_Decrypt((char *)&pb->Header.BodyLength, (char *)&pb->Header.BodyLength, 
		    sizeof(struct RPC2_PacketHeader)-4*sizeof(RPC2_Integer),
		    ce->SessionKey, ce->EncryptionType);
	    return;
	
	case RPC2_SECURE:
	    rpc2_Decrypt((char *)&pb->Header.BodyLength, (char *)&pb->Header.BodyLength, 
		    pb->Prefix.LengthOfPacket-4*sizeof(RPC2_Integer),
		    ce->SessionKey, ce->EncryptionType);
	    return;
	}
    }

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
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <assert.h>
#include <rpc2/secure.h>
#include "rpc2.private.h"

#define RNDPOOL 256
static uint8_t RNState[RNDPOOL];
static unsigned int RNStateAvail;

int rpc2_XDebug;

void rpc2_Encrypt(IN FromBuffer, OUT ToBuffer, IN HowManyBytes, IN WhichKey,
                  IN EncryptionType) char
    *FromBuffer; /* The string of bytes to be encrypted.*/
char *ToBuffer; /* Where to put the encrypted string.
				    Equal to FromBuffer ==> inplace. */
size_t HowManyBytes; /* The number of bytes in FromBuffer. */
RPC2_EncryptionKey WhichKey; /* The encryption key to be used. */
RPC2_Integer EncryptionType; /* one of the supported types */

/* Does a trivial Exclusive-OR of FromBuffer and puts result in ToBuffer */

/* NOTE: the assembler fast xor routine fxor has a bug somewhere; I have no time
             to go into it; am removing its invocation here, and just using the slower
	     C version below  --- Satya 3/7/1990 */

{
    unsigned char *p, *q, *r, *s;
    long i;

    assert(EncryptionType == RPC2_XOR); /* for now */

    p = (unsigned char *)FromBuffer; /* ptr to next input char */
    q = (unsigned char *)WhichKey; /* ptr to next key char */
    r = q + RPC2_KEYSIZE; /* right limit of q */
    s = (unsigned char *)ToBuffer; /* ptr to next output char */
    for (i = HowManyBytes; i > 0; i--) {
        *s++ = (*p++) ^ (*q++);
        if (q >= r)
            q = (unsigned char *)WhichKey;
    }
}

void rpc2_Decrypt(IN FromBuffer, OUT ToBuffer, IN HowManyBytes, IN WhichKey,
                  IN EncryptionType) char
    *FromBuffer; /* The string of bytes to be decrypted. */
char *ToBuffer; /* Where to put the decrypted bytes. Equal to FromBuffer for inplace encryption */
size_t HowManyBytes; /* The number of bytes in Buffer */
RPC2_EncryptionKey WhichKey; /* The decryption key to be used */
RPC2_Integer EncryptionType;

{
    assert(EncryptionType == RPC2_XOR);
    rpc2_Encrypt(FromBuffer, ToBuffer, HowManyBytes, WhichKey, EncryptionType);
}

void rpc2_InitRandom()
{
    /* just in case someone uses rpc2_InitRandom before calling RPC2_Init */
    secure_init(0);
    RNStateAvail = 0;
}

unsigned int rpc2_NextRandom(char *StatePtr)
{
    unsigned int x;

    if (RNStateAvail < sizeof(x)) {
        secure_random_bytes(RNState, sizeof(RNState));
        RNStateAvail = sizeof(RNState);
    }
    memcpy(&x, RNState + (sizeof(RNState) - RNStateAvail), sizeof(x));
    RNStateAvail -= sizeof(x);

    return (x);
}

void rpc2_ApplyE(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
    if (ce->sa.encrypt)
        return;

    switch ((int)ce->SecurityLevel) {
    case RPC2_OPENKIMONO:
    case RPC2_AUTHONLY:
        return;

    case RPC2_HEADERSONLY:
        rpc2_Encrypt(
            (char *)&pb->Header.BodyLength, (char *)&pb->Header.BodyLength,
            sizeof(struct RPC2_PacketHeader) - 4 * sizeof(RPC2_Integer),
            ce->SessionKey, ce->EncryptionType);
        break;

    case RPC2_SECURE:
        rpc2_Encrypt((char *)&pb->Header.BodyLength,
                     (char *)&pb->Header.BodyLength,
                     pb->Prefix.LengthOfPacket - 4 * sizeof(RPC2_Integer),
                     ce->SessionKey, ce->EncryptionType);
        break;
    }

    pb->Header.Flags = htonl(ntohl(pb->Header.Flags) | RPC2_ENCRYPTED);
}

void rpc2_ApplyD(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
    if (!(ntohl(pb->Header.Flags) & RPC2_ENCRYPTED))
        return;

    switch ((int)ce->SecurityLevel) {
    case RPC2_HEADERSONLY:
        rpc2_Decrypt(
            (char *)&pb->Header.BodyLength, (char *)&pb->Header.BodyLength,
            sizeof(struct RPC2_PacketHeader) - 4 * sizeof(RPC2_Integer),
            ce->SessionKey, ce->EncryptionType);
        break;

    case RPC2_SECURE:
        rpc2_Decrypt((char *)&pb->Header.BodyLength,
                     (char *)&pb->Header.BodyLength,
                     pb->Prefix.LengthOfPacket - 4 * sizeof(RPC2_Integer),
                     ce->SessionKey, ce->EncryptionType);
        break;

    default:
        break;
    }
    pb->Header.Flags = htonl(ntohl(pb->Header.Flags) & ~RPC2_ENCRYPTED);
}

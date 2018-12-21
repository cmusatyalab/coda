/* BLURB lgpl

                        Coda File System
                            Release 6

            Copyright (c) 2006 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
#*/

#include <stdlib.h>

#include "rpc2.private.h"

/* I don't think we should expect to wake up within a millisecond --JH */
#define RPC2_MINDELAY 1000

struct DelayEntry {
    int sock;
    struct RPC2_addrinfo *addr;
    size_t len;
    struct security_association *sa;
};

int rpc2_DelayedSend(int s, struct RPC2_addrinfo *addr, RPC2_PacketBuffer *pb,
                     struct timeval *tv)
{
    struct SL_Entry *sl;
    struct DelayEntry *de;

    /* If delay is too short, just send it */
    if (tv->tv_sec == 0 && tv->tv_usec < RPC2_MINDELAY)
        return 0;

    sl = rpc2_AllocSle(DELAYED_SEND, NULL);
    if (!sl)
        return 0; /* allocation failed, just send the packet now */

    de = malloc(sizeof(struct DelayEntry) + pb->Prefix.LengthOfPacket);
    if (!de) {
        rpc2_FreeSle(&sl);
        return 0;
    }

    de->sock = s;
    de->len  = pb->Prefix.LengthOfPacket;
    de->addr = RPC2_copyaddrinfo(addr);
    /* hopefully the connection entry (and security association) are not
   * destroyed during the delay period */
    de->sa = pb->Prefix.sa;
    memcpy(&de[1], &pb->Header, de->len);

    /* enqueue */
    sl->data = de;
    say(9, RPC2_DebugLevel,
        "Delaying packet transmission for %p by %ld.%06lus\n", de, tv->tv_sec,
        tv->tv_usec);
    rpc2_ActivateSle(sl, tv);
    return 1;
}

void rpc2_SendDelayedPacket(struct SL_Entry *sl)
{
    struct DelayEntry *de = (struct DelayEntry *)sl->data;
    say(9, RPC2_DebugLevel, "Sending delayed packet %p\n", de);
    (void)secure_sendto(de->sock, &de[1], de->len, 0, de->addr->ai_addr,
                        de->addr->ai_addrlen, de->sa);
    RPC2_freeaddrinfo(de->addr);
    free(de);
    rpc2_FreeSle(&sl);
}

int rpc2_DelayedRecv(RPC2_PacketBuffer *pb, struct timeval *tv)
{
    struct SL_Entry *sl;

    /* update the time we supposedly 'received' this packet */
    pb->Prefix.RecvStamp.tv_usec += tv->tv_usec;
    while (pb->Prefix.RecvStamp.tv_usec >= 1000000) {
        pb->Prefix.RecvStamp.tv_usec -= 1000000;
        pb->Prefix.RecvStamp.tv_sec++;
    }
    pb->Prefix.RecvStamp.tv_sec += tv->tv_sec;

    /* If delay is too short, just accept it */
    if (tv->tv_sec == 0 && tv->tv_usec < RPC2_MINDELAY)
        return 0;

    sl = rpc2_AllocSle(DELAYED_RECV, NULL);
    if (!sl)
        return 0; /* allocation failed, accept the packet now */

    sl->data = pb;
    say(9, RPC2_DebugLevel, "Delaying packet reception for %p by %ld.%06lus\n",
        pb, tv->tv_sec, tv->tv_usec);
    rpc2_ActivateSle(sl, tv);
    return 1;
}

RPC2_PacketBuffer *rpc2_RecvDelayedPacket(struct SL_Entry *sl)
{
    RPC2_PacketBuffer *pb = (RPC2_PacketBuffer *)sl->data;
    say(9, RPC2_DebugLevel, "Receiving delayed packet %p\n", pb);
    rpc2_FreeSle(&sl);
    return pb;
}

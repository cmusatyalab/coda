/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2025 Carnegie Mellon University
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

/*
        -- SFTP Globals and routines common to sftp1.c and sftp2.c
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include <rpc2/se.h>
#include <rpc2/sftp.h>

#include "rpc2.private.h"

/* Globals: see sftp.h for descriptions; set by SFTP_Activate(), via
 * SFTP_SetDefaults() */
long SFTP_PacketSize;
long SFTP_WindowSize;
long SFTP_EnforceQuota;
long SFTP_SendAhead;
long SFTP_AckPoint;
long SFTP_DoPiggy;
long SFTP_DupThreshold;
long SFTP_MaxPackets;

/* long SFTP_DebugLevel; */ /* defined to RPC2_DebugLevel for now */
long sftp_PacketsInUse;

long sftp_datas, sftp_datar, sftp_acks, sftp_ackr, sftp_busy, sftp_triggers,
    sftp_starts, sftp_retries, sftp_timeouts, sftp_windowfulls, sftp_duplicates,
    sftp_bogus, sftp_ackslost, sftp_didpiggy, sftp_starved, sftp_rttupdates;
struct sftpStats sftp_Sent, sftp_MSent;
struct sftpStats sftp_Recvd, sftp_MRecvd;

static int CheckWorried(struct SFTP_Entry *sEntry);
static int ResendWorried(struct SFTP_Entry *sEntry);
static int SendSendAhead(struct SFTP_Entry *sEntry);
static int SendFirstUnacked(struct SFTP_Entry *sEntry, int winopen);
static int WinIsOpen(struct SFTP_Entry *sEntry);
static void sftp_SendAck(struct SFTP_Entry *sEntry);
static int sftp_vfwritev(struct SFTP_Entry *se, struct iovec *iovarray,
                         long howMany);
static int sftp_vfreadv(struct SFTP_Entry *se, struct iovec iovarray[],
                        long howMany);

/* sftp5.c */
void B_ShiftLeft(unsigned int *bMask, int bShift);
void B_Assign(unsigned int *dest, unsigned int *src);
void B_CopyToPacket(unsigned int *bMask, RPC2_PacketBuffer *whichPacket);
void B_CopyFromPacket(RPC2_PacketBuffer *whichPacket, unsigned int *bMask);

#ifdef RPC2DEBUG
static void PrintDb(struct SFTP_Entry *se, RPC2_PacketBuffer *pb);
#define BOGOSITY(se, pb)                                                     \
    (fprintf(rpc2_tracefile, "SFTP bogosity:  file %s, line %d\n", __FILE__, \
             __LINE__),                                                      \
     PrintDb(se, pb))
#else
#define BOGOSITY(se, pb)
#endif

/* -------------------- Common file open routine -------------------- */

int sftp_InitIO(struct SFTP_Entry *sEntry)
/* Fills the openfd field of sEntry by opening and seeking to the
   file/offset specified by its SDesc.  Merely initializes internal
   data structure for FILEINVM Returns 0 on success, -1 on failure.

   NOTE: seek offsets <= 0 cause file truncation.  This should
   probably only be for seek offsets < 0, but there are too many
   pieces of code out there that set it to 0 and expect truncation.  */
{
    struct SFTP_Descriptor *sftpd;
    long omode, oflags;

    sftpd = &sEntry->SDesc->Value.SmartFTPD;

    /* Short-circuit if file is in VM */
    if (sftpd->Tag == FILEINVM) {
        sEntry->openfd = -1; /* no descriptor really */
        if (sftpd->SeekOffset > 0) {
            if (sftpd->SeekOffset > sftpd->FileInfo.ByAddr.vmfile.SeqLen)
                return (-1); /* bogus seek */
            else
                sftpd->FileInfo.ByAddr.vmfilep = sftpd->SeekOffset;
        } else {
            /* no seek required */
            sftpd->FileInfo.ByAddr.vmfilep = 0;
        }
        if (IsSink(sEntry))
            sftpd->FileInfo.ByAddr.vmfile.SeqLen = 0;
        return (0);
    }

    /* Determine open flags */
    if (IsSink(sEntry)) {
        omode  = sftpd->FileInfo.ByName.ProtectionBits;
        oflags = O_WRONLY;
        if (sftpd->SeekOffset <= 0)
            oflags |= O_TRUNC;
        if (sftpd->Tag == FILEBYNAME)
            oflags |= O_CREAT;
    } else {
        omode  = 0;
        oflags = O_RDONLY;
    }
    oflags |= O_BINARY;

    switch (sftpd->Tag) {
    case FILEBYNAME:
        sEntry->openfd =
            open(sftpd->FileInfo.ByName.LocalFileName, oflags, omode);
        sEntry->fd_offset = 0;
        if (sEntry->openfd < 0) {
            if (RPC2_Perror)
                perror(sftpd->FileInfo.ByName.LocalFileName);
            return (-1);
        }
        break;

    case FILEBYFD:
        /* trust the user to have given a good fd!
         * However the fd might be shared, so we actually need to re-seek to
         * the fileoffset that we expect on every operation, so we have to know
         * where we currently are */
        sEntry->openfd    = dup(sftpd->FileInfo.ByFD.fd);
        sEntry->fd_offset = lseek(sEntry->openfd, 0, SEEK_CUR);
        break;

#ifdef HAVE_IOPEN
    case FILEBYINODE:
        sEntry->openfd    = iopen(sftpd->FileInfo.ByInode.Device,
                                  sftpd->FileInfo.ByInode.Inode, oflags);
        sEntry->fd_offset = 0;
        if (sEntry->openfd < 0) {
            if (RPC2_Perror)
                perror("iopen");
            return (-1);
        }
        break;
#endif

    default:
        return (-1);
    }

    if ((int32_t)sftpd->SeekOffset >= 0) {
        sEntry->fd_offset = (off_t)sftpd->SeekOffset;
        (void)lseek(sEntry->openfd, sEntry->fd_offset, SEEK_SET);
    }

    return (0);
}

void sftp_UpdateRTT(RPC2_PacketBuffer *pb, struct SFTP_Entry *sEntry,
                    unsigned long inbytes, unsigned long outbytes)
/*
  Updates the round trip time estimate and variance in sEntry->HostInfo
  using the observation in pb->Header.TimeEcho, Called by AckArrived and
  DataArrived.
*/
{
    unsigned long obs;

    if (!pb->Header.TimeEcho)
        return;

    /* We only get reliable RTT data at the receiving side. -JH */
    if (IsSink(sEntry)) {
        sftp_rttupdates++;

        TVTOTS(&pb->Prefix.RecvStamp, obs);
        obs = TSDELTA(obs, pb->Header.TimeEcho);
        RPC2_UpdateEstimates(sEntry->HostInfo, obs, inbytes, outbytes);
    }

    return;
}

void sftp_UpdateBW(RPC2_PacketBuffer *pb, unsigned long inbytes,
                   unsigned long outbytes, struct SFTP_Entry *sEntry)
/* Updates the bandwidth estimate using the data in the packetbuffer
   and the amount of data in bytes. Called by AckArrived and DataArrived.
*/
{
    unsigned long obs;
    RPC2_NetLogEntry entry;

    if (!pb->Header.TimeEcho)
        return;

    TVTOTS(&pb->Prefix.RecvStamp, obs);
    obs = TSDELTA(obs, pb->Header.TimeEcho);
    RPC2_UpdateEstimates(sEntry->HostInfo, obs, inbytes, outbytes);

    obs /= 1000;
    if ((long)obs <= 0)
        obs = 1;

    entry.Tag                        = RPC2_MEASURED_NLE;
    entry.Value.Measured.Conn        = sEntry->LocalHandle;
    entry.Value.Measured.Bytes       = inbytes + outbytes;
    entry.Value.Measured.ElapsedTime = obs;
    (void)rpc2_AppendHostLog(sEntry->HostInfo, &entry, SE_MEASUREMENT);
    say(1 /*4*/, SFTP_DebugLevel,
        "sftp_UpdateBW: conn %#x, %ld inbytes, %ld outbytes, %ld ms\n",
        sEntry->LocalHandle, inbytes, outbytes, obs);
}

/* -------------------- Sink Side Common Routines -------------------- */

int sftp_DataArrived(RPC2_PacketBuffer *pBuff, struct SFTP_Entry *sEntry)
/* Handles packet pBuff on a connection whose state is sEntry.
   Invokes write strategy routine on all packets that request ack.
   Sets XferState in sEntry to XferInProgress if this file
   transfer is not over yet, and to XferCompleted if it is over.
   Returns 0 if normal, -1 if fatal error of some kind occurred.  */
{
    long moffset; /* bit position of TheseBits corresponding to pBuff */
    long i, j;

    if (sEntry->SentParms == FALSE && sEntry->WhoAmI == SFSERVER)
        sEntry->SentParms =
            TRUE; /* this data packet is evidence that my Start got to the client */

    sftp_datar++;
    sftp_Recvd.Datas++;

    say(/*9*/ 4, SFTP_DebugLevel, "R-%u [%u] {%d} %s%s\n",
        pBuff->Header.SeqNumber, pBuff->Header.TimeStamp,
        pBuff->Header.TimeEcho, (pBuff->Header.SEFlags & SFTP_FIRST) ? "F" : "",
        (pBuff->Header.Flags & SFTP_ACKME) ? "A" : "");

    if ((SFTP_MaxPackets > 0) && (sftp_PacketsInUse > SFTP_MaxPackets)) {
        /* Drop this packet.  Since the packet could have been dropped
           by the kernel or the net, the SFTP code will still function
           correctly.  The effect of dropping this packet is thus to make
           the network appear rather lossy.  WARNING: we may end up with
           SEFAIL2s because of this, since packet buffers are a shared
           resource for which there is now contention.  Instead of a
           deadlock, we will end up with a timeout.  More sophisticated
           contention resolution algorithms are a pain and not likely to
           be really useful.  */
        sftp_starved++;
        SFTP_FreeBuffer(&pBuff);
        return (0);
    }

    moffset = (long)pBuff->Header.SeqNumber - (long)sEntry->RecvLastContig;

    if (moffset > (long)sEntry->WindowSize) {
        BOGOSITY(sEntry, pBuff);
        return (-1);
    }

    if (moffset <= 0 || TESTBIT(sEntry->RecvTheseBits, moffset)) {
        /* we have already seen this packet */
        sftp_duplicates++;
        sEntry->DupsSinceAck++;
        sftp_Recvd.DataRetries++;

        /* If we are the (passive) server side of the transfer, and we get
         * either an ACKME or have seen too many retransmitted data packets,
         * we assume our ACKs may have gotten lost. Send an ACK back so that
         * the sender may advance its window.
         * If we are the server we only want to respond with an ACK if we
         * were able to advance the window, we will timeout and retransmit
         * an ACK if necessary. -JH */
        if (sEntry->WhoAmI == SFSERVER &&
            ((pBuff->Header.Flags & SFTP_ACKME) ||
             sEntry->DupsSinceAck > sEntry->DupThreshold)) {
            sftp_SendAck(sEntry);
            /* we need write here 'cause we may not flush buffers otherwise */
            if (sftp_WriteStrategy(sEntry) < 0)
                return (-1);
            sEntry->DupsSinceAck = 0;
        }
        SFTP_FreeBuffer(&pBuff);
        return (0);
    }

#if 0
    /* Harvest the RTT observation if this is the first packet. (And try to
     * get an estimate of the amount of bytes transferred on this roundtrip) */
    if (pBuff->Header.SEFlags & SFTP_FIRST)
	sftp_UpdateRTT(pBuff, sEntry, pBuff->Prefix.LengthOfPacket,
				      sizeof(struct RPC2_PacketHeader));
#endif

    sEntry->RecvSinceAck++;

#if 0
    /* I thought we needed: (pBuff->Header.TimeStamp > sEntry->TimeEcho), but
     * then the TimeEcho returned on the next ack doesn't encompass the amount
     * of data the source sent between the previous ack and this one. -JH */
    if (pBuff->Header.SeqNumber == sEntry->RecvLastContig+1)
	sEntry->TimeEcho = pBuff->Header.TimeStamp;
    else
	sEntry->TimeEcho = 0;
#else
    if (sEntry->TimeEcho < pBuff->Header.TimeStamp)
        sEntry->TimeEcho = pBuff->Header.TimeStamp;
#endif

    sEntry->XferState =
        XferInProgress; /* this is how it gets turned on in Client for fetch */
    SETBIT(sEntry->RecvTheseBits, moffset);
    pBuff->Header.SEFlags &=
        ~SFTP_COUNTED; /* might have been set on the other side */

    if (pBuff->Header.SeqNumber > sEntry->RecvMostRecent)
        sEntry->RecvMostRecent = pBuff->Header.SeqNumber;
    j                       = PBUFF(pBuff->Header.SeqNumber);
    sEntry->ThesePackets[j] = pBuff;

    /* ackme flag is set? */
    if (pBuff->Header.Flags & SFTP_ACKME) {
        if (pBuff->Header.TimeEcho) {
            /* update bandwidth estimate.  pBuff->Header.TimeEcho is the
               timestamp from the last ack we sent.  figure out how much
               data we have seen */
            RPC2_PacketBuffer *pb;
            unsigned long dataThisRound = 0;
            for (i = 1; sEntry->RecvLastContig + i <= sEntry->RecvMostRecent;
                 i++)
                if (TESTBIT(sEntry->RecvTheseBits, i)) {
                    pb =
                        sEntry
                            ->ThesePackets[PBUFF((sEntry->RecvLastContig + i))];
                    if (pb->Header.TimeEcho >= pBuff->Header.TimeEcho &&
                        !(pb->Header.SEFlags & SFTP_COUNTED)) {
                        dataThisRound += pb->Prefix.LengthOfPacket;
                        pb->Header.SEFlags |= SFTP_COUNTED;
                    }
                }
            if (dataThisRound)
                /* XXX The dataThisRound value is NOT equal to the data that
                 * was actually sent if we sent duplicate packets! -JH */
                sftp_UpdateBW(pBuff, dataThisRound,
                              sizeof(struct RPC2_PacketHeader), sEntry);
        }
    }

    /* we haven't sent an ack for a while, but did see a lot of data packets
     * flying by? Send a gratitious ack reply */
    if ((pBuff->Header.Flags & SFTP_ACKME) ||
        (sEntry->WhoAmI == SFSERVER &&
         sEntry->RecvSinceAck >= sEntry->WindowSize)) {
        sftp_SendAck(sEntry);

        /* WriteStrategy may modify RecvLastContig and RecvTheseBits */
        if (sftp_WriteStrategy(sEntry) < 0)
            return (-1);
    }

    /* Is this the last packet for the file? */
    if ((pBuff->Header.SEFlags & SFTP_MOREDATA) == FALSE) {
        sEntry->HitEOF = TRUE;
    } else {
        if (sEntry->HitEOF == FALSE)
            return (0);
    }
    /* else this must be a non-EOF packet after EOF packet was received */

    /* We have seen very last packet of this file (RecvMostRecent),
       but are all preceding packets in? */
    for (i = 1; sEntry->RecvLastContig + i <= sEntry->RecvMostRecent; i++)
        if (!TESTBIT(sEntry->RecvTheseBits, i))
            return (0);

    /* Yes, we did receive every packet! */
    if (sftp_WriteStrategy(sEntry) < 0)
        return (-1); /* one last time */
    sEntry->XferState = XferCompleted;
    sftp_vfclose(sEntry);
    return (0);
}

int sftp_WriteStrategy(struct SFTP_Entry *sEntry)
/* Strategy routine to write packets to disk.  Defers write until
   a contiguous set of packets starting at RecvLastContig has been
   collected Writes them all out in one fell swoop, then bumps
   counters.

   Returns 0 if normal, -1 if fatal error of some kind occurred.
   Marks connection DISKERROR on failure.  */
{
    RPC2_PacketBuffer *pb;
    struct iovec iovarray[MAXOPACKETS];
    long i, iovlen, bytesnow;

    iovlen   = 0;
    bytesnow = 0;
    for (i = 1; i < MAXOPACKETS + 1; i++) {
        long x;

        if (!TESTBIT(sEntry->RecvTheseBits, i))
            break;

        pb = sEntry->ThesePackets[PBUFF((sEntry->RecvLastContig + i))];
        iovarray[i - 1].iov_base = (caddr_t)pb->Body;

        x = sEntry->SDesc->Value.SmartFTPD.BytesTransferred + bytesnow;
        if (SFTP_EnforceQuota && sEntry->SDesc->Value.SmartFTPD.ByteQuota > 0 &&
            (x + pb->Header.BodyLength) >
                sEntry->SDesc->Value.SmartFTPD.ByteQuota) {
            sEntry->SDesc->Value.SmartFTPD.QuotaExceeded = 1;
            iovarray[i - 1].iov_len =
                sEntry->SDesc->Value.SmartFTPD.ByteQuota - x;
            /* may result in 0 len for trailing packets after the
               one exceeding the quota */
        } else
            iovarray[i - 1].iov_len = pb->Header.BodyLength;

        bytesnow += iovarray[i - 1].iov_len;
        iovlen++;
    }
    if (iovlen == 0)
        return (0); /* 0-length initial run of packets */

    if (bytesnow != sftp_vfwritev(sEntry, iovarray, iovlen)) {
        sftp_SetError(sEntry, DISKERROR); /* probably disk full */
        say(1, SFTP_DebugLevel, "WriteStrategy: write failed\n");
        return (-1);
    }

    for (i = sEntry->RecvLastContig + 1;
         i < sEntry->RecvLastContig + iovlen + 1; i++)
        SFTP_FreeBuffer(&sEntry->ThesePackets[PBUFF(i)]);
    sEntry->RecvLastContig += iovlen;
    B_ShiftLeft(sEntry->RecvTheseBits, iovlen);

    sftp_Progress(sEntry->SDesc,
                  sEntry->SDesc->Value.SmartFTPD.BytesTransferred + bytesnow);

    return (0);
}

static void sftp_SendAck(struct SFTP_Entry *sEntry)
/* Send out an ack for the current state of sEntry.  The ack will
   have GotEmAll as high as possible (leading 1's are gobbled)

   Returns 0 if normal, -1 if fatal error of some kind occurred.  */
{
    RPC2_PacketBuffer *pb;
    long i, shiftlen;
    unsigned int btemp[BITMASKWIDTH], now;
    int confirm = 1;

    sftp_acks++;
    sftp_Sent.Acks++;

    SFTP_AllocBuffer(0, &pb);
    sftp_InitPacket(pb, sEntry, 0);
    pb->Header.SeqNumber = ++(sEntry->CtrlSeqNumber);
    pb->Header.Opcode    = SFTP_ACK;
    pb->Header.GotEmAll  = sEntry->RecvLastContig;

    now                  = rpc2_MakeTimeStamp();
    pb->Header.TimeStamp = now;
#ifdef VERY_FAST_SERVERS
    pb->Header.TimeEcho =
        VALID_TIMEECHO(sEntry) ?
            sEntry->TimeEcho + ((long)TSDELTA(now, sEntry->RequestTime)) :
            0;
#else
    /* The sftp protocol seems to like it better if we do not subtract
     * processing time (only affects network BW estimate when clients are
     * pushing more data than the servers can handle) */
    pb->Header.TimeEcho = VALID_TIMEECHO(sEntry) ? sEntry->TimeEcho : 0;
#endif

    if (sEntry->Retransmitting == TRUE) {
        sEntry->Retransmitting = FALSE;
        confirm                = 0;
    }

    B_Assign(btemp, sEntry->RecvTheseBits);

    /* Bump GotEmAll here; this allows write to disk to occur after ack is sent */
    shiftlen = 0;
    for (i = 1; i <= sEntry->WindowSize; i++)
        if (TESTBIT(btemp, i)) {
            pb->Header.GotEmAll++;
            shiftlen++;
        } else
            break;
    if (shiftlen > 0)
        B_ShiftLeft(btemp, shiftlen);
    B_CopyToPacket(btemp, pb);
    rpc2_htonp(pb);
    sftp_XmitPacket(sEntry, pb, confirm);
    sEntry->RecvSinceAck = 0;

    say(/*9*/ 4, SFTP_DebugLevel, "A-%lu [%lu] {%lu} %lu\n",
        (unsigned long)ntohl(pb->Header.SeqNumber),
        (unsigned long)ntohl(pb->Header.TimeStamp),
        (unsigned long)ntohl(pb->Header.TimeEcho),
        (unsigned long)ntohl(pb->Header.GotEmAll));
    SFTP_FreeBuffer(&pb);
    return;
}

/* -------------------- Source Side Common Routines -------------------- */

int sftp_AckArrived(RPC2_PacketBuffer *pBuff, struct SFTP_Entry *sEntry)
/*
   Ack pBuff arrived on connection whose state is in sEntry.
   Returns 0 if normal, -1 if fatal error of some kind occurred.
*/
{
    long prun, i;
    unsigned long dataThisRound = 0;
    RPC2_PacketBuffer *pb;

    sftp_ackr++;
    sftp_Recvd.Acks++;
    say(/*9*/ 4, SFTP_DebugLevel, "A-%u [%u] {%u} %u\n",
        pBuff->Header.SeqNumber, pBuff->Header.TimeStamp,
        pBuff->Header.TimeEcho, pBuff->Header.GotEmAll);

    /* calculate length of initial run of acked packets */
    prun = pBuff->Header.GotEmAll - sEntry->SendLastContig;

    if (prun < 0) /* Out-of-sequence Ack, probably */
        return (0);

    /* duplicate ack arrived, if we are the active side of the connection
     * this means we were too eager and sent unnecessary duplicate data.
     * Don't make things worse, we already sent data when we got the ack
     * the first time. */
    if (prun == 0 && sEntry->WhoAmI == SFCLIENT)
        return 0;

    if (prun > sEntry->SendMostRecent - sEntry->SendLastContig) {
        BOGOSITY(sEntry, pBuff);
        return (-1);
    }

    /*  update the RTT estimate if
     *
     * 1. the timestamp-echo field is set * (so we can live with SFTPs
     * without timestamps)
     *
     * 2. if the caller is a client (client to server transfer), the
     * ack is not a trigger. If the caller is a server, the ack can't
     * be a trigger.
     *
     * We might want to add another condition:
     * 3. the timer is not backed off (retransmitting)
     *
     * This condition is part of Karn's algorithm for dealing with
     * ambiguous acks.  But with timestamps, observations are
     * unambiguous.  If we cancel backoff, and the current RTT is low,
     * it will converge to a larger value eventually.  The current TCP
     * code (4.4free+ext) does this.  */
    if (pBuff->Header.TimeEcho && (sEntry->WhoAmI != SFCLIENT ||
                                   !(pBuff->Header.SEFlags & SFTP_TRIGGER))) {
        sftp_UpdateRTT(pBuff, sEntry, pBuff->Prefix.LengthOfPacket, /* ack */
                       /* data */ sEntry->PacketSize +
                           sizeof(struct RPC2_PacketHeader));

        /*  Update the bandwidth estimate.  To determine the amount of
         * useful data involved, first look at the initial run of
         * acked packets, from SendLastContig+1 to GotEmAll.  Then
         * look at packets represented in the bitmask.  Note these,
         * unlike the received packets, are in network order!!  */
        for (i = sEntry->SendLastContig + 1; i <= pBuff->Header.GotEmAll; i++) {
            pb = sEntry->ThesePackets[PBUFF(i)];
            if (!(ntohl(pb->Header.SEFlags) & SFTP_COUNTED))
                dataThisRound += pb->Prefix.LengthOfPacket;
        }

        for (i = 1; i <= sizeof(int) * BITMASKWIDTH; i++)
            if (TESTBIT(&pBuff->Header.BitMask0, i)) {
                pb = sEntry->ThesePackets[PBUFF(pBuff->Header.GotEmAll + i)];
                if (!(ntohl(pb->Header.SEFlags) & SFTP_COUNTED) &&
                    (pBuff->Header.TimeEcho <= ntohl(pb->Header.TimeStamp))) {
                    dataThisRound += pb->Prefix.LengthOfPacket;
                    pb->Header.SEFlags =
                        htonl(ntohl(pb->Header.SEFlags | SFTP_COUNTED));
                }
            }

        if (dataThisRound)
            /* XXX This is bogus, the successfully acked data is NOT equal to
             * the data we actually transferred over the wire! -JH */
            sftp_UpdateBW(pBuff, sizeof(struct RPC2_PacketHeader),
                          dataThisRound, sEntry);
    }

    /* grab the timestamp because we're going to send more data. */
    sEntry->TimeEcho = pBuff->Header.TimeStamp;

    /* Update counters to match other side */
    sEntry->SendLastContig = pBuff->Header.GotEmAll;
    B_CopyFromPacket(pBuff, sEntry->SendTheseBits);

    /* Release prefix sequence of packets received by other side */
    /* acked non-prefix packets are still kept, even though not needed */
    for (i = 0; i < prun; i++)
        SFTP_FreeBuffer(
            &sEntry->ThesePackets[PBUFF((sEntry->SendLastContig - i))]);

    /* Do we have more work to do? */
    if (sEntry->HitEOF && sEntry->ReadAheadCount == 0 &&
        sEntry->SendMostRecent ==
            sEntry->SendLastContig) { /* I have nothing more to send, and peer
                                         has seen all I have sent */
        sEntry->XferState = XferCompleted;
        return (0);
    } else {
        sEntry->XferState = XferInProgress;
    }

    /* Send more data */
    if (sftp_SendStrategy(sEntry) < 0)
        return (-1);

    /* There must be at least ONE unacked packet at this point */
    assert(sEntry->SendMostRecent > sEntry->SendLastContig);

    return (0);
}

int sftp_SendStrategy(struct SFTP_Entry *sEntry)
/* Send packets out on connection whose state is in sEntry.
   Returns 0 if normal, -1 if fatal error of some kind occurred.
*/
{
    int winopen, worried = 0;

    /* On entry to this routine there are four sets of packets for us
       to consider:

       1. Worried set: these are the packets for which an Ack has
       been requested but not received, and a retransmission interval
       has passed.

       2. NeedAck set: these are packets for which an Ack has been
       requested but not received, and a retransmission interval has
       not yet passed.

       3. InTransit set: these are the packets which have been sent
       out, but for which an Ack has not yet been requested.

       4. SendAhead set: these are packets which have been read in
       from the disk and are ready to be sent out.

       Next time's NeedAck set will include this time's InTransit set.
       Next time's InTransit set will be this time's SendAhead.

       We treat each of these sets as a separate class:

       1. We retransmit EVERY packet in the Worried set; ask for an
       ack for the last one only if the SendAhead set is empty. This
       could occur on EOF or if max window size is reached.
       Otherwise do not ask for any acks.

       2. We do not do anything in the NeedAck or InTransit sets.

       3. We transmit all packets in the SendAhead set and request an
       ack for the one at sftp_AckPoint.

       Returns 0 if normal, -1 if fatal error of some kind occurred.

       Added (JH '98): if Window is full, send only the first packet
       and ask for an ack; this serves as implicit flow-control and
       prevents connections from breaking.

       (JH '03): This still seems to be insufficient. When a router
       is loaded it is more likely to drop packet that arrive later.
       The combination of the last packet containing the ACKME flag
       and the higher chance of losing that packet creates a very
       abysmal situation where a transfer that is larger than the
       router queue length and shorter than a full window size can
       time out without ever getting the packets with ACKME flags to
       the other side (~16 packets in the case I'm looking at)

       Proposed solution, set ackme on the first retransmitted packet
       in the worried set, keep sending the other packets but do not
       include further ackme flags. This way the other side will send
       back a selective ack for any received packets from the worried
       set and we can advance the window. The next transmission will
       contain the first lost packet and the new sendahead, the ack
       will then identify which packets were received in the same
       window after the previous ACKME
    */

    sftp_TraceStatus(sEntry, 3, __LINE__);
    FT_GetTimeOfDay(&sEntry->LastSS, 0); /* remember this invocation */

    /* Prime sendahead set, if necessary */
    if (sEntry->ReadAheadCount == 0) {
        /* we could have been at max window size, or be starting */
        if (sftp_ReadStrategy(sEntry) < 0)
            return (-1); /* non-overlapped */
    }

    /* Obtain window status */
    winopen = WinIsOpen(sEntry);

    /* Sanity check */
    assert(sEntry->ReadAheadCount > 0 || sEntry->HitEOF || !winopen);

    /* see if we should be worried about anything new yet */
    if (sEntry->WhoAmI == SFCLIENT || sEntry->Retransmitting)
        worried = CheckWorried(sEntry);

    /*  If there is no more new data to send, this call is for
     * retransmission.  Ensure progress is made by sending the first
     * unacked packet.  This should be considered a last-ditch effort
     * at getting a response from the receiver, when all looks
     * hopeless.  As such, the packet should not be sent unless it is
     * in the worried set.  We are guaranteed forward progress because
     * the retransmission timer will expire eventually, and the packet
     * will be placed in the worried set.  The first unacked packet is
     * SendLastContig+1.  */
    if (!winopen) {
        /* Window is closed: try to send the oldest worried packet */
        sftp_windowfulls++;
        return SendFirstUnacked(sEntry, 0);
    }

    /* Window is open: be more ambitious */
    if (sEntry->ReadAheadCount > 0) {
        /* If we're starting to get worried about packets, send the first
         * unacknowledged packet in the worried set. If that doesn't fix
         * it, we will hit the end of the send window, and start sending
         * the complete worried set anyways. -JH */
        if (worried && SendFirstUnacked(sEntry, 1) < 0)
            return (-1);

        return SendSendAhead(sEntry); /* may close window */
    }

    /* Hit EOF, try to flush the last packets to the other side. We should be
     * able to be more aggressive compared to the case where we're in the middle
     * of the transfer with a closed window */
    return ResendWorried(sEntry);
}

static int CheckWorried(struct SFTP_Entry *sEntry)
/* Check the packets from SendWorriedLimit to SendAckLimit, and
   see if we should be Worried about any of them.  */
{
    long i;
    unsigned long now, then;
    RPC2_PacketBuffer *thePacket;
    struct CEntry *ce;
    uint32_t queued, rto;
    struct timeval tv;

    ce = rpc2_GetConn(sEntry->LocalHandle);
    if (!ce || sEntry->HitEOF) {
        sEntry->SendWorriedLimit = sEntry->SendAckLimit;
        return 1;
    }

    if (sEntry->SendWorriedLimit < sEntry->SendLastContig)
        sEntry->SendWorriedLimit = sEntry->SendLastContig;

    TVTOTS(&sEntry->LastSS, now);
    queued = (sEntry->PacketSize + sizeof(struct RPC2_PacketHeader)) *
             (sEntry->SendAckLimit - sEntry->SendLastContig);

    for (i = sEntry->SendAckLimit; i > sEntry->SendWorriedLimit; i--) {
        queued -= sEntry->PacketSize + sizeof(struct RPC2_PacketHeader);
        if (TESTBIT(sEntry->SendTheseBits, i - sEntry->SendLastContig))
            continue;

        rpc2_RetryInterval(ce, 0, &tv, queued, sizeof(struct RPC2_PacketHeader),
                           1);
        rto = tv.tv_sec * 1000000 + tv.tv_usec;

        /* check the timestamp and see if a timeout interval has
           occurred, if so let's start thinking about retransmitting
           the packet */
        thePacket = sEntry->ThesePackets[PBUFF(i)];
        if (!thePacket)
            continue;

        then = ntohl(thePacket->Header.TimeStamp);
        if ((long)TSDELTA(now, then) > rto) {
            say(4, SFTP_DebugLevel,
                "Worried packet %ld, sent %lu, (%lu msec ago)\n", i, then,
                (long)TSDELTA(now, then));
            break;
        }
    }
    sEntry->SendWorriedLimit = i;
    say(/*9*/ 4, SFTP_DebugLevel,
        "LastContig = %d, Worried = %d, AckLimit = %d, MostRecent = %d\n",
        sEntry->SendLastContig, sEntry->SendWorriedLimit, sEntry->SendAckLimit,
        sEntry->SendMostRecent);

    return (sEntry->SendWorriedLimit > sEntry->SendLastContig);
}

static int ResendWorried(struct SFTP_Entry *sEntry)
/* Resend worried set for sEntry.
   Side effects: flag settings on resent packets
   Returns 0 if normal, -1 if fatal error of some kind occurred.  */
{
    RPC2_PacketBuffer *pb;
    long i;
    unsigned long now;
    int acked = 0;

    /* Now send them out */
    for (i = sEntry->SendLastContig + 1; i <= sEntry->SendWorriedLimit; i++) {
        if (!TESTBIT(sEntry->SendTheseBits, i - sEntry->SendLastContig)) {
            pb               = sEntry->ThesePackets[PBUFF(i)];
            pb->Header.Flags = ntohl(pb->Header.Flags);
            if (pb->Header.Flags & SFTP_ACKME)
                sftp_ackslost++;
            if (acked) {
                pb->Header.Flags &= ~SFTP_ACKME;
            } else {
                /* Demand ack for first of the worried set */
                pb->Header.Flags |= SFTP_ACKME;
                acked = 1;
            }
            if (i == sEntry->SendLastContig + 1) { /* first packet */
                pb->Header.SEFlags = ntohl(pb->Header.SEFlags);
                pb->Header.SEFlags |= SFTP_FIRST;
                pb->Header.SEFlags = htonl(pb->Header.SEFlags);
            }
            pb->Header.Flags |= RPC2_RETRY;

            sftp_Sent.Datas++;
            sftp_Sent.DataRetries++;
            sftp_datas++;
            sftp_retries++;

            pb->Header.Flags = htonl(pb->Header.Flags);

            now                  = rpc2_MakeTimeStamp();
            pb->Header.TimeStamp = htonl(now);
#ifdef VERY_FAST_SERVERS
            pb->Header.TimeEcho =
                VALID_TIMEECHO(sEntry) ?
                    htonl(sEntry->TimeEcho +
                          (long)TSDELTA(now, sEntry->RequestTime)) :
                    htonl(0);
#else
            pb->Header.TimeEcho =
                VALID_TIMEECHO(sEntry) ? htonl(sEntry->TimeEcho) : htonl(0);
#endif

            say(/*9*/ 4, SFTP_DebugLevel, "Worried S-%lu [%lu] {%lu}\n",
                (unsigned long)ntohl(pb->Header.SeqNumber),
                (unsigned long)ntohl(pb->Header.TimeStamp),
                (unsigned long)ntohl(pb->Header.TimeEcho));
            sftp_XmitPacket(sEntry, pb, 0);
        }
    }

    return (0);
}

static int SendFirstUnacked(struct SFTP_Entry *sEntry, int winopen)
{
    /* Note: it's better to resend the first unacked rather than last
       unacked, because it will narrow the window faster than the
       latter */

    RPC2_PacketBuffer *pb;
    unsigned long now;

    /* By definition, SendLastContig+1 is first unacked pkt */
    pb = sEntry->ThesePackets[PBUFF(sEntry->SendLastContig + 1)];

    /* Resend it */
    pb->Header.Flags = ntohl(pb->Header.Flags);
    if (winopen) { /* expecting to send more to advance the window */
        if (pb->Header.Flags & SFTP_ACKME)
            sftp_ackslost++;
        pb->Header.Flags &= ~(SFTP_ACKME);
    } else { /* resending only a single packet (to reduce queueing) */
        pb->Header.Flags |= SFTP_ACKME;
    }
    pb->Header.Flags |= RPC2_RETRY;
    pb->Header.SEFlags = ntohl(pb->Header.SEFlags);
    pb->Header.SEFlags |= SFTP_FIRST;

    sftp_Sent.Datas++;
    sftp_Sent.DataRetries++;
    sftp_datas++;
    sftp_retries++;

    pb->Header.Flags   = htonl(pb->Header.Flags);
    pb->Header.SEFlags = htonl(pb->Header.SEFlags);

    now                  = rpc2_MakeTimeStamp();
    pb->Header.TimeStamp = htonl(now);
#ifdef VERY_FAST_SERVERS
    pb->Header.TimeEcho =
        VALID_TIMEECHO(sEntry) ?
            htonl(sEntry->TimeEcho + (long)TSDELTA(now, sEntry->RequestTime)) :
            htonl(0);
#else
    pb->Header.TimeEcho = VALID_TIMEECHO(sEntry) ? htonl(sEntry->TimeEcho) :
                                                   htonl(0);
#endif

    say(/*9*/ 4, SFTP_DebugLevel, "First Unacked S-%lu [%lu] {%lu}\n",
        (unsigned long)ntohl(pb->Header.SeqNumber),
        (unsigned long)ntohl(pb->Header.TimeStamp),
        (unsigned long)ntohl(pb->Header.TimeEcho));
    sftp_XmitPacket(sEntry, pb, 0);
    return (1);
}

static int SendSendAhead(struct SFTP_Entry *sEntry)
/* Send out SendAhead set, adds to the InTransit set and requests
   ack for AckPoint packet.  Caller should ensure that sending
   ReadAheadCount packets will not cause WindowSize to be
   exceeded.  Sets SendAckLimit always Sets SendMostRecent and
   ReadAheadCount if something gets sent Returns 0 if normal, -1
   if fatal error of some kind occurred.  */
{
    RPC2_PacketBuffer *pb;
    long i, j;
    unsigned long now;
    int dont_ackme;

    if (sEntry->ReadAheadCount ==
        0) { /* Nothing to send; but caller expects need ack limit to be set */
        sEntry->SendAckLimit = sEntry->SendMostRecent;
        return (0);
    }

    /* try to avoid generating data when we're not about to send a full
     * window's worth */
    dont_ackme = ((sEntry->ReadAheadCount < sEntry->SendAhead) &&
                  (rpc2_MorePackets() != -1));

    /* j is the packet to be acked */
    if (sEntry->AckPoint > sEntry->ReadAheadCount)
        j = sEntry->SendMostRecent + sEntry->ReadAheadCount; /* last one */
    else
        j = sEntry->SendMostRecent + sEntry->AckPoint;

    for (i = 0; i < sEntry->ReadAheadCount; i++) {
        sEntry->SendMostRecent++;
        pb = sEntry->ThesePackets[PBUFF((sEntry->SendMostRecent))];
        if (!dont_ackme &&
            sEntry->SendMostRecent == j) { /* Middle packet: demand ack */
            sEntry->SendAckLimit = sEntry->SendMostRecent;
            pb->Header.Flags     = ntohl(pb->Header.Flags);
            pb->Header.Flags |= SFTP_ACKME;
            pb->Header.Flags = htonl(pb->Header.Flags);
        }
        if (i == 0 &&
            sEntry->SendLastContig ==
                sEntry->SendWorriedLimit) { /* mark first packet only if no
                                               worried packets sent */
            pb->Header.SEFlags = ntohl(pb->Header.SEFlags);
            pb->Header.SEFlags |= SFTP_FIRST;
            pb->Header.SEFlags = htonl(pb->Header.SEFlags);
        }
        sftp_Sent.Datas++;
        sftp_datas++;

        now                  = rpc2_MakeTimeStamp();
        pb->Header.TimeStamp = htonl(now);
#ifdef VERY_FAST_SERVERS
        pb->Header.TimeEcho =
            VALID_TIMEECHO(sEntry) ?
                htonl(sEntry->TimeEcho +
                      (long)TSDELTA(now, sEntry->RequestTime)) :
                htonl(0);
#else
        pb->Header.TimeEcho = VALID_TIMEECHO(sEntry) ? htonl(sEntry->TimeEcho) :
                                                       htonl(0);
#endif

        sftp_XmitPacket(sEntry, pb, 1);
        say(/*9*/ 4, SFTP_DebugLevel, "S-%lu [%lu] {%lu}\n",
            (unsigned long)ntohl(pb->Header.SeqNumber),
            (unsigned long)ntohl(pb->Header.TimeStamp),
            (unsigned long)ntohl(pb->Header.TimeEcho));
    }

    sEntry->ReadAheadCount = 0; /* we have eaten all of them */
    return (0);
}

int sftp_ReadStrategy(struct SFTP_Entry *sEntry)
/* On entry, assumes SendMostRecent is the last packet read so
   far.  Fills SendAhead buffers from disk in one fell swoop.  If
   EOF is seen, sets HitEOF and sets unfilled packet pointers to
   NULL.  Does not read in any packets if WindowSize would be
   exceeded.  Sets ReadAheadCount to the number of packets
   actually read in.  Returns 0 if normal, -1 if fatal error of
   some kind occurred.  */
{
    RPC2_PacketBuffer *pb;
    struct iovec iovarray[MAXOPACKETS];
    long i, byteswanted, bytesread, j;
    int bodylength;

    if (sEntry->HitEOF)
        return (0);
    if (!WinIsOpen(sEntry))
        return (0);

    /* Be optimistic: assume you won't hit EOF */
    bodylength  = sEntry->PacketSize - sizeof(struct RPC2_PacketHeader);
    byteswanted = sEntry->SendAhead * bodylength; /* what we expect normally */
    for (i = 1; i < 1 + sEntry->SendAhead; i++) {
        SFTP_AllocBuffer(bodylength, &pb);
        sftp_InitPacket(pb, sEntry, bodylength); /* BodyLength set */
        pb->Header.Flags     = 0;
        pb->Header.SEFlags   = SFTP_MOREDATA; /* tentative assumption */
        pb->Header.Opcode    = SFTP_DATA;
        pb->Header.SeqNumber = sEntry->SendMostRecent + i;
        rpc2_htonp(pb);

        j                        = PBUFF((sEntry->SendMostRecent + i));
        sEntry->ThesePackets[j]  = pb;
        iovarray[i - 1].iov_base = (caddr_t)pb->Body;
        iovarray[i - 1].iov_len  = bodylength;
    }

    /* Read in one fell swoop */
    bytesread = sftp_vfreadv(sEntry, iovarray, sEntry->SendAhead);
    if (bytesread < 0) {
        BOGOSITY(sEntry, 0);
        perror("sftp_vfreadv");
        return (-1);
    }

    /* If ByteQuota exceeded make it appear like an EOF */
    if (SFTP_EnforceQuota && sEntry->SDesc->Value.SmartFTPD.ByteQuota > 0 &&
        (sEntry->SDesc->Value.SmartFTPD.BytesTransferred + bytesread) >
            sEntry->SDesc->Value.SmartFTPD.ByteQuota) {
        sEntry->SDesc->Value.SmartFTPD.QuotaExceeded = 1;
        bytesread = sEntry->SDesc->Value.SmartFTPD.ByteQuota -
                    sEntry->SDesc->Value.SmartFTPD.BytesTransferred;
    }

    /* Update BytesTransferred field */
    sftp_Progress(sEntry->SDesc,
                  sEntry->SDesc->Value.SmartFTPD.BytesTransferred + bytesread);

    /* Did we hit EOF? */
    if (bytesread == byteswanted) { /* EOF not seen yet */
        sEntry->ReadAheadCount = sEntry->SendAhead;
        if (!sEntry->sa->encrypt &&
            sEntry->PInfo.SecurityLevel == RPC2_SECURE) {
            /* Encrypt all packets here */
            for (i = 1; i < 1 + sEntry->SendAhead; i++) {
                j  = PBUFF((sEntry->SendMostRecent + i));
                pb = sEntry->ThesePackets[j];
                sftp_Encrypt(pb, sEntry);
                pb->Header.Flags =
                    htonl(ntohl(pb->Header.Flags) | RPC2_ENCRYPTED);
            }
        }
        return (0);
    }

    /* Alas, we did  */
    sEntry->HitEOF = TRUE;
    for (i = 1; i < sEntry->SendAhead + 1; i++) {
        if (bytesread > iovarray[i - 1].iov_len) {
            bytesread -= iovarray[i - 1].iov_len;
            if (!sEntry->sa->encrypt &&
                sEntry->PInfo.SecurityLevel == RPC2_SECURE) {
                /* encrypt packet */
                j  = PBUFF((sEntry->SendMostRecent + i));
                pb = sEntry->ThesePackets[j];
                sftp_Encrypt(pb, sEntry);
                pb->Header.Flags |= RPC2_ENCRYPTED;
            }
            continue;
        }

        /* this is the packet with the last data byte */
        pb = sEntry->ThesePackets[PBUFF((sEntry->SendMostRecent + i))];
        rpc2_ntohp(pb);
        pb->Header.BodyLength = bytesread;
        pb->Header.SEFlags    = 0; /* turn off MOREDATA */
        pb->Header.Flags |=
            SFTP_ACKME; /* first transmission of last packet always acked */
        pb->Prefix.LengthOfPacket =
            sizeof(struct RPC2_PacketHeader) + bytesread;
        rpc2_htonp(pb);
        if (!sEntry->sa->encrypt &&
            sEntry->PInfo.SecurityLevel == RPC2_SECURE) {
            sftp_Encrypt(pb, sEntry);
            pb->Header.Flags = htonl(ntohl(pb->Header.Flags) | RPC2_ENCRYPTED);
        }
        break;
    }

    sEntry->ReadAheadCount = i;
    /* release excess packets */
    for (i++; i < sEntry->SendAhead + 1; i++)
        SFTP_FreeBuffer(
            &sEntry->ThesePackets[PBUFF((sEntry->SendMostRecent + i))]);

    return (0);
}

int sftp_SendStart(struct SFTP_Entry *sEntry)
/*
    Sends out a start packet on sEntry.
    Returns 0 if normal, -1 if fatal error of some kind occurred.
*/
{
    RPC2_PacketBuffer *pb;
    unsigned long now;

    sftp_starts++;
    sftp_Sent.Starts++;
    say(9, SFTP_DebugLevel, "sftp_SendStart()\n");

    /* Allocating a "0-length" buffer below is shaky, since we may
       append Parms below! */
    SFTP_AllocBuffer(0, &pb);
    sftp_InitPacket(pb, sEntry, 0);
    pb->Header.SeqNumber = ++(sEntry->CtrlSeqNumber);
    pb->Header.Opcode    = SFTP_START;

    now                  = rpc2_MakeTimeStamp();
    pb->Header.TimeStamp = now;
#ifdef VERY_FAST_SERVERS
    pb->Header.TimeEcho =
        VALID_TIMEECHO(sEntry) ?
            sEntry->TimeEcho + ((long)TSDELTA(now, sEntry->RequestTime)) :
            0;
#else
    pb->Header.TimeEcho = VALID_TIMEECHO(sEntry) ? sEntry->TimeEcho : 0;
#endif

    /* JJK: Parameters are screwed up! For now I will always send them! */
    /*    if (sEntry->SentParms == FALSE)*/
    {
        RPC2_PacketBuffer *saved_pkt = pb;
        if (sftp_AppendParmsToPacket(sEntry, &pb) < 0) {
            SFTP_FreeBuffer(&pb);
            return (-1);
        }
        if (saved_pkt != pb)
            RPC2_FreeBuffer(&saved_pkt);
        /* Can't set SentParms to TRUE yet, packet may be lost; set it in
         * sftp_DataArrived(). */
    }

    rpc2_htonp(pb);

    sftp_XmitPacket(sEntry, pb, 1);
    say(/*9*/ 4, SFTP_DebugLevel, "X-%lu [%lu]\n",
        (unsigned long)ntohl(pb->Header.SeqNumber),
        (unsigned long)ntohl(pb->Header.TimeStamp));
    SFTP_FreeBuffer(&pb);
    return (0);
}

int sftp_StartArrived(RPC2_PacketBuffer *pBuff, struct SFTP_Entry *sEntry)
/* Returns 0 if normal, -1 if fatal error of some kind occurred. */
{
    struct SFTP_Descriptor *sftpd = &sEntry->SDesc->Value.SmartFTPD;

    sftp_starts++;
    sftp_Recvd.Starts++;
    say(9, SFTP_DebugLevel, "sftp_StartArrived()\n");

    if (sEntry->XferState == XferNotStarted) {
        /* JJK: Parameters are screwed up!  For now I will always send them! */
        /*	if (sEntry->GotParms == FALSE)*/
        if (sftp_ExtractParmsFromPacket(sEntry, pBuff) < 0)
            return (-1);

        say(/*9*/ 4, SFTP_DebugLevel, "X-%u\n", pBuff->Header.SeqNumber);
        if (sftpd->hashmark != 0)
            switch (sftpd->Tag) {
            case FILEBYNAME:
                say(1, SFTP_DebugLevel,
                    "%s: ", sftpd->FileInfo.ByName.LocalFileName);
                break;

            case FILEBYFD:
                say(1, SFTP_DebugLevel, "%ld: ", sftpd->FileInfo.ByFD.fd);
                break;

            case FILEBYINODE:
                say(1, SFTP_DebugLevel,
                    "%ld.%ld: ", sftpd->FileInfo.ByInode.Device,
                    sftpd->FileInfo.ByInode.Inode);
                break;
            case FILEINVM:
                say(1, SFTP_DebugLevel, "FILEINVM ");
                break;
            }
    }

    say(/*9*/ 4, SFTP_DebugLevel, "X-%u [%u]\n", pBuff->Header.SeqNumber,
        pBuff->Header.TimeStamp);
    /*
     * grab the timestamp whether the transfer has started or not,
     * because we're going to send more data anyway.
     */
    sEntry->TimeEcho = pBuff->Header.TimeStamp;

    sEntry->XferState = XferInProgress;

    return (sftp_SendStrategy(sEntry));
}

int sftp_SendTrigger(struct SFTP_Entry *sEntry)
{
    sftp_triggers++;
    sftp_SendAck(sEntry);
    if (sftp_WriteStrategy(sEntry) < 0)
        return (-1); /* to flush buffers */
    return (0);
}

static int WinIsOpen(struct SFTP_Entry *sEntry)
{
    if ((sEntry->SendAhead + sEntry->SendMostRecent - sEntry->SendLastContig) >
        sEntry->WindowSize)
        return (FALSE);
    if ((SFTP_MaxPackets > 0) &&
        (sftp_PacketsInUse + sEntry->SendAhead > SFTP_MaxPackets)) {
        sftp_starved++;
        return (FALSE);
    }
    return (TRUE);
}

void sftp_InitPacket(RPC2_PacketBuffer *pb, struct SFTP_Entry *sfe,
                     long bodylen)
{
    memset(&pb->Header, 0, sizeof(struct RPC2_PacketHeader));
    pb->Header.ProtoVersion     = SFTPVERSION;
    pb->Header.BodyLength       = bodylen;
    pb->Prefix.LengthOfPacket   = sizeof(struct RPC2_PacketHeader) + bodylen;
    pb->Prefix.RecvStamp.tv_sec = pb->Prefix.RecvStamp.tv_usec = 0;
    if (sfe) {
        pb->Prefix.sa           = sfe->sa;
        pb->Header.RemoteHandle = sfe->PInfo.RemoteHandle;
        pb->Header.LocalHandle  = sfe->LocalHandle;
        pb->Header.SubsysId     = SMARTFTP;
        pb->Header.ThisRPCCall  = sfe->ThisRPCCall;
    }
}

#ifdef RPC2DEBUG
static void PrintDb(struct SFTP_Entry *se, RPC2_PacketBuffer *pb)
{
    fprintf(rpc2_tracefile, "SFTP_Entry:\n");
    fprintf(rpc2_tracefile,
            "\tMagic = %ld  WhoAmI = %d  LocalHandle = %#x  "
            "GotParms = %d  SentParms = %d\n",
            se->Magic, se->WhoAmI, se->LocalHandle, se->GotParms,
            se->SentParms);
    fprintf(rpc2_tracefile,
            "\topenfd = %ld  XferState = %d  HitEOF = %d  CtrlSeqNumber = %d\n",
            se->openfd, se->XferState, se->HitEOF, se->CtrlSeqNumber);
    fprintf(rpc2_tracefile,
            "\tSendLastContig = %d   SendMostRecent = %d  "
            "SendAckLimit = %d SendWorriedLimit = %d  "
            "ReadAheadCount = %d\n",
            se->SendLastContig, se->SendMostRecent, se->SendAckLimit,
            se->SendWorriedLimit, se->ReadAheadCount);
    fprintf(rpc2_tracefile, "\tRecvLastContig = %d   RecvMostRecent = %d\n",
            se->RecvLastContig, se->RecvMostRecent);

    if (!pb)
        return;

    fprintf(rpc2_tracefile, "\nSFTP_Packet:\n");
    rpc2_htonp(pb);
    rpc2_PrintPacketHeader(pb, rpc2_tracefile);
    rpc2_ntohp(pb);
}
#endif

/*--------------- Virtual file manipulation routines ------------*/

/* These routines are the only ones that know whether a file is a real
   one (specified by name, fd or inode) or a fake one (in memory).  The first
   two args to all the routines are the same: an SE descriptor that defines
   the file, and a file descriptor that is already open in the correct mode */

/* Returns length of file defined by se->SDesc
   Returns RPC2 error code (< 0)  on failure

   !!!!! BEWARE !!!!
   This function is only used by the ftp_AppendFileToPacket and
   SFTP_CheckSE functions (directly and through sftp_piggybackreadfile),
   to check whether the first CLIENTTOSERVER or SERVERTOCLIENT packet is
   able to send the file as well. It returns the filesize _limited to_
   ByteQuota if a quota has been specified.		- JH
   !!!!! BEWARE !!!!
*/
off_t sftp_piggybackfilesize(struct SFTP_Entry *se)
{
    struct stat stbuf;
    off_t length;

    if (MEMFILE(se->SDesc)) {
        length = se->SDesc->Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
    } else {
        if (fstat(se->openfd, &stbuf) < 0)
            return (RPC2_SEFAIL4);

        length = stbuf.st_size;
    }

    /* When we try to transfer too much, return a value so that we avoid
     * piggybacking */
    if (SFTP_EnforceQuota && se->SDesc->Value.SmartFTPD.ByteQuota > 0 &&
        length > se->SDesc->Value.SmartFTPD.ByteQuota)
        return (se->SDesc->Value.SmartFTPD.ByteQuota);

    return (length);
}

int sftp_piggybackfileread(struct SFTP_Entry *se, char *buf)
/* Reads entire file defined by se->SDesc into buf.
   Returns 0 on success, RPC2 error code (< 0) on failure
   WARNING: buf must be big enough -- no checks are done!
*/
{
    struct FileInfoByAddr *p;
    ssize_t n, len;

    if (MEMFILE(se->SDesc)) {
        p = &se->SDesc->Value.SmartFTPD.FileInfo.ByAddr;
        memcpy(buf, p->vmfile.SeqBody, sftp_piggybackfilesize(se));
    } else {
        if (BYFDFILE(se->SDesc))
            (void)lseek(se->openfd, se->fd_offset, SEEK_SET);

        len = sftp_piggybackfilesize(se);
        n   = read(se->openfd, buf, len);
        if (n < len)
            return (RPC2_SEFAIL4);
    }
    return (0);
}

/* writes out nbytes from buf to file sdesc using openfd
   Returns 0 on success,  RPC2 error code (< 0) on failure
*/
int sftp_vfwritefile(struct SFTP_Entry *se, char *buf, int nbytes)
{
    struct FileInfoByAddr *p;
    ssize_t n;

    if (MEMFILE(se->SDesc)) {
        p = &se->SDesc->Value.SmartFTPD.FileInfo.ByAddr;
        if (nbytes > p->vmfile.MaxSeqLen)
            return (RPC2_SEFAIL3);

        memcpy(p->vmfile.SeqBody, buf, nbytes);
        p->vmfile.SeqLen = nbytes;
    } else {
        if (BYFDFILE(se->SDesc))
            (void)lseek(se->openfd, se->fd_offset, SEEK_SET);

        n = write(se->openfd, buf, nbytes);
        if (n < nbytes) {
            if (errno == ENOSPC)
                return (RPC2_SEFAIL3);

            return (RPC2_SEFAIL4);
        }
        se->fd_offset += n;
    }
    return (0);
}

/* close any still open filedescriptor */
void sftp_vfclose(struct SFTP_Entry *se)
{
    if (se->openfd == -1) { /* we closed this fd when CheckSE fails, so this is
                               not a problem. -JH */
        say(10, SFTP_DebugLevel, "sftp_vfclose: fd was already closed.\n");
        return;
    }
    close(se->openfd); /* ignoring errors */
    se->openfd    = -1;
    se->fd_offset = 0;
}

static int sftp_vfreadv(struct SFTP_Entry *se, struct iovec iovarray[],
                        long howMany)
/* Like Unix readv().  Returns total number of bytes read.
   Can deal with in-memory files */
{
    long i, rc, bytesleft;
    char *initp;
    struct FileInfoByAddr *x;
    int n;

    /* Go to the disk if we must */
    if (!MEMFILE(se->SDesc)) {
        if (BYFDFILE(se->SDesc))
            (void)lseek(se->openfd, se->fd_offset, SEEK_SET);

        n = readv(se->openfd, iovarray, howMany);

        if (n > 0)
            se->fd_offset += n;

        return n;
    }

    /* This is a vm file; vmfilep indicates first byte not yet consumed */
    x         = &se->SDesc->Value.SmartFTPD.FileInfo.ByAddr;
    bytesleft = x->vmfile.SeqLen - x->vmfilep;
    initp     = (char *)&x->vmfile.SeqBody[x->vmfilep];

    rc = 0;
    for (i = 0; i < howMany; i++) {
        /* Fill one iov element on each iteration */
        if (bytesleft < iovarray[i].iov_len) {
            memcpy(iovarray[i].iov_base, initp, bytesleft);
            rc += bytesleft;
            break;
        }

        memcpy(iovarray[i].iov_base, initp, iovarray[i].iov_len);
        rc += iovarray[i].iov_len;
        initp += iovarray[i].iov_len;
        bytesleft -= iovarray[i].iov_len;
    }

    /* update offset info for next invocation */
    x->vmfilep += rc;
    return (rc);
}

static int sftp_vfwritev(struct SFTP_Entry *se, struct iovec *iovarray,
                         long howMany)
/* Iterates through the array and returns the total number of
   bytes sent out.  */
{
    long left, thistime, result, rc, i;
    struct FileInfoByAddr *p;
    struct iovec *thisiov;

    result = 0;
    left   = howMany;

    /* let's hope we won't have to share a non-seekable fd because the lseek
     * will fail */
    if (BYFDFILE(se->SDesc))
        (void)lseek(se->openfd, se->fd_offset, SEEK_SET);

    while (left > 0) {
        thistime = (left > 16) ? 16 : left;

        if (!MEMFILE(se->SDesc)) {
            rc = writev(se->openfd, &iovarray[howMany - left], thistime);
            if (rc > 0)
                se->fd_offset += rc;
        } else { /* in-memory file; copy it to the user's buffer */
            rc = 0;
            for (i = 0; i < thistime; i++) {
                thisiov = &iovarray[howMany - left + i];
                p       = &se->SDesc->Value.SmartFTPD.FileInfo.ByAddr;
                if (thisiov->iov_len >
                    (p->vmfile.MaxSeqLen -
                     p->vmfilep)) { /* file too big for buffer provided */
                    rc = -1; /* error bubbles up as SEFAIL3 eventually */
                    break;
                }
                memcpy(&p->vmfile.SeqBody[p->vmfilep], thisiov->iov_base,
                       thisiov->iov_len);
                rc += thisiov->iov_len;
                p->vmfilep += thisiov->iov_len;
                p->vmfile.SeqLen = p->vmfilep;
            }
        }
        if (rc < 0)
            return (rc);
        result += rc;
        left -= thistime;
    }

    return (result);
}

void sftp_Progress(SE_Descriptor *sdesc, off_t BytesTransferred)
{
    sdesc->Value.SmartFTPD.BytesTransferred = BytesTransferred;

    if (sdesc->XferCB)
        sdesc->XferCB(sdesc->userp,
                      /* needs LFS fixup */
                      (int)sdesc->Value.SmartFTPD.SeekOffset +
                          BytesTransferred);
}

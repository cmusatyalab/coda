/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *
 *   Implementation of the Venus CallBack server.
 *
 *
 *    ToDo:
 *        1. Allow for authenticated callback connections, and use them for CallBackFetch.
 *        2. Create/manage multiple worker threads (at least one per user).
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <stdlib.h>

#include <rpc2/rpc2.h>
#include <rpc2/se.h>

extern void rpc2_PrintSEDesc(SE_Descriptor *, FILE *);
/* interfaces */
#include <callback.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "comm.h"
#include "mariner.h"
#include "venus.private.h"
#include "venuscb.h"
#include "venusvol.h"
#include "worker.h"

const char CBSubsys[]             = "Vice2-CallBack";
const int CallBackServerStackSize = 65536;

static int MaxCBServers = UNSET_MAXCBSERVERS;
int cbbreaks            = 0; /* count of broken callbacks */

void CallBackInit()
{
    MaxCBServers = GetVenusConf().get_int_value("maxcbservers");
    /* Export the service. */
    RPC2_SubsysIdent server;
    server.Tag            = RPC2_SUBSYSBYID;
    server.Value.SubsysId = SUBSYS_CB;
    if (RPC2_Export(&server) != RPC2_SUCCESS)
        CHOKE("CallBackInit: RPC2_Export failed");

    /* Start up the CB servers. */
    for (int i = 0; i < MaxCBServers; i++)
        (void)new callbackserver;
}

callbackserver::callbackserver()
    : vproc("CallBackServer", NULL, VPT_CallBack, CallBackServerStackSize)
{
    LOG(100, ("callbackserver::callbackserver(%#x): %-16s : lwpid = %d\n", this,
              name, lwpid));

    filter.FromWhom              = ONESUBSYS;
    filter.OldOrNew              = OLDORNEW;
    filter.ConnOrSubsys.SubsysId = SUBSYS_CB;
    handle                       = 0;
    packet                       = 0;

    /* Poke main procedure. */
    start_thread();
}

/*
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
callbackserver::callbackserver(callbackserver &c)
    : vproc(*((vproc *)&c))
{
    abort();
}

int callbackserver::operator=(callbackserver &c)
{
    abort();
    return (0);
}

callbackserver::~callbackserver()
{
    LOG(100,
        ("callbackserver::~callbackserver: %-16s : lwpid = %d\n", name, lwpid));
}

void callbackserver::main(void)
{
    for (;;) {
        idle = 1;
        long code =
            RPC2_GetRequest(&filter, &handle, &packet, 0, 0, RPC2_XOR, 0);
        idle = 0;

        /* Handle RPC2 errors. */
        if (code <= RPC2_WLIMIT)
            LOG(1, ("callbackserver::main: GetRequest -> %s\n",
                    RPC2_ErrorMsg((int)code)));
        if (code <= RPC2_ELIMIT) {
            srvent *s = FindServerByCBCid(handle);
            if (s)
                s->Reset();
            else
                RPC2_Unbind(handle);
            continue;
        }

        /* Server MUST exist with this cid UNLESS this is a NewConnection message. */
        if (packet->Header.Opcode != RPC2_NEWCONNECTION) {
            srvent *s = FindServerByCBCid(handle);

            if (s == 0) {
                LOG(0,
                    ("callbackserver::main: can't find server (handle = %d)\n",
                     handle));

                /* Send a "bad client" reply back to the requestor. */
                /* Punt Alloc and Free failures! -JJK */
                (void)RPC2_FreeBuffer(&packet);
                RPC2_PacketBuffer *reply = 0;
                (void)RPC2_AllocBuffer(0, &reply);
                reply->Header.ReturnCode = RPC2_NOTCLIENT;
                (void)RPC2_SendResponse(handle, reply);
                (void)RPC2_FreeBuffer(&reply);

                RPC2_Unbind(handle);
                continue;
            }
        }

        /*DEBUG*/
        unsigned int *body = (unsigned int *)packet->Body;
        LOG(100, ("CBPKT: %x %x %x %x\n", body[0], body[1], body[2], body[3]));

        code = cb_ExecuteRequest(handle, packet, 0);
        if (code <= RPC2_WLIMIT)
            LOG(1, ("callbackserver::main: ExecuteRequest -> %s\n",
                    RPC2_ErrorMsg((int)code)));

        seq++;
    }
}

/* Some very tricky code here.  Essentially, when we make a call and are
 * awarded a callback, we have to ensure that the callback connection stayed
 * valid until we ran again (done by checking that the cbconnid field in the
 * connection block didn't change), and that no break callback was done while
 * we were waiting to run.  If we're creating a file, however, this last is
 * hard, since we do not even know the fid of the file when the callback is
 * broken.  For this then, any callback break that fails to find the fid
 * increments a counter that is compared over the ViceCreate call.  If it
 * changed, we assume it was for us. */
long VENUS_CallBack(RPC2_Handle RPCid, ViceFid *fid)
{
    ViceFid nullf = { 0, 0, 0 };
    VenusFid vf;

    srvent *s = FindServerByCBCid(RPCid);
    if (!s) {
        LOG(0, ("Callback from unknown host?\n"));
        return 0;
    }

    MakeVenusFid(&vf, s->realmid, fid);
    LOG(1, ("CallBack: host = %s, fid = (%s)\n", s->name, FID_(&vf)));

    /* Notify Codacon. */
    {
        if (FID_EQ(fid, &nullf))
            MarinerLog("callback::BackProbe %s\n", s->name);
        else
            MarinerLog("callback::Callback %s (%s)\n", s->name, FID_(&vf));
    }

    if (!vf.Volume)
        return (0); /* just a probe */

    if (vf.Vnode && vf.Unique) /* file callback */
        if (FSDB->CallBackBreak(&vf))
            cbbreaks++;

    if (VDB->CallBackBreak(MakeVolid(&vf)))
        cbbreaks++;

    return (0);
}

long VENUS_CallBackFetch(RPC2_Handle RPCid, ViceFid *Fid, SE_Descriptor *BD)
{
    VenusFid vf;
    srvent *s = FindServerByCBCid(RPCid);

    MakeVenusFid(&vf, s->realmid, Fid);

    LOG(1, ("CallBackFetch: host = %s, fid = (%s)\n", s->name, FID_(&vf)));

    long code = 0, fd = -1;

    /* Get the object. */
    fsobj *f = FSDB->Find(&vf);
    if (!f) {
        code = ENOENT;
        goto GetLost;
    }

    /*
     * We do not lock the object, because the reintegrator thread has already
     * created a shadow copy. However, we check to make sure there is a shadow
     * file just in case. This is a choke for now, because it really is not
     * supposed to happen.
     */
    if (!f->shadow) {
        eprint("CallBackFetch: no shadow file! (%s). Attempting to fix up.\n",
               FID_(&f->fid));
        f->MakeShadow();
    }

    /* Sanity checks. */
    if (!f->IsFile() || !HAVEALLDATA(f)) {
        code = EINVAL;
        goto GetLost;
    }

    /* Notify Codacon. */
    MarinerLog("callback::BackFetch %s, %s [%d]\n", s->name, f->GetComp(),
               BLOCKS(f));

    /* Do the transfer. */
    {
        SE_Descriptor sid;
        memset(&sid, 0, sizeof(SE_Descriptor));
        sid.Tag                     = SMARTFTP;
        struct SFTP_Descriptor *sei = &sid.Value.SmartFTPD;
        sei->TransmissionDirection  = SERVERTOCLIENT;
        sei->hashmark               = (GetLogLevel() >= 10 ? '#' : '\0');
        sei->SeekOffset             = 0;
        sei->ByteQuota              = -1;

        /* and open a safe fd to the containerfile */
        fd = f->shadow->Open(O_RDONLY);

        sei->Tag              = FILEBYFD;
        sei->FileInfo.ByFD.fd = fd;

        if (GetLogLevel() >= 1000) {
            rpc2_PrintSEDesc(&sid, GetLogFile());
        }

        if ((code = RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
            LOG(1, ("CallBackFetch: InitSE failed (%d)\n", code));
            goto GetLost;
        }

        if ((code = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <=
            RPC2_ELIMIT) {
            LOG(1, ("CallBackFetch: CheckSE failed (%d)\n", code));
            if (code == RPC2_SEFAIL1)
                code = EIO;
            goto GetLost;
        }

        LOG(100, ("CallBackFetch: transferred %d bytes\n",
                  sid.Value.SmartFTPD.BytesTransferred));
        if (f->vol->IsReadWrite())
            ((reintvol *)f->vol)->BytesBackFetched +=
                sid.Value.SmartFTPD.BytesTransferred;
    }

GetLost:
    if (f && fd != -1)
        f->shadow->Close(fd);
    LOG(1, ("CallBackFetch: returning %d\n", code));
    return (code);
}

/* CallBackNEWCONNECTION() */
long VENUS_CallBackConnect(RPC2_Handle RPCid, RPC2_Integer SideEffectType,
                           RPC2_Integer SecurityLevel,
                           RPC2_Integer EncryptionType, RPC2_Integer AuthType,
                           RPC2_CountedBS *ClientIdent)
{
    /* Get the {host,port} pair for this call. */
    RPC2_PeerInfo thePeer;
    RPC2_GetPeerInfo(RPCid, &thePeer);
    if (thePeer.RemoteHost.Tag != RPC2_HOSTBYINETADDR ||
        thePeer.RemotePort.Tag != RPC2_PORTBYINETNUMBER)
        CHOKE("CallBackConnect: getpeerinfo returned bogus type!");

    LOG(100, ("CallBackConnect: host = %s, port = %d\n",
              inet_ntoa(thePeer.RemoteHost.Value.InetAddress),
              ntohs(thePeer.RemotePort.Value.InetPortNumber)));

    /* Get the server entry and install the new connid. */
    /* It is NOT a fatal error if the srvent doesn't already exist, because the
     * server may be "calling-back" as a result of a bind by a PREVIOUS Venus
     * incarnation at this client! */
    srvent *s = FindServer(&thePeer.RemoteHost.Value.InetAddress);
    if (!s)
        return 0;

    s->GetRef();
    LOG(1, ("CallBackConnect: host = %s\n", s->name));
    MarinerLog("callback::NewConnection %s\n", s->name);
    s->ServerUp(RPCid);
    PutServer(&s);

    return (0);
}

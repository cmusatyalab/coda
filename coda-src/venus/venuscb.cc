/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
#endif __cplusplus

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>

#ifdef __BSD44__
#include <machine/endian.h>
#endif

#include <unistd.h>
#include <stdlib.h>

#include <rpc2.h>
#include <se.h>

extern void rpc2_PrintSEDesc(SE_Descriptor *, FILE *);
/* interfaces */
#include <callback.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from venus */
#include "comm.h"
#include "mariner.h"
#include "venus.private.h"
#include "venuscb.h"
#include "worker.h"


const char CBSubsys[] = "Vice2-CallBack";
const int CallBackServerStackSize = 32768;

int MaxCBServers = UNSET_MAXCBSERVERS;
int cbbreaks = 0;	/* count of broken callbacks */


void CallBackInit() {
    if (MaxCBServers == UNSET_MAXCBSERVERS)
	MaxCBServers = DFLT_MAXCBSERVERS;

    /* Export the service. */
    RPC2_SubsysIdent server;
    server.Tag = RPC2_SUBSYSBYID;
    server.Value.SubsysId = SUBSYS_CB;
    if (RPC2_Export(&server) != RPC2_SUCCESS)
	CHOKE("CallBackInit: RPC2_Export failed");

    /* Start up the CB servers. */
    for (int i = 0; i < MaxCBServers; i++)
	(void)new callbackserver;
}


callbackserver::callbackserver() :
    vproc("CallBackServer", NULL, VPT_CallBack, CallBackServerStackSize)
{
    LOG(100, ("callbackserver::callbackserver(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    filter.FromWhom = ONESUBSYS;
    filter.OldOrNew = OLDORNEW;
    filter.ConnOrSubsys.SubsysId = SUBSYS_CB;
    handle = 0;
    packet = 0;

    /* Poke main procedure. */
    start_thread();
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
callbackserver::callbackserver(callbackserver& c) : vproc(*((vproc *)&c)) {
    abort();
}


int callbackserver::operator=(callbackserver& c) {
    abort();
    return(0);
}


callbackserver::~callbackserver() {
    LOG(100, ("callbackserver::~callbackserver: %-16s : lwpid = %d\n", name, lwpid));
}


void callbackserver::main(void)
{
    for(;;) {
	idle = 1;
	long code = RPC2_GetRequest(&filter, &handle, &packet,
				   0, 0, RPC2_XOR, 0);
	idle = 0;

	/* Handle RPC2 errors. */
	if (code <= RPC2_WLIMIT)
	    LOG(1, ("callbackserver::main: GetRequest -> %s\n",
		    RPC2_ErrorMsg((int) code)));
	if (code <= RPC2_ELIMIT) {
	    srvent *s = FindServerByCBCid(handle);
	    if (s) s->Reset();
	    else RPC2_Unbind(handle);
	    continue;
	}

	/* Server MUST exist with this cid UNLESS this is a NewConnection message. */
	if (packet->Header.Opcode != RPC2_NEWCONNECTION) {
	    srvent *s = FindServerByCBCid(handle);

	    if (s == 0) {
		LOG(0, ("callbackserver::main: can't find server (handle = %d)\n", handle));

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
	LOG(100, ("CBPKT: %x %x %x %x\n",
		  ((long *)(packet->Body))[0], ((long *)(packet->Body))[1],
		  ((long *)(packet->Body))[2], ((long *)(packet->Body))[3]));
	code = cb_ExecuteRequest(handle, packet, 0);
	if (code <= RPC2_WLIMIT)
	    LOG(1, ("callbackserver::main: ExecuteRequest -> %s\n",
		    RPC2_ErrorMsg((int) code)));

	seq++;
    }
}


/* Some very tricky code here.  Essentially, when we make a call and are awarded a callback, */
/* we have to ensure that the callback connection stayed valid until we ran again (done by */
/* checking that the cbconnid field in the connection block didn't change), and that no break */
/* callback was done while we were waiting to run.  If we're creating a file, however, this last */
/* is hard, since we do not even know the fid of the file when the callback is broken.  For this */
/* then, any callback break that fails to find the fid increments a counter that is compared over */
/* the ViceCreate call.  If it changed, we assume it was for us. */
long CallBack(RPC2_Handle RPCid, ViceFid *fid) {
    srvent *s = FindServerByCBCid(RPCid);
    LOG(1, ("CallBack: host = %s, fid = (%x.%x.%x)\n",
	     s->name, fid->Volume, fid->Vnode, fid->Unique));

    /* Notify Codacon. */
    {
	if (FID_EQ(fid, &NullFid))
	    MarinerLog("callback::BackProbe %s\n", s->name);
	else
	    MarinerLog("callback::Callback %s (%x.%x.%x)\n",
		       s->name, fid->Volume, fid->Vnode, fid->Unique);
    }

    if (fid->Volume == 0) return(0);	/* just a probe */

    if (fid->Vnode && fid->Unique)	/* file callback */
	if (FSDB->CallBackBreak(fid))
	    cbbreaks++;

    if (VDB->CallBackBreak(fid->Volume)) {
	InitVCBData(fid->Volume);    
	if (fid->Vnode == 0 && fid->Unique == 0)
		AddVCBData(1);
	ReportVCBEvent(Break, fid->Volume);
	DeleteVCBData();

        cbbreaks++;
    }

    return(0);
}


long CallBackFetch(RPC2_Handle RPCid, ViceFid *Fid, SE_Descriptor *BD) {
    srvent *s = FindServerByCBCid(RPCid);
    LOG(1, ("CallBackFetch: host = %s, fid = (%x.%x.%x)\n",
	     s->name, Fid->Volume, Fid->Vnode, Fid->Unique));

    long code = 0;

    /* Get the object. */
    fsobj *f = FSDB->Find(Fid);
    if (f == 0)
	{ code = ENOENT; goto GetLost; }

    /* 
     * We do not lock the object, because the reintegrator thread
     * has already read locked it for our purposes (unless a shadow
     * copy has been created).  However, we check to make sure there 
     * is a read lock just in case.  This is a choke for now, because 
     * it really is not supposed to happen.
     */
    if (f->readers <= 0 && !f->shadow) 
	CHOKE("CallBackFetch: object not locked! (%x.%x.%x)\n",
	      f->fid.Volume, f->fid.Vnode, f->fid.Unique);

    /* Sanity checks. */
    if (!f->IsFile() || !HAVEALLDATA(f)) {
	code = EINVAL;
	goto GetLost;
    }

    /* Notify Codacon. */
    {
	char *comp = f->comp;
	char buf[CODA_MAXNAMLEN];
	if (comp[0] == '\0') {
	    sprintf(buf, "%s", FID_(&f->fid));
	    comp = buf;
	}
	MarinerLog("callback::BackFetch %s, %s [%d]\n", s->name, comp, BLOCKS(f));
    }

    /* Do the transfer. */
    {
        int fd;
	SE_Descriptor sid;
	memset(&sid, 0, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	struct SFTP_Descriptor *sei = &sid.Value.SmartFTPD;
	sei->TransmissionDirection = SERVERTOCLIENT;
	sei->hashmark = (LogLevel >= 10 ? '#' : '\0');
	sei->SeekOffset = 0;
	sei->ByteQuota = -1;

        /* and open a safe fd to the containerfile */
        if (f->shadow) fd = f->shadow->Open(&f->fid, O_RDONLY);
        else           fd = f->data.file->Open(&f->fid, O_RDONLY);
        CODA_ASSERT(fd != -1);

        sei->Tag = FILEBYFD;
        sei->FileInfo.ByFD.fd = fd;

	if (LogLevel >= 1000) {
	    rpc2_PrintSEDesc(&sid, logFile);
	}

	if ((code = RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	    LOG(1, ("CallBackFetch: InitSE failed (%d)\n", code));
	    goto GetLost;
	}

	if ((code = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	    LOG(1, ("CallBackFetch: CheckSE failed (%d)\n", code));
	    if (code == RPC2_SEFAIL1) code = EIO;
	    goto GetLost;
	}

	LOG(100, ("CallBackFetch: transferred %d bytes\n",
		  sid.Value.SmartFTPD.BytesTransferred));
	f->vol->BytesBackFetched += sid.Value.SmartFTPD.BytesTransferred;
    }

GetLost:
    if (f->shadow) f->shadow->Close();
    else           f->data.file->Close();
    LOG(1, ("CallBackFetch: returning %d\n", code));
    return(code);
}


/* CallBackNEWCONNECTION() */
long CallBackConnect(RPC2_Handle RPCid, RPC2_Integer SideEffectType,
		     RPC2_Integer SecurityLevel, RPC2_Integer EncryptionType,
		     RPC2_Integer AuthType, RPC2_CountedBS *ClientIdent) 
{
    /* Get the {host,port} pair for this call. */
    RPC2_PeerInfo thePeer;
    RPC2_GetPeerInfo(RPCid, &thePeer);
    if (thePeer.RemoteHost.Tag != RPC2_HOSTBYINETADDR ||
	 thePeer.RemotePort.Tag != RPC2_PORTBYINETNUMBER)
	CHOKE("CallBackConnect: getpeerinfo returned bogus type!");

    unsigned long host = ntohl(thePeer.RemoteHost.Value.InetAddress.s_addr);
    unsigned short port = ntohs(thePeer.RemotePort.Value.InetPortNumber);
    LOG(100, ("CallBackConnect: host = %x, port = %d\n",
	      inet_ntoa(thePeer.RemoteHost.Value.InetAddress), port));

    /* Get the server entry and install the new connid. */
    /* It is NOT a fatal error if the srvent doesn't already exist, because the server may be */
    /* "calling-back" as a result of a bind by a PREVIOUS Venus incarnation at this client! */
    srvent *s = 0;
    GetServer(&s, host);
    LOG(1, ("CallBackConnect: host = %s\n", s->name));
    MarinerLog("callback::NewConnection %s\n", s->name);
    s->ServerUp(RPCid);
    PutServer(&s);

    return(0);
}


/* ---------------------------------------------------------------------- */
/* This really ought to be implemented by a separate subsystem since it performs an independent */
/*  function.  The purpose is to allow us to return to the user once it is established that a close */
/*  will either succeed or fail due to communications or server disk error (i.e., all other error checks */
/*  have already been made).  The (weak?) justification for this is that virtually no one checks return */
/*  codes from close anyway, so this is actually analagous to the local case. */

int EarlyReturnAllowed = 1;	/* can we return early on stores? */


long CallBackReceivedStore(RPC2_Handle RPCid, ViceFid *fid) {
    LOG(1, ("CallBackReceivedStore: fid = (%x.%x.%x)\n",
	     fid->Volume, fid->Vnode, fid->Unique));

    MarinerLog("callback::ReceivedStore (%x.%x.%x)\n",
		fid->Volume, fid->Vnode, fid->Unique);

    /* Check global flag to see if early returns are permitted. */
    if (!EarlyReturnAllowed) return(0);

    /* Allow worker to return early if possible. */
    WorkerReturnEarly((ViceFid *)fid);

    return(0);
}

/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

#ifdef __BSD44__
#include <machine/endian.h>
#endif

#include <unistd.h>
#include <stdlib.h>

#include <rpc2/rpc2.h>
#include <rpc2/se.h>

extern void rpc2_PrintSEDesc(SE_Descriptor *, FILE *);
/* interfaces */
#include <writeback.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "comm.h"
#include "mariner.h"
#include "venus.private.h"
#include "venuswb.h"
#include "worker.h"


const int WritebackServerStackSize = 131072;

int MaxWBServers = UNSET_MAXWBSERVERS;

void WritebackInit() {
    if (MaxWBServers == UNSET_MAXWBSERVERS)
	MaxWBServers = DFLT_MAXWBSERVERS;

    /* export */
    RPC2_SubsysIdent server;
    server.Tag = RPC2_SUBSYSBYID;
    server.Value.SubsysId = SUBSYS_WB;
    if (RPC2_Export(&server) != RPC2_SUCCESS)
	CHOKE("WritebackInit: RPC2_Export failed");

    /* start up the WB servers */
    for (int i = 0; i < MaxWBServers; i++)
	(void)new writebackserver;

}

writebackserver::writebackserver() :
    vproc("WritebackServer", NULL, VPT_WriteBack, WritebackServerStackSize)
{
    filter.FromWhom = ONESUBSYS;
    filter.OldOrNew = OLDORNEW;
    filter.ConnOrSubsys.SubsysId = SUBSYS_WB;
    handle = 0;
    packet = 0;

    /* And start the new thread */
    start_thread();

    LOG(100, ("writebackserver::writebackserver(%#x): %-16s : lwpid = %d\n",
	      this, name, lwpid));
}

/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
writebackserver::writebackserver(writebackserver& c) : vproc(*((vproc *)&c)) {
    abort();
}


int writebackserver::operator=(writebackserver& c) {
    abort();
    return(0);
}


writebackserver::~writebackserver() {
    LOG(100, ("writebackserver::~writebackserver: %-16s : lwpid = %d\n", name, lwpid));
}


void writebackserver::main(void)
{
    for(;;) {
	idle = 1;
	long code = RPC2_GetRequest(&filter, &handle, &packet,
				   0, 0, RPC2_XOR, 0);
	idle = 0;

	/* Handle RPC2 errors. */
	if (code <= RPC2_WLIMIT)
	    LOG(1, ("writebackserver::main: GetRequest -> %s\n",
		    RPC2_ErrorMsg((int) code)));
	if (code <= RPC2_ELIMIT) {
	    RPC2_Unbind(handle);
	    continue;
	}

#if 0
	/* Server MUST exist with this cid UNLESS this is a NewConnection message. */
	if (packet->Header.Opcode != RPC2_NEWCONNECTION) {
	    srvent *s = FindServerByCBCid(handle);

	    if (s == 0) {
		LOG(0, ("writebackserver::main: can't find server (handle = %d)\n", handle));

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
#endif

/*DEBUG*/
	LOG(100, ("WBPKT: %x %x %x %x\n",
		  ((long *)(packet->Body))[0], ((long *)(packet->Body))[1],
		  ((long *)(packet->Body))[2], ((long *)(packet->Body))[3]));
	code = WB_ExecuteRequest(handle, packet, 0);
	if (code <= RPC2_WLIMIT)
	    LOG(1, ("writebackserver::main: ExecuteRequest -> %s\n",
		    RPC2_ErrorMsg((int) code)));

	seq++;
    }
}

long VENUS_RevokeWBPermit(RPC2_Handle RPCid, VolumeId Vid)
{
    RPC2_PeerInfo thePeer;
    volent *v;
    Volid vid;

    LOG(1, ("RevokeWBPermit(): Vid = %d\n", Vid));

    RPC2_GetPeerInfo(RPCid, &thePeer);
    srvent *s = FindServer(&thePeer.RemoteHost.Value.InetAddress);

    if (!s) {
	LOG(0, ("RevokeWBPermit from unknown host?\n"));
	return 0;
    }

    if (!Vid) return 0;

    vid.Realm = s->realmid;
    vid.Volume = Vid;

    v = VDB->Find(&vid);
    if (!v) return 0;

    if (v->IsReplicated()) {
	repvol *vp = (repvol *)v;
	vp->StopWriteback(NULL);
    }

    v->release();

    return 0;
}

long VENUS_WriteBackFetch(RPC2_Handle RPCid, VolumeId Vid, ViceFid *Fid)
{
    return 0;
}

/* WriteBackNEWCONNECTION() */
long VENUS_WriteBackConnect(RPC2_Handle RPCid, RPC2_Integer SideEffectType,
			    RPC2_Integer SecurityLevel, RPC2_Integer
			    EncryptionType, RPC2_Integer AuthType,
			    RPC2_CountedBS *ClientIdent) 
{
    /* Get the {host,port} pair for this call. */
    RPC2_PeerInfo thePeer;
    RPC2_GetPeerInfo(RPCid, &thePeer);
    if (thePeer.RemoteHost.Tag != RPC2_HOSTBYINETADDR ||
	thePeer.RemotePort.Tag != RPC2_PORTBYINETNUMBER)
	CHOKE("WriteBackConnect: getpeerinfo returned bogus type!");

    unsigned short port = ntohs(thePeer.RemotePort.Value.InetPortNumber);
    LOG(100, ("WriteBackConnect: host = %s, port = %d\n",
	      inet_ntoa(thePeer.RemoteHost.Value.InetAddress), port));

    return(0);
}

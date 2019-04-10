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
 * Implementation of the Venus Communications subsystem.
 *
 *    This module should be split up into:
 *        1. A subsystem independent module, libcomm.a, containing base classes srvent and connent.
 *        2. A subsystem dependent module containing derived classes v_srvent and v_connent.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <sys/time.h>
#include <errno.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <stdio.h>
#include "coda_string.h"
#include <struct.h>
#include <unistd.h>
#include <stdlib.h>

#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/errors.h>

extern int Fcon_Init();
extern void SFTP_SetDefaults(SFTP_Initializer *initPtr);
extern void SFTP_Activate(SFTP_Initializer *initPtr);

/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif

/* from vv */
#include <inconsist.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "user.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"

static int COPModes = 6; /* ASYNCCOP2 | PIGGYCOP2 */
char myHostName[MAXHOSTNAMELEN];
static int sftp_windowsize = UNSET_WS;
static int sftp_sendahead  = UNSET_SA;
static int sftp_ackpoint   = UNSET_AP;
static int sftp_packetsize = UNSET_PS;

extern long RPC2_Perror;
struct CommQueueStruct CommQueue;

olist *srvent::srvtab;
char srvent::srvtab_sync;
olist *connent::conntab;
char connent::conntab_sync;

#ifdef VENUSDEBUG
int connent::allocs   = 0;
int connent::deallocs = 0;
int srvent::allocs    = 0;
int srvent::deallocs  = 0;
#endif /* VENUSDEBUG */

void CommInit()
{
    COPModes         = GetVenusConf().get_int_value("copmodes");
    sftp_windowsize  = GetVenusConf().get_int_value("sftp_windowsize");
    sftp_sendahead   = GetVenusConf().get_int_value("sftp_sendahead");
    sftp_ackpoint    = GetVenusConf().get_int_value("sftp_ackpoint");
    sftp_packetsize  = GetVenusConf().get_int_value("sftp_packetsize");
    srv_ElapseSwitch = GetVenusConf().get_int_value("von");
    srv_MultiStubWork[0].opengate = GetVenusConf().get_int_value("vmon");

    /* Sanity check COPModes. */
    if ((ASYNCCOP1 && !ASYNCCOP2) || (PIGGYCOP2 && !ASYNCCOP2))
        CHOKE("CommInit: bogus COPModes (%x)", COPModes);

    /* Initialize comm queue */
    memset((void *)&CommQueue, 0, sizeof(CommQueueStruct));

    /* Hostname is needed for file server connections. */
    if (gethostname(myHostName, MAXHOSTNAMELEN) < 0)
        CHOKE("CommInit: gethostname failed");

    /* Initialize Connections. */
    connent::conntab = new olist;

    /* Initialize Servers. */
    srvent::srvtab = new olist;

    RPC2_Perror = 0;

    /* Port initialization. */
    RPC2_PortIdent port1;
    port1.Tag = RPC2_PORTBYINETNUMBER;
    port1.Value.InetPortNumber =
        htons(GetVenusConf().get_int_value("masquerade_port"));

    /* SFTP initialization. */
    SFTP_Initializer sei;
    SFTP_SetDefaults(&sei);
    sei.WindowSize   = sftp_windowsize;
    sei.SendAhead    = sftp_sendahead;
    sei.AckPoint     = sftp_ackpoint;
    sei.PacketSize   = sftp_packetsize;
    sei.EnforceQuota = 1;
    sei.Port.Tag     = (PortTag)0;
    SFTP_Activate(&sei);

    /* RPC2 initialization. */
    struct timeval tv;
    tv.tv_sec  = GetVenusConf().get_int_value("RPC2_timeout");
    tv.tv_usec = 0;
    if (RPC2_Init(RPC2_VERSION, 0, &port1,
                  GetVenusConf().get_int_value("RPC2_retries"),
                  &tv) != RPC2_SUCCESS)
        CHOKE("CommInit: RPC2_Init failed");

    /* Fire up the probe daemon. */
    PROD_Init();
}

int GetCOPModes()
{
    return COPModes;
}

void SetCOPModes(int copmodes)
{
    COPModes = copmodes;
}

/* *****  Connection  ***** */

int srvent::GetConn(connent **cpp, uid_t uid, int Force)
{
    LOG(100, ("srvent::GetConn: host = %s, uid = %d, force = %d\n", name, uid,
              Force));

    *cpp       = 0;
    int code   = 0;
    connent *c = 0;

    /* Grab an existing connection if one is free. */
    {
        /* Check whether there is already a free connection. */
        struct ConnKey Key;
        Key.host = host;
        Key.uid  = uid;
        conn_iterator next(&Key);
        while ((c = next())) {
            /* the iterator grabs a reference, so in-use connections
	     * will have a refcount of 2 */
            if (c->RefCount() <= 1 && !c->dying) {
                c->GetRef();
                *cpp = c;
                return 0;
            }
        }
    }

    /* Try to connect to the server on behalf of the user. */
    RPC2_Handle ConnHandle = 0;
    int auth               = 1;
    code                   = Connect(&ConnHandle, &auth, uid, Force);

    switch (code) {
    case 0:
        break;
    case EINTR:
        return (EINTR);
    case EPERM:
    case ERETRY:
        return (ERETRY);
    default:
        return (ETIMEDOUT);
    }

    /* Create and install the new connent. */
    c = new connent(this, uid, ConnHandle, auth);
    if (!c)
        return (ENOMEM);

    c->GetRef();
    connent::conntab->insert(&c->tblhandle);
    *cpp = c;
    return (0);
}

void PutConn(connent **cpp)
{
    connent *c = *cpp;
    *cpp       = 0;
    if (!c) {
        LOG(100, ("PutConn: null conn\n"));
        return;
    }

    LOG(100, ("PutConn: host = %s, uid = %d, cid = %d, auth = %d\n",
              c->srv->Name(), c->uid, c->connid, c->authenticated));

    c->PutRef();

    if (c->RefCount() || !c->dying)
        return;

    connent::conntab->remove(&c->tblhandle);
    delete c;
}

void ConnPrint()
{
    ConnPrint(stdout);
}

void ConnPrint(FILE *fp)
{
    fflush(fp);
    ConnPrint(fileno(fp));
}

void ConnPrint(int fd)
{
    if (connent::conntab == 0)
        return;

    fdprint(fd, "Connections: count = %d\n", connent::conntab->count());

    /* Iterate through the individual entries. */
    conn_iterator next;
    connent *c;
    while ((c = next()))
        c->print(fd);

    fdprint(fd, "\n");
}

connent::connent(srvent *server, uid_t Uid, RPC2_Handle cid, int authflag)
{
    LOG(1, ("connent::connent: host = %s, uid = %d, cid = %d, auth = %d\n",
            server->Name(), uid, cid, authflag));

    /* These members are immutable. */
    server->GetRef();
    srv           = server;
    uid           = Uid;
    connid        = cid;
    authenticated = authflag;

    /* These members are mutable. */
    inuse = 0;
    dying = 0;

#ifdef VENUSDEBUG
    allocs++;
#endif
}

connent::~connent()
{
    int code;
#ifdef VENUSDEBUG
    deallocs++;
#endif

    LOG(1, ("connent::~connent: host = %s, uid = %d, cid = %d, auth = %d\n",
            srv->Name(), uid, connid, authenticated));

    CODA_ASSERT(!RefCount());

    /* Be nice and disconnect if the server is available. */
    if (srv->ServerIsUp()) {
        /* Make the RPC call. */
        MarinerLog("fetch::DisconnectFS %s\n", srv->Name());
        UNI_START_MESSAGE(ViceDisconnectFS_OP);
        code = (int)ViceDisconnectFS(connid);
        UNI_END_MESSAGE(ViceDisconnectFS_OP);
        MarinerLog("fetch::disconnectfs done\n");
        code = CheckResult(code, 0);
        UNI_RECORD_STATS(ViceDisconnectFS_OP);
    }

    code   = (int)RPC2_Unbind(connid);
    connid = 0;
    PutServer(&srv);
    LOG(1, ("connent::~connent: RPC2_Unbind -> %s\n", RPC2_ErrorMsg(code)));
}

/* Mark this conn as dying. */
void connent::Suicide(void)
{
    LOG(1, ("connent::Suicide\n"));
    dying = 1;
}

/* Maps return codes from Vice:
	0		Success,
	EINTR		Call was interrupted,
	ETIMEDOUT	Host did not respond,
	ERETRY		Retryable error,
	Other (> 0)	Non-retryable error (valid kernel return code).
*/
int connent::CheckResult(int code, VolumeId vid, int TranslateEINCOMP)
{
    LOG(100, ("connent::CheckResult: code = %d, vid = %x\n", code, vid));

    /* ViceOp succeeded. */
    if (code == 0)
        return (0);

    /* Translate RPC and Volume errors, and update server state. */
    switch (code) {
    default:
        if (code < 0)
            srv->ServerError(&code);

        if (code == ETIMEDOUT || code == ERETRY)
            dying = 1;
        break;

    case VBUSY:
        code = EWOULDBLOCK;
        break;

    case VNOVOL:
        code = ENXIO;
        break;

    case VNOVNODE:
        code = ENOENT;
        break;

    case VLOGSTALE:
        code = EALREADY;
        break;

    case VSALVAGE:
    case VVOLEXISTS:
    case VNOSERVICE:
    case VOFFLINE:
    case VONLINE:
    case VNOSERVER:
    case VMOVED:
    case VFAIL:
        eprint("connent::CheckResult: illegal code (%d)", code);
        code = EINVAL;
        break;
    }

    /* Coerce EINCOMPATIBLE to ERETRY. */
    if (TranslateEINCOMP && code == EINCOMPATIBLE)
        code = ERETRY;

    if (code == ETIMEDOUT && VprocInterrupted())
        return (EINTR);
    return (code);
}

void connent::print(int fd)
{
    fdprint(
        fd,
        "%#08x : host = %s, uid = %d, cid = %d, auth = %d, inuse = %u, dying = %d\n",
        (long)this, srv->Name(), uid, connid, authenticated, inuse, dying);
}

conn_iterator::conn_iterator(struct ConnKey *Key)
    : olist_iterator((olist &)*connent::conntab)
{
    key = Key;
}

conn_iterator::~conn_iterator()
{
    if (clink && clink != (void *)-1) {
        connent *c = strbase(connent, clink, tblhandle);
        PutConn(&c);
    }
}

connent *conn_iterator::operator()()
{
    olink *o, *prev = clink;
    connent *next = NULL;

    while ((o = olist_iterator::operator()())) {
        next = strbase(connent, o, tblhandle);
        if (next->dying)
            continue;
        if (!key || ((key->host.s_addr == next->srv->host.s_addr ||
                      key->host.s_addr == INADDR_ANY) &&
                     (key->uid == next->uid || key->uid == ANYUSER_UID))) {
            next->GetRef();
            break;
        }
    }
    if (prev && prev != (void *)-1) {
        connent *c = strbase(connent, prev, tblhandle);
        PutConn(&c);
    }
    return o ? next : NULL;
}

/* ***** Server  ***** */

/*
 *    Notes on the srvent::connid field:
 *
 *    The server's connid is the "local handle" of the current callback connection.
 *
 *    A srvent::connid value of 0 serves as a flag that the server is incommunicado
 *    (i.,e., "down" from the point of view of this Venus).  Two other values are distinguished
 *    and mean that the server is "quasi-up": -1 indicates that the server has never been
 *    contacted (i.e., at initialization), -2 indicates that the server has just NAK'ed an RPC.
 */

void Srvr_Wait()
{
    LOG(0, ("WAITING(SRVRQ):\n"));
    START_TIMING();
    VprocWait((char *)&srvent::srvtab_sync);
    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
}

void Srvr_Signal()
{
    LOG(10, ("SIGNALLING(SRVRQ):\n"));
    VprocSignal((char *)&srvent::srvtab_sync);
}

srvent *FindServer(struct in_addr *host)
{
    srv_iterator next;
    srvent *s;

    while ((s = next()))
        if (s->host.s_addr == host->s_addr)
            return (s);

    return (0);
}

srvent *FindServerByCBCid(RPC2_Handle connid)
{
    if (connid == 0)
        return (0);

    srv_iterator next;
    srvent *s;

    while ((s = next()))
        if (s->connid == connid)
            return (s);

    return (0);
}

srvent *GetServer(struct in_addr *host, RealmId realmid)
{
    CODA_ASSERT(host && host->s_addr);
    LOG(100, ("GetServer: host = %s\n", inet_ntoa(*host)));

    srvent *s = FindServer(host);
    if (s) {
        s->GetRef();
        if (s->realmid == realmid)
            return s;

        s->Reset();
        PutServer(&s);
    }

    s = new srvent(host, realmid);

    srvent::srvtab->insert(&s->tblhandle);

    return s;
}

void PutServer(srvent **spp)
{
    if (*spp) {
        LOG(100, ("PutServer: %s\n", (*spp)->name));
        (*spp)->PutRef();
    }
    *spp = NULL;
}

/*
 *    The probe routines exploit parallelism in three ways:
 *       1. MultiRPC is used to perform the Probe RPC (actually, a ViceGetTime)
 *       2. Slave vprocs are used to overlap the probing of "up" servers and
 * 	    the binding/probing of "down" servers.  Otherwise probing of "up"
 *	    servers may be delayed for the binding to "down" servers.
 *       3. (Additional) slave vprocs are used to overlap the binding of
 *          "down" servers
 *
 *    Note that item 3 is only needed because MultiBind functionality is not
 *    yet a part of MultiRPC.
 */

probeslave::probeslave(ProbeSlaveTask Task, void *Arg, void *Result, char *Sync)
    : vproc("ProbeSlave", NULL, VPT_ProbeDaemon, 32768)
{
    LOG(100, ("probeslave::probeslave(%#x): %-16s : lwpid = %d\n", this, name,
              lwpid));

    task   = Task;
    arg    = Arg;
    result = Result;
    sync   = Sync;

    /* Poke main procedure. */
    start_thread();
}

void probeslave::main(void)
{
    switch (task) {
    case ProbeUpServers:
        ProbeServers(1);
        break;

    case ProbeDownServers:
        ProbeServers(0);
        break;

    case BindToServer: {
        /* *result gets pointer to connent on success, 0 on failure. */
        struct in_addr *Host = (struct in_addr *)arg;
        srvent *s            = FindServer(Host);
        if (s) {
            s->GetRef();
            s->GetConn((connent **)result, ANYUSER_UID, 1);
            PutServer(&s);
        }
    } break;

    default:
        CHOKE("probeslave::main: bogus task (%d)", task);
    }

    /* Signal reaper, then commit suicide. */
    (*sync)++;
    VprocSignal(sync);
    idle = 1;
    delete VprocSelf();
}

void ProbeServers(int Up)
{
    LOG(1, ("ProbeServers: %s\n", Up ? "Up" : "Down"));

    /* Hosts and Connections are arrays of addresses and connents respectively
     * representing the servers to be probed.  HowMany is the current size of
     * these arrays, and ix is the number of entries actually used. */
    const int GrowSize = 32;
    int HowMany        = GrowSize;
    struct in_addr *Hosts;
    int ix = 0;

    Hosts = (struct in_addr *)malloc(HowMany * sizeof(struct in_addr));
    /* Fill in the Hosts array for each server that is to be probed. */
    {
        srv_iterator next;
        srvent *s;
        while ((s = next())) {
            if (!s->probeme)
                continue;

            if ((Up && s->ServerIsDown()) || (!Up && !s->ServerIsDown()))
                continue;

            /* Grow the Hosts array if necessary. */
            if (ix == HowMany) {
                HowMany += GrowSize;
                Hosts = (struct in_addr *)realloc(
                    Hosts, HowMany * sizeof(struct in_addr));
                memset(&Hosts[ix], 0, GrowSize * sizeof(struct in_addr));
            }

            /* Stuff the address in the Hosts array. */
            memcpy(&Hosts[ix], &s->host, sizeof(struct in_addr));
            ix++;
        }
    }

    if (ix)
        DoProbes(ix, Hosts);

    /* the incorrect "free" in DoProbes() is moved here */
    free(Hosts);
}

void DoProbes(int HowMany, struct in_addr *Hosts)
{
    connent **Connections = 0;
    int i;

    CODA_ASSERT(HowMany > 0);

    Connections = (connent **)malloc(HowMany * sizeof(connent *));
    memset(Connections, 0, HowMany * sizeof(connent *));

    /* Bind to the servers. */
    MultiBind(HowMany, Hosts, Connections);

    /* Probe them. */
    int AnyHandlesValid  = 0;
    RPC2_Handle *Handles = (RPC2_Handle *)malloc(HowMany * sizeof(RPC2_Handle));
    for (i = 0; i < HowMany; i++) {
        if (Connections[i] == 0) {
            Handles[i] = 0;
            continue;
        }

        AnyHandlesValid = 1;
        Handles[i]      = Connections[i]->connid;
    }

    if (AnyHandlesValid)
        MultiProbe(HowMany, Handles);

    free(Handles);

    for (i = 0; i < HowMany; i++)
        PutConn(&Connections[i]);
    free(Connections);
}

void MultiBind(int HowMany, struct in_addr *Hosts, connent **Connections)
{
    if (GetLogLevel() >= 1) {
        dprint("MultiBind: HowMany = %d\n\tHosts = [ ", HowMany);
        for (int i = 0; i < HowMany; i++)
            fprintf(GetLogFile(), "%s ", inet_ntoa(Hosts[i]));
        fprintf(GetLogFile(), "]\n");
    }

    int ix, slaves = 0;
    char slave_sync = 0;
    for (ix = 0; ix < HowMany; ix++) {
        /* Try to get a connection without forcing a bind. */
        connent *c = 0;
        int code;
        srvent *s = FindServer(&Hosts[ix]);

        if (!s)
            continue;

        s->GetRef();
        code = s->GetConn(&c, ANYUSER_UID);
        PutServer(&s);

        if (code == 0) {
            /* Stuff the connection in the array. */
            Connections[ix] = c;
            continue;
        }

        /* Force a bind, but have a slave do it so we can bind in parallel. */
        {
            slaves++;
            (void)new probeslave(BindToServer, (void *)(&Hosts[ix]),
                                 (void *)(&Connections[ix]), &slave_sync);
        }
    }

    /* Reap any slaves we created. */
    while (slave_sync != slaves) {
        LOG(1, ("MultiBind: waiting (%d, %d)\n", slave_sync, slaves));
        VprocWait(&slave_sync);
    }
}

void MultiProbe(int HowMany, RPC2_Handle *Handles)
{
    if (GetLogLevel() >= 1) {
        dprint("MultiProbe: HowMany = %d\n\tHandles = [ ", HowMany);
        for (int i = 0; i < HowMany; i++)
            fprintf(GetLogFile(), "%x ", Handles[i]);
        fprintf(GetLogFile(), "]\n");
    }

    /* Make multiple copies of the IN/OUT and OUT parameters. */
    RPC2_Unsigned **secs_ptrs =
        (RPC2_Unsigned **)malloc(HowMany * sizeof(RPC2_Unsigned *));
    CODA_ASSERT(secs_ptrs);
    RPC2_Unsigned *secs_bufs =
        (RPC2_Unsigned *)malloc(HowMany * sizeof(RPC2_Unsigned));
    CODA_ASSERT(secs_bufs);
    for (int i = 0; i < HowMany; i++)
        secs_ptrs[i] = &secs_bufs[i];
    RPC2_Integer **usecs_ptrs =
        (RPC2_Integer **)malloc(HowMany * sizeof(RPC2_Integer *));
    CODA_ASSERT(usecs_ptrs);
    RPC2_Integer *usecs_bufs =
        (RPC2_Integer *)malloc(HowMany * sizeof(RPC2_Integer));
    CODA_ASSERT(usecs_bufs);
    for (int ii = 0; ii < HowMany; ii++)
        usecs_ptrs[ii] = &usecs_bufs[ii];

    /* Make the RPC call. */
    MarinerLog("fetch::Probe\n");
    MULTI_START_MESSAGE(ViceGetTime_OP);
    int code = (int)MRPC_MakeMulti(ViceGetTime_OP, ViceGetTime_PTR, HowMany,
                                   Handles, 0, 0, HandleProbe, 0, secs_ptrs,
                                   usecs_ptrs);
    MULTI_END_MESSAGE(ViceGetTime_OP);
    MarinerLog("fetch::probe done\n");

    /* CheckResult is done dynamically by HandleProbe(). */
    MULTI_RECORD_STATS(ViceGetTime_OP);

    /* Discard dynamic data structures. */
    free(secs_ptrs);
    free(secs_bufs);
    free(usecs_ptrs);
    free(usecs_bufs);
}

long HandleProbe(int HowMany, RPC2_Handle Handles[], long offset, long rpcval,
                 ...)
{
    RPC2_Handle RPCid = Handles[offset];

    if (RPCid != 0) {
        /* Get the {host,port} pair for this call. */
        RPC2_PeerInfo thePeer;
        int rc = RPC2_GetPeerInfo(RPCid, &thePeer);
        if (thePeer.RemoteHost.Tag != RPC2_HOSTBYINETADDR ||
            thePeer.RemotePort.Tag != RPC2_PORTBYINETNUMBER) {
            LOG(0, ("HandleProbe: RPC2_GetPeerInfo return code = %d\n", rc));
            LOG(0, ("HandleProbe: thePeer.RemoteHost.Tag = %d\n",
                    thePeer.RemoteHost.Tag));
            LOG(0, ("HandleProbe: thePeer.RemotePort.Tag = %d\n",
                    thePeer.RemotePort.Tag));
            return 0;
            /* CHOKE("HandleProbe: getpeerinfo returned bogus type!"); */
        }

        /* Locate the server and update its status. */
        srvent *s = FindServer(&thePeer.RemoteHost.Value.InetAddress);
        if (!s)
            CHOKE("HandleProbe: no srvent (RPCid = %d, PeerHost = %s)", RPCid,
                  inet_ntoa(thePeer.RemoteHost.Value.InetAddress));
        LOG(1, ("HandleProbe: (%s, %d)\n", s->name, rpcval));
        if (rpcval < 0) {
            int rc = rpcval;
            s->ServerError(&rc);
        }
    }

    return (0);
}

/* Report which servers are down. */
void DownServers(char *buf, unsigned int *bufsize)
{
    char *cp             = buf;
    unsigned int maxsize = *bufsize;
    *bufsize             = 0;

    /* Copy each down server's address into the buffer. */
    srv_iterator next;
    srvent *s;
    while ((s = next()))
        if (s->ServerIsDown()) {
            /* Make sure there is room in the buffer for this entry. */
            if ((cp - buf) + sizeof(struct in_addr) > maxsize)
                return;

            memcpy(cp, &s->host, sizeof(struct in_addr));
            cp += sizeof(struct in_addr);
        }

    /* Null terminate the list.  Make sure there is room in the buffer for the
     * terminator. */
    if ((cp - buf) + sizeof(struct in_addr) > maxsize)
        return;
    memset(cp, 0, sizeof(struct in_addr));
    cp += sizeof(struct in_addr);

    *bufsize = (cp - buf);
}

/* Report which of a given set of servers is down. */
void DownServers(int nservers, struct in_addr *hostids, char *buf,
                 unsigned int *bufsize)
{
    char *cp             = buf;
    unsigned int maxsize = *bufsize;
    *bufsize             = 0;

    /* Copy each down server's address into the buffer. */
    for (int i = 0; i < nservers; i++) {
        srvent *s = FindServer(&hostids[i]);
        if (s && s->ServerIsDown()) {
            /* Make sure there is room in the buffer for this entry. */
            if ((cp - buf) + sizeof(struct in_addr) > maxsize)
                return;

            memcpy(cp, &s->host, sizeof(struct in_addr));
            cp += sizeof(struct in_addr);
        }
    }

    /* Null terminate the list.  Make sure there is room in the buffer for the
     * terminator. */
    if ((cp - buf) + sizeof(struct in_addr) > maxsize)
        return;
    memset(cp, 0, sizeof(struct in_addr));
    cp += sizeof(struct in_addr);

    *bufsize = (cp - buf);
}

/*
 * Update bandwidth estimates for all up servers.
 * Reset estimates and declare connectivity strong if there are
 * no recent observations.  Called by the probe daemon.
 */
void CheckServerBW(long curr_time)
{
    srv_iterator next;
    srvent *s;
    unsigned long bw = INIT_BW;

    while ((s = next())) {
        if (s->ServerIsUp())
            (void)s->GetBandwidth(&bw);
    }
}

void ServerPrint()
{
    ServerPrint(stdout);
}

void ServerPrint(FILE *f)
{
    if (srvent::srvtab == 0)
        return;

    fprintf(f, "Servers: count = %d\n", srvent::srvtab->count());

    srv_iterator next;
    srvent *s;
    while ((s = next()))
        s->print(f);

    fprintf(f, "\n");
}

srvent::srvent(struct in_addr *Host, RealmId realm)
{
    LOG(1, ("srvent::srvent: host = %s\n", inet_ntoa(*Host)));

    struct hostent *h =
        gethostbyaddr((char *)Host, sizeof(struct in_addr), AF_INET);
    if (h) {
        name = new char[strlen(h->h_name) + 1];
        strcpy(name, h->h_name);
        TRANSLATE_TO_LOWER(name);
    } else {
        name = new char[16];
        sprintf(name, "%s", inet_ntoa(*Host));
    }

    host           = *Host;
    realmid        = realm;
    connid         = -1;
    Xbinding       = 0;
    probeme        = 0;
    bw             = INIT_BW;
    lastobs.tv_sec = lastobs.tv_usec = 0;
    refcount                         = 1;
    fetchpartial_support             = 0;

#ifdef VENUSDEBUG
    allocs++;
#endif
}

srvent::~srvent()
{
#ifdef VENUSDEBUG
    deallocs++;
#endif

    LOG(1, ("srvent::~srvent: host = %s, conn = %d\n", name, connid));

    srvent::srvtab->remove(&tblhandle);

    Reset();

    delete[] name;
}

int srvent::Connect(RPC2_Handle *cidp, int *authp, uid_t uid, int Force)
{
    LOG(100, ("srvent::Connect: host = %s, uid = %d, force = %d\n", name, uid,
              Force));

    int code = 0;

    /* See whether this server is down or already binding. */
    for (;;) {
        if (ServerIsDown() && !Force) {
            LOG(100, ("srvent::Connect: server (%s) is down\n", name));
            return (ETIMEDOUT);
        }

        if (!Xbinding)
            break;
        if (VprocInterrupted())
            return (EINTR);
        Srvr_Wait();
        if (VprocInterrupted())
            return (EINTR);
    }

    /* Get the user entry and attempt to connect to it. */
    Xbinding = 1;
    {
        Realm *realm = REALMDB->GetRealm(realmid);
        userent *u   = realm->GetUser(uid);
        code         = u->Connect(cidp, authp, &host);
        PutUser(&u);
        realm->PutRef();
    }
    Xbinding = 0;
    Srvr_Signal();

    /* Interpret result. */
    if (code < 0)
        switch (code) {
        case RPC2_NOTAUTHENTICATED:
            code = EPERM;
            break;

        case RPC2_NOBINDING:
        case RPC2_SEFAIL2:
        case RPC2_FAIL:
            code = ETIMEDOUT;
            break;

        default:
            /*
		CHOKE("srvent::Connect: illegal RPC code (%s)", RPC2_ErrorMsg(code));
*/
            code = ETIMEDOUT;
            break;
        }
    if (!ServerIsDown() && code == ETIMEDOUT) {
        /* Not already considered down. */
        MarinerLog("connection::unreachable %s\n", name);
        Reset();
    }

    if (code == ETIMEDOUT && VprocInterrupted())
        return (EINTR);
    return (code);
}

int srvent::GetStatistics(ViceStatistics *Stats)
{
    LOG(100, ("srvent::GetStatistics: host = %s\n", name));

    int code   = 0;
    connent *c = 0;

    memset(Stats, 0, sizeof(ViceStatistics));

    code = GetConn(&c, ANYUSER_UID);
    if (code != 0)
        goto Exit;

    /* Make the RPC call. */
    MarinerLog("fetch::GetStatistics %s\n", name);
    UNI_START_MESSAGE(ViceGetStatistics_OP);
    code = (int)ViceGetStatistics(c->connid, Stats);
    UNI_END_MESSAGE(ViceGetStatistics_OP);
    MarinerLog("fetch::getstatistics done\n");
    code = c->CheckResult(code, 0);
    UNI_RECORD_STATS(ViceGetStatistics_OP);

Exit:
    PutConn(&c);
    return (code);
}

void srvent::Reset()
{
    LOG(1, ("srvent::Reset: host = %s\n", name));

    /* Unbind callback connection for this server. */
    if (connid) {
        int code = (int)RPC2_Unbind(connid);
        LOG(1, ("srvent::Reset: RPC2_Unbind -> %s\n", RPC2_ErrorMsg(code)));
        connid = 0;
    }

    /* Kill all direct connections to this server. */
    {
        struct ConnKey Key;
        Key.host = host;
        Key.uid  = ANYUSER_UID;
        conn_iterator conn_next(&Key);
        connent *c = 0;
        while ((c = conn_next()))
            c->Suicide();
    }

    /* Send a downevent to volumes associated with this server */
    /* Also kills all indirect connections to the server. */
    VDB->DownEvent(&host);
}

void srvent::ServerError(int *codep)
{
    LOG(1,
        ("srvent::ServerError: %s error (%s)\n", name, RPC2_ErrorMsg(*codep)));

    /* Translate the return code. */
    switch (*codep) {
    case RPC2_FAIL:
    case RPC2_NOCONNECTION:
    case RPC2_TIMEOUT:
    case RPC2_DEAD:
    case RPC2_SEFAIL2:
        *codep = ETIMEDOUT;
        break;

    case RPC2_SEFAIL1:
    case RPC2_SEFAIL3:
    case RPC2_SEFAIL4:
        *codep = EIO;
        break;

    case RPC2_NAKED:
    case RPC2_NOTCLIENT:
        *codep = ERETRY;
        break;

    case RPC2_INVALIDOPCODE:
        *codep = EOPNOTSUPP;
        break;

    default:
        /* Map RPC2 warnings into EINVAL. */
        if (*codep > RPC2_ELIMIT) {
            *codep = EINVAL;
            break;
        }
        CHOKE("srvent::ServerError: illegal RPC code (%d)", *codep);
    }

    if (!ServerIsDown()) {
        /* Reset if TIMED'out or NAK'ed. */
        switch (*codep) {
        case ETIMEDOUT:
            MarinerLog("connection::unreachable %s\n", name);
            Reset();
            break;

        case ERETRY:
            /* Must have missed a down event! */
            eprint("%s nak'ed", name);
            Reset();
            connid = -2;
            VDB->UpEvent(&host);
            break;

        default:
            break;
        }
    }
}

void srvent::ServerUp(RPC2_Handle newconnid)
{
    LOG(1, ("srvent::ServerUp: %s, connid = %d, newconnid = %d\n", name, connid,
            newconnid));

    switch (connid) {
    case 0:
        MarinerLog("connection::up %s\n", name);
        connid = newconnid;
        VDB->UpEvent(&host);
        break;

    case -1:
        /* Initial case.  */
        connid = newconnid;
        VDB->UpEvent(&host);
        break;

    case -2:
        /* Following NAK.  Don't signal another UpEvent! */
        connid = newconnid;
        break;

    default:
        /* Already considered up.  Must have missed a down event! */
        Reset();
        connid = newconnid;
        VDB->UpEvent(&host);
    }

    /* Poke any threads waiting for a change in communication state. */
    Rtry_Signal();
}

long srvent::GetLiveness(struct timeval *tp)
{
    long rc = 0;
    struct timeval t;

    LOG(100, ("srvent::GetLiveness (%s)\n", name));

    tp->tv_sec = tp->tv_usec = 0;
    t.tv_sec = t.tv_usec = 0;

    /* we don't have a real connid if the server is down or "quasi-up" */
    if (connid <= 0)
        return (ETIMEDOUT);

    /* Our peer is at the other end of the callback connection */
    if ((rc = RPC2_GetPeerLiveness(connid, tp, &t)) != RPC2_SUCCESS)
        return (rc);

    LOG(100, ("srvent::GetLiveness: (%s), RPC %ld.%0ld, SE %ld.%0ld\n", name,
              tp->tv_sec, tp->tv_usec, t.tv_sec, t.tv_usec));

    if (tp->tv_sec < t.tv_sec ||
        (tp->tv_sec == t.tv_sec && tp->tv_usec < t.tv_usec))
        *tp = t; /* structure assignment */

    return (0);
}

/*
 * calculates current bandwidth to server, taking the current estimates from
 * RPC2/SFTP.
 *
 * Triggers weakly/strongly connected transitions if appropriate.
 */

/* returns bandwidth in Bytes/sec, or INIT_BW if it couldn't be obtained */
long srvent::GetBandwidth(unsigned long *Bandwidth)
{
    long rc             = 0;
    unsigned long oldbw = bw;
    unsigned long bwmin, bwmax;

    LOG(1, ("srvent::GetBandwidth (%s) lastobs %ld.%06ld\n", name,
            lastobs.tv_sec, lastobs.tv_usec));

    /* we don't have a real connid if the server is down or "quasi-up" */
    if (connid <= 0)
        return (ETIMEDOUT);

    /* retrieve the bandwidth information from RPC2 */
    if ((rc = RPC2_GetBandwidth(connid, &bwmin, &bw, &bwmax)) != RPC2_SUCCESS)
        return (rc);

    LOG(1, ("srvent:GetBandWidth: --> new BW %d bytes/sec\n", bw));

    /* update last observation time */
    RPC2_GetLastObs(connid, &lastobs);

    *Bandwidth = bw;
    /* only report new bandwidth estimate if it has changed by more than ~10% */
    if ((bw > (oldbw + oldbw / 8)) || (bw < (oldbw - oldbw / 8))) {
        MarinerLog("connection::bandwidth %s %d %d %d\n", name, bwmin, bw,
                   bwmax);
    }
    LOG(1,
        ("srvent::GetBandwidth (%s) returns %d bytes/sec\n", name, *Bandwidth));
    return (0);
}

void srvent::print(FILE *f)
{
    fprintf(f, "%p : %-16s : cid = %d, host = %s, binding = %d, bw = %ld\n",
            this, name, (int)connid, inet_ntoa(host), Xbinding, bw);
    PrintRef(f);
}

srv_iterator::srv_iterator()
    : olist_iterator((olist &)*srvent::srvtab)
{
}

srvent *srv_iterator::operator()()
{
    olink *o = olist_iterator::operator()();
    if (!o)
        return (0);

    srvent *s = strbase(srvent, o, tblhandle);
    return (s);
}

/* hooks in librpc2 that existed for the old libfail functionality. We use
 * these leftover hooks to implement cfs disconnect and cfs reconnect */
extern int (*Fail_SendPredicate)(unsigned char ip1, unsigned char ip2,
                                 unsigned char ip3, unsigned char ip4,
                                 unsigned char color, RPC2_PacketBuffer *pb,
                                 struct sockaddr_in *sin, int fd);
extern int (*Fail_RecvPredicate)(unsigned char ip1, unsigned char ip2,
                                 unsigned char ip3, unsigned char ip4,
                                 unsigned char color, RPC2_PacketBuffer *pb,
                                 struct sockaddr_in *sin, int fd);

static int DropPacket(unsigned char ip1, unsigned char ip2, unsigned char ip3,
                      unsigned char ip4, unsigned char color,
                      RPC2_PacketBuffer *pb, struct sockaddr_in *sin, int fd)
{
    /* Tell rpc2 to drop the packet */
    return 0;
}

int FailDisconnect(int nservers, struct in_addr *hostids)
{
    Fail_SendPredicate = Fail_RecvPredicate = &DropPacket;
    return 0;
}

int FailReconnect(int nservers, struct in_addr *hostids)
{
    Fail_SendPredicate = Fail_RecvPredicate = NULL;
    return 0;
}

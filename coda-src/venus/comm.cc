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
 * Implementation of the Venus Communications subsystem.
 *
 *    This module should be split up into:
 *        1. A subsystem independent module, libcomm.a, containing base classes srvent and connent.
 *        2. A subsystem dependent module containing derived classes v_srvent and v_connent.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

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
#ifdef __CYGWIN__
#include <cygwinextra.h>
#endif
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include "coda_string.h"
#include <struct.h>
#include <unistd.h>
#include <stdlib.h>

#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/fail.h>
#include <rpc2/errors.h>

extern int Fcon_Init(); 
extern void SFTP_SetDefaults (SFTP_Initializer *initPtr);
extern void SFTP_Activate (SFTP_Initializer *initPtr);

/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from vv */
#include <inconsist.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "user.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvm.h"
#include "venusvol.h"
#include "vproc.h"
#include "adv_monitor.h"
#include "adv_daemon.h"


int COPModes = 6;	/* ASYNCCOP2 | PIGGYCOP2 */
int UseMulticast = 0;
char myHostName[MAXHOSTNAMELEN];
int rpc2_retries = UNSET_RT;
int rpc2_timeout = UNSET_TO;
int sftp_windowsize = UNSET_WS;
int sftp_sendahead = UNSET_SA;
int sftp_ackpoint = UNSET_AP;
int sftp_packetsize = UNSET_PS;
int rpc2_timeflag = UNSET_ST;
int mrpc2_timeflag = UNSET_MT;
unsigned long WCThresh = UNSET_WCT;  	/* in Bytes/sec */
int WCStale = UNSET_WCS;	/* seconds */

int RoundRobin = 1;
int AllowIPAddrs = 1;

extern long RPC2_Perror;
struct CommQueueStruct CommQueue;

#define MAXFILTERS 32

struct FailFilterInfoStruct {
	unsigned id;
	unsigned long host;
	FailFilterSide side;
	unsigned char used;
} FailFilterInfo[MAXFILTERS];


static int VSG_HashFN(void *);

char mgrpent::mgrp_sync;
olist *srvent::srvtab;
char srvent::srvtab_sync;
olist *connent::conntab;
char connent::conntab_sync;

#ifdef	VENUSDEBUG
int connent::allocs = 0;
int connent::deallocs = 0;
int srvent::allocs = 0;
int srvent::deallocs = 0;
int mgrpent::allocs = 0;
int mgrpent::deallocs = 0;
#endif	VENUSDEBUG


void CommInit() {
    /* Initialize unset command-line parameters. */
    if (rpc2_retries == UNSET_RT) rpc2_retries = DFLT_RT;
    if (rpc2_timeout == UNSET_TO) rpc2_timeout = DFLT_TO;
    if (sftp_windowsize == UNSET_WS) sftp_windowsize = DFLT_WS;
    if (sftp_sendahead == UNSET_SA) sftp_sendahead = DFLT_SA;
    if (sftp_ackpoint == UNSET_AP) sftp_ackpoint = DFLT_AP;
    if (sftp_packetsize == UNSET_PS) sftp_packetsize = DFLT_PS;
    if (rpc2_timeflag == UNSET_ST)
	srv_ElapseSwitch = DFLT_ST;
    else
	srv_ElapseSwitch = rpc2_timeflag;
    if (mrpc2_timeflag == UNSET_MT)
	srv_MultiStubWork[0].opengate = DFLT_MT;
    else
	srv_MultiStubWork[0].opengate = mrpc2_timeflag;

    if (WCThresh == UNSET_WCT) WCThresh = DFLT_WCT;
    if (WCStale == UNSET_WCS) WCStale = DFLT_WCS;

    /* Sanity check COPModes. */
    if ( (ASYNCCOP1 && !ASYNCCOP2) ||
	 (PIGGYCOP2 && !ASYNCCOP2) )
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

    /* Create server entries for each bootstrap host. */
    int hcount = 0;
    for (char *hp = fsname; hp;) {
	/* Parse the next entry in the hostname list. */
	char ServerName[MAXHOSTNAMELEN];
	char *cp = index(hp, ',');

	if (cp) {
	    /* This is not the last hostname. */
	    int len = cp - hp;
	    strncpy(ServerName, hp, len);
	    ServerName[len] = 0;
	    hp = cp + 1;
	}
	else {
	    /* This is the last hostname. */
	    strcpy(ServerName, hp);
	    hp = 0;
	}

	/* Get the host address and make a server entry. */
	struct in_addr  addr;
	struct hostent *h;
	srvent         *s = NULL;
	/* Allow use of IP addrs */
	if (AllowIPAddrs && inet_aton(ServerName, &addr) != 0) {
	    s = new srvent(&addr, 1);
	} else if ((h = gethostbyname(ServerName)) != NULL) {
	    s = new srvent((struct in_addr *)h->h_addr, 1);
	}
        if (s != NULL) {
	    srvent::srvtab->insert(&s->tblhandle);
	    hcount++;
        }
    }
    if (hcount == 0)
	CHOKE("CommInit: no bootstrap server");

    RPC2_Perror = 0;

    /* Port initialization. */
    RPC2_PortIdent port1;
    port1.Tag = RPC2_PORTBYINETNUMBER;

    struct servent *s = getservbyname("venus", "udp");
    if (s != 0)
	port1.Value.InetPortNumber = s->s_port;
    else {
	eprint("getservbyname(venus,udp) failed, using 2430/udp\n");
	port1.Value.InetPortNumber = htons(2430);
    }

    /* SFTP initialization. */
    SFTP_Initializer sei;
    SFTP_SetDefaults(&sei);
    sei.WindowSize = sftp_windowsize;
    sei.SendAhead = sftp_sendahead;
    sei.AckPoint = sftp_ackpoint;
    sei.PacketSize = sftp_packetsize;
    sei.EnforceQuota = 1;
    sei.Port.Tag = RPC2_PORTBYINETNUMBER;
    s = getservbyname("venus-se", "udp");
    if (s != 0) 
	sei.Port.Value.InetPortNumber = s->s_port;
    else {
	eprint("getservbyname(venus-se,udp) failed, using 2431/udp\n");
	sei.Port.Value.InetPortNumber = htons(2431);
    }

    SFTP_Activate(&sei);

    /* RPC2 initialization. */
    struct timeval tv;
    tv.tv_sec = rpc2_timeout;
    tv.tv_usec = 0;
    if (RPC2_Init(RPC2_VERSION, 0, &port1, rpc2_retries, &tv) != RPC2_SUCCESS)
	CHOKE("CommInit: RPC2_Init failed");

    /* Failure package initialization. */
    memset((void *)FailFilterInfo, 0, (int) (MAXFILTERS * sizeof(struct FailFilterInfoStruct)));
    Fail_Initialize("venus", 0);
    Fcon_Init();

    /* Fire up the probe daemon. */
    PROD_Init();
}


/* *****  Connection  ***** */

const int MAXCONNSPERUSER = 9;		    /* Max simultaneous conns per user per server. */

#define	CONNQ_LOCK()
#define	CONNQ_UNLOCK()
#define	CONNQ_WAIT()	    VprocWait((char *)&connent::conntab_sync)
#define	CONNQ_SIGNAL()	    VprocSignal((char *)&connent::conntab_sync)

void Conn_Wait() {
    CONNQ_LOCK();
    LOG(0, ("WAITING(CONNQ):\n"));
    START_TIMING();
    CONNQ_WAIT();
    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
    CONNQ_UNLOCK();
}


void Conn_Signal() {
    CONNQ_LOCK();
    CONNQ_SIGNAL();
    CONNQ_UNLOCK();
}


/* Get a connection to any server (as root). */
int GetAdmConn(connent **cpp) {
    LOG(100, ("GetAdmConn: \n"));

    *cpp = 0;
    int code = 0;

    /* Get a connection to any custodian. */
    for (;;) {
	int tryagain = 0;
	srv_iterator next;
	srvent *s;
	while ((s = next())) {
            if (!s->IsRootServer()) continue;
	    code = GetConn(cpp, &s->host, V_UID, 0);
	    switch(code) {
		case 0:
		    return(0);

		case ERETRY:
		    tryagain = 1;
		    continue;

		case EINTR:
		    return(EINTR);

		case ETIMEDOUT:
		    continue;

		default:
		    if (code < 0)
			CHOKE("GetAdmConn: bogus code (%d)", code);
		    return(code);
	    }
	}
	if (tryagain) continue;

	return(ETIMEDOUT);
    }
}


int GetConn(connent **cpp, struct in_addr *host, vuid_t vuid, int Force)
{
    LOG(100, ("GetConn: host = %s, vuid = %d, force = %d\n",
              inet_ntoa(*host), vuid, Force));

    *cpp = 0;
    int code = 0;
    connent *c = 0;
    int found = 0;

    /* Grab an existing connection if one is free. */
    /* Before creating a new connection, make sure the per-user limit is not exceeded. */
    for (;;) {
	/* Check whether there is already a free connection. */
	struct ConnKey Key; Key.host = *host; Key.vuid = vuid;
	conn_iterator next(&Key);
	int count = 0;
	while ((c = next())) {
	    count++;
	    if (!c->inuse) {
		c->inuse = 1;
		found = 1;
		break;
	    }
	}
	if (found) break;

	/* Wait here if MAX conns are already in use. */
	/* Synchronization needs fixed for MP! -JJK */
	if (count < MAXCONNSPERUSER) break;
	if (VprocInterrupted()) return(EINTR);
/*	    RPCOpStats.RPCOps[opcode].busy++;*/
	Conn_Wait();
	if (VprocInterrupted()) return(EINTR);
    }

    if (!found) {
	/* Try to connect to the server on behalf of the user. */
	srvent *s = 0;
	GetServer(&s, host);
	RPC2_Handle ConnHandle = 0;
	int auth = 0;
	code = s->Connect(&ConnHandle, &auth, vuid, Force);
	PutServer(&s);

	switch(code) {
	    case 0:
		break;

	    case EINTR:
		return(EINTR);

	    case EPERM:
	    case ERETRY:
		return(ERETRY);

	    default:
		return(ETIMEDOUT);
	}

	/* Create and install the new connent. */
	c = new connent(host, vuid, ConnHandle, auth);
	c->inuse = 1;
	connent::conntab->insert(&c->tblhandle);
    }

    *cpp = c;
    return(0);
}


void PutConn(connent **cpp)
{
    connent *c = *cpp;
    *cpp = 0;
    if (c == 0) {
	LOG(100, ("PutConn: null conn\n"));
	return;
    }

    LOG(100, ("PutConn: host = %s, uid = %d, cid = %d, auth = %d\n",
	     inet_ntoa(c->Host), c->uid, c->connid, c->authenticated));

    if (!c->inuse)
	{ c->print(logFile); CHOKE("PutConn: conn not in use"); }

    if (c->dying) {
	connent::conntab->remove(&c->tblhandle);
	delete c;
    }
    else {
	c->inuse = 0;
    }

    Conn_Signal();
}


void ConnPrint() {
    ConnPrint(stdout);
}


void ConnPrint(FILE *fp) {
    fflush(fp);
    ConnPrint(fileno(fp));
}


void ConnPrint(int fd) {
    if (connent::conntab == 0) return;

    fdprint(fd, "Connections: count = %d\n", connent::conntab->count());

    /* Iterate through the individual entries. */
    conn_iterator next;
    connent *c;
    while ((c = next())) c->print(fd);

    fdprint(fd, "\n");
}


connent::connent(struct in_addr *host, vuid_t vuid, RPC2_Handle cid, int authflag)
{
    LOG(1, ("connent::connent: host = %s, uid = %d, cid = %d, auth = %d\n",
	     inet_ntoa(*host), vuid, cid, authflag));

    /* These members are immutable. */
    Host = *host;
    uid = vuid;
    connid = cid;
    authenticated = authflag;

    /* These members are mutable. */
    inuse = 0;
    dying = 0;

#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


connent::~connent() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG

    LOG(1, ("connent::~connent: host = %#08x, uid = %d, cid = %d, auth = %d\n",
	    Host, uid, connid, authenticated));

    int code = (int) RPC2_Unbind(connid);
    connid = 0;
    LOG(1, ("connent::~connent: RPC2_Unbind -> %s\n", RPC2_ErrorMsg(code)));
}


int connent::Suicide(int disconnect)
{
    LOG(1, ("connent::Suicide: disconnect = %d\n", disconnect));

    /* Mark this conn as dying. */
    dying = 1;

    /* Can't do any more if it is busy. */
    if (inuse) return(0);

    inuse = 1;

    /* Be nice and disconnect if requested. */
    if (disconnect) {
	/* Make the RPC call. */
	MarinerLog("fetch::DisconnectFS %s\n", (FindServer(&Host))->name);
	UNI_START_MESSAGE(ViceDisconnectFS_OP);
	int code = (int) ViceDisconnectFS(connid);
	UNI_END_MESSAGE(ViceDisconnectFS_OP);
	MarinerLog("fetch::disconnectfs done\n");
	code = CheckResult(code, 0);
	UNI_RECORD_STATS(ViceDisconnectFS_OP);
    }

    connent *myself = this; // needed because &this is illegal
    PutConn(&myself);

    return(1);
}


/* Maps return codes from Vice:
	0		Success,
	EINTR		Call was interrupted,
	ETIMEDOUT	Host did not respond,
	ERETRY		Retryable error,
	Other (> 0)	Non-retryable error (valid kernel return code).
*/
int connent::CheckResult(int code, VolumeId vid, int TranslateEINCOMP) {
    LOG(100, ("connent::CheckResult: code = %d, vid = %x\n",
	     code, vid));

    /* ViceOp succeeded. */
    if (code == 0) return(0);

    /* Translate RPC and Volume errors, and update server state. */
    switch(code) {
	default:
	    if (code < 0) {
		srvent *s = 0;
		GetServer(&s, &Host);
		s->ServerError(&code);
		PutServer(&s);
	    }
	    if (code == ETIMEDOUT || code == ERETRY) {
		dying = 1;
	    }
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

    if (code == ETIMEDOUT && VprocInterrupted()) return(EINTR);
    return(code);
}


void connent::print(int fd) {
    fdprint(fd, "%#08x : host = %s, uid = %d, cid = %d, auth = %d, inuse = %d, dying = %d\n",
	     (long)this, inet_ntoa(Host), uid, connid, authenticated, inuse, dying);
}


conn_iterator::conn_iterator(struct ConnKey *Key) : olist_iterator((olist&)*connent::conntab) {
    key = Key;
}


connent *conn_iterator::operator()() {
    olink *o;
    while ((o = olist_iterator::operator()())) {
	connent *c = strbase(connent, o, tblhandle);
	if (key == (struct ConnKey *)0) return(c);
	if ((key->host.s_addr == c->Host.s_addr ||
             key->host.s_addr == INADDR_ANY) &&
	    (key->vuid == c->uid || key->vuid == ALL_UIDS))
	    return(c);
    }

    return(0);
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

#define	SRVRQ_LOCK()
#define	SRVRQ_UNLOCK()
#define	SRVRQ_WAIT()	    VprocWait((char *)&srvent::srvtab_sync)
#define	SRVRQ_SIGNAL()	    VprocSignal((char *)&srvent::srvtab_sync)

void Srvr_Wait() {
    SRVRQ_LOCK();
    LOG(0, ("WAITING(SRVRQ):\n"));
    START_TIMING();
    SRVRQ_WAIT();
    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
    SRVRQ_UNLOCK();
}


void Srvr_Signal() {
    SRVRQ_LOCK();
    LOG(10, ("SIGNALLING(SRVRQ):\n"));
    SRVRQ_SIGNAL();
    SRVRQ_UNLOCK();
}


srvent *FindServer(struct in_addr *host)
{
    srv_iterator next;
    srvent *s;

    while ((s = next()))
	if (s->host.s_addr == host->s_addr)
            return(s);

    return(0);
}


srvent *FindServerByCBCid(RPC2_Handle connid)
{
    if (connid == 0) return(0);

    srv_iterator next;
    srvent *s;

    while ((s = next()))
	if (s->connid == connid) return(s);

    return(0);
}


void GetServer(srvent **spp, struct in_addr *host)
{
    LOG(100, ("GetServer: host = %s\n", inet_ntoa(*host)));
    CODA_ASSERT(host != 0);

    srvent *s = FindServer(host);
    if (s) {
	*spp = s;
	return;
    }

    s = new srvent(host, 0);
    srvent::srvtab->insert(&s->tblhandle);

    *spp = s;
}


void PutServer(srvent **spp)
{
    LOG(100, ("PutServer: \n"));

    *spp = 0;
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

probeslave::probeslave(ProbeSlaveTask Task, void *Arg, void *Result, char *Sync) :
	vproc("ProbeSlave", NULL, VPT_ProbeDaemon, 16384) {
    LOG(100, ("probeslave::probeslave(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    task = Task;
    arg = Arg;
    result = Result;
    sync = Sync;

    /* Poke main procedure. */
    start_thread();
}


void probeslave::main(void)
{
    switch(task) {
	case ProbeUpServers:
	    ProbeServers(1);
	    break;

	case ProbeDownServers:
	    ProbeServers(0);
	    break;

	case BindToServer:
	    {
	    /* *result gets pointer to connent on success, 0 on failure. */
	    struct in_addr *host = (struct in_addr *)arg;
	    (void)GetConn((connent **)result, host, V_UID, 1);
	    }
	    break;

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

    /* Hosts and Connections are arrays of addresses and connents respectively representing the servers to */
    /* be probed.  HowMany is the current size of these arrays, and ix is the number of entries actually used. */
    const int GrowSize = 32;
    int HowMany = GrowSize;
    struct in_addr *Hosts = (struct in_addr *)malloc(HowMany * sizeof(struct in_addr));
    int ix = 0;

    /* Fill in the Hosts array for each server that is to be probed. */
    {
	srv_iterator next;
	srvent *s;
	while ((s = next())) {
	    if (!s->probeme ||
		(Up && s->ServerIsDown()) || (!Up && !s->ServerIsDown())) continue;

	    /* Grow the Hosts array if necessary. */
	    if (ix == HowMany) {
		/* I am terrified of realloc */
		HowMany += GrowSize;
		struct in_addr *newHosts = (struct in_addr *)malloc(HowMany * sizeof(struct in_addr));
		memcpy(newHosts, Hosts, ix * sizeof(struct in_addr));
		free(Hosts);
		Hosts = newHosts;
	    }

	    /* Stuff the address in the Hosts array. */
	    Hosts[ix] = s->host;
	    ix++;
	}
    }

    if (ix > 0) DoProbes(ix, Hosts);

    /* the incorrect "free" in DoProbes() is moved here */
    free(Hosts);
}


void DoProbes(int HowMany, struct in_addr *Hosts)
{
    connent **Connections = 0;
    int i;

    CODA_ASSERT(HowMany > 0);

    /* Bind to the servers. */
    Connections = (connent **)malloc(HowMany * sizeof(connent *));
    memset(Connections, 0, HowMany * sizeof(connent *));
    MultiBind(HowMany, Hosts, Connections);

    /* Probe them. */
    int AnyHandlesValid = 0;
    RPC2_Handle *Handles = (RPC2_Handle *)malloc(HowMany * sizeof(RPC2_Handle));
    for (i = 0; i < HowMany; i++) {
	if (Connections[i] == 0) { Handles[i] = 0; continue; }

	AnyHandlesValid = 1;
	Handles[i] = (Connections[i])->connid;
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
    if (LogLevel >= 1) {
	dprint("MultiBind: HowMany = %d\n\tHosts = [ ", HowMany);
	for (int i = 0; i < HowMany; i++)
	    fprintf(logFile, "%s ", inet_ntoa(Hosts[i]));
	fprintf(logFile, "]\n");
    }

    int slaves = 0;
    char slave_sync = 0;
    for (int ix = 0; ix < HowMany; ix++) {
	/* Try to get a connection without forcing a bind. */
	connent *c = 0;
	if (GetConn(&c, &Hosts[ix], V_UID, 0) == 0) {
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
    if (LogLevel >= 1) {
	dprint("MultiProbe: HowMany = %d\n\tHandles = [ ", HowMany);
	for (int i = 0; i < HowMany; i++)
	    fprintf(logFile, "%lx ", Handles[i]);
	fprintf(logFile, "]\n");
    }

    /* Make multiple copies of the IN/OUT and OUT parameters. */
    RPC2_Unsigned  **secs_ptrs =
	(RPC2_Unsigned **)malloc(HowMany * sizeof(RPC2_Unsigned *));
    CODA_ASSERT(secs_ptrs);
    RPC2_Unsigned   *secs_bufs =
	(RPC2_Unsigned *)malloc(HowMany * sizeof(RPC2_Unsigned));
    CODA_ASSERT(secs_bufs);
    for (int i = 0; i < HowMany; i++)
	secs_ptrs[i] = &secs_bufs[i]; 
    RPC2_Integer  **usecs_ptrs =
	(RPC2_Integer **)malloc(HowMany * sizeof(RPC2_Integer *));
    CODA_ASSERT(usecs_ptrs);
    RPC2_Integer   *usecs_bufs =
	(RPC2_Integer *)malloc(HowMany * sizeof(RPC2_Integer));
    CODA_ASSERT(usecs_bufs);
    for (int ii = 0; ii < HowMany; ii++)
	usecs_ptrs[ii] = &usecs_bufs[ii]; 

    /* Make the RPC call. */
    MarinerLog("fetch::Probe\n");
    MULTI_START_MESSAGE(ViceGetTime_OP);
    int code = (int) MRPC_MakeMulti(ViceGetTime_OP, ViceGetTime_PTR,
			       HowMany, Handles, (RPC2_Integer *)0, 0,
			       (long (*)(...))&HandleProbe, 0, secs_ptrs, usecs_ptrs);
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


long HandleProbe(int HowMany, RPC2_Handle *Handles, long offset, long rpcval)
{
    RPC2_Handle RPCid = Handles[offset];

    if (RPCid != 0) {
	/* Get the {host,port} pair for this call. */
	RPC2_PeerInfo thePeer;
	long rc = RPC2_GetPeerInfo(RPCid, &thePeer);
	if (thePeer.RemoteHost.Tag != RPC2_HOSTBYINETADDR ||
	    thePeer.RemotePort.Tag != RPC2_PORTBYINETNUMBER) {
	    LOG(0, ("HandleProbe: RPC2_GetPeerInfo return code = %d\n", rc));
	    LOG(0, ("HandleProbe: thePeer.RemoteHost.Tag = %d\n", thePeer.RemoteHost.Tag));
	    LOG(0, ("HandleProbe: thePeer.RemotePort.Tag = %d\n", thePeer.RemotePort.Tag));
	    return 0;
	    /* CHOKE("HandleProbe: getpeerinfo returned bogus type!"); */
	}

	/* Locate the server and update its status. */
	srvent *s = FindServer(&thePeer.RemoteHost.Value.InetAddress);
	if (!s)
	    CHOKE("HandleProbe: no srvent (RPCid = %d, PeerHost = %s)",
                  RPCid, inet_ntoa(thePeer.RemoteHost.Value.InetAddress));
	LOG(1, ("HandleProbe: (%s, %d)\n", s->name, rpcval));
	if (rpcval < 0)
	    s->ServerError((int *)&rpcval);
    }

    return(0);
}


/* Report which servers are down. */
void DownServers(char *buf, unsigned int *bufsize)
{
    char *cp = buf;
    unsigned int maxsize = *bufsize;
    *bufsize = 0;

    /* Copy each down server's address into the buffer. */
    srv_iterator next;
    srvent *s;
    while ((s = next()))
	if (s->ServerIsDown()) {
	    /* Make sure there is room in the buffer for this entry. */
	    if ((cp - buf) + sizeof(struct in_addr) > maxsize) return;

	    memcpy(cp, &s->host, sizeof(struct in_addr));
	    cp += sizeof(struct in_addr);
	}

    /* Null terminate the list.  Make sure there is room in the buffer for the
     * terminator. */
    if ((cp - buf) + sizeof(struct in_addr) > maxsize) return;
    memset(cp, 0, sizeof(struct in_addr));
    cp += sizeof(struct in_addr);

    *bufsize = (cp - buf);
}


/* Report which of a given set of servers is down. */
void DownServers(int nservers, struct in_addr *hostids,
                 char *buf, unsigned int *bufsize)
{
    char *cp = buf;
    unsigned int maxsize = *bufsize;
    *bufsize = 0;

    /* Copy each down server's address into the buffer. */
    for (int i = 0; i < nservers; i++) {
	srvent *s = FindServer(&hostids[i]);
	if (s && s->ServerIsDown()) {
	    /* Make sure there is room in the buffer for this entry. */
	    if ((cp - buf) + sizeof(struct in_addr) > maxsize) return;

	    memcpy(cp, &s->host, sizeof(struct in_addr));
	    cp += sizeof(struct in_addr);
	}
    }

    /* Null terminate the list.  Make sure there is room in the buffer for the
     * terminator. */
    if ((cp - buf) + sizeof(struct in_addr) > maxsize) return;
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
	    (void) s->GetBandwidth(&bw);
    }
}


void ServerPrint() {
    ServerPrint(stdout);
}


void ServerPrint(FILE *fp) {
    fflush(fp);
    ServerPrint(fileno(fp));
}


void ServerPrint(int fd)
{
    if (srvent::srvtab == 0) return;

    fdprint(fd, "Servers: count = %d\n", srvent::srvtab->count());

    srv_iterator next;
    srvent *s;
    while ((s = next())) s->print(fd);

    fdprint(fd, "\n");
}


srvent::srvent(struct in_addr *Host, int isrootserver)
{
    LOG(1, ("srvent::srvent: host = %s, isroot = %d\n",
            inet_ntoa(*Host), isrootserver));

    struct hostent *h = gethostbyaddr((char *)Host, sizeof(struct in_addr), AF_INET);
    if (h) {
	name = new char[strlen(h->h_name) + 1];
	strcpy(name, h->h_name);
	TRANSLATE_TO_LOWER(name);
    }
    else {
	name = new char[16];
	sprintf(name, "%s", inet_ntoa(*Host));
    }

    host = *Host;
    connid = -1;
    Xbinding = 0;
    probeme = 0;
    forcestrong = 0;
    rootserver = isrootserver;
    isweak = 0;
    bw     = INIT_BW;
    bwmax  = 0;
    timerclear(&lastobs);

#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


srvent::~srvent() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG

    LOG(1, ("srvent::~srvent: host = %s, conn = %d\n", name, connid));

    delete name;

    int code = (int) RPC2_Unbind(connid);
    LOG(1, ("srvent::~srvent: RPC2_Unbind -> %s\n", RPC2_ErrorMsg(code)));
}


int srvent::Connect(RPC2_Handle *cidp, int *authp, vuid_t vuid, int Force)
{
    LOG(100, ("srvent::Connect: host = %s, uid = %d, force = %d\n",
	     name, vuid, Force));

    int code = 0;

    /* See whether this server is down or already binding. */
    for (;;) {
	if (ServerIsDown() && !Force) {
	    LOG(100, ("srvent::Connect: server (%s) is down\n", name));
	    return(ETIMEDOUT);
	}

	if (!Xbinding) break;
	if (VprocInterrupted()) return(EINTR);
	Srvr_Wait();
	if (VprocInterrupted()) return(EINTR);
    }

    /* Get the user entry and attempt to connect to it. */
    Xbinding = 1;
    {
	userent *u = 0;
	GetUser(&u, vuid);
	code = u->Connect(cidp, authp, &host);
	PutUser(&u);
    }
    Xbinding = 0;
    Srvr_Signal();

    /* Interpret result. */
    if (code < 0)
	switch (code) {
	    case RPC2_NOTAUTHENTICATED:
		code = EPERM; break;

	    case RPC2_NOBINDING:
	    case RPC2_SEFAIL2:
	    case RPC2_FAIL:
		code = ETIMEDOUT; break;

	    default:
/*
		CHOKE("srvent::Connect: illegal RPC code (%s)", RPC2_ErrorMsg(code));
*/
		code = ETIMEDOUT; break;
	}
    if (code == ETIMEDOUT) {
	/* Not already considered down. */
	if (!ServerIsDown()) {
	    MarinerLog("connection::unreachable %s\n", name);
	    Reset();
	    VDB->DownEvent(&host);
  	    adv_mon.ServerInaccessible(name);
	}
    }

    if (code == ETIMEDOUT && VprocInterrupted()) return(EINTR);
    return(code);
}

int srvent::GetStatistics(ViceStatistics *Stats)
{
    LOG(100, ("srvent::GetStatistics: host = %s\n", name));

    int code = 0;

    connent *c = 0;

    memset(Stats, 0, sizeof(ViceStatistics));
    
    code = GetConn(&c, &host, V_UID, 0);
    if (code != 0) goto Exit;

    /* Make the RPC call. */
    MarinerLog("fetch::GetStatistics %s\n", name);
    UNI_START_MESSAGE(ViceGetStatistics_OP);
    code = (int) ViceGetStatistics(c->connid, Stats);
    UNI_END_MESSAGE(ViceGetStatistics_OP);
    MarinerLog("fetch::getstatistics done\n");
    code = c->CheckResult(code, 0);
    UNI_RECORD_STATS(ViceGetStatistics_OP);

Exit:
    PutConn(&c);
    return(code);
}


void srvent::Reset()
{
    LOG(1, ("srvent::Reset: host = %s\n", name));

    /* Kill all direct connections to this server. */
    {
	struct ConnKey Key; Key.host = host; Key.vuid = ALL_UIDS;
	conn_iterator conn_next(&Key);
	connent *c = 0;
	connent *tc = 0;
	for (c = conn_next(); c != 0; c = tc) {
	    tc = conn_next();		/* read ahead */
	    (void)c->Suicide(0);
	}
    }

    /* Kill all indirect connections to this server. */
    {
	vol_iterator next;
	volent *v;
	while ((v = next())) {
            if (v->IsReadWriteReplica() && v->IsHostedBy(&host))
                v->KillMgrpMember(&host);
        }
    }

    /* Unbind callback connection for this server. */
    int code = (int) RPC2_Unbind(connid);
    LOG(1, ("srvent::Reset: RPC2_Unbind -> %s\n", RPC2_ErrorMsg(code)));
    connid = 0;
}


void srvent::ServerError(int *codep)
{
    LOG(1, ("srvent::ServerError: %s error (%s)\n",
	    name, RPC2_ErrorMsg(*codep)));

    /* Translate the return code. */
    switch (*codep) {
	case RPC2_FAIL:
	case RPC2_NOCONNECTION:
	case RPC2_TIMEOUT:
	case RPC2_DEAD:
	case RPC2_SEFAIL2:
	    *codep = ETIMEDOUT; break;

	case RPC2_SEFAIL1:
	case RPC2_SEFAIL3:
	case RPC2_SEFAIL4:
	    *codep = EIO; break;

	case RPC2_NAKED:
	case RPC2_NOTCLIENT:
	    *codep = ERETRY; break;

        case RPC2_INVALIDOPCODE:
	    *codep = EOPNOTSUPP; break;

	default:
	    /* Map RPC2 warnings into EINVAL. */
	    if (*codep > RPC2_ELIMIT) { *codep = EINVAL; break; }
	    CHOKE("srvent::ServerError: illegal RPC code (%d)", *codep);
    }

    if (connid == 0) {
	/* Already considered down. */
	;
    }
    else {
	/* Reset if TIMED'out or NAK'ed. */
	switch (*codep) {
	    case ETIMEDOUT:
	        MarinerLog("connection::unreachable %s\n", name);
		Reset();
		VDB->DownEvent(&host);
		adv_mon.ServerInaccessible(name);
		break;

	    case ERETRY:
		/* Must have missed a down event! */
		eprint("%s nak'ed", name);
		Reset();
		VDB->DownEvent(&host);
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
    LOG(1, ("srvent::ServerUp: %s, connid = %d, newconnid = %d\n",
	     name, connid, newconnid));

    switch(connid) {
    case 0:
	MarinerLog("connection::up %s\n", name);
	connid = newconnid;
	VDB->UpEvent(&host);
	adv_mon.ServerAccessible(name);
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
	VDB->DownEvent(&host);
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

    timerclear(tp);
    timerclear(&t);

    /* we don't have a real connid if the server is down or "quasi-up" */
    if (connid <= 0) 
	return(ETIMEDOUT);

    /* Our peer is at the other end of the callback connection */
    if ((rc = RPC2_GetPeerLiveness(connid, tp, &t)) != RPC2_SUCCESS)
	return(rc);

    LOG(100, ("srvent::GetLiveness: (%s), RPC %ld.%0ld, SE %ld.%0ld\n",
	      name, tp->tv_sec, tp->tv_usec, t.tv_sec, t.tv_usec));

    if (timercmp(tp, &t, <))
	*tp = t;	/* structure assignment */

    return(0);
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
    long rc = 0;
    unsigned long oldbw    = bw;
    unsigned long oldbwmax = bwmax;
    unsigned long bwmin;

    LOG(1, ("srvent::GetBandwidth (%s) lastobs %ld.%06ld\n", 
	      name, lastobs.tv_sec, lastobs.tv_usec));
    
    /* we don't have a real connid if the server is down or "quasi-up" */
    if (connid <= 0) 
	return(ETIMEDOUT);
    
    /* retrieve the bandwidth information from RPC2 */
    if ((rc = RPC2_GetBandwidth(connid, &bwmin, &bw, &bwmax)) != RPC2_SUCCESS)
	return(rc);

    LOG(1, ("srvent:GetBandWidth: --> new BW %d bytes/sec\n", bw));

    /* update last observation time */
    RPC2_GetLastObs(connid, &lastobs);

    /* 
     * Signal if we've crossed the weakly-connected threshold. Note
     * that the connection is considered strong until proven otherwise.
     *
     * The user can block the strong->weak transition using the
     * 'cfs strong' command (and turn adaptive mode back on with
     * 'cfs adaptive').
     */
    if (!isweak && !forcestrong && bwmax < WCThresh) {
	isweak = 1;
	MarinerLog("connection::weak %s\n", name);
	VDB->WeakEvent(&host);
        adv_mon.ServerConnectionWeak(name);
    }
    else if (isweak && bwmin > WCThresh) {
	isweak = 0;
	MarinerLog("connection::strong %s\n", name);
	VDB->StrongEvent(&host);
        adv_mon.ServerConnectionStrong(name);
    }
	
    *Bandwidth = bw;
    if (bw != oldbw || bwmax != oldbwmax) {
	MarinerLog("connection::bandwidth %s %d %d %d\n", name,bwmin,bw,bwmax);
        adv_mon.ServerBandwidthEstimate(name, *Bandwidth);
    }
    LOG(1, ("srvent::GetBandwidth (%s) returns %d bytes/sec\n",
	      name, *Bandwidth));
    return(0);
}


/* 
 * Force server connectivity to strong, or resume with the normal bandwidth
 * adaptive mode (depending on the `on' flag).
 */
void srvent::ForceStrong(int on) {
    forcestrong = on;

    /* forced switch to strong mode */
    if (forcestrong && isweak) {
	isweak = 0;
	MarinerLog("connection::strong %s\n", name);
	VDB->StrongEvent(&host);
        adv_mon.ServerConnectionStrong(name);
    }

    /* switch back to adaptive mode */
    if (!forcestrong && !isweak && bwmax < WCThresh) {
	isweak = 1;
	MarinerLog("connection::weak %s\n", name);
	VDB->WeakEvent(&host);
        adv_mon.ServerConnectionWeak(name);
    }

    return;
}


int srvent::IsRootServer(void)
{
    return rootserver;
}

void srvent::print(int fd)
{
    fdprint(fd, "%#08x : %-16s : cid = %d, host = %s, binding = %d, bw = %d, isroot = %d\n",
            (long)this, name, connid, inet_ntoa(host), Xbinding, bw, rootserver);
}


srv_iterator::srv_iterator() : olist_iterator((olist&)*srvent::srvtab) {
}


srvent *srv_iterator::operator()() {
    olink *o = olist_iterator::operator()();
    if (!o) return(0);

    srvent *s = strbase(srvent, o, tblhandle);
    return(s);
}


/* ***** Replicated operation context  ***** */

RepOpCommCtxt::RepOpCommCtxt()
{
    LOG(100, ("RepOpCommCtxt::RepOpCommCtxt: \n"));

    HowMany = 0;
    memset(handles, 0, VSG_MEMBERS * sizeof(RPC2_Handle));
    memset(hosts, 0, VSG_MEMBERS * sizeof(struct in_addr));
    memset(retcodes, 0, VSG_MEMBERS * sizeof(RPC2_Integer));
    memset(&primaryhost, 0, sizeof(struct in_addr));
    MIp = 0;
    memset(dying, 0, VSG_MEMBERS * sizeof(unsigned));
}


/* ***** Mgroup  ***** */

#define	MGRPQ_LOCK()
#define	MGRPQ_UNLOCK()
#define	MGRPQ_WAIT()	    VprocWait((char *)&mgrpent::mgrp_sync)
#define	MGRPQ_SIGNAL()	    VprocSignal((char *)&mgrpent::mgrp_sync)

void Mgrp_Wait() {
    MGRPQ_LOCK();
    LOG(0, ("WAITING(MGRPQ):\n"));
    START_TIMING();
    MGRPQ_WAIT();
    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
    MGRPQ_UNLOCK();
}


void Mgrp_Signal() {
    MGRPQ_LOCK();
    MGRPQ_SIGNAL();
    MGRPQ_UNLOCK();
}

void PutMgrp(mgrpent **mpp)
{
    mgrpent *m = *mpp;
    *mpp = 0;
    if (m == 0) {
	LOG(100, ("PutMgrp: null mgrp\n"));
	return;
    }

    LOG(100, ("PutMgrp: volumeid = %#08x, uid = %d, mid = %d, auth = %d, inuse = %d, dying = %d\n",
	     m->vid, m->uid, m->McastInfo.Mgroup, m->authenticated, m->inuse, m->dying));

    if (!m->inuse)
	{ m->print(logFile); CHOKE("PutMgrp: mgrp not in use"); }

    /* Clean up the host set. */
    m->PutHostSet();

    if (m->dying) {
        list_del(&m->volhandle);
        delete m;
    } else
        m->inuse = 0;

    Mgrp_Signal();
}


void MgrpPrint() {
    MgrpPrint(stdout);
}


void MgrpPrint(FILE *fp) {
    fflush(fp);
    MgrpPrint(fileno(fp));
}


void MgrpPrint(int fd) {
#warning "MgrpPrint missing"
#if 0
    if (mgrpent::mgrptab == 0) return;

    fdprint(fd, "Mgroups: count = %d\n", mgrpent::mgrptab->count());

    /* Iterate through the individual entries. */
    mgrp_iterator next;
    mgrpent *m;
    while ((m = next())) m->print(fd);

    fdprint(fd, "\n");
#endif
}


mgrpent::mgrpent(volent *vol, vuid_t vuid, RPC2_Handle mid, int authflag)
{
    LOG(1,("mgrpent::mgrpent volumeid = %#08x, uid = %d, mid = %d, auth = %d\n",
           vol->GetVid(), vuid, mid, authflag));

    /* These members are immutable. */
    uid = vuid;
    vid = vol->GetVid();
    memset(&McastInfo, 0, sizeof(RPC2_Multicast));
    McastInfo.Mgroup = mid;
    McastInfo.ExpandHandle = 0;
    vol->GetHosts(Hosts);
    nhosts = 0;
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (Hosts[i].s_addr) nhosts++;
    authenticated = authflag;

    /* These members are mutable. */
    inuse = 1;
    dying = 0;

#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


mgrpent::~mgrpent()
{
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG
    LOG(1,("mgrpent::~mgrpent vid = %#08x uid = %d, mid = %d, auth = %d\n",
           vid, uid, McastInfo.Mgroup, authenticated));

    int code = 0;

    /* Kill active members. */
    for (int i = 0; i < VSG_MEMBERS; i++)
	KillMember(&rocc.hosts[i], 1);

    /* Delete Mgroup. */
    code = (int) RPC2_DeleteMgrp(McastInfo.Mgroup);
    LOG(1, ("mgrpent::~mgrpent: RPC2_DeleteMgrp -> %s\n", RPC2_ErrorMsg(code)));
}

int mgrpent::Suicide(int disconnect)
{
    LOG(1, ("mgrpent::Suicide: volid = %#08x, uid = %d, mid = %d, disconnect = %d\n", 
	    vid, uid, McastInfo.Mgroup, disconnect));

    dying = 1;

    if (inuse) return(0);

    inuse = 1;

    if (disconnect) {
	/* Make the RPC call. */
	MarinerLog("fetch::DisconnectFS (%#x)\n", vid);
	MULTI_START_MESSAGE(ViceDisconnectFS_OP);
	int code = (int) MRPC_MakeMulti(ViceDisconnectFS_OP, ViceDisconnectFS_PTR,
			      VSG_MEMBERS, rocc.handles,
			      rocc.retcodes, rocc.MIp, 0, 0);
	MULTI_END_MESSAGE(ViceDisconnectFS_OP);
	MarinerLog("fetch::disconnectfs done\n");

	/* Collate responses from individual servers and decide what to do next. */
	code = CheckNonMutating(code);
	MULTI_RECORD_STATS(ViceDisconnectFS_OP);
    }

    mgrpent *myself = this; // needed because &this is illegal
    PutMgrp(&myself);

    return(1);
}


/* Bit-masks for ignoring certain classes of errors. */
#define	_ETIMEDOUT	1
#define	_EINVAL		2
#define	_ENXIO		4
#define	_ENOSPC		8
#define	_EDQUOT		16
#define	_EIO		32
#define	_EACCES		64
#define	_EWOULDBLOCK	128

static int Unanimity(int *codep, struct in_addr *hosts, RPC2_Integer *retcodes, int mask)
{
    int	code = -1;

    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (!hosts[i].s_addr) continue;

	switch(retcodes[i]) {
	    case ETIMEDOUT:
		if (mask & _ETIMEDOUT) continue;
		break;

	    case EINVAL:
		if (mask & _EINVAL) continue;
		break;

	    case ENXIO:
		if (mask & _ENXIO) continue;
		break;

	    case EIO:
		if (mask & _EIO) continue;
		break;

	    case ENOSPC:
		if (mask & _ENOSPC) continue;
		break;

	    case EDQUOT:
		if (mask & _EDQUOT) continue;
		break;

	    case EACCES:
		if (mask & _EACCES) continue;
		break;

	    case EWOULDBLOCK:
		if (mask & _EWOULDBLOCK) continue;
		break;
	}

	if (code == -1)
	    code = (int) retcodes[i];
	else
	    if (code != retcodes[i])
		return(0);
    }

    *codep = (code == -1 ? ERETRY : code);
    return(1);
}


int RepOpCommCtxt::AnyReturned(int code)
{
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (!hosts[i].s_addr) continue;

	if (retcodes[i] == code) return(1);
    }

    return(0);
}


/* Translate RPC and Volume errors, and update server state. */
void mgrpent::CheckResult()
{
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (!rocc.hosts[i].s_addr) continue;

	switch(rocc.retcodes[i]) {
	    default:
		if (rocc.retcodes[i] < 0) {
		    srvent *s = 0;
		    GetServer(&s, &rocc.hosts[i]);
		    s->ServerError((int *)&rocc.retcodes[i]);
		    PutServer(&s);
		}
		/* Note that KillMember may zero rocc.hosts[i] !!! */
		if (rocc.retcodes[i] == ETIMEDOUT || rocc.retcodes[i] == ERETRY)
		    KillMember(&rocc.hosts[i], 1);
		break;

	    case VBUSY:
		rocc.retcodes[i] = EWOULDBLOCK;
		break;

	    case VNOVOL:
		rocc.retcodes[i] = ENXIO;
		break;

	    case VNOVNODE:
		rocc.retcodes[i] = ENOENT;
		break;

	    case VLOGSTALE:
		rocc.retcodes[i] = EALREADY;
		break;

	    case VSALVAGE:
	    case VVOLEXISTS:
	    case VNOSERVICE:
	    case VOFFLINE:
	    case VONLINE:
	    case VNOSERVER:
	    case VMOVED:
	    case VFAIL:
		eprint("mgrpent::CheckResult: illegal code (%d)",
		       rocc.retcodes[i]);
		rocc.retcodes[i] = EINVAL;
		break;
	}
    }
}


/* Maps return codes from Vice:
	0		Call succeeded at all responding hosts (|responders| > 0),
	ETIMEDOUT	No hosts responded,
	ESYNRESOLVE	Multiple non-maskable results were returned,
	EASYRESOLVE	Call succeeded at (at least) one host, and no non-maskable
			errors were returned, but some maskable errors were,
	ERETRY		Some responders NAK'ed,
	Other (> 0)	Call succeeded at no responding host, and all non-maskable errors
			were the same, but some maskable errors may have been returned.
*/
int mgrpent::CheckNonMutating(int acode)
{
    LOG(100, ("mgrpent::CheckNonMutating: acode = %d\n\t\thosts = [%#x %#x %#x %#x %#x %#x %#x %#x],\n\t\tretcodes = [%d %d %d %d %d %d %d %d]\n",
	    acode, rocc.hosts[0], rocc.hosts[1], rocc.hosts[2], rocc.hosts[3],
	    rocc.hosts[4], rocc.hosts[5], rocc.hosts[6], rocc.hosts[7],
	    rocc.retcodes[0], rocc.retcodes[1], rocc.retcodes[2], rocc.retcodes[3],
	    rocc.retcodes[4], rocc.retcodes[5], rocc.retcodes[6], rocc.retcodes[7]));

    int code = 0;
    int i;
    
    CheckResult();
    
    /* check for this here because CheckResult may nuke hosts */
    if (rocc.HowMany == 0) return(ETIMEDOUT);

    /* Perform additional translations. */
    for (i = 0; i < VSG_MEMBERS; i++) {
	if (!rocc.hosts[i].s_addr) continue;

	switch(rocc.retcodes[i]) {
	    case ENOSPC:
	    case EDQUOT:
	    case EINCOMPATIBLE:
		eprint("mgrpent::CheckNonMutating: illegal code (%d)", rocc.retcodes[i]);
		rocc.retcodes[i] = EINVAL;
		break;
	}
    }

    /* The ideal case is a unanimous response. */
    if (Unanimity(&code, rocc.hosts, rocc.retcodes, 0))
	return(code);

    /* Since this operation is non-mutating, we can retry immediately if any
     * host NAK'ed. */
    if (rocc.AnyReturned(ERETRY))
	return(ERETRY);

    /* Look for unanimity, masking off more and more error types. */
    static int ErrorMasks[] = {
	_ETIMEDOUT,
	_ETIMEDOUT | _EINVAL,
	_ETIMEDOUT | _EINVAL | _ENXIO,
	_ETIMEDOUT | _EINVAL | _ENXIO | _EIO,
	_ETIMEDOUT | _EINVAL | _ENXIO | _EIO | _EACCES,
	_ETIMEDOUT | _EINVAL | _ENXIO | _EIO | _EACCES | _EWOULDBLOCK
    };
    static int nErrorMasks = (int) (sizeof(ErrorMasks) / sizeof(int));
    int mask = ErrorMasks[0];
    for (i = 0; i < nErrorMasks; i++, mask = ErrorMasks[i])
	if (Unanimity(&code, rocc.hosts, rocc.retcodes, mask))
	    { if (code == 0) code = EASYRESOLVE; return(code); }

    /* We never achieved consensus. */
    /* Force a synchronous resolve. */
    return(ESYNRESOLVE);
}


/* Maps return codes from Vice:
	0		Call succeeded at all responding hosts (|responders| > 0),
	ETIMEDOUT	No hosts responded,
	EASYRESOLVE	Call succeeded at (at least) one host, and some (maskable or
			non-maskable) errors were returned,
	ERETRY		All responders NAK'ed, or call succeeded at no responding host
			and multiple non-maskable errors were returned,
	Other (> 0)	Call succeeded at no responding host, and all non-maskable errors
			were the same, but some maskable errors may have been returned.

   OUT parameter, UpdateSet, indicates which sites call succeeded at.
*/
int mgrpent::CheckCOP1(int acode, vv_t *UpdateSet, int TranslateEincompatible) {
    LOG(100, ("mgrpent::CheckCOP1: acode = %d\n\t\thosts = [%#x %#x %#x %#x %#x %#x %#x %#x],\n\t\tretcodes = [%d %d %d %d %d %d %d %d]\n",
	       acode, rocc.hosts[0], rocc.hosts[1], rocc.hosts[2], rocc.hosts[3],
	       rocc.hosts[4], rocc.hosts[5], rocc.hosts[6], rocc.hosts[7],
	       rocc.retcodes[0], rocc.retcodes[1], rocc.retcodes[2], rocc.retcodes[3],
	       rocc.retcodes[4], rocc.retcodes[5], rocc.retcodes[6], rocc.retcodes[7]));

    int code = 0;
    int i;

    InitVV(UpdateSet);

    CheckResult();
    
    /* check for this here because CheckResult may nuke hosts */
    if (rocc.HowMany == 0) return(ETIMEDOUT);

    /* Perform additional translations. */
    for (i = 0; i < VSG_MEMBERS; i++) {
	if (!rocc.hosts[i].s_addr) continue;

	switch(rocc.retcodes[i]) {
	    case EINCOMPATIBLE:
		if (TranslateEincompatible)
		    rocc.retcodes[i] = ERETRY;	    /* NOT for reintegrate! */
		break;
	}
    }

    /* Record successes in the UpdateSet. */
    for (i = 0; i < VSG_MEMBERS; i++) {
	if (!rocc.hosts[i].s_addr) continue;

	if (rocc.retcodes[i] == 0)
	    (&(UpdateSet->Versions.Site0))[i] = 1;
    }

    /* The ideal case is a unanimous response. */
    if (Unanimity(&code, rocc.hosts, rocc.retcodes, 0))
	return(code);

    /* Look for unanimity, masking off more and more error types. */
    static int ErrorMasks[] = {
	_ETIMEDOUT,
	_ETIMEDOUT | _EINVAL,
	_ETIMEDOUT | _EINVAL | _ENXIO,
	_ETIMEDOUT | _EINVAL | _ENXIO | _EIO,
	_ETIMEDOUT | _EINVAL | _ENXIO | _EIO | ENOSPC,
	_ETIMEDOUT | _EINVAL | _ENXIO | _EIO | ENOSPC | EDQUOT,
	_ETIMEDOUT | _EINVAL | _ENXIO | _EIO | ENOSPC | EDQUOT | _EACCES,
	_ETIMEDOUT | _EINVAL | _ENXIO | _EIO | ENOSPC | EDQUOT | _EACCES | _EWOULDBLOCK
    };
    static int nErrorMasks = (int) (sizeof(ErrorMasks) / sizeof(int));
    int mask = ErrorMasks[0];
    for (i = 0; i < nErrorMasks; i++, mask = ErrorMasks[i])
	if (Unanimity(&code, rocc.hosts, rocc.retcodes, mask))
	    { if (code == 0) code = EASYRESOLVE; return(code); }

    /* We never achieved consensus. */
    /* Return ASYRESOLVE if operation succeeded at any host. */
    /* Otherwise, return RETRY, which will induce a RESOLVE at a more convenient point. */
    if (rocc.AnyReturned(0))
	return(EASYRESOLVE);
    return(ERETRY);
}


/* This is identical to mgrpent::CheckCOP1(), EXCEPT that we want to treat */
/* EINCOMPATIBLE results as non-maskable rather that translating them to ERETRY. */
int mgrpent::CheckReintegrate(int acode, vv_t *UpdateSet)
{
    int ret = CheckCOP1(acode, UpdateSet, 0);

    /* CheckCOP1 doesn't know how to handle EALREADY. If any host had
     * returned EALREADY we can get rid of some CML entries. */
    if (ret == ERETRY) {
        if (rocc.AnyReturned(EALREADY))
            return(EALREADY);
    }
    return(ret);
}


/* Check the remote vectors. */
/* Returns:conf
	0		Version check succeeded
	ESYNRESOLVE	Version check failed
	EASYRESOLVE	!EqReq and check yielded Dom/Sub
*/
int mgrpent::RVVCheck(vv_t **RVVs, int EqReq)
{
    /* Construct the array so that only valid VVs are checked. */
    for (int j = 0; j < VSG_MEMBERS; j++)
	if (!rocc.hosts[j].s_addr || rocc.retcodes[j]) RVVs[j] = 0;
    if (LogLevel >= 100) VVPrint(logFile, RVVs);

    int dom_cnt = 0;
    if (!VV_Check(&dom_cnt, RVVs, EqReq)) {
	return(ESYNRESOLVE);
    }
    else {
	if (dom_cnt <= 0 || dom_cnt > rocc.HowMany) {
	    print(logFile);
	    CHOKE("mgrpent::RVVCheck: bogus dom_cnt (%d)", dom_cnt);
	}

	/* Notify servers which have out of date copies. */
	if (dom_cnt < rocc.HowMany) return(EASYRESOLVE);
    }

    return(0);
}

#define DOMINANT(idx) (rocc.hosts[idx].s_addr && \
                       rocc.retcodes[idx] == 0 && \
                       (RVVs == 0 || RVVs[idx] != 0))

int mgrpent::PickDH(vv_t **RVVs)
{
    int i, dominators = 0, chosen;

    /* count % of hosts in the dominant set. */
    for (i = 0; i < VSG_MEMBERS; i++)
	if (DOMINANT(i))
            dominators++;

    /* randomly choose one. If not only for lack of information, then simply
     * to improve load balancing of the clients */
    chosen = rpc2_NextRandom(NULL) % dominators;

    /* And walk the hosts again to find the one we chose */
    for (i = 0; i < VSG_MEMBERS; i++)
	if (DOMINANT(i) && (chosen-- == 0))
                return(i);

    CHOKE("mgrpent::PickDH: dominant set is empty");
    return(0);
}

/* Validate the existence of a dominant host; return its index in OUT parameter. */
/* If there are multiple hosts in the dominant set, prefer the primary host. */
/* The caller may specify that the PH must be dominant. */
/* Returns {0, ERETRY}. */
int mgrpent::DHCheck(vv_t **RVVs, int ph_ix, int *dh_ixp, int PHReq)
{
    *dh_ixp = -1;

    /* Return the primary host if it is in the dominant set. */
    if (ph_ix != -1 && DOMINANT(ph_ix))
    {
        *dh_ixp = ph_ix;
        return(0);
    }
	
    /* Find a non-primary host from the dominant set. */
    *dh_ixp = PickDH(RVVs);

    if (PHReq) {
        LOG(1, ("DHCheck: Volume (%x) PH -> %x", vid, rocc.hosts[*dh_ixp]));
        rocc.primaryhost = rocc.hosts[*dh_ixp];
        return(ERETRY);
    }

    return(0);
}


int mgrpent::GetHostSet()
{
    int i, idx;
    LOG(100, ("mgrpent::GetHostSet: volumeid = %#08x, uid = %d, mid = %d\n",
	      vid, uid, McastInfo.Mgroup));

    /* Create members of the specified set which are not already in the
     * Mgroup. */
    for (i = 0; i < VSG_MEMBERS; i++)
	if (Hosts[i].s_addr && !rocc.hosts[i].s_addr) {
	    switch(CreateMember(i)) {
		case EINTR:
		    return(EINTR);

		case EPERM:
		    return(EPERM);

		default:
		    break;
	    }
	}

    /* Kill members of the Mgroup which are not in the specified set. */
    for (i = 0; i < VSG_MEMBERS; i++)
	if (!Hosts[i].s_addr && rocc.hosts[i].s_addr)
	    KillMember(&rocc.hosts[i], 1);

    /* Ensure that Mgroup is not empty. */
    if (rocc.HowMany == 0) return(ETIMEDOUT);

    /* Validate primaryhost. */
    if (!rocc.primaryhost.s_addr)
    {
        /* When the rocc.retcodes are all be zero, all available
         * hosts are Dominant Hosts, and we can use PickDH */
        memset(rocc.retcodes, 0, sizeof(RPC2_Integer) * VSG_MEMBERS);
        idx = PickDH(NULL);
	rocc.primaryhost = rocc.hosts[idx];
    }

    return(0);
}


int mgrpent::CreateMember(int idx)
{
    int i;
    LOG(100, ("mgrpent::CreateMember: volumeid = %#08x, uid = %d, mid = %d, host = %s\n", 
	      vid, uid, McastInfo.Mgroup, inet_ntoa(Hosts[idx])));

    if (!Hosts[idx].s_addr)
        CHOKE("mgrpent::CreateMember: no host at index %d", idx);

    int code = 0;

    /* Bind/Connect to the server. */
    srvent *s = 0;
    GetServer(&s, &Hosts[idx]);
    RPC2_Handle ConnHandle = 0;
    int auth = 0;
    code = s->Connect(&ConnHandle, &auth, uid, 0);
    PutServer(&s);
    if (code != 0) return(code);

    /* Add new connection to the Mgrp. */
    code = (int) RPC2_AddToMgrp(McastInfo.Mgroup, ConnHandle);
    LOG(1, ("mgrpent::CreateMember: RPC_AddToMgrp -> %s\n",
	     RPC2_ErrorMsg(code)));
    if (code != 0) {
/*
	print(logFile);
	CHOKE("mgrpent::CreateMember: AddToMgrp failed (%d)", code);
*/
	(void) RPC2_Unbind(ConnHandle);
	return(ETIMEDOUT);
    }

    /* Update rocc state. */
    rocc.HowMany++;
    rocc.handles[idx] = ConnHandle;
    rocc.hosts[idx] = Hosts[idx];
    rocc.retcodes[idx] = 0;
    rocc.dying[idx] = 0;

    return(0);
}


void mgrpent::PutHostSet() {
    LOG(100, ("mgrpent::PutHostSet: \n"));

    /* Kill dying members. */
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (rocc.dying[i]) KillMember(&rocc.hosts[i], 0);
}


void mgrpent::KillMember(struct in_addr *host, int forcibly)
{
    LOG(100, ("mgrpent::KillMember: volumeid = %#08x, uid = %d, mid = %d, host = %s, forcibly = %d\n",
	      vid, uid, McastInfo.Mgroup, inet_ntoa(*host), forcibly));

    long code = 0;

    if (!host->s_addr) return;

    /* we first mark the host that should die to avoid making the passed
     * host pointer useless (f.i. when it is &rocc.hosts[i]) */
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (rocc.hosts[i].s_addr == host->s_addr)
            rocc.dying[i] = 1;

    if (inuse && !forcibly)
        return;

    /* now we can safely kill dying members */
    for (int i = 0; i < VSG_MEMBERS; i++) {
        if (rocc.dying[i]) {
            if (rocc.hosts[i].s_addr == rocc.primaryhost.s_addr) {
                rocc.primaryhost.s_addr = 0;
            }

            code = RPC2_RemoveFromMgrp(McastInfo.Mgroup, rocc.handles[i]);
	    LOG(1, ("mgrpent::KillMember: RPC2_RemoveFromMgrp(%s, %d) -> %s\n",
                    inet_ntoa(rocc.hosts[i]), rocc.handles[i],
                    RPC2_ErrorMsg((int) code)));

	    code = RPC2_Unbind(rocc.handles[i]);
	    LOG(1, ("mgrpent::KillMember: RPC2_Unbind(%s, %d) -> %s\n",
                    inet_ntoa(rocc.hosts[i]), rocc.handles[i],
                    RPC2_ErrorMsg((int) code)));

	    rocc.HowMany--;
	    rocc.handles[i] = 0;
	    rocc.hosts[i].s_addr = 0;
            rocc.retcodes[i] = 0;
	    rocc.dying[i] = 0;
	}
    }
}


struct in_addr *mgrpent::GetPrimaryHost(int *ph_ixp)
{
    int i;

    if (ph_ixp) *ph_ixp = -1;

    if (!rocc.primaryhost.s_addr)
	rocc.primaryhost = rocc.hosts[PickDH(NULL)];

    /* Sanity check. */
    for (i = 0; i < VSG_MEMBERS; i++)
	if (rocc.hosts[i].s_addr == rocc.primaryhost.s_addr) {
	    if (ph_ixp) *ph_ixp = i;
            /* Add a round robin distribution, primarily to spread fetches
             * across AVSG. */
            /* Added a random factor to reduce the amount of switching
             * between servers to only of 1 out of every 32 calls --JH */
	    if (RoundRobin && ((rpc2_NextRandom(NULL) & 0x1f) == 0)) {
		int j;
		for (j = i + 1; j != i; j = (j + 1) % VSG_MEMBERS)
		    if (rocc.hosts[j].s_addr) {
			/* We have a valid host. It'd be nice to use strongly
			   connected hosts in preference to weak ones, but I'm
			   not sure how to access to srvent from here.
			   -- DCS 2/2/96
			   */
			rocc.primaryhost = rocc.hosts[j];
			break;
		    }
	    }
	    return(&rocc.hosts[i]);
	}

    CHOKE("mgrpent::GetPrimaryHost: ph (%x) not found", rocc.primaryhost);
    return(NULL); /* dummy to keep g++ happy */
}

/* *****  Fail library manipulations ***** */

/* 
 * Simulate "pulling the plug". Insert filters on the
 * send and receive sides of venus.
 */
int FailDisconnect(int nservers, struct in_addr *hostids)
{
    int rc, k = 0;
    FailFilter filter;
    FailFilterSide side;

    do {
	srv_iterator next;
	srvent *s;
	while ((s = next()))
	    if (nservers == 0 || s->host.s_addr == hostids[k].s_addr) {
		/* we want a pair of filters for server s. */

		struct in_addr addr = s->host;    
		filter.ip1 = ((unsigned char *)&addr)[0];
		filter.ip2 = ((unsigned char *)&addr)[1];
		filter.ip3 = ((unsigned char *)&addr)[2];
		filter.ip4 = ((unsigned char *)&addr)[3];
		filter.color = -1;
		filter.lenmin = 0;
		filter.lenmax = 65535;
		filter.factor = 0;
		filter.speed = 0;

		for (int i = 0; i < 2; i++) {
		    if (i == 0) side = sendSide;
		    else side = recvSide;

		    /* 
		     * do we have this filter already?  Note that this only
		     * checks the filters inserted from Venus.  To check all
		     * the filters, we need to use Fail_GetFilters.
		     */
		    char gotit = 0;
		    for (int j = 0; j < MAXFILTERS; j++) 
			if (FailFilterInfo[j].used && 
			    htonl(FailFilterInfo[j].host) == s->host.s_addr &&
			    FailFilterInfo[j].side == side) {
				gotit = 1;
				break;
			}

		    if (!gotit) {  /* insert a new filter */
			int ix = -1;    
			for (int j = 0; j < MAXFILTERS; j++) 
			    if (!FailFilterInfo[j].used) {
				ix = j;
				break;
			    }

			if (ix == -1) { /* no room */
			    LOG(0, ("FailDisconnect: couldn't insert %s filter for %s, table full!\n", 
				(side == recvSide)?"recv":"send", s->name));
			    return(ENOBUFS);
			}

			if ((rc = Fail_InsertFilter(side, 0, &filter)) < 0) {
			    LOG(0, ("FailDisconnect: couldn't insert %s filter for %s\n", 
				(side == recvSide)?"recv":"send", s->name));
			    return(rc);
			}
			LOG(10, ("FailDisconnect: inserted %s filter for %s, id = %d\n", 
			    (side == recvSide)?"recv":"send", s->name, rc));

			FailFilterInfo[ix].id = filter.id;
			FailFilterInfo[ix].side = side;
			FailFilterInfo[ix].host = ntohl(s->host.s_addr);
			FailFilterInfo[ix].used = 1;
		    }
		} 
	    }

    } while (k++ < nservers-1);

    return(0);
}

/* 
 * Remove fail filters inserted by VIOC_SLOW or VIOC_DISCONNECT.
 * Filters inserted by other applications (ttyfcon, etc) are not
 * affected, since we don't have their IDs. If there is a problem 
 * removing a filter, we print a message in the log and forget 
 * about it. This allows the user to remove the filter using another
 * tool and not have to deal with leftover state in venus.
 */
int FailReconnect(int nservers, struct in_addr *hostids)
{
    int rc, s = 0;

    do {
	for (int i = 0; i < MAXFILTERS; i++) 
	    if (FailFilterInfo[i].used &&
                (nservers == 0 ||
                 (htonl(FailFilterInfo[i].host) == hostids[s].s_addr))) {
                if ((rc = Fail_RemoveFilter(FailFilterInfo[i].side,
                                            FailFilterInfo[i].id)) < 0) {
		    LOG(0, ("FailReconnect: couldn't remove %s filter, id = %d\n", 
			(FailFilterInfo[i].side == sendSide)?"send":"recv", 
			FailFilterInfo[i].id));
		} else {
		    LOG(10, ("FailReconnect: removed %s filter, id = %d\n", 
			(FailFilterInfo[i].side == sendSide)?"send":"recv", 
			FailFilterInfo[i].id));
		    FailFilterInfo[i].used = 0;
		}
            }
    } while (s++ < nservers-1);

    return(0);
}

/* 
 * Simulate a slow network. Insert filters with the
 * specified speed to all known servers.
 */
int FailSlow(unsigned *speedp) {

    return(-1);
}


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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/comm.cc,v 4.14 1998/06/11 15:29:23 braam Exp $";
#endif /*_BLURB_*/







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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <struct.h>
#ifdef __MACH__
  /* sigh if gcc 2.3.3 */
  /* #define binding Xbinding*/
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#ifdef	__BSD44__
#include <machine/endian.h>
#endif

#include <netinet/in.h>
#ifdef __linux__
#include <endian.h>
#endif
#include <setjmp.h>

#include <rpc2.h>
#include <se.h>
#include <fail.h>

#include <errors.h>

extern int Fcon_Init(); 
extern void SFTP_SetDefaults (SFTP_Initializer *initPtr);
extern void SFTP_Activate (SFTP_Initializer *initPtr);

/* interfaces */
#include <vice.h>
#include <adsrv.h>

#ifdef __cplusplus
}
#endif __cplusplus



/* from vol */
#include <errors.h>

/* from vv */
#include <inconsist.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "simulate.h"
#include "user.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvm.h"
#include "venusvol.h"
#include "vproc.h"
#include "advice_daemon.h"


int COPModes = 6;	/* ASYNCCOP2 | PIGGYCOP2 */
int UseMulticast = 0;
char myHostName[MAXHOSTNAMELEN];
unsigned long myHostId;
int rpc2_retries = UNSET_RT;
int rpc2_timeout = UNSET_TO;
int sftp_windowsize = UNSET_WS;
int sftp_sendahead = UNSET_SA;
int sftp_ackpoint = UNSET_AP;
int sftp_packetsize = UNSET_PS;
int rpc2_timeflag = UNSET_ST;
int mrpc2_timeflag = UNSET_MT;
long WCThresh = UNSET_WCT;  	/* in Bytes/sec */
int WCStale = UNSET_WCS;	/* seconds */

int RoundRobin = 0;
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


PRIVATE int VSG_HashFN(void *);

olist *mgrpent::mgrptab;
char mgrpent::mgrptab_sync;
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
    if (WCStale == UNSET_WCT) WCStale = DFLT_WCS;

    /* Sanity check COPModes. */
    if ( (ASYNCCOP1 && !ASYNCCOP2) ||
	 (PIGGYCOP2 && !ASYNCCOP2) )
	Choke("CommInit: bogus COPModes (%x)", COPModes);

    /* Initialize comm queue */
    bzero((void *)&CommQueue, sizeof(CommQueueStruct));

    /* Hostname is needed for file server connections. */
    if (gethostname(myHostName, MAXHOSTNAMELEN) < 0)
	Choke("CommInit: gethostname failed");

    /* Hostid is needed for storeid generation. */
#ifdef DJGPP
    myHostId = 0x12345678;
#else
    myHostId = gethostid();
#endif

    /* Initialize Connections. */
    connent::conntab = new olist;

    /* Initialize Servers. */
    srvent::srvtab = new olist;

    /* Initialize Mgroups. */
    mgrpent::mgrptab = new olist;

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
	srvent *s;
	int a, b, c, d;
	struct hostent *h = gethostbyname(ServerName);
	if (h) {
	    s = new srvent(ntohl(*((unsigned long *)h->h_addr)));
	    srvent::srvtab->insert(&s->tblhandle);
	    hcount++;
	}
	/* CHANGE */
	else if (AllowIPAddrs && sscanf(ServerName, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
	    /* Allow use of IP addrs */
	    dprint("a %u b %u c %u d %u\n", a, b, c, d);
	    d &= 0x000000ff;
	    d |= (((a << 24) & 0xff000000) | (b << 16 & 0x00ff0000) | ((c <<  8) & 0x0000ff00));
	    srvent *s = new srvent(d);
	    srvent::srvtab->insert(&s->tblhandle);
	    hcount++;
	}
	/* CHANGE */	
    }
    if (hcount == 0)
	Choke("CommInit: no bootstrap server");

    if (!Simulating) {
	RPC2_Perror = 0;

	/* Portal initialization. */
	/* Multicast requires that (sftp_portal = rpc2_portal + 1). */
	struct servent *s = getservbyname("coda_venus", 0);
	if (s == 0) Choke("CommInit: getservbyname failed");
	RPC2_PortalIdent portal1;
	portal1.Tag = RPC2_PORTALBYINETNUMBER;
	portal1.Value.InetPortNumber = htons(ntohs(s->s_port));

	/* SFTP initialization. */
	SFTP_Initializer sei;
	SFTP_SetDefaults(&sei);
	sei.WindowSize = sftp_windowsize;
	sei.SendAhead = sftp_sendahead;
	sei.AckPoint = sftp_ackpoint;
	sei.PacketSize = sftp_packetsize;
	sei.EnforceQuota = 1;
	sei.Portal.Tag = RPC2_PORTALBYINETNUMBER;
	sei.Portal.Value.InetPortNumber = htons(ntohs(s->s_port) + 1);

	SFTP_Activate(&sei);

	/* RPC2 initialization. */
	struct timeval tv;
	tv.tv_sec = rpc2_timeout;
	tv.tv_usec = 0;
	if (RPC2_Init(RPC2_VERSION, 0, &portal1, rpc2_retries, &tv) != RPC2_SUCCESS)
	    Choke("CommInit: RPC2_Init failed");

	/* Failure package initialization. */
	bzero((void *)FailFilterInfo, (int) (MAXFILTERS * sizeof(struct FailFilterInfoStruct)));
	Fail_Initialize("venus", 0);
	Fcon_Init();
    }

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
	while (s = next()) {
	    code = GetConn(cpp, s->host, V_UID, 0);
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
			Choke("GetAdmConn: bogus code (%d)", code);
		    return(code);
	    }
	}
	if (tryagain) continue;

	return(ETIMEDOUT);
    }
}


int GetConn(connent **cpp, unsigned long host, vuid_t vuid, int Force) {
    LOG(100, ("GetConn: host = %x, vuid = %d, force = %d\n",
	     host, vuid, Force));

    *cpp = 0;
    int code = 0;
    connent *c = 0;
    int found = 0;

    /* Grab an existing connection if one is free. */
    /* Before creating a new connection, make sure the per-user limit is not exceeded. */
    for (;;) {
	/* Make sure tokens are not expired. */
	userent *u = 0;
	GetUser(&u, vuid);
	u->CheckTokenExpiry();
	PutUser(&u);

	/* Check whether there is already a free connection. */
	struct ConnKey Key; Key.host = host; Key.vuid = vuid;
	conn_iterator next(&Key);
	int count = 0;
	while (c = next()) {
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


void PutConn(connent **cpp) {
    connent *c = *cpp;
    *cpp = 0;
    if (c == 0) {
	LOG(100, ("PutConn: null conn\n"));
	return;
    }

    LOG(100, ("PutConn: host = %#08x, uid = %d, cid = %d, auth = %d\n",
	     c->Host, c->uid, c->connid, c->authenticated));

    if (!c->inuse)
	{ c->print(logFile); Choke("PutConn: conn not in use"); }

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
    while (c = next()) c->print(fd);

    fdprint(fd, "\n");
}


connent::connent(unsigned long host, vuid_t vuid, RPC2_Handle cid, int authflag) {
    LOG(1, ("connent::connent: host = %#08x, uid = %d, cid = %d, auth = %d\n",
	     host, vuid, cid, authflag));

    /* These members are immutable. */
    Host = host;
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
    LOG(1, ("connent::~connent: RPC2_Unbind -> %s\n", RPC2_ErrorMsg(code)));
}


int connent::Suicide(int disconnect) {
    LOG(1, ("connent::Suicide: disconnect = %d\n", disconnect));

    /* Mark this conn as dying. */
    dying = 1;

    /* Can't do any more if it is busy. */
    if (inuse) return(0);

    inuse = 1;

    /* Be nice and disconnect if requested. */
    if (disconnect) {
	/* Make the RPC call. */
	MarinerLog("fetch::DisconnectFS %s\n", (FindServer(Host))->name);
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
int connent::CheckResult(int code, VolumeId vid) {
    LOG(100, ("connent::CheckResult: code = %d, vid = %x\n",
	     code, vid));

    /* ViceOp succeeded. */
    if (code == 0) return(0);

    /* Translate RPC and Volume errors, and update server state. */
    switch(code) {
	default:
	    if (code < 0) {
		srvent *s = 0;
		GetServer(&s, Host);
		s->ServerError(&code);
		PutServer(&s);

		if (code == ETIMEDOUT || code == ERETRY) {
		    dying = 1;
		}
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
    if (code == EINCOMPATIBLE) code = ERETRY;

    if (code == ETIMEDOUT && VprocInterrupted()) return(EINTR);
    return(code);
}


void connent::print(int fd) {
    fdprint(fd, "%#08x : host = %#08x, uid = %d, cid = %d, auth = %d, inuse = %d, dying = %d\n",
	     (long)this, Host, uid, connid, authenticated, inuse, dying);
}


conn_iterator::conn_iterator(struct ConnKey *Key) : olist_iterator((olist&)*connent::conntab) {
    key = Key;
}


connent *conn_iterator::operator()() {
    olink *o;
    while (o = olist_iterator::operator()()) {
	connent *c = strbase(connent, o, tblhandle);
	if (key == (struct ConnKey *)0) return(c);
	if ((key->host == c->Host || key->host == ALL_HOSTS) &&
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
    SRVRQ_SIGNAL();
    SRVRQ_UNLOCK();
}


srvent *FindServer(unsigned long host) {
    srv_iterator next;
    srvent *s;

    while (s = next())
	if (s->host == host) return(s);

    return(0);
}


srvent *FindServerByCBCid(RPC2_Handle connid) {
    if (connid == 0) return(0);

    srv_iterator next;
    srvent *s;

    while (s = next())
	if (s->connid == connid) return(s);

    return(0);
}


void GetServer(srvent **spp, unsigned long host) {
    LOG(100, ("GetServer: host = %x\n", host));

    srvent *s = FindServer(host);
    if (s) {
	*spp = s;
	return;
    }

    s = new srvent(host);
    srvent::srvtab->insert(&s->tblhandle);

    *spp = s;
}


void PutServer(srvent **spp) {
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
	vproc("ProbeSlave", (PROCBODY) &probeslave::main, VPT_ProbeDaemon, 16384) {
    LOG(100, ("probeslave::probeslave(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    task = Task;
    arg = Arg;
    result = Result;
    sync = Sync;

    /* Poke main procedure. */
    VprocSignal((char *)this, 1);
}


void probeslave::main(void *parm) {
    /* Wait for ctor to poke us. */
    VprocWait((char *)this);

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
	    unsigned long host = (unsigned long)arg;
	    int code = GetConn((connent **)result, host, V_UID, 1);
	    }
	    break;

	default:
	    Choke("probeslave::main: bogus task (%d)", task);
    }

    /* Signal reaper, then commit suicide. */
    (*sync)++;
    VprocSignal(sync);
    idle = 1;
    delete VprocSelf();
}


void ProbeServers(int Up) {
    LOG(1, ("ProbeServers: %s\n", Up ? "Up" : "Down"));

    /* Hosts and Connections are arrays of addresses and connents respectively representing the servers to */
    /* be probed.  HowMany is the current size of these arrays, and ix is the number of entries actually used. */
    const int GrowSize = 32;
    int HowMany = GrowSize;
    unsigned long *Hosts = (unsigned long *)malloc(HowMany * sizeof(unsigned long));
    connent **Connections = 0;
    int ix = 0;

    /* Fill in the Hosts array for each server that is to be probed. */
    {
	srv_iterator next;
	srvent *s;
	while (s = next()) {
	    if (!s->probeme ||
		(Up && s->ServerIsDown()) || (!Up && !s->ServerIsDown())) continue;

	    /* Grow the Hosts array if necessary. */
	    if (ix == HowMany) {
		/* I am terrified of realloc */
		HowMany += GrowSize;
		unsigned long *newHosts = (unsigned long *)malloc(HowMany * sizeof(unsigned long));
		bcopy((char *) Hosts, (char *) newHosts, ix * (int) sizeof(unsigned long));
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


void DoProbes(int HowMany, unsigned long *Hosts) {
    connent **Connections = 0;
    int i;

    ASSERT(HowMany > 0);

    /* Bind to the servers. */
    Connections = (connent **)malloc(HowMany * sizeof(connent *));
    bzero((void *)Connections, (int) (HowMany * sizeof(connent *)));
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

    /* Clean up before returning. */
    /* this looks insane: if it's called by vproc::do_ioctl(), Hosts points to
       the middle of a msgbuffer ! If it's called by ProbeServers(), then
       it should be ProbeServers() to do the free */
#if 0
    free(Hosts);
#endif
    for (i = 0; i < HowMany; i++)
	PutConn(&Connections[i]);
    free(Connections);
}


void MultiBind(int HowMany, unsigned long *Hosts, connent **Connections) {
    if (LogLevel >= 1) {
	dprint("MultiBind: HowMany = %d\n\tHosts = [ ", HowMany);
	for (int i = 0; i < HowMany; i++)
	    fprintf(logFile, "%x ", Hosts[i]);
	fprintf(logFile, "]\n");
    }

    int slaves = 0;
    char slave_sync = 0;
    for (int ix = 0; ix < HowMany; ix++) {
	/* Try to get a connection without forcing a bind. */
	connent *c = 0;
	if (GetConn(&c, Hosts[ix], V_UID, 0) == 0) {
	    /* Stuff the connection in the array. */
	    Connections[ix] = c;

	    continue;
	}

	/* Force a bind, but have a slave do it so we can bind in parallel. */
	{
	    slaves++;
	    (void)new probeslave(BindToServer, (void *)(Hosts[ix]), 
				 (void *)(&Connections[ix]), &slave_sync);
	}
    }

    /* Reap any slaves we created. */
    while (slave_sync != slaves) {
	LOG(1, ("MultiBind: waiting (%d, %d)\n", slave_sync, slaves));
	VprocWait(&slave_sync);
    }
}


void MultiProbe(int HowMany, RPC2_Handle *Handles) {
    if (LogLevel >= 1) {
	dprint("MultiProbe: HowMany = %d\n\tHandles = [ ", HowMany);
	for (int i = 0; i < HowMany; i++)
	    fprintf(logFile, "%x ", Handles[i]);
	fprintf(logFile, "]\n");
    }

    /* Make multiple copies of the IN/OUT and OUT parameters. */
    RPC2_Unsigned  **secs_ptrs =
	(RPC2_Unsigned **)malloc(HowMany * sizeof(RPC2_Unsigned *));
    ASSERT(secs_ptrs);
    RPC2_Unsigned   *secs_bufs =
	(RPC2_Unsigned *)malloc(HowMany * sizeof(RPC2_Unsigned));
    ASSERT(secs_bufs);
    for (int i = 0; i < HowMany; i++)
	secs_ptrs[i] = &secs_bufs[i]; 
    RPC2_Integer  **usecs_ptrs =
	(RPC2_Integer **)malloc(HowMany * sizeof(RPC2_Integer *));
    ASSERT(usecs_ptrs);
    RPC2_Integer   *usecs_bufs =
	(RPC2_Integer *)malloc(HowMany * sizeof(RPC2_Integer));
    ASSERT(usecs_bufs);
    for (int ii = 0; ii < HowMany; ii++)
	usecs_ptrs[ii] = &usecs_bufs[ii]; 

    /* Make the RPC call. */
    MarinerLog("fetch::Probe\n");
    MULTI_START_MESSAGE(ViceGetTime_OP);
    int code = (int) MRPC_MakeMulti(ViceGetTime_OP, ViceGetTime_PTR,
			       HowMany, Handles, (RPC2_Integer *)0, 0,
			       (long (*)())HandleProbe, 0,
			       secs_ptrs, usecs_ptrs);
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


long HandleProbe(int HowMany, RPC2_Handle *Handles, long offset, long rpcval) {
    RPC2_Handle RPCid = Handles[offset];

    if (RPCid != 0) {
	/* Get the {host,portal} pair for this call. */
	RPC2_PeerInfo thePeer;
	long rc = RPC2_GetPeerInfo(RPCid, &thePeer);
	if (thePeer.RemoteHost.Tag != RPC2_HOSTBYINETADDR ||
	    thePeer.RemotePortal.Tag != RPC2_PORTALBYINETNUMBER) {
	    LOG(0, ("HandleProbe: RPC2_GetPeerInfo return code = %d\n", rc));
	    LOG(0, ("HandleProbe: thePeer.RemoteHost.Tag = %d\n", thePeer.RemoteHost.Tag));
	    LOG(0, ("HandleProbe: thePeer.RemotePortal.Tag = %d\n", thePeer.RemotePortal.Tag));
	    return 0;
	    /* Choke("HandleProbe: getpeerinfo returned bogus type!"); */
	}
	unsigned long host = ntohl(thePeer.RemoteHost.Value.InetAddress);

	/* Locate the server and update its status. */
	srvent *s = FindServer(host);
	if (!s)
	    Choke("HandleProbe: no srvent (RPCid = %d, PeerHost = %x)", RPCid, host);
	LOG(1, ("HandleProbe: (%s, %d)\n", s->name, rpcval));
	if (rpcval < 0)
	    s->ServerError((int *)&rpcval);
    }

    return(0);
}


/* Report which servers are down. */
void DownServers(char *buf, int *bufsize) {
    char *cp = buf;
    int maxsize = *bufsize;
    *bufsize = 0;

    /* Copy each down server's address into the buffer. */
    srv_iterator next;
    srvent *s;
    while (s = next())
	if (s->ServerIsDown()) {
	    /* Make sure there is room in the buffer for this entry. */
	    if ((cp - buf) + sizeof(unsigned long) > maxsize) return;

	    bcopy((const void *)&s->host, (void *) cp, (int) sizeof(unsigned long));
	    cp += (int) sizeof(unsigned long);
	}

    /* Null terminate the list.  Make sure there is room in the buffer for the terminator. */
    if ((cp - buf) + sizeof(unsigned long) > maxsize) return;
    unsigned long nullint = 0;
    bcopy((const void *)&nullint, (void *) cp, (int) sizeof(unsigned long));
    cp += sizeof(unsigned long);

    *bufsize = (cp - buf);
}


/* Report which of a given set of servers is down. */
void DownServers(int nservers, unsigned long *hostids, char *buf, int *bufsize) {
    char *cp = buf;
    int maxsize = *bufsize;
    *bufsize = 0;

    /* Copy each down server's address into the buffer. */
    for (int i = 0; i < nservers; i++) {
	srvent *s = FindServer(hostids[i]);
	if (s && s->ServerIsDown()) {
	    /* Make sure there is room in the buffer for this entry. */
	    if ((cp - buf) + sizeof(unsigned long) > maxsize) return;

	    bcopy((const void *)&s->host, (void *) cp, (int) sizeof(unsigned long));
	    cp += sizeof(unsigned long);
	}
    }

    /* Null terminate the list.  Make sure there is room in the buffer for the terminator. */
    if ((cp - buf) + sizeof(unsigned long) > maxsize) return;
    unsigned long nullint = 0;
    bcopy((const void *)&nullint, (void *) cp, (int) sizeof(unsigned long));
    cp += sizeof(unsigned long);

    *bufsize = (cp - buf);
}


/* 
 * Update bandwidth estimates for all up servers.
 * Reset estimates and declare connectivity strong if there are
 * no recent observations.  Called by the probe daemon.
 */
void CheckServerBW(unsigned long curr_time) {
    srv_iterator next;
    srvent *s;
    long bw = UNSET_BW;

    while (s = next()) {
	if (s->ServerIsUp()) 
	    (void) s->GetBandwidth(&bw);

	if (s->ServerIsWeak() && 
	    (s->lastobs.tv_sec + WCStale < curr_time))
	    (void) s->InitBandwidth(UNSET_BW);
    }
}


void ServerPrint() {
    ServerPrint(stdout);
}


void ServerPrint(FILE *fp) {
    fflush(fp);
    ServerPrint(fileno(fp));
}


void ServerPrint(int fd) {
    if (srvent::srvtab == 0) return;

    fdprint(fd, "Servers: count = %d\n", srvent::srvtab->count());

    srv_iterator next;
    srvent *s;
    while (s = next()) s->print(fd);

    fdprint(fd, "\n");
}


srvent::srvent(unsigned long Host) {
    LOG(1, ("srvent::srvent: host = %x\n", Host));

    unsigned long nHost = htonl(Host);
    struct hostent *h = gethostbyaddr((char *)&nHost, (int)sizeof(unsigned long), AF_INET);
    if (h) {
	name = new char[strlen(h->h_name) + 1];
	strcpy(name, h->h_name);
	TRANSLATE_TO_LOWER(name);
    }
    else {
	char buf[12];
	sprintf(buf, "%u", Host);
	name = new char[strlen(buf) + 1];
	strcpy(name, buf);
    }

    host = Host;
    connid = -1;
    Xbinding = 0;
    probeme = 0;
    initialbw = 0;
    EventCounter = 0;
    bw = UNSET_BW;
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


int srvent::Connect(RPC2_Handle *cidp, int *authp, vuid_t vuid, int Force) {
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
	code = u->Connect(cidp, authp, host);
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
		Choke("srvent::Connect: illegal RPC code (%s)", RPC2_ErrorMsg(code));
*/
		code = ETIMEDOUT; break;
	}
    if (code == ETIMEDOUT) {
	/* Not already considered down. */
	if (connid != 0) {
	    eprint("%s down", name);
	    Reset();
	    VSGDB->DownEvent(host);
  	    NotifyUsersOfServerDownEvent(name);
	}
    }

    if (code == ETIMEDOUT && VprocInterrupted()) return(EINTR);
    return(code);
}


int srvent::GetStatistics(ViceStatistics *Stats) {
    LOG(100, ("srvent::GetStatistics: host = %s\n", name));

    int code = 0;

    connent *c = 0;
    code = GetConn(&c, host, V_UID, 0);
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


void srvent::Reset() {
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
	mgrp_iterator mgrp_next;
	mgrpent *m;
	while (m = mgrp_next())
	    m->KillMember(host, 0);
    }

    /* Unbind callback connection for this server. */
    int code = (int) RPC2_Unbind(connid);
    LOG(1, ("srvent::Reset: RPC2_Unbind -> %s\n", RPC2_ErrorMsg(code)));
    connid = 0;
}


void srvent::ServerError(int *codep) {
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
	    Choke("srvent::ServerError: illegal RPC code (%d)", *codep);
    }

    if (connid == 0) {
	/* Already considered down. */
	;
    }
    else {
	/* Reset if TIMED'out or NAK'ed. */
	switch (*codep) {
	    case ETIMEDOUT:
		eprint("%s down", name);
		Reset();
		VSGDB->DownEvent(host);
		NotifyUsersOfServerDownEvent(name);
		break;

	    case ERETRY:
		/* Must have missed a down event! */
		eprint("%s naked", name);
		Reset();
		VSGDB->DownEvent(host);
		connid = -2;
		VSGDB->UpEvent(host);
		break;

	    default:
		break;
	}
    }
}


void srvent::ServerUp(RPC2_Handle newconnid) {
    LOG(1, ("srvent::ServerUp: %s, connid = %d, newconnid = %d\n",
	     name, connid, newconnid));

    if (connid == 0) {
	eprint("%s up", name);
	connid = newconnid;
	VSGDB->UpEvent(host);
	NotifyUsersOfServerUpEvent(name);
    }
    else if (connid == -1) {
	/* Initial case.  */
	connid = newconnid;
	VSGDB->UpEvent(host);
    }
    else if (connid == -2) {
	/* Following NAK.  Don't signal another UpEvent! */
	connid = newconnid;
    }
    else {
	/* Already considered up.  Must have missed a down event! */
	Reset();
	VSGDB->DownEvent(host);
	connid = newconnid;
	VSGDB->UpEvent(host);
    }

    /* 
     * if this is the first connection to this server, and we have a
     * saved static estimate, deposit the estimate into the host log.
     */
    if (initialbw) 
	(void) InitBandwidth(bw);

    /* Poke any threads waiting for a change in communication state. */
    Rtry_Signal();
}


long srvent::GetLiveness(struct timeval *tp) {
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
 * calculates current bandwidth to server given observations from
 * RPC2/SFTP.  Returns bandwidth in Bytes/sec, or UNSET_BW if it
 * couldn't be ascertained.  Uses the familiar 
 *	newbw = a*bw + (1-a)*obs 
 * for averaging in new observations, with a=.875.  The calculation
 * is implemented using the equivalent form 
 * 	newbw = bw + g*(obs-bw)
 * where g = 1-a.
 *
 * Triggers weakly/strongly connected transitions if appropriate.
 */

#define BWSHIFT 3

/* returns bandwidth in Bytes/sec, or UNSET_BW if couldn't be obtained */
long srvent::GetBandwidth(long *Bandwidth) {
    RPC2_NetLog Log;
    RPC2_NetLogEntry Entries[RPC2_MAXLOGLENGTH];
    long rc = 0;
    long oldbw = bw;
    float b;

    LOG(100, ("srvent::GetBandwidth (%s) lastobs %ld.%06ld\n", 
	      name, lastobs.tv_sec, lastobs.tv_usec));
    
    *Bandwidth = UNSET_BW;

    /* we don't have a real connid if the server is down or "quasi-up" */
    if (connid <= 0) 
	return(ETIMEDOUT);

    Log.NumEntries = RPC2_MAXLOGLENGTH;
    Log.Quantum = RPC2_MAXQUANTUM;
    Log.Entries = Entries;
    
    /* retrieve the network logs from SFTP */
    if ((rc = RPC2_GetNetInfo(connid, NULL, &Log)) != RPC2_SUCCESS)
	return(rc);
    
    LOG(1000,
	("srvent::GetBandwidth: RPC2_GetNetInfo (%d), %d valid entries\n",
	 connid, Log.ValidEntries));

    /* 
     * average in the observations, least recent first, ignoring
     * ones that have already been seen.
     */
    int newEntries = 0;
    for (int i = Log.ValidEntries-1; i >= 0; i--) 
	if (timercmp(&lastobs, &Log.Entries[i].TimeStamp, <)) {
	    newEntries++;
	    long obs = 0;

	    switch ((int) Log.Entries[i].Tag) {
	    case RPC2_MEASURED_NLE:
		LOG(1000, 
		    ("\tobs at %ld.%06ld: conn %d, %d bytes %d msec\n",
		     Log.Entries[i].TimeStamp.tv_sec,
		     Log.Entries[i].TimeStamp.tv_usec,
		     Log.Entries[i].Value.Measured.Conn,
		     Log.Entries[i].Value.Measured.Bytes,
		     Log.Entries[i].Value.Measured.ElapsedTime));

		/* first part of this could overflow a long */
		b = (Log.Entries[i].Value.Measured.Bytes * 1000)/
		    Log.Entries[i].Value.Measured.ElapsedTime;
		obs = (long) b;

		break;

	    case RPC2_STATIC_NLE:
		LOG(1000,
		    ("\tobs at %ld.%06ld: (static) %d bytes/sec\n",
		     Log.Entries[i].TimeStamp.tv_sec,
		     Log.Entries[i].TimeStamp.tv_usec,
		     Log.Entries[i].Value.Static.Bandwidth));

		obs = Log.Entries[i].Value.Static.Bandwidth;

	    default:
		break;
	    }

	    if (obs) {
		if (bw == UNSET_BW)  	/* initialize w/this obs */
		    bw = obs;	
		else			/* else average it in */
		    bw += (obs-bw) >> BWSHIFT;
		LOG(1000, ("\t-->new BW %d bytes/sec\n", bw));
	    }
	}

    /* update last observation time */
    if (Log.ValidEntries > 0) 
	lastobs = Log.Entries[0].TimeStamp; 

    /* 
     * Signal if we've crossed the weakly-connected threshold. Note
     * that the connection is considered strong until proven otherwise.
     */
    if ((oldbw == UNSET_BW || oldbw > WCThresh) && 
	bw != UNSET_BW && bw <= WCThresh) {
	eprint("%s connection is weak", name);
	VSGDB->WeakEvent(host);
        NotifyUsersOfServerWeakEvent(name);
    }
    else if (oldbw != UNSET_BW && oldbw <= WCThresh && bw > WCThresh) {
	eprint("%s connection is strong", name);
	VSGDB->StrongEvent(host);
        NotifyUsersOfServerStrongEvent(name);
    }
	
    *Bandwidth = bw;
    if (bw != oldbw) {
	MarinerLog("Bandwidth %s (%d) --> %d B/s\n", name, newEntries, bw);
        NotifyUsersOfServerBandwidthEvent(name,*Bandwidth);
    }
    LOG(100, ("srvent::GetBandwidth (%s) returns %d bytes/sec\n",
	      name, *Bandwidth));
    return(0);
}


/* 
 * Initialize the bandwidth to a server, clearing all history.
 * If an estimate is supplied, deposit a static estimate in the
 * transmission log.  If the server is not up yet, the transmission
 * log record will be written when the server becomes available.
 * Note that the ServerIsUp check is not sufficient because it 
 * does not exclude "quasi-up" states, which have no associated 
 * connection ids.  Finally, provoke a transition if necessary.
 */
long srvent::InitBandwidth(long b) {
    long rc = 0;
    long oldbw = bw;

    /* check if we have a real connection. */
    if (connid > 0) {
	/* clear the host logs and drop in a static log entry */
	if ((rc = RPC2_ClearNetInfo(connid)) != RPC2_SUCCESS)
	    return(rc);

	if (b != UNSET_BW) {
	    RPC2_NetLog rlog;
	    RPC2_NetLogEntry rle;

	    rlog.NumEntries = 1;
	    rlog.Entries = &rle;
	    rle.Tag = RPC2_STATIC_NLE;
	    rle.Value.Static.Bandwidth = b;

	    if ((rc = RPC2_PutNetInfo(connid, &rlog, &rlog)) != RPC2_SUCCESS)
		return(rc);
	}

	lastobs.tv_sec = Vtime(); 
	initialbw = 0;
    } else {
	if (b != UNSET_BW)
	    initialbw = 1;
    }

    /* 
     * Install the new estimate and signal weak/strong transitions.
     * Note that the "unset" value is considered strong. Unlike in 
     * srvent::GetBandwidth, it is possible to go from the "set" to 
     * "unset" state.
     */
    bw = b;
    if ((oldbw == UNSET_BW || oldbw > WCThresh) && 
	(bw != UNSET_BW && bw <= WCThresh)) {
	eprint("%s connection is weak", name);
	VSGDB->WeakEvent(host);
        NotifyUsersOfServerWeakEvent(name);
    }
    else if ((oldbw != UNSET_BW && oldbw <= WCThresh) && 
	     (bw == UNSET_BW || bw > WCThresh)) {
	eprint("%s connection is strong", name);
	VSGDB->StrongEvent(host);
        NotifyUsersOfServerStrongEvent(name);
    }

    NotifyUsersOfServerBandwidthEvent(name,bw);

    return(rc);
}


void srvent::print(int fd) {
    fdprint(fd, "%#08x : %-16s : cid = %d, host = %#08x, binding = %d, bw = %d\n",
	     (long)this, name, connid, host, Xbinding, bw);
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

RepOpCommCtxt::RepOpCommCtxt() {
    LOG(100, ("RepOpCommCtxt::RepOpCommCtxt: \n"));

    HowMany = 0;
    bzero((void *)handles, (int)(VSG_MEMBERS * sizeof(RPC2_Handle)));
    bzero((void *)hosts, (int)(VSG_MEMBERS * sizeof(unsigned long)));
    bzero((void *)retcodes, (int)(VSG_MEMBERS * sizeof(int)));
    primaryhost = 0;
    MIp = 0;
    bzero((void *)dying, (int)(VSG_MEMBERS * sizeof(unsigned)));
}


/* ***** Mgroup  ***** */

const int MAXMGRPSPERUSER = 27;  /* Max simultaneous mgrps per user per vsg. */
                                /* 3 requests/AVSG member */
#define	MGRPQ_LOCK()
#define	MGRPQ_UNLOCK()
#define	MGRPQ_WAIT()	    VprocWait((char *)&mgrpent::mgrptab_sync)
#define	MGRPQ_SIGNAL()	    VprocSignal((char *)&mgrpent::mgrptab_sync)

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


int GetMgrp(mgrpent **mpp, unsigned long VSGAddr, vuid_t vuid) {
    LOG(100, ("GetMgrp: VSGAddr = %x, vuid = %d\n", VSGAddr, vuid));

    *mpp = 0;
    int code = 0;
    mgrpent *m = 0;
    int found = 0;

    /* Grab an existing mgrp if one is free. */
    /* Before creating a new mgrp, make sure the per-user limit is not exceeded. */
    for (;;) {
	/* Make sure tokens are not expired. */
	userent *u = 0;
	GetUser(&u, vuid);
	u->CheckTokenExpiry();
	PutUser(&u);

	/* Check whether there is already a free mgroup. */
	struct MgrpKey Key; Key.vsgaddr = VSGAddr; Key.vuid = vuid;
	mgrp_iterator next(&Key);
	int count = 0;
	while (m = next()) {
	    count++;
	    if (!m->inuse) {
		m->inuse = 1;
		found = 1;
		break;
	    }
	}
	if (found) break;

	/* Wait here if MAX mgrps are already in use. */
	/* Synchronization needs fixed for MP! -JJK */
	if (count < MAXMGRPSPERUSER) break;
	if (VprocInterrupted()) return(EINTR);
	Mgrp_Wait();
	if (VprocInterrupted()) return(EINTR);
    }

    if (!m) {
	/* Try to connect to the VSG on behalf of the user. */
	vsgent *vsgp = 0;
	if ((code = VSGDB->Get(&vsgp, VSGAddr)) != 0) return(code);
	RPC2_Handle MgrpHandle = 0;
	int auth = 0;
	code = vsgp->Connect(&MgrpHandle, &auth, vuid);
	VSGDB->Put(&vsgp);
	if (code != 0) return(code);

	/* Create and install the new mgrpent. */
	m = new mgrpent(VSGAddr, vuid, MgrpHandle, auth);
	m->inuse = 1;
	mgrpent::mgrptab->insert(&m->tblhandle);
    }

    /* Form the host set. */
    code = m->GetHostSet();
    if (m->dying || code != 0) {
	if (m->dying) code = ERETRY;
	PutMgrp(&m);
	return(code);
    }

    /* Choose whether to multicast or not. */
    m->rocc.MIp = (UseMulticast) ? &m->McastInfo : 0;

    *mpp = m;
    return(0);
}


void PutMgrp(mgrpent **mpp) {
    mgrpent *m = *mpp;
    *mpp = 0;
    if (m == 0) {
	LOG(100, ("PutMgrp: null mgrp\n"));
	return;
    }

    LOG(100, ("PutMgrp: vsgaddr = %#08x, uid = %d, mid = %d, auth = %d, inuse = %d, dying = %d\n",
	     m->VSGAddr, m->uid, m->McastInfo.Mgroup, m->authenticated, m->inuse, m->dying));

    if (!m->inuse)
	{ m->print(logFile); Choke("PutMgrp: mgrp not in use"); }

    /* Clean up the host set. */
    m->PutHostSet();

    if (m->dying) {
	mgrpent::mgrptab->remove(&m->tblhandle);
	delete m;
    }
    else {
	m->inuse = 0;
    }

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
    if (mgrpent::mgrptab == 0) return;

    fdprint(fd, "Mgroups: count = %d\n", mgrpent::mgrptab->count());

    /* Iterate through the individual entries. */
    mgrp_iterator next;
    mgrpent *m;
    while (m = next()) m->print(fd);

    fdprint(fd, "\n");
}


mgrpent::mgrpent(unsigned long vsgaddr, vuid_t vuid,
		  RPC2_Handle mid, int authflag) {
    LOG(1, ("mgrpent::mgrpent: vsgaddr = %#08x, uid = %d, mid = %d, auth = %d\n",
	     vsgaddr, vuid, mid, authflag));

    /* These members are immutable. */
    VSGAddr = vsgaddr;
    uid = vuid;
    bzero((void *)&McastInfo, (int)sizeof(RPC2_Multicast));
    McastInfo.Mgroup = mid;
    McastInfo.ExpandHandle = 0;
    vsgent *vsgp;
    if (VSGDB->Get(&vsgp, vsgaddr) != 0)
	Choke("mgrpent::mgrpent: can't get VSG (%x)", vsgaddr);
    vsgp->GetHosts(Hosts);
    nhosts = 0;
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (Hosts[i]) nhosts++;
    VSGDB->Put(&vsgp);
    authenticated = authflag;

    /* These members are mutable. */
    inuse = 0;
    dying = 0;

#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


mgrpent::~mgrpent() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG

    LOG(1, ("mgrpent::~mgrpent: vsgaddr = %#08x, uid = %d, mid = %d, auth = %d\n",
	    VSGAddr, uid, McastInfo.Mgroup, authenticated));

    int code = 0;

    /* Kill active members. */
    for (int i = 0; i < VSG_MEMBERS; i++)
	KillMember(rocc.hosts[i], 1);

    /* Delete Mgroup. */
    code = (int) RPC2_DeleteMgrp(McastInfo.Mgroup);
    LOG(1, ("mgrpent::~mgrpent: RPC2_DeleteMgrp -> %s\n", RPC2_ErrorMsg(code)));
}


int mgrpent::Suicide(int disconnect) {
    LOG(1, ("mgrpent::Suicide: vsgaddr = %#08x, uid = %d, mid = %d, disconnect = %d\n", 
	    VSGAddr, uid, McastInfo.Mgroup, disconnect));

    dying = 1;

    if (inuse) return(0);

    inuse = 1;

    if (disconnect) {
	/* Make the RPC call. */
	MarinerLog("fetch::DisconnectFS (%#x)\n", VSGAddr);
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

PRIVATE int Unanimity(int *codep, unsigned long *hosts, RPC2_Integer *retcodes, int mask) {
    int	code = -1;

    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (hosts[i] == 0) continue;

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


PRIVATE int AnyReturned(unsigned long *hosts, RPC2_Integer *retcodes, int code) {
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (hosts[i] == 0) continue;

	if (retcodes[i] == code) return(1);
    }

    return(0);
}


/* Translate RPC and Volume errors, and update server state. */
void mgrpent::CheckResult() {
    
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (rocc.hosts[i] == 0) continue;

	switch(rocc.retcodes[i]) {
	    default:
		if (rocc.retcodes[i] < 0) {
		    srvent *s = 0;
		    GetServer(&s, rocc.hosts[i]);
		    s->ServerError((int *)&rocc.retcodes[i]);
		    PutServer(&s);

		    /* Note that KillMember may zero rocc.hosts[i] !!! */
		    if (rocc.retcodes[i] == ETIMEDOUT || rocc.retcodes[i] == ERETRY)
			KillMember(rocc.hosts[i], 1);
		}
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
		eprint("mgrpent::CheckResult: illegal code (%d)", rocc.retcodes[i]);
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
int mgrpent::CheckNonMutating(int acode) {
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
	if (rocc.hosts[i] == 0) continue;

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

    /* Since this operation is non-mutating, we can retry immediately if any host NAK'ed. */
    if (AnyReturned(rocc.hosts, rocc.retcodes, ERETRY))
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

    CheckResult();
    
    /* check for this here because CheckResult may nuke hosts */
    if (rocc.HowMany == 0) return(ETIMEDOUT);

    /* Perform additional translations. */
    for (i = 0; i < VSG_MEMBERS; i++) {
	if (rocc.hosts[i] == 0) continue;

	switch(rocc.retcodes[i]) {
	    case EINCOMPATIBLE:
		if (TranslateEincompatible)
		    rocc.retcodes[i] = ERETRY;	    /* NOT for reintegrate! */
		break;
	}
    }

    /* Record successes in the UpdateSet. */
    InitVV(UpdateSet);
    for (i = 0; i < VSG_MEMBERS; i++) {
	if (rocc.hosts[i] == 0) continue;

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
    if (AnyReturned(rocc.hosts, rocc.retcodes, 0))
	return(EASYRESOLVE);
    return(ERETRY);
}


/* This is identical to mgrpent::CheckCOP1(), EXCEPT that we want to treat */
/* EINCOMPATIBLE results as non-maskable rather that translating them to ERETRY. */
int mgrpent::CheckReintegrate(int acode, vv_t *UpdateSet) {
    return(CheckCOP1(acode, UpdateSet, 0));
}


/* Check the remote vectors. */
/* Returns:
	0		Version check succeeded
	ESYNRESOLVE	Version check failed
	EASYRESOLVE	!EqReq and check yielded Dom/Sub
*/
int mgrpent::RVVCheck(vv_t **RVVs, int EqReq) {
    /* Construct the array so that only valid VVs are checked. */
    for (int j = 0; j < VSG_MEMBERS; j++)
	if (rocc.hosts[j] == 0 || rocc.retcodes[j] != 0) RVVs[j] = 0;
    if (LogLevel >= 100) VVPrint(logFile, RVVs);

    int dom_cnt = 0;
    if (!VV_Check(&dom_cnt, RVVs, EqReq)) {
	return(ESYNRESOLVE);
    }
    else {
	if (dom_cnt <= 0 || dom_cnt > rocc.HowMany) {
	    print(logFile);
	    Choke("mgrpent::RVVCheck: bogus dom_cnt (%d)", dom_cnt);
	}

	/* Notify servers which have out of date copies. */
	if (dom_cnt < rocc.HowMany) return(EASYRESOLVE);
    }

    return(0);
}


/* Validate the existence of a dominant host; return its index in OUT parameter. */
/* If there are multiple hosts in the dominant set, prefer the primary host. */
/* The caller may specify that the PH must be dominant. */
/* Returns {0, ERETRY}. */
int mgrpent::DHCheck(vv_t **RVVs, int ph_ix, int *dh_ixp, int PHReq) {
    *dh_ixp = -1;

    /* Return the primary host if it is in the dominant set. */
    if (ph_ix != -1) 
	if (rocc.hosts[ph_ix] != 0 && rocc.retcodes[ph_ix] == 0 &&
	    (RVVs == 0 || RVVs[ph_ix] != 0))
	    {
		*dh_ixp = ph_ix;
		return(0);
	    }
	
    /* Find a non-primary host from the dominant set. */
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (rocc.hosts[i] != 0 && rocc.retcodes[i] == 0 && (RVVs == 0 || RVVs[i] != 0)) {
	    if (PHReq) {
		LOG(1, ("VSG (%x) PH -> %x", VSGAddr, rocc.hosts[i]));
		SetPrimaryHost(rocc.hosts[i]);
		return(ERETRY);
	    }

	    *dh_ixp = i;
	    return(0);
	}

    /* Dominant set is empty. */
    Choke("mgrpent::DHCheck: dominant set is empty");
    return(0);	/* dummy to keep g++ happy */
}


int mgrpent::GetHostSet() {
    int i;
    LOG(100, ("mgrpent::GetHostSet: vsgaddr = %#08x, uid = %d, mid = %d\n",
	      VSGAddr, uid, McastInfo.Mgroup));

    /* Create members of the specified set which are not already in the Mgroup. */
    for (i = 0; i < VSG_MEMBERS; i++)
	if (Hosts[i] != 0 && rocc.hosts[i] == 0) {
	    switch(CreateMember(Hosts[i])) {
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
	if (Hosts[i] == 0 && rocc.hosts[i] != 0)
	    KillMember(rocc.hosts[i], 1);

    /* Ensure that Mgroup is not empty. */
    if (rocc.HowMany == 0) return(ETIMEDOUT);

    /* Validate primaryhost. */
    if (rocc.primaryhost == 0)
	SetPrimaryHost();

    return(0);
}


int mgrpent::CreateMember(unsigned long host) {
    int i;
    LOG(100, ("mgrpent::CreateMember: vsgaddr = %#08x, uid = %d, mid = %d, host = %x\n", 
	      VSGAddr, uid, McastInfo.Mgroup, host));

    /* Don't re-create members that already exist. */
    for (i = 0; i < VSG_MEMBERS; i++)
	if (rocc.hosts[i] == host) return(0);

    /* Deduce index of specified host. */
    for (i = 0; i < VSG_MEMBERS; i++)
	if (Hosts[i] == host) break;
    if (i == VSG_MEMBERS) Choke("mgrpent::CreateMember: no host (%x)", host);

    int code = 0;

    /* Bind/Connect to the server. */
    srvent *s = 0;
    GetServer(&s, host);
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
	Choke("mgrpent::CreateMember: AddToMgrp failed (%d)", code);
*/
	(void) RPC2_Unbind(ConnHandle);
	return(ETIMEDOUT);
    }

    /* Update rocc state. */
    rocc.HowMany++;
    rocc.handles[i] = ConnHandle;
    rocc.hosts[i] = host;
    rocc.retcodes[i] = 0;
    rocc.dying[i] = 0;

    return(0);
}


void mgrpent::PutHostSet() {
    LOG(100, ("mgrpent::PutHostSet: \n"));

    /* Kill dying members. */
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (rocc.dying[i]) KillMember(rocc.hosts[i], 1);
}


void mgrpent::KillMember(unsigned long host, int forcibly) {
    LOG(100, ("mgrpent::KillMember: vsgaddr = %#08x, uid = %d, mid = %d, host = %x, forcibly = %d\n",
	      VSGAddr, uid, McastInfo.Mgroup, host, forcibly));

    long code = 0;

    if (host == 0) return;

    for (int i = 0; i < VSG_MEMBERS; i++)
	if (rocc.hosts[i] == host) {
	    if (inuse && !forcibly) {
		rocc.dying[i] = 1;
		continue;
	    }

	    if (rocc.hosts[i] == rocc.primaryhost) {
		rocc.primaryhost = 0;
	    }
	    code = RPC2_RemoveFromMgrp(McastInfo.Mgroup, rocc.handles[i]);
	    LOG(1, ("mgrpent::KillMember: RPC2_RemoveFromMgrp(%x, %d) -> %s\n",
		    rocc.hosts[i], rocc.handles[i], RPC2_ErrorMsg((int) code)));
	    code = RPC2_Unbind(rocc.handles[i]);
	    LOG(1, ("mgrpent::KillMember: RPC2_Unbind(%x, %d) -> %s\n",
		    rocc.hosts[i], rocc.handles[i], RPC2_ErrorMsg((int) code)));
	    rocc.HowMany -= 1;
	    rocc.handles[i] = 0;
	    rocc.hosts[i] = 0;
	    rocc.dying[i] = 0;
	}
}


unsigned long mgrpent::GetPrimaryHost(int *ph_ixp) {
    if (ph_ixp != 0) *ph_ixp = -1;

    if (rocc.primaryhost == 0) return(0);

    /* Sanity check. */
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (rocc.hosts[i] == rocc.primaryhost) {
	    if (ph_ixp != 0) *ph_ixp = i;
	    /* Add a round robin distribution, primarily to spread fetches across AVSG */
	    if (RoundRobin) {
		int j;
		for (j = i + 1; j != i; j = (j + 1) % VSG_MEMBERS)
		    if (rocc.hosts[j] != 0) {
			/* We have a valid host. It'd be nice to use strongly
			   connected hosts in preference to weak ones, but I'm
			   not sure how to access to srvent from here.
			   -- DCS 2/2/96
			   */
			rocc.primaryhost = rocc.hosts[j];
			break;
		    }
	    }
	    return(rocc.hosts[i]);
	}

    Choke("mgrpent::GetPrimaryHost: ph (%x) not found", rocc.primaryhost);
    return(0);	/* dummy to keep g++ happy */
}


/* Arg of 0 --> set primary host to any valid host. */
/* Choice of host ought to be made more intelligently! -JJK */
void mgrpent::SetPrimaryHost(unsigned long host) {
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (rocc.hosts[i] == 0) continue;

	if (host == 0 || rocc.hosts[i] == host) {
	    rocc.primaryhost = rocc.hosts[i];
	    return;
	}
    }

    Choke("mgrpent::SetPrimaryHost: ph not found");
}


mgrp_iterator::mgrp_iterator(struct MgrpKey *Key) : olist_iterator((olist&)*mgrpent::mgrptab) {
    key = Key;
}


mgrpent *mgrp_iterator::operator()() {
    olink *o;
    while (o = olist_iterator::operator()()) {
	mgrpent *m = strbase(mgrpent, o, tblhandle);
	if (key == (struct MgrpKey *)0) return(m);
	if ((key->vsgaddr == m->VSGAddr || key->vsgaddr == ALL_VSGS) &&
	    (key->vuid == m->uid || key->vuid == ALL_UIDS))
	    return(m);
    }

    return(0);
}


/* ***** VSG  ***** */

/* This really should be in a separate module. */

void VSGInit() {
    LOG(10, ("VSGInit: VSGDB = %x, InitMetaData = %d\n", VSGDB, InitMetaData));

    /* Allocate the database if requested. */
    if (InitMetaData) {					/* <==> VSGDB == 0 */
	TRANSACTION(
	    RVMLIB_REC_OBJECT(VSGDB);
	    VSGDB = new vsgdb;
	)
    }

    /* Initialize transient members. */
    VSGDB->ResetTransient();

    /* Scan the database. */
    eprint("starting VSGDB scan");
    {
	/* Check entries in the table. */
	{
	    vsg_iterator next;
	    vsgent *v;
	    while (v = next())
		/* Initialize transient members. */
		v->ResetTransient();

	    eprint("\t%d vsg entries in table", VSGDB->htab.count());
	}

	/* Check entries on the freelist. */
	{
	    /* Nothing useful to do! */

	    eprint("\t%d vsg entries on free-list", VSGDB->freelist.count());
	}

	if (VSGDB->htab.count() + VSGDB->freelist.count() > CacheFiles)
	    Choke("VSGInit: too many vsg entries (%d + %d > %d)",
		VSGDB->htab.count(), VSGDB->freelist.count(), CacheFiles);
    }

    if (!Simulating) {
	RecovFlush(1);
	RecovTruncate(1);
    }

    /* Fire up the daemon. */
    VSGD_Init();
}


PRIVATE int VSG_HashFN(void *key) {
    return(*((unsigned long *)key));
}


/* Allocate database from recoverable store. */
void *vsgdb:: operator new(size_t size){
    vsgdb *v = 0;

    /* Allocate recoverable store for the object. */
    v = (vsgdb *)RVMLIB_REC_MALLOC(size);
    assert(v);
    return(v);
}


vsgdb::vsgdb() : htab(VSGDB_NBUCKETS, VSG_HashFN) {
    LOG(10, ("vsgdb::vsgdb: this = %x\n", this));

    /* Initialize the persistent members. */
    RVMLIB_REC_OBJECT(*this);
    MagicNumber = VSGDB_MagicNumber;
}


void vsgdb::ResetTransient() {
    LOG(10, ("vsgdb::ResetTransient: this = %x\n", this));

    /* Sanity checks. */
    if (MagicNumber != VSGDB_MagicNumber)
	Choke("vsgdb::Init: bad magic number (%d)", MagicNumber);

    htab.SetHFn(VSG_HashFN);
}


void vsgdb::operator delete(void *deadobj, size_t size) {
    abort();
}

vsgent *vsgdb::Find(unsigned long Addr) {
    vsg_iterator next(&Addr);
    vsgent *v;
    while (v = next())
	if (v->Addr == Addr) return(v);

    return(0);
}


/* MUST NOT be called from within transaction! */
vsgent *vsgdb::Create(unsigned long Addr, unsigned long *Hosts) {
    vsgent *v = 0;

    /* Check whether the key is already in the database. */
    if ((v = Find(Addr)) != 0)
	{ v->print(logFile); Choke("vsgdb::Create: key exists"); }

    /* Fashion a new object. */
    ATOMIC(
	v = new vsgent(0/*priority*/, Addr, Hosts);
    , MAXFP)

    if (v == 0)
	LOG(0, ("vsgdb::Create: (%x, %d) failed\n", Addr, 0/*priority*/));
    return(v);
}


/* MUST NOT be called from within transaction! */
int vsgdb::Get(vsgent **vpp, unsigned long Addr, unsigned long *Hosts) {
    LOG(100, ("vsgdb::Get: Addr = %x, Hosts = %x\n", Addr, Hosts));

    int code = 0;
    *vpp = 0;
    vsgent *v = 0;

    /* Check for existing VSG entry. */
    v = Find(Addr);
    if (v == 0) {
	if (Hosts == 0)
	    Choke("vsgdb::Get: Addr (%x) not found and Hosts == 0", Addr);

	v = Create(Addr, Hosts);
	if (v == 0)
	    Choke("vsgdb::Get: Create (%x, [%x %x %x %x %x %x %x %x]) failed",
		Addr, Hosts[0], Hosts[1], Hosts[2], Hosts[3], Hosts[4], Hosts[5], Hosts[6], Hosts[7]);
    }
    else {
	if (Hosts != 0) {
	    /* Sanity check. */
	    if (bcmp((const void *)Hosts, (const void *) v->Hosts, (int)(VSG_MEMBERS * sizeof(unsigned long))) != 0)
		{ v->print(logFile); Choke("vsgdb::Get: inconsistent VSG entries"); }
	}
    }

    v->hold();
    *vpp = v;
    return(0);
}


void vsgdb::Put(vsgent **vpp) {
    if (!(*vpp)) { LOG(100, ("vsgdb::Put: Null vpp\n")); return; }

    vsgent *v = *vpp;
    LOG(100, ("vsgdb::Put: (%x), refcnt = %d\n", v->Addr, v->refcnt));

    v->release();
    *vpp = 0;
}


void vsgdb::DownEvent(unsigned long host) {
    LOG(10, ("vsgdb::DownEvent: host = %x\n", host));

    long eventTime = Vtime();
    VmonEnqueueCommEvent(host, eventTime, ::ServerDown);

    /* Notify each VSG that includes given host of its failure. */
    {
	vsg_iterator next;
	vsgent *v;
	while (v = next())
	    for (int i = 0; i < VSG_MEMBERS; i++)
		if (v->Hosts[i] == host) {
		    v->DownMember(eventTime);
		    break;
		}
    }

    /* Annoyingly, we must effectively do vsgent::DownMember for the host in question, since non-replicated */
    /* volumes do not currently belong to a VSG (and notifying the volents is what we really care about)! */
    {
	srvent *s = 0;
	GetServer(&s, host);
	s->EventCounter++;
	PutServer(&s);

	vol_iterator next;
	volent *v;
	while (v = next())
	    if (v->type != ROVOL && v->type != REPVOL && v->host == host)
		v->DownMember(eventTime);
    }

    /* provoke state transitions now */
    VprocSignal(&vol_sync);
}


void vsgdb::UpEvent(unsigned long host) {
    LOG(10, ("vsgdb::UpEvent: host = %x\n", host));

    long eventTime = Vtime();
    VmonEnqueueCommEvent(host, eventTime, ::ServerUp);

    /* Notify each VSG that includes given host of its recovery. */
    {
	vsg_iterator next;
	vsgent *v;
	while (v = next())
	    for (int i = 0; i < VSG_MEMBERS; i++)
		if (v->Hosts[i] == host) {
		    v->UpMember(eventTime);
		    break;
		}
    }

    /* Annoyingly, we must effectively do vsgent::UpEvent for the host in question, since non-replicated */
    /* volumes do not currently belong to a VSG (and notifying the volents is what we really care about)! */
    {
	srvent *s = 0;
	GetServer(&s, host);
	s->EventCounter++;
	PutServer(&s);

	vol_iterator next;
	volent *v;
	while (v = next())
	    if (v->type != ROVOL && v->type != REPVOL && v->host == host)
		v->UpMember(eventTime);
    }

    /* provoke state transitions now */
    VprocSignal(&vol_sync);
}


/* 
 * vsgdb::{Weak,Strong}Event.  Notifies each VSG that includes
 * the given host of its change in connectivity.
 */
void vsgdb::WeakEvent(unsigned long host) {
    LOG(0/*10*/, ("vsgdb::WeakEvent: host = %x\n", host));

    vsg_iterator next;
    vsgent *v;
    while (v = next())
	for (int i = 0; i < VSG_MEMBERS; i++)
	    if (v->Hosts[i] == host) {
		v->WeakMember();
		break;
	    }
}

void vsgdb::StrongEvent(unsigned long host) {
    LOG(0/*10*/, ("vsgdb::StrongEvent: host = %x\n", host));

    vsg_iterator next;
    vsgent *v;
    while (v = next())
	for (int i = 0; i < VSG_MEMBERS; i++)
	    if (v->Hosts[i] == host) {
		v->StrongMember();
		break;
	    }
}


void vsgdb::print(int fd, int SummaryOnly) {
    if (this == 0) return;

    fdprint(fd, "VSGDB:\n");
    fdprint(fd, "htab count = %d, freelist count = %d\n",
	     htab.count(), freelist.count());

    if (!SummaryOnly) {
	vsg_iterator next;
	vsgent *v;
	while (v = next())
	    v->print(fd);
    }

    fdprint(fd, "\n");
}


/* MUST be called from within transaction! */
void *vsgent::operator new(size_t size){
    vsgent *v = 0;
    
    if (VSGDB->freelist.count() > 0)
	v = strbase(vsgent, VSGDB->freelist.get(), handle);
    else v = (vsgent *)RVMLIB_REC_MALLOC(size);
    assert(v);
    return(v);
}

vsgent::vsgent(int AllocPriority, unsigned long addr, unsigned long *hosts) {
    LOG(10, ("vsgent::vsgent: (%x, [%x %x %x %x %x %x %x %x])\n",
	      addr, hosts[0], hosts[1], hosts[2], hosts[3], hosts[4], hosts[5], hosts[6], hosts[7]));

    RVMLIB_REC_OBJECT(*this);
    MagicNumber = VSGENT_MagicNumber;
    Addr = addr;
    bcopy((const void *)hosts, (void *) Hosts, (int)(VSG_MEMBERS * sizeof(unsigned long)));
    ResetTransient();

    /* Insert into hash table. */
    VSGDB->htab.append(&Addr, &handle);
}


void vsgent::ResetTransient() {
    /* Sanity checks. */
    if (MagicNumber != VSGENT_MagicNumber)
	{ print(logFile); Choke("vsgent::ResetTransient: bogus MagicNumber"); }

    refcnt = 0;
    EventCounter = 0;
}


/* MUST be called from within transaction! */
vsgent::~vsgent() {
    LOG(10, ("vsgent::~vsgent: (%x, [%x %x %x %x %x %x %x %x]), refcnt = %d\n",
	      Addr, Hosts[0], Hosts[1], Hosts[2], Hosts[3], Hosts[4], Hosts[5], Hosts[6], Hosts[7], refcnt));

    if (refcnt != 0)
	{ print(logFile); Choke("vsgent::~vsgent: non-zero refcnt"); }

    /* Remove from hash table. */
    if (VSGDB->htab.remove(&Addr, &handle) != &handle)
	{ print(logFile); Choke("vsgent::~vsgent: htab remove"); }

}

void vsgent::operator delete(void *deadobj, size_t size) {
    vsgent *v;
    
    v = (vsgent *)deadobj;

    /* Stick on free list or give back to heap. */
    if (VSGDB->freelist.count() < VSGMaxFreeEntries)
	VSGDB->freelist.append(&v->handle);
    else
	RVMLIB_REC_FREE(v);
}


void vsgent::hold() {
    refcnt++;
}


void vsgent::release() {
    refcnt--;

    if (refcnt < 0)
	{ print(logFile); Choke("vsgent::release: refcnt < 0"); }
}


void vsgent::GetHosts(unsigned long *hosts) {
    bcopy((const void *)Hosts, (void *)hosts, (int)(VSG_MEMBERS * sizeof(unsigned long)));
}


int vsgent::IsMember(unsigned long host) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (host == Hosts[i]) return(1);

    return(0);
}


void vsgent::DownMember(long eventTime) {
    EventCounter++;

    /* Yuck!  We really need a list of volumes belonging to a given VSG! */
    vol_iterator next;
    volent *v;
    while (v = next())
	if (v->vsg == this)
	    v->DownMember(eventTime);
}


void vsgent::UpMember(long eventTime) {
    EventCounter++;

    /* Yuck!  We really need a list of volumes belonging to a given VSG! */
    vol_iterator next;
    volent *v;
    while (v = next())
	if (v->vsg == this)
	    v->UpMember(eventTime);
}


void vsgent::WeakMember() {
    vol_iterator next;
    volent *v;
    while (v = next())
	if (v->vsg == this)
	    v->WeakMember();
}


void vsgent::StrongMember() {
    vol_iterator next;
    volent *v;
    while (v = next())
	if (v->vsg == this)
	    v->StrongMember();
}


/* returns minimum bandwidth in Bytes/sec, or UNSET_BW if none obtainable */
void vsgent::GetBandwidth(long *Bandwidth) {
    *Bandwidth = UNSET_BW;

    for (int i = 0; i < VSG_MEMBERS; i++)
	if (Hosts[i]) {
	    long bw = UNSET_BW;
	    srvent *s = 0;
	    GetServer(&s, Hosts[i]);

	    if (s->ServerIsUp()) {	/* need a connection */
		(void) s->GetBandwidth(&bw);
		if (bw != UNSET_BW && 
		    (*Bandwidth == UNSET_BW ||
		     (*Bandwidth != UNSET_BW && bw < *Bandwidth)))
		    *Bandwidth = bw;
	    }
	}

    LOG(100, ("vsgent::GetBandwidth: (%x) returns %d\n",
		   Addr, *Bandwidth));
}


int vsgent::Connect(RPC2_Handle *midp, int *authp, vuid_t vuid) {
    LOG(100, ("vsgent::Connect: addr = %x, uid = %d\n", Addr, vuid));

    int code = 0;

    /* Get the user entry and attempt to form the mgrp. */
    userent *u = 0;
    GetUser(&u, vuid);
    code = u->Connect(midp, authp, Addr);
    PutUser(&u);

    if (code != 0)
	Choke("vsgent::Connect: (%x) failed (%d)", Addr, code);
    return(0);
}


void vsgent::print(int fd) {
    fdprint(fd, "%#08x : addr = %x, refcnt = %d, eventcnt = %d\n",
	     (long)this, Addr, refcnt, EventCounter);
    fdprint(fd, "\tHosts: [");
    for (int i = 0; i < VSG_MEMBERS; i++)
	fdprint(fd, " %x", Hosts[i]);
    fdprint(fd, " ]\n");
}


vsg_iterator::vsg_iterator(void *key) : rec_ohashtab_iterator(VSGDB->htab, key) {
}


vsgent *vsg_iterator::operator()() {
    rec_olink *o = rec_ohashtab_iterator::operator()();
    if (!o) return(0);

    vsgent *v = strbase(vsgent, o, handle);
    return(v);
}

/* *****  Fail library manipulations ***** */

/* 
 * Simulate "pulling the plug". Insert filters on the
 * send and receive sides of venus.
 */
int FailDisconnect(int nservers, unsigned long *hostids)
{
    int rc, k = 0;
    FailFilter filter;
    FailFilterSide side;

    do {
	srv_iterator next;
	srvent *s;
	while (s = next())
	    if (nservers == 0 || s->host == hostids[k]) {
		/* we want a pair of filters for server s. */

		unsigned long addr = htonl(s->host);    
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
			    FailFilterInfo[j].host == s->host &&
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

			FailFilterInfo[ix].id = rc;
			FailFilterInfo[ix].side = side;
			FailFilterInfo[ix].host = s->host;
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
int FailReconnect(int nservers, unsigned long *hostids)
{
    int rc, s = 0;

    do {
	for (int i = 0; i < MAXFILTERS; i++) 
	    if (FailFilterInfo[i].used && (nservers == 0 || (FailFilterInfo[i].host == hostids[s])))
		if (rc = Fail_RemoveFilter(FailFilterInfo[i].side, FailFilterInfo[i].id) < 0) {
		    LOG(0, ("FailReconnect: couldn't remove %s filter, id = %d\n", 
			(FailFilterInfo[i].side == sendSide)?"send":"recv", 
			FailFilterInfo[i].id));
		} else {
		    LOG(10, ("FailReconnect: removed %s filter, id = %d\n", 
			(FailFilterInfo[i].side == sendSide)?"send":"recv", 
			FailFilterInfo[i].id));
		    FailFilterInfo[i].used = 0;
		}
    } while (s++ < nservers-1);

    return(0);
}

/* 
 * Simulate a slow network. Insert filters with the
 * specified speed to all known servers.
 */
int FailSlow(unsigned *speedp) {
    struct timeval timeout;
    srvent *s;
    srv_iterator next;
    long rc;

    return(-1);
}


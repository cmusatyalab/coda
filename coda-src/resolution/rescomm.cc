/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/* implementation of replicated communication for resolve 
 * Puneet Kumar, Created June 1990
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include "coda_assert.h"
#include <struct.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <rpc2/errors.h>

#ifdef __cplusplus
}
#endif

#include <coda_getservbyname.h>
#include <util.h>
#include <srv.h>
#include "rescomm.private.h"
#include "rescomm.h"
#include "resolution.h"

/* Storage for static class members defined in rescomm.h */
dlist *res_mgrpent::ResMgrpTab = 0;
int res_mgrpent::resmgrps      = 0;
olist *srvent::srvtab          = 0;
int srvent::servers            = 0;
olist *conninfo::CInfoTab      = 0;
int conninfo::ncinfos          = 0;

int PutResMgroup(res_mgrpent **);

void ResProcWait(char *addr)
{
    if (LWP_WaitProcess(addr) != LWP_SUCCESS)
        CODA_ASSERT(0);
}

void ResProcSignal(char *addr, int yield)
{
    int lwprc = (yield ? LWP_SignalProcess(addr) : LWP_NoYieldSignal(addr));
    if (lwprc != LWP_SUCCESS && lwprc != LWP_ENOWAIT)
        CODA_ASSERT(0);
}

void ResCommInit()
{
    /* Initialize Mgroups */
    res_mgrpent::ResMgrpTab = new dlist;
    res_mgrpent::resmgrps   = 0;

    /* Initialize servers */
    srvent::srvtab  = new olist;
    srvent::servers = 0;

    /* Initialize connection infos */
    conninfo::CInfoTab = new olist;
    conninfo::ncinfos  = 0;
}

/* RepResCommCtxt - implementation */
RepResCommCtxt::RepResCommCtxt()
{
    LogMsg(100, SrvDebugLevel, stdout, "RepResCommCtxt::RepResCommCtxt()");

    HowMany = 0;
    memset((void *)handles, 0, VSG_MEMBERS * sizeof(RPC2_Handle));
    memset((void *)hosts, 0, VSG_MEMBERS * sizeof(unsigned long));
    memset((void *)retcodes, 0, VSG_MEMBERS * sizeof(int));
    primaryhost = 0;
    MIp         = 0;
    memset((void *)dying, 0, VSG_MEMBERS * sizeof(unsigned));
}

RepResCommCtxt::~RepResCommCtxt() {}

void RepResCommCtxt::print()
{
    print(stdout);
}

void RepResCommCtxt::print(FILE *fp)
{
    fflush(fp);
    print(fileno(fp));
}

void RepResCommCtxt::print(int fd)
{
    char buf[80];
    sprintf(buf, "%p : HowMany = %d\n", this, HowMany);
    write(fd, buf, strlen(buf));
}

/* res_mgrpent implementation */
res_mgrpent::res_mgrpent(unsigned long hosts[VSG_MEMBERS], RPC2_Handle mid)
{
    SLog(20, "res_mgrpent::resmgrpent mid = %d", mid);
    memset((void *)&McastInfo, 0, sizeof(RPC2_Multicast));
    McastInfo.Mgroup       = mid;
    McastInfo.ExpandHandle = 0;

    memcpy(Hosts, hosts, sizeof(unsigned long) * VSG_MEMBERS);

    inuse = 0;
    dying = 0;
}

res_mgrpent::~res_mgrpent()
{
    /* kill the member connectins */
    LogMsg(100, SrvDebugLevel, stdout, "~res_mgrpent::");
    for (int i = 0; i < VSG_MEMBERS; i++)
        KillMember(rrcc.hosts[i], 1);
    /* Delete Mgroup */
    /* code = RPC2_DeleteMgrp(McastInfo.Mgroup); */
}

int res_mgrpent::CreateMember(unsigned long host)
{
    int i;
    int code = 0;

    CODA_ASSERT(host != 0);

    /* Don't re-create members that already exist. */
    SLog(20, "res_mgrepent::CreateMember(%x)", host);
    for (i = 0; i < VSG_MEMBERS; i++)
        if (rrcc.hosts[i] == host)
            return (0);

    /* Deduce index of specified host. */
    for (i = 0; i < VSG_MEMBERS; i++)
        if (Hosts[i] == host)
            break;
    if (i == VSG_MEMBERS) {
        SLog(0, "res_mgrpent::CreateMember: no host (%x)", host);
        CODA_ASSERT(0);
    }

    /* bind to server */
    srvent *s = 0;
    GetServer(&s, host);
    RPC2_Handle ConnHandle = 0;
    code                   = s->Connect(&ConnHandle, 0);
    PutServer(&s);
    if (code != 0)
        return code;

    rrcc.HowMany++;
    rrcc.handles[i]  = ConnHandle;
    rrcc.hosts[i]    = host;
    rrcc.retcodes[i] = 0;
    rrcc.dying[i]    = 0;

    SLog(20, "res_mgrpent::CreateMember Added %x at index %d", host, i);
    return (0);
}

void res_mgrpent::KillMember(unsigned long host, int forcibly)
{
    LogMsg(20, SrvDebugLevel, stdout, "res_mgrpent::KillMember(%x, %d)", host,
           forcibly);
    int i;

    if (host == 0)
        return;

    for (i = 0; i < VSG_MEMBERS; i++)
        if (rrcc.hosts[i] == host) {
            if (inuse && !forcibly) {
                rrcc.dying[i] = 1;
                break;
            }
            // might add primary host stuff here
            // code = RPC2_RemoveFromMgrp(McastInfo.Mgroup,
            //rrcc.handles[i]);
            RPC2_Unbind(rrcc.handles[i]);
            rrcc.HowMany--;
            rrcc.handles[i] = 0;
            rrcc.hosts[i]   = 0;
            rrcc.dying[i]   = 0;
        }
    if (i < VSG_MEMBERS)
        LogMsg(20, SrvDebugLevel, stdout,
               "KillMember - removed host at index %d", i);
}

/* Assumes supplied host set has hosts in canonical order */
int res_mgrpent::GetHostSet(unsigned long *HostSet)
{
    int i;
    if (HostSet == 0)
        HostSet = Hosts;

    /* Create Members already not members */
    for (i = 0; i < VSG_MEMBERS; i++)
        if (HostSet[i] != 0 && rrcc.hosts[i] == 0)
            CreateMember(HostSet[i]);
    /* Kill Members not in the host set */
    for (i = 0; i < VSG_MEMBERS; i++)
        if (HostSet[i] == 0 && rrcc.hosts[i] != 0)
            KillMember(rrcc.hosts[i], 1);

    /* Make sure at least 1 server is up */
    if (rrcc.HowMany == 0)
        return (ETIMEDOUT);

    return (0);
}

void res_mgrpent::PutHostSet()
{
    LogMsg(20, SrvDebugLevel, stdout, "res_mgrpent::PutHostSet()");
    /* Kill dying members. */
    for (int i = 0; i < VSG_MEMBERS; i++)
        if (rrcc.dying[i]) {
            LogMsg(20, SrvDebugLevel, stdout, "PutHostSet: Killing Host %x",
                   rrcc.hosts[i]);
            KillMember(rrcc.hosts[i], 1);
        }
}

/* check results from call - if rpc2 related error codes reset state maybe*/
int res_mgrpent::CheckResult()
{
    int code = 0;
    SLog(20, "res_mgrpent::CheckResult()");
    for (int i = 0; i < VSG_MEMBERS; i++) {
        if (rrcc.hosts[i] == 0)
            continue;

        if (rrcc.retcodes[i] < 0) {
            code      = ETIMEDOUT;
            srvent *s = 0;
            GetServer(&s, rrcc.hosts[i]);
            s->ServerError((int *)&rrcc.retcodes[i]);
            PutServer(&s);

            if (rrcc.retcodes[i] == ETIMEDOUT)
                KillMember(rrcc.hosts[i], 1);
        }
    }
    return (code);
}

int res_mgrpent::IncompleteVSG()
{
    int countVSGhosts  = 0;
    int countmgrphosts = 0;
    for (int i = 0; i < VSG_MEMBERS; i++)
        if (Hosts[i])
            countVSGhosts++;
    { /* drop scope for int i below; to avoid identifier clash */
        for (int i = 0; i < VSG_MEMBERS; i++)
            if (rrcc.hosts[i])
                countmgrphosts++;
    } /* drop scope for int i above; to avoid identifier clash */

    if (countmgrphosts != countVSGhosts)
        return (1);
    return (0);
}

void res_mgrpent::print()
{
    print(stdout);
}
void res_mgrpent::print(FILE *fp)
{
    fflush(fp);
    print(fileno(fp));
}
void res_mgrpent::print(int fd)
{
    char buf[80];
    sprintf(buf, "%p : VSGhosts = %lx %lx %lx %lx\n", this, Hosts[0], Hosts[1],
            Hosts[2], Hosts[3]);
    write(fd, buf, strlen(buf));
}

/* iterator implementation */
resmgrp_iterator::resmgrp_iterator(unsigned long hosts[VSG_MEMBERS])
    : dlist_iterator((dlist &)*res_mgrpent::ResMgrpTab)
{
    memcpy(Hosts, hosts, sizeof(unsigned long) * VSG_MEMBERS);
    allhosts = 1;

    for (int i = 0; i < VSG_MEMBERS; i++)
        if (hosts[i])
            allhosts = 0;
}

res_mgrpent *resmgrp_iterator::operator()()
{
    dlink *d;
    while ((d = dlist_iterator::operator()())) {
        res_mgrpent *m = strbase(res_mgrpent, d, tblhandle);
        if (allhosts ||
            memcmp(Hosts, m->Hosts, sizeof(unsigned long) * VSG_MEMBERS) == 0)
            return (m);
    }
    return 0;
}

/* implementation of server ents */
#define Srvr_Wait() ResProcWait((char *)&srvent::srvtab)
#define Srvr_Signal() ResProcSignal((char *)&srvent::srvtab, 0)

srvent *FindServer(unsigned long host)
{
    srv_iterator next;
    srvent *s;

    LogMsg(20, SrvDebugLevel, stdout, "FindServer(%x)", host);
    while ((s = next()))
        if (s->host == host)
            return (s);
    LogMsg(20, SrvDebugLevel, stdout, "FindServer didnt find host %x", host);
    return (0);
}

void GetServer(srvent **spp, unsigned long host)
{
    LogMsg(20, SrvDebugLevel, stdout, "GetServer(%x)", host);
    srvent *s = FindServer(host);
    if (s) {
        *spp = s;
        return;
    }

    s = new srvent(host);
    srvent::srvtab->insert(&s->tblhandle);
    srvent::servers++;

    *spp = s;
}

void PutServer(srvent **spp)
{
    LogMsg(20, SrvDebugLevel, stdout, "PutServer: ");
    *spp = NULL;
}

void ServerPrint()
{
    ServerPrint(stdout);
}

void ServerPrint(FILE *fp)
{
    fflush(fp);
    ServerPrint(fileno(fp));
}

void ServerPrint(int fd)
{
    char buffer[80];
    sprintf(buffer, "Servers: count = %d\n", srvent::servers);
    write(fd, buffer, strlen(buffer));

    srv_iterator next;
    srvent *s;
    while ((s = next()))
        s->print(fd);

    sprintf(buffer, "\n");
    write(fd, buffer, strlen(buffer));
}

srvent::srvent(unsigned long Host)
{
    unsigned long nHost = htonl(Host);
    LogMsg(100, SrvDebugLevel, stdout, "srvent::srvent (%x)", nHost);
    struct hostent *h =
        gethostbyaddr((char *)&nHost, sizeof(unsigned long), AF_INET);
    if (h) {
        name = new char[strlen(h->h_name) + 1];
        strcpy(name, h->h_name);
        TRANSLATE_TO_LOWER(name);
    } else {
        char buf[12];
        sprintf(buf, "%lu", Host);
        name = new char[strlen(buf) + 1];
        strcpy(name, buf);
    }

    host      = Host;
    binding   = 0;
    srvrstate = unknown;
}

srvent::~srvent()
{
    delete[] name;
}

int srvent::Connect(RPC2_Handle *cidp, int Force)
{
    LogMsg(100, SrvDebugLevel, stdout, "srvent::Connect %x", host);
    int code = 0;

    for (;;) {
        if (ServerIsDown() && !Force) {
            LogMsg(20, SrvDebugLevel, stdout, "Connect: server %x is down",
                   host);
            /* asynchronously trigger a server probe */
            LWP_SignalProcess((char *)ResCheckServerLWP);
            return (ETIMEDOUT);
        }
        if (!binding)
            break;
        Srvr_Wait();
    }
    binding = 1;
    {
        /* do the bind */
        RPC2_HostIdent hid;
        RPC2_PortIdent pid;
        RPC2_SubsysIdent ssid;
        RPC2_BindParms bp;
        struct servent *s = coda_getservbyname("codasrv", "udp");

        hid.Tag                      = RPC2_HOSTBYINETADDR;
        hid.Value.InetAddress.s_addr = htonl(host);

        pid.Tag                  = RPC2_PORTBYINETNUMBER;
        pid.Value.InetPortNumber = s->s_port;

        ssid.Tag            = RPC2_SUBSYSBYID;
        ssid.Value.SubsysId = RESOLUTIONSUBSYSID;

        bp.SecurityLevel  = RPC2_OPENKIMONO;
        bp.EncryptionType = 0;
        bp.SideEffectType = SMARTFTP;
        bp.ClientIdent    = NULL;
        bp.SharedSecret   = NULL;
        code              = RPC2_NewBinding(&hid, &pid, &ssid, &bp, cidp);
        if (code != RPC2_SUCCESS) {
            LogMsg(100, SrvDebugLevel, stdout, "Connect: Bind didn't succeed");
            RPC2_Unbind(*cidp);
            *cidp = 0;
            Reset();
        } else if (srvrstate == down)
            srvrstate = up;
    }
    binding = 0;
    Srvr_Signal();

    return (code);
}
void srvent::Reset()
{
    LogMsg(100, SrvDebugLevel, stdout, "srvent::Reset(%x)", host);
    srvrstate = down;
    /* Kill all indirect connections to this server. */
    resmgrp_iterator mgrp_next;
    res_mgrpent *m;
    while ((m = mgrp_next()))
        m->KillMember(host, 0);

    /* Kill all conninfos with this server */
    conninfo_iterator conninfo_next;
    conninfo *cip, *n = NULL;
    while ((cip = n ? n : conninfo_next())) {
        n = NULL;
        if (cip->GetRemoteHost() == host) {
            conninfo::CInfoTab->remove(&cip->tblhandle);
            conninfo::ncinfos--;
            n = conninfo_next();
            delete cip;
        }
    }
}

void srvent::ServerError(int *codep)
{
    LogMsg(100, SrvDebugLevel, stdout, "srvent::ServerError(%d)", *codep);
    int retry = 0;
    switch (*codep) {
    case RPC2_FAIL:
    case RPC2_NOCONNECTION:
    case RPC2_TIMEOUT:
    case RPC2_DEAD:
    case RPC2_SEFAIL2:
        *codep = ETIMEDOUT;
        break;

    case RPC2_SEFAIL3:
    case RPC2_SEFAIL4:
        *codep = EIO;
        break;

    case RPC2_NAKED:
        *codep = ETIMEDOUT;
        retry  = 1;
        break;

    default:
        /* Map RPC2 warnings into EINVAL. */
        if (*codep > RPC2_ELIMIT) {
            *codep = EINVAL;
            break;
        }
    }

    /* Already considered down. */
    if (srvrstate == down)
        return;

    /* Reset if TIMED'out or NAK'ed. */
    if (*codep == ETIMEDOUT) {
        Reset();
        if (retry)
            srvrstate = unknown;
    }
}
int srvent::ServerIsDown()
{
    return (srvrstate == down);
}

int srvent::ServerIsUp()
{
    return (srvrstate == up);
}

void srvent::print()
{
    print(stdout);
}

void srvent::print(FILE *fp)
{
    fflush(fp);
    print(fileno(fp));
}

void srvent::print(int fd)
{
    char buf[80];
    sprintf(buf, "%p : %-16s : host = %#08lx, binding = %d\n", this, name, host,
            binding);
    write(fd, buf, strlen(buf));
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

/* Procedures to interface with res_mgrp class */
int GetResMgroup(res_mgrpent **mpp, unsigned long hosts[VSG_MEMBERS])
{
    LogMsg(20, SrvDebugLevel, stdout, "GetResMgroup()");
    *mpp           = 0;
    int code       = 0;
    res_mgrpent *m = 0;

    /* Grab an existing free mgroup if possible */
    resmgrp_iterator next(hosts);
    while ((m = next()))
        if (!m->inuse) {
            LogMsg(20, SrvDebugLevel, stdout,
                   "GetResMgroup: Found existing mgroup");
            m->inuse = 1;
            break;
        }
    if (!m) {
        /* didnt find an existing mgroup - create one */
        LogMsg(20, SrvDebugLevel, stdout, "GetResMgroup: Creating new mgroup");
        RPC2_Handle MgrpHandle = 0;
        m                      = new res_mgrpent(hosts, MgrpHandle);
        m->inuse               = 1;
        res_mgrpent::ResMgrpTab->insert(&m->tblhandle);
        res_mgrpent::resmgrps++;
    }

    /* Validate all the connections */
    LogMsg(20, SrvDebugLevel, stdout, "GetResMgroup: Validating connections");
    code = m->GetHostSet(NULL);
    if (m->dying || code != 0) {
        PutResMgroup(&m);
        return (code);
    }
    /* Multicast or not ??? Puneet */
    *mpp = m;
    return (0);
}

int PutResMgroup(res_mgrpent **mpp)
{
    res_mgrpent *m = *mpp;
    *mpp           = 0;
    LogMsg(20, SrvDebugLevel, stdout, "PutResMgroup()");
    if (m == 0)
        return (0);
    if (!m->inuse) {
        LogMsg(0, SrvDebugLevel, stdout, "Putting a Mgroup not in use ");
        m->print();
    }

    LogMsg(20, SrvDebugLevel, stdout, "PutResMgroup: Putting Host Set");
    /* clean up host set */
    m->PutHostSet();

    if (m->dying) {
        LogMsg(20, SrvDebugLevel, stdout,
               "PutResMgroup: Mgroup is dying...deleting");
        res_mgrpent::ResMgrpTab->remove(&m->tblhandle);
        delete m;
        res_mgrpent::resmgrps--;
    } else
        m->inuse = 0;
    return (0);
}
void ResMgrpPrint()
{
    ResMgrpPrint(stdout);
}

void ResMgrpPrint(FILE *fp)
{
    fflush(fp);
    ResMgrpPrint(fileno(fp));
}

void ResMgrpPrint(int fd)
{
    char buffer[80];
    sprintf(buffer, "Mgroups: count = %d\n", res_mgrpent::resmgrps);
    write(fd, buffer, strlen(buffer));

    /* Iterate through the individual entries. */
    resmgrp_iterator next;
    res_mgrpent *m;
    while ((m = next()))
        m->print(fd);
    sprintf(buffer, "\n");
    write(fd, buffer, strlen(buffer));
}

/* implementation of connection infos */
conninfo::conninfo(RPC2_Handle rpcid, int sl)
{
    RPC2_PeerInfo peer;
    LogMsg(100, SrvDebugLevel, stdout, "conninfo::conninfo for %x", rpcid);
    SecLevel = sl;
    cid      = rpcid;

    CODA_ASSERT(RPC2_GetPeerInfo(rpcid, &peer) == RPC2_SUCCESS);
    CODA_ASSERT(peer.RemoteHost.Tag == RPC2_HOSTBYINETADDR);
    CODA_ASSERT(peer.RemotePort.Tag == RPC2_PORTBYINETNUMBER);
    RemoteAddr    = peer.RemoteHost.Value.InetAddress.s_addr;
    RemotePortNum = peer.RemotePort.Value.InetPortNumber;
    LogMsg(100, SrvDebugLevel, stdout, "conninfo: remote host = %s.%x",
           inet_ntoa(peer.RemoteHost.Value.InetAddress), RemotePortNum);
}

conninfo::~conninfo() {}

unsigned long conninfo::GetRemoteHost()
{
    return (RemoteAddr);
}

int conninfo::GetSecLevel()
{
    return (SecLevel);
}

unsigned short conninfo::GetRemotePort()
{
    return (RemotePortNum);
}

conninfo_iterator::conninfo_iterator(RPC2_Handle cid)
    : olist_iterator((olist &)*conninfo::CInfoTab)
{
    key = cid;
}
conninfo *conninfo_iterator::operator()()
{
    olink *o;
    while ((o = olist_iterator::operator()())) {
        conninfo *cip = strbase(conninfo, o, tblhandle);
        if (cip->cid == key || key == 0)
            return (cip);
    }
    return (0);
}
conninfo *GetConnectionInfo(RPC2_Handle cid)
{
    conninfo_iterator next(cid);
    return (next());
}

void ResCheckServerLWP(void *arg)
{
    struct timeval delay;
    LogMsg(1, SrvDebugLevel, stdout, "Starting RecCheckServerLWP");
    delay.tv_sec  = 120;
    delay.tv_usec = 0;

    while (1) {
        LWP_SignalProcess((char *)ResCheckServerLWP);
        IOMGR_Select(0, 0, 0, 0, &delay);
    }
}

/* LWP to check if servers are alive */
void ResCheckServerLWP_worker(void *arg)
{
    LogMsg(1, SrvDebugLevel, stdout, "Starting RecCheckServerLWP_worker");

    srvent *s;
    while (1) {
        srv_iterator *next;
        next = new srv_iterator;
        while ((s = (*next)()))
            if (s->ServerIsDown()) {
                LogMsg(19, SrvDebugLevel, stdout,
                       "ResCheckServerLWP_worker - checking server %s\n",
                       s->name);
                /* forcibly try and get a connection - which resets state */
                RPC2_Handle ConnHandle = 0;
                int code               = s->Connect(&ConnHandle, 1);
                if (code == RPC2_SUCCESS)
                    RPC2_Unbind(ConnHandle);
            }
        delete next;
        next = 0;
        LogMsg(80, SrvDebugLevel, stdout,
               "ResCheckServerLWP_worker sleeping \n");
        LWP_WaitProcess((char *)ResCheckServerLWP);
        LogMsg(80, SrvDebugLevel, stdout,
               "ResCheckServerLWP_worker woken up\n");
    }
}

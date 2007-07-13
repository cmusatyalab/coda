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

/*
 * Smon is a modified cmon to print output in a format that can be piped into
 * rrdtool. rrdtool is a flexible and nifty program from the author of MRTG
 * that logs & graphs arbitrary dataseries. See
 *	http://www.caida.org/Tools/RRDtool/.
 *
 * -- Jan
 *
 * The description from cmon follows:
 *
 * Simple program to monitor Coda servers
 *   M. Satyanarayanan, June 1990
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include "coda_string.h"
#include <errno.h>
#include <signal.h>

#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/sftp.h>
#include <coda_getservbyname.h>
#include <coda_getaddrinfo.h>

#include "vice.h"

struct server
{
    PROCESS pid; /* process id of lwp for this server */
    char *srvname;
    RPC2_Handle cid;
    int  old;
    long binds;  /* since starting of monitor */
    long probe;  /* time when server was last probed */
    long succ;   /* time of last successful probe */
    struct ViceStatistics vs;  /* result of call */
};

#define MAXSRV 16
struct server srv[MAXSRV];
int SrvCount = 0; /* how many servers */

/* how often to probe servers, in seconds */
int probeinterval = 300;

/* single shot, or continuous operation */
int loop = 1;

/* needed to signal the main thread when we're done */
PROCESS parent;

static void RRDCreate(struct server *s)
{
    printf("create %s.rrd ", s->srvname);
    
    puts("DS:conns:GAUGE:600:0:U "
	 "DS:nrpcs:COUNTER:600:0:100000 "
	 "DS:ftotl:COUNTER:600:0:100000 "
	 "DS:fdata:COUNTER:600:0:100000 "
	 "DS:fsize:COUNTER:600:U:5000000 "
	 "DS:frate:GAUGE:600:0:U:5000000 "
	 "DS:stotl:COUNTER:600:0:100000 "
	 "DS:sdata:COUNTER:600:U:100000 "
	 "DS:ssize:COUNTER:600:U:5000000 "
	 "DS:srate:GAUGE:600:0:U:5000000 "
	 "DS:rbsnd:COUNTER:600:U:5000000 "
	 "DS:rbrcv:COUNTER:600:U:5000000 "
	 "DS:rpsnd:COUNTER:600:0:100000 "
	 "DS:rprcv:COUNTER:600:0:100000 "
	 "DS:rplst:COUNTER:600:0:100000 "
	 "DS:rperr:COUNTER:600:0:100000 "
	 "DS:sscpu:COUNTER:600:0:2000 "
	 "DS:sucpu:COUNTER:600:0:2000 "
	 "DS:sncpu:COUNTER:600:0:2000 "
	 "DS:sicpu:COUNTER:600:0:2000 "
	 "DS:totio:COUNTER:600:U:100000 "
	 "DS:vmuse:GAUGE:600:0:U "
	 "DS:vmmax:GAUGE:600:0:U "
	 "DS:eerrs:COUNTER:600:0:1000000 "
	 "DS:epsnd:COUNTER:600:0:1000000 "
	 "DS:ecols:COUNTER:600:0:1000000 "
	 "DS:eprcv:COUNTER:600:0:1000000 "
	 "DS:ebsnd:COUNTER:600:0:13000000 "
	 "DS:ebrcv:COUNTER:600:0:13000000 "
	 "DS:vmsiz:GAUGE:600:0:U "
	 "DS:clnts:GAUGE:600:0:10000 "
	 "DS:aclnt:GAUGE:600:0:10000 "
	 "DS:mnflt:COUNTER:600:U:1000000 "
	 "DS:mjflt:COUNTER:600:U:1000000 "
	 "DS:nswap:COUNTER:600:U:1000000 "
	 "DS:utime:COUNTER:600:U:100 "
	 "DS:stime:COUNTER:600:U:100 "
	 "DS:vmrss:GAUGE:600:0:U "
	 "DS:vmdat:GAUGE:600:0:U "
	 "DS:d1avl:GAUGE:600:0:U "
	 "DS:d1tot:GAUGE:600:0:U "
	 "DS:d2avl:GAUGE:600:0:U "
	 "DS:d2tot:GAUGE:600:0:U "
	 "DS:d3avl:GAUGE:600:0:U "
	 "DS:d3tot:GAUGE:600:0:U "
	 "DS:d4avl:GAUGE:600:0:U "
	 "DS:d4tot:GAUGE:600:0:U "

    /* store averages & maximum values */
    /* 600 5-minute samples   (+/- 2 days)  */
    /* 600 30-minute averages (+/- 2 weeks)  */
    /* 600 2-hour averages    (+/- 2 months) */
    /* 732 1-day averages     (+/- 2 years)  */

	 "RRA:AVERAGE:0.5:1:600 "
	 "RRA:AVERAGE:0.5:6:600 "
	 "RRA:AVERAGE:0.5:24:600 "
	 "RRA:AVERAGE:0.5:288:732 "
	 "RRA:MAX:0.5:1:600 "
	 "RRA:MAX:0.5:6:600 "
	 "RRA:MAX:0.5:24:600 "
	 "RRA:MAX:0.5:288:732 ");

    fflush(stdout);
}

static void RRDUpdate(struct server *s)
{
    printf("update %s.rrd %u:%u:", s->srvname,
           s->vs.CurrentTime, s->vs.CurrentConnections);

    printf("%u:%u:%u:%u:%u:%u:%u:%u:%u:",
           s->vs.TotalViceCalls, s->vs.TotalFetches, s->vs.FetchDatas,
           s->vs.FetchedBytes, s->vs.FetchDataRate, s->vs.TotalStores,
           s->vs.StoreDatas, s->vs.StoredBytes, s->vs.StoreDataRate);

    printf("%u:%u:%u:%u:%u:%u:",
           s->vs.TotalRPCBytesSent, s->vs.TotalRPCBytesReceived,
           s->vs.TotalRPCPacketsSent, s->vs.TotalRPCPacketsReceived,
           s->vs.TotalRPCPacketsLost, s->vs.TotalRPCBogusPackets);

    printf("%u:%u:%u:%u:%u:%u:%u:",
           s->vs.SystemCPU, s->vs.UserCPU, s->vs.NiceCPU, s->vs.IdleCPU,
           s->vs.TotalIO, s->vs.ActiveVM, s->vs.TotalVM);

    printf("%u:%u:%u:%u:%u:%u:",
           s->vs.EtherNetTotalErrors, s->vs.EtherNetTotalWrites,
           s->vs.EtherNetTotalInterupts, s->vs.EtherNetGoodReads,
           s->vs.EtherNetTotalBytesWritten, s->vs.EtherNetTotalBytesRead);

    printf("%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:",
           s->vs.ProcessSize, s->vs.WorkStations, s->vs.ActiveWorkStations,
           s->vs.MinFlt, s->vs.MajFlt, s->vs.NSwaps, s->vs.UsrTime,
           s->vs.SysTime, s->vs.VmRSS, s->vs.VmData);

    printf("%u:%u:%u:%u:%u:%u:%u:%u",
           s->vs.Disk1.Name ? s->vs.Disk1.BlocksAvailable : 0,
           s->vs.Disk1.Name ? s->vs.Disk1.TotalBlocks : 0,
           s->vs.Disk2.Name ? s->vs.Disk2.BlocksAvailable : 0,
           s->vs.Disk2.Name ? s->vs.Disk2.TotalBlocks : 0,
           s->vs.Disk3.Name ? s->vs.Disk3.BlocksAvailable : 0,
           s->vs.Disk3.Name ? s->vs.Disk3.TotalBlocks : 0,
           s->vs.Disk4.Name ? s->vs.Disk4.BlocksAvailable : 0,
           s->vs.Disk4.Name ? s->vs.Disk4.TotalBlocks : 0);

    printf("\n");
    fflush(stdout);
}

static int ValidServer(char *s)
{
    struct RPC2_addrinfo hints, *res = NULL;
    int ret;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    ret = coda_getaddrinfo(s, "codasrv", &hints, &res);
    RPC2_freeaddrinfo(res);

    return (ret == 0);
}

static void GetArgs(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind;
    int c, next, usage = 0;

    while ((c = getopt(argc, argv, "t:1")) != EOF) {
	switch(c) {
	case 't':
	    probeinterval = atoi(optarg);
	    break;

	case '1':
	    loop = 0;
	    break;

	default:
	    usage = 1;
	    break;
	}
    }

    if (usage) {
	fprintf(stderr, "Usage: %s [-1] [-t probeinterval] server [server]* | rrdtool -\n", argv[0]);
	exit(0);
    }

    memset(srv, 0, sizeof(struct server) * MAXSRV);

    for (next = optind; next < argc; next++) {
	if (SrvCount >= MAXSRV) {
	    fprintf(stderr, "Too many servers: should be %d or less\n",
		    MAXSRV);
	    exit(-1);
	}

	srv[SrvCount].srvname = argv[next];
	if (!ValidServer(srv[SrvCount].srvname)) {
	    fprintf(stderr, "%s is not a valid server\n",
		    srv[SrvCount].srvname);
	    exit(-1);
	}

	SrvCount++;
    }

    if (!SrvCount) {
	fprintf(stderr, "no servers specified\n");
	exit(-1);
    }
}

static void InitRPC(void)
{
    RPC2_Options options;
    int rc;
    SFTP_Initializer sei;
    struct timeval tv;

    /* Init RPC2 */
    rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &parent);
    if (rc != LWP_SUCCESS) {
	fprintf(stderr, "LWP_Init() failed\n");
	exit(-1);
    }

    SFTP_SetDefaults(&sei);
    SFTP_Activate(&sei);
    tv.tv_sec = 15;
    tv.tv_usec = 0;

    memset(&options, 0, sizeof(options));
    options.Flags = RPC2_OPTION_IPV6;

    RPC2_Init(RPC2_VERSION, &options, NULL, -1, &tv);
}


static void DoProbe(struct server *srv)
{
    int rc;

    srv->probe = time(NULL);
    if (!srv->cid)
    {
	RPC2_HostIdent hi;
	RPC2_PortIdent pi;
	RPC2_SubsysIdent si;
	RPC2_BindParms bparms;
	struct servent *s = coda_getservbyname("codasrv", "udp");

	hi.Tag = RPC2_HOSTBYNAME;
	strcpy(hi.Value.Name, srv->srvname);

	pi.Tag = RPC2_PORTBYINETNUMBER;
	pi.Value.InetPortNumber = s->s_port;

	si.Tag = RPC2_SUBSYSBYID;
	si.Value.SubsysId= SUBSYS_SRV;

	bparms.SideEffectType = 0;
	bparms.SecurityLevel = RPC2_OPENKIMONO;
	bparms.ClientIdent = NULL;

	rc = (int) RPC2_NewBinding(&hi, &pi, &si, &bparms, &srv->cid);
	if (rc == RPC2_SUCCESS)
	    srv->binds++;
	else
	    srv->cid = 0;
    }

    if (!srv->cid)
	return;

    /* Keeping backward compatibility is a crime to humanity, but kind
     * of useful when you want to log data from both the 5.2.x and the
     * 5.3.x servers, at least the structure is still the same... */
retry:
    if (srv->old) rc = (int) ViceGetOldStatistics(srv->cid, &srv->vs);
    else          rc = (int) ViceGetStatistics(srv->cid, &srv->vs);

    if (rc != RPC2_SUCCESS) {
	if (rc == RPC2_INVALIDOPCODE && !srv->old) {
	    srv->old = 1;
	    goto retry; /* retry the sample */
	}
	RPC2_Unbind(srv->cid);
	srv->cid = 0;
    } else
	srv->succ = srv->probe; /* ignoring RPC call delays */
}

static void DoSleep(struct server *srv)
{
    struct timeval NextProbe = { 60, 0 };

    /* Compensate the waiting time for the time it took to do the RPC */
    if (srv->succ == srv->probe)
	NextProbe.tv_sec = (srv->probe + probeinterval) - time(NULL);

    if (NextProbe.tv_sec > 0)
	IOMGR_Select(0, NULL, NULL, NULL, &NextProbe);  /* sleep */
}

static void srvlwp(void *arg)
{
    int slot = *(int *)arg;
    srv[slot].cid = 0;
    srv[slot].old = 0;

    while (1)
    {
	DoProbe(&srv[slot]);

	if (srv[slot].cid)
	    RRDUpdate(&srv[slot]);

	if (!loop) break;

	DoSleep(&srv[slot]);
    }
    SrvCount--;
    LWP_QSignal(parent);
}

int main(int argc, char *argv[])
{
    char buf[MAXPATHLEN];
    int i;

    GetArgs(argc, argv);

    /* create missing rrd databases */
    for (i = 0; i < SrvCount; i++) {
	sprintf(buf, "%s.rrd", srv[i].srvname);
	if (access(buf, F_OK) == 0) continue;

	RRDCreate(&srv[i]);
    }

    InitRPC();

    /* start monitoring */
    for (i = 0; i < SrvCount; i++) {
        LWP_CreateProcess(srvlwp, 0x8000, LWP_NORMAL_PRIORITY,
                          (void *)&i, srv[i].srvname, &srv[i].pid);
    }

    /* QSignal/QWait doesn't actually queue anything so we still have to rely
     * on a counter to catch possibly missed events */
    while (SrvCount)
	LWP_QWait();

    exit(0);
}


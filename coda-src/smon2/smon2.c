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
 * Smon is a modified cmon to print output in a format that can be piped into
 * rrdtool. rrdtool is a flexible and nifty program from the author of MRTG
 * that logs & graphs arbitrary dataseries. See
 *	http://www.caida.org/Tools/RRDtool/.
 *
 * Simple program to monitor Coda servers
 *   M. Satyanarayanan, June 1990
 */

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <strings.h>
#include <errno.h>

#include <ports.h>
#include <lwp.h>
#include <rpc2.h>
#include <se.h>
#include <timer.h>
#include <sftp.h>
#include <signal.h>

#include "vice.h"

int iopen(int x, int y, int z){return(0);};  /* BLETCH!! */

typedef enum {DEAD, NEWBORN, ALIVE} SrvState;

struct server
{
    int pid; /* process id of lwp for this server */
    char *srvname;
    SrvState state;
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
FILE *rrdtool;

char Dummy; /* dummy variable for LWP_WaitProcess() */

static void RRDCreate(struct server *s)
{
    fprintf(rrdtool, "create %s.rrd ", s->srvname);
    
    fprintf(rrdtool, "DS:conns:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:nrpcs:COUNTER:600:0:100000 ");
    fprintf(rrdtool, "DS:ftotl:COUNTER:600:0:100000 ");
    fprintf(rrdtool, "DS:fdata:COUNTER:600:0:100000 ");
    fprintf(rrdtool, "DS:fsize:COUNTER:600:U:5000000 ");
    fprintf(rrdtool, "DS:frate:GAUGE:600:0:U:5000000 ");
    fprintf(rrdtool, "DS:stotl:COUNTER:600:0:100000 ");
    fprintf(rrdtool, "DS:sdata:COUNTER:600:U:100000 ");
    fprintf(rrdtool, "DS:ssize:COUNTER:600:U:5000000 ");
    fprintf(rrdtool, "DS:srate:GAUGE:600:0:U:5000000 ");
    fprintf(rrdtool, "DS:rbsnd:COUNTER:600:U:5000000 ");
    fprintf(rrdtool, "DS:rbrcv:COUNTER:600:U:5000000 ");
    fprintf(rrdtool, "DS:rpsnd:COUNTER:600:0:100000 ");
    fprintf(rrdtool, "DS:rprcv:COUNTER:600:0:100000 ");
    fprintf(rrdtool, "DS:rplst:COUNTER:600:0:100000 ");
    fprintf(rrdtool, "DS:rperr:COUNTER:600:0:100000 ");
    fprintf(rrdtool, "DS:sscpu:COUNTER:600:0:2000 ");
    fprintf(rrdtool, "DS:sucpu:COUNTER:600:0:2000 ");
    fprintf(rrdtool, "DS:sncpu:COUNTER:600:0:2000 ");
    fprintf(rrdtool, "DS:sicpu:COUNTER:600:0:2000 ");
    fprintf(rrdtool, "DS:totio:COUNTER:600:U:100000 ");
    fprintf(rrdtool, "DS:vmuse:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:vmmax:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:eerrs:COUNTER:600:0:1000000 ");
    fprintf(rrdtool, "DS:epsnd:COUNTER:600:0:1000000 ");
    fprintf(rrdtool, "DS:ecols:COUNTER:600:0:1000000 ");
    fprintf(rrdtool, "DS:eprcv:COUNTER:600:0:1000000 ");
    fprintf(rrdtool, "DS:ebsnd:COUNTER:600:0:13000000 ");
    fprintf(rrdtool, "DS:ebrcv:COUNTER:600:0:13000000 ");
    fprintf(rrdtool, "DS:vmsiz:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:clnts:GAUGE:600:0:10000 ");
    fprintf(rrdtool, "DS:aclnt:GAUGE:600:0:10000 ");
    fprintf(rrdtool, "DS:mnflt:COUNTER:600:U:1000000 ");
    fprintf(rrdtool, "DS:mjflt:COUNTER:600:U:1000000 ");
    fprintf(rrdtool, "DS:nswap:COUNTER:600:U:1000000 ");
    fprintf(rrdtool, "DS:utime:COUNTER:600:U:100 ");
    fprintf(rrdtool, "DS:stime:COUNTER:600:U:100 ");
    fprintf(rrdtool, "DS:vmrss:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:vmdat:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:d1avl:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:d1tot:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:d2avl:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:d2tot:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:d3avl:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:d3tot:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:d4avl:GAUGE:600:0:U ");
    fprintf(rrdtool, "DS:d4tot:GAUGE:600:0:U ");

    /* store averages & maximum values */
    /* 600 5-minute samples   (+/- 2 days)  */
    /* 600 30-minute averages (+/- 2 weeks)  */
    /* 600 2-hour averages    (+/- 2 months) */
    /* 732 1-day averages     (+/- 2 years)  */

    fprintf(rrdtool, "RRA:AVERAGE:0.5:1:600 ");
    fprintf(rrdtool, "RRA:AVERAGE:0.5:6:600 ");
    fprintf(rrdtool, "RRA:AVERAGE:0.5:24:600 ");
    fprintf(rrdtool, "RRA:AVERAGE:0.5:288:732 ");
    fprintf(rrdtool, "RRA:MAX:0.5:1:600 ");
    fprintf(rrdtool, "RRA:MAX:0.5:6:600 ");
    fprintf(rrdtool, "RRA:MAX:0.5:24:600 ");
    fprintf(rrdtool, "RRA:MAX:0.5:288:732 ");

    fprintf(rrdtool, "\n");
    fflush(rrdtool);
}

static void RRDUpdate(struct server *s)
{
    fprintf(rrdtool, "update %s.rrd %lu:%lu:", s->srvname,
            s->vs.CurrentTime, s->vs.CurrentConnections);

    fprintf(rrdtool, "%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:",
	    s->vs.TotalViceCalls, s->vs.TotalFetches, s->vs.FetchDatas,
	    s->vs.FetchedBytes, s->vs.FetchDataRate, s->vs.TotalStores,
	    s->vs.StoreDatas, s->vs.StoredBytes, s->vs.StoreDataRate);

    fprintf(rrdtool, "%lu:%lu:%lu:%lu:%lu:%lu:",
            s->vs.TotalRPCBytesSent, s->vs.TotalRPCBytesReceived,
            s->vs.TotalRPCPacketsSent, s->vs.TotalRPCPacketsReceived,
            s->vs.TotalRPCPacketsLost, s->vs.TotalRPCBogusPackets);

    fprintf(rrdtool, "%lu:%lu:%lu:%lu:%lu:%lu:%lu:",
            s->vs.SystemCPU, s->vs.UserCPU, s->vs.NiceCPU, s->vs.IdleCPU,
            s->vs.TotalIO, s->vs.ActiveVM, s->vs.TotalVM);

    fprintf(rrdtool, "%lu:%lu:%lu:%lu:%lu:%lu:",
            s->vs.EtherNetTotalErrors, s->vs.EtherNetTotalWrites,
            s->vs.EtherNetTotalInterupts, s->vs.EtherNetGoodReads,
            s->vs.EtherNetTotalBytesWritten, s->vs.EtherNetTotalBytesRead);

    fprintf(rrdtool, "%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:",
            s->vs.ProcessSize, s->vs.WorkStations, s->vs.ActiveWorkStations,
            s->vs.MinFlt, s->vs.MajFlt, s->vs.NSwaps, s->vs.UsrTime,
            s->vs.SysTime, s->vs.VmRSS, s->vs.VmData);

    fprintf(rrdtool, "%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu",
            s->vs.Disk1.Name ? s->vs.Disk1.BlocksAvailable : 0,
            s->vs.Disk1.Name ? s->vs.Disk1.TotalBlocks : 0,
            s->vs.Disk2.Name ? s->vs.Disk2.BlocksAvailable : 0,
            s->vs.Disk2.Name ? s->vs.Disk2.TotalBlocks : 0,
            s->vs.Disk3.Name ? s->vs.Disk3.BlocksAvailable : 0,
            s->vs.Disk3.Name ? s->vs.Disk3.TotalBlocks : 0,
            s->vs.Disk4.Name ? s->vs.Disk4.BlocksAvailable : 0,
            s->vs.Disk4.Name ? s->vs.Disk4.TotalBlocks : 0);

    fprintf(rrdtool, "\n");
    fflush(rrdtool);
}

static int ValidServer(char *s)
{
    struct hostent *he;
    
    he = gethostbyname(s);
    if (he) return(1);
    else return(0);
}

static void GetArgs(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind;
    int c, next;

    while ((c = getopt(argc, argv, "t:")) != EOF) {
	switch(c) {
	case 't':
	    probeinterval = atoi(optarg);
	    break;

	default:
	    optind = argc;
	    break;
	}
    }

    if (argc == optind) {
	fprintf(stderr, "Usage: %s [-t probeinterval] server [server]*\n",
		argv[0]);
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

    return;
}


static void InitRPC()
{
    int pid;
    int rc;
    SFTP_Initializer sei;
    struct timeval tv;

    /* Init RPC2 */
    rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, (PROCESS *)&pid);
    if (rc != LWP_SUCCESS) {
	fprintf(stderr, "LWP_Init() failed\n");
	exit(-1);
    }

    SFTP_SetDefaults(&sei);
    SFTP_Activate(&sei);
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    RPC2_Init(RPC2_VERSION, 0, 0, -1, &tv);
}


static void srvlwp(int slot)
{
    struct server *moi;
    RPC2_HostIdent hi;
    RPC2_PortIdent pi;
    RPC2_SubsysIdent si;
    RPC2_Handle cid;
    int rc;
    struct timeval NextProbe;

    moi = &srv[slot];

    hi.Tag = RPC2_HOSTBYNAME;
    strcpy(hi.Value.Name, moi->srvname);
    pi.Tag = RPC2_PORTBYINETNUMBER;
    pi.Value.InetPortNumber = htons(PORT_codasrv);
    si.Tag = RPC2_SUBSYSBYID;
    si.Value.SubsysId= SUBSYS_SRV;

    moi->state = DEAD;
    while (1)
    {
        if (moi->state == NEWBORN) moi->state = ALIVE;
        moi->probe = time(0);
        if (moi->state == DEAD)
        {
            RPC2_BindParms bparms;

            bparms.SideEffectType = 0;
            bparms.SecurityLevel = RPC2_OPENKIMONO;
            bparms.ClientIdent = 0;

            rc = (int) RPC2_NewBinding(&hi, &pi, &si, &bparms, &cid);
            if (rc == RPC2_SUCCESS)
            {
                moi->state = NEWBORN;
                moi->binds++;
            }
        }

        if (!(moi->state == DEAD))
        {
	    /* Keeping backward compatibility is a crime to humanity, but kind
	     * of useful when you want to log data from both the 5.2.x and the
	     * 5.3.x servers, at least the structure is still the same... */
	    if (moi->old) rc = (int) ViceGetOldStatistics(cid, &moi->vs);
	    else          rc = (int) ViceGetStatistics(cid, &moi->vs);

            if (rc != RPC2_SUCCESS) {
		if (rc == RPC2_INVALIDOPCODE && !moi->old) {
		    moi->old = 1;
		    continue; /* reattempt the sample */
		}
                moi->state = DEAD;
                moi->succ = 0;
                RPC2_Unbind(cid);
            } else
                moi->succ = moi->probe; /* ignoring RPC call delays */
        }

	if (moi->state != DEAD) {
	    RRDUpdate(moi);
	}

	/* Compensate the waiting time for the time it took to do the RPC */
	NextProbe.tv_sec = moi->succ ?
	    (moi->probe + probeinterval) - time(0) : 45;
        NextProbe.tv_usec = 0;
        if (NextProbe.tv_sec > 0)
            IOMGR_Select(32, 0, 0, 0, &NextProbe);  /* sleep */
    }
}

int main(int argc, char *argv[])
{
    char buf[MAXPATHLEN];
    int i;

    GetArgs(argc, argv);
    
    rrdtool = stdout; /* popen("rrdtool -", "w"); */

    /* create missing rrd databases */
    for (i = 0; i < SrvCount; i++) {
	sprintf(buf, "%s.rrd", srv[i].srvname);
	if (access(buf, F_OK) == 0) continue;

	RRDCreate(&srv[i]);
    }

    InitRPC();

    /* start monitoring */
    for (i = 0; i < SrvCount; i++) {
        LWP_CreateProcess((PFIC)srvlwp, 0x8000, LWP_NORMAL_PRIORITY,
                          (char *)i, (char *)srv[i].srvname,
                          (PROCESS *)&srv[i].pid);
    }

    LWP_WaitProcess(&Dummy); /* wait for Godot */
    
    exit(0);
}


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

/*  Simple program to monitor Coda servers
    M. Satyanarayanan, June 1990
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include "coda_string.h"
#include <errno.h>

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#else
#include <curses.h>
#endif

#include <ports.h>
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/rpc2_addrinfo.h>
#include <rpc2/se.h>
#include <rpc2/sftp.h>
#include <signal.h>

#include "vice.h"

#ifdef __cplusplus
}
#endif

static time_t MonBirthTime; /* when this monitor was born */

typedef enum {DEAD, NEWBORN, ALIVE} SrvState;

struct server
    {
    int pid; /* process id of lwp for this server */
    char *srvname;
    int hz; /*  jiffies per second on this server (64 for RT) */
    WINDOW *win;
    SrvState state;
    long binds;  /* since starting of monitor */
    long probe;  /* time when server was last probed */
    long succ;  /* time of last successful probe */
    struct ViceStatistics oldvs;  /* result of previous call */
    struct ViceStatistics newvs;  /* result of current call */
    };

#define MAXSRV 16
struct server srv[MAXSRV];
int SrvCount = 0; /* how many servers */

struct timeval ProbeInterval = {60, 0};  /* how often to probe servers, in seconds */

int AbsFlag = 0; /* non-zero ==> print absolute values, not relative */
int CpuFlag = 0; /* show relative cpu time usage among types */

#define SRVCOLWIDTH 9  /* width of each server's window */
#define SRVCOLDECML 999999
#define FIRSTSRVCOL 7 /* index of first server's column */
#define SDNLEN (SRVCOLWIDTH - 3)   /* length of short disk names */

/* Digested data for printing */
struct printvals
    {
    unsigned long cpu_sys;
    unsigned long cpu_user;
    unsigned long cpu_util;
    unsigned long cpu_srv;
    unsigned long rpc_conn;
    unsigned long rpc_wkst;
    unsigned long rpc_call;
    unsigned long rpc_pki;
    unsigned long rpc_pko;
    unsigned long rpc_byi;
    unsigned long rpc_byo;
    char diskname[3][SDNLEN+1];
    unsigned long diskutil[3];
    };


WINDOW *curWin;  /* where cursor sits */
#define HOME() wmove(curWin, 0, 0); wclear(curWin); wrefresh(curWin);

static void GetArgs(int argc, char *argv[]);
static void InitRPC();
static void DrawCaptions();
static void PrintServer(struct server *);
static void srvlwp(void *);
static void kbdlwp(void *);
static int CmpDisk(ViceDisk **, ViceDisk **);
static int ValidServer(char *);
static void ComputePV(struct server *s, struct printvals *pv);
static char *ShortDiskName(char *s);

extern FILE *rpc2_logfile;
FILE *dbg;

char Dummy; /* dummy variable for LWP_WaitProcess() */

#ifdef	DEBUG
void print_stats(struct ViceStatistics *stats);
#endif

void
cleanup_and_go(int ignored)
{
    endwin();
    exit(0);
}

int main(int argc, char *argv[])
{
    int i;

    /* lets try to leave the tty sane */
    signal(SIGINT,  cleanup_and_go);
#if 0
    signal(SIGKILL, cleanup_and_go);
    signal(SIGTERM, cleanup_and_go);
    signal(SIGSEGV, cleanup_and_go);
    signal(SIGBUS,  cleanup_and_go);
#endif
    
#ifdef	DEBUG
    dbg = fopen("/tmp/cmon_dbg", "w");
    if (dbg == NULL) {
	fprintf(stderr, "cmon can not open /tmp/cmon_dbg\n");
	exit(1);
    }
#else
    dbg = fopen("/dev/null", "w");
    if (dbg == NULL) {
	fprintf(stderr, "cmon can not open /dev/null\n");
	exit(1);
    }
#endif

    initscr();
    curWin = newwin(1, 1, 0, 0);
    DrawCaptions();

    MonBirthTime = time(NULL);
    GetArgs(argc, argv);
    InitRPC();
    rpc2_logfile = dbg;

    LWP_CreateProcess(kbdlwp, 0x4000, LWP_NORMAL_PRIORITY, NULL, "KBD", (PROCESS *)&i);
    for (i = 0; i < SrvCount; i++)
	{
	LWP_CreateProcess(srvlwp, 0x8000, LWP_NORMAL_PRIORITY, (char *)i, (char *)srv[i].srvname, (PROCESS *)&srv[i].pid);
	}
    
    LWP_WaitProcess(&Dummy); /* wait for Godot */
    cleanup_and_go(0);
    }

static void srvlwp(void *arg)
{
    int slot = (int)arg;
    struct server *moi;
    RPC2_HostIdent hi;
    RPC2_PortIdent pi;
    RPC2_SubsysIdent si;
    RPC2_Handle cid;
    int rc;
    
    moi = &srv[slot];

    hi.Tag = RPC2_HOSTBYNAME;
    strcpy(hi.Value.Name, moi->srvname);
    pi.Tag = RPC2_PORTBYINETNUMBER;
    pi.Value.InetPortNumber = htons(PORT_codasrv);
    si.Tag = RPC2_SUBSYSBYID;
    si.Value.SubsysId= SUBSYS_SRV;
    
    
    moi->state = DEAD;
    /* Get this out quick so we don't have to wait for a
       RPC2_NOBINDING to tell the user there is NO BINDING */
    PrintServer(moi);
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
	    moi->oldvs = moi->newvs;
	    rc = (int) ViceGetStatistics(cid, &moi->newvs);
	    if (rc != RPC2_SUCCESS) {
		moi->state = DEAD;
		moi->succ = 0;
		RPC2_Unbind(cid);
	    } else {
	        if (!moi->succ && moi->hz == 0) {
		    if (moi->newvs.Spare4 != 0)
		         moi->hz = moi->newvs.Spare4;
		    else
		         moi->hz = 1;
		}
	        moi->succ = moi->probe; /* ignoring RPC call delays */
	    }
	}


	PrintServer(moi);

	IOMGR_Select(32, 0, 0, 0, &ProbeInterval);  /* sleep */
	}
    }

static void kbdlwp(void *arg)
    /* LWP to listen for keyboard activity */
{
    fd_set rset;
    int rc;
    char c;
    
    
    while(1)
	{
	FD_ZERO(&rset);
	FD_SET(fileno(stdin), &rset);
	/* await activity */
	rc = IOMGR_Select(fileno(stdin) + 1, &rset, NULL, NULL, NULL);
	if (rc < 0)
	    {
	    if (errno == EINTR) continue; /* signal occurred */
	    else {perror("select"); exit(-1);}
	    }
	c = getchar();
	
	switch(c)
	    {
	    case 'a':   AbsFlag = 1;
			break;
	    
	    case 'r':   AbsFlag = 0;
			break;
	    
	    default:    break; 
	    }
	HOME();
	}
    }

static void GetArgs(int argc, char *argv[])
    {
    int next;
    char *c;
    
    if (argc < 2) goto BadArgs;

    for (next = 1; next < argc; next++)
	{
	if (argv[next][0] == '-')
	    {
	    if (!(strcmp(argv[next], "-t")))
		{
		next++;
		if (!(next < argc)) goto BadArgs;
		ProbeInterval.tv_sec = atoi(argv[next]);
		continue;
		}

	    if (!(strcmp(argv[next], "-a")))
		{
		AbsFlag = 1;
		continue;
		}

	    if (!(strcmp(argv[next], "-c")))
		{
		CpuFlag = 1;
		continue;
		}

	    goto BadArgs;
	    }
	else
	    {
	    if (!(SrvCount < MAXSRV))
		{
		printf("Too many servers: should be %d or less\n", MAXSRV);
		exit(-1);
		}
	    srv[SrvCount].srvname = argv[next];
	    c = index(argv[next], ':');
	    if (c) {
	        *c = 0; /* so servername is null terminated */
	    }
	    if (!ValidServer(srv[SrvCount].srvname))
		{
		printf("%s is not a valid server\n", srv[SrvCount].srvname);
		exit(-1);
		}
	    if (c) {
	        c++;
	        srv[SrvCount].hz  = atoi(c);
	    } else 
	        srv[SrvCount].hz  = 0; /* tmp */
	    srv[SrvCount].win = newwin(24, SRVCOLWIDTH + 1, 0,
		FIRSTSRVCOL + SrvCount*(SRVCOLWIDTH + 2));
	    SrvCount++;
	    }
	}


    return;

BadArgs:
    printf("Usage: cmon [-a] [-c] [-t probeinterval] server1[:hz1] server2[:hz2] ...\n");
    cleanup_and_go(0);
    }


static void InitRPC()
{
    RPC2_Options options;
    int pid;
    int rc;
    SFTP_Initializer sei;
    struct timeval tv;

    /* Init RPC2 */
    rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, (PROCESS *)&pid);
    if (rc != LWP_SUCCESS) 
    	{printf("LWP_Init() failed\n"); exit(-1);}

    SFTP_SetDefaults(&sei);
    SFTP_Activate(&sei);
    tv.tv_sec = 15;
    tv.tv_usec = 0;

    memset(&options, 0, sizeof(options));
    options.Flags = RPC2_OPTION_IPV6;

    RPC2_Init(RPC2_VERSION, &options, 0, -1, &tv);
}


static void DrawCaptions()
    {
    WINDOW *w1;
    
    w1 = newwin(23, FIRSTSRVCOL - 1, 1, 0);
    
    wprintw(w1, "TIM  \n");
    wprintw(w1, "  mon\n");
    wprintw(w1, " prob\n");
    wprintw(w1, " succ\n");
    wprintw(w1, "   up\n");
    wprintw(w1, " bind\n");
    if (CpuFlag)
    wprintw(w1, "CPU %%\n");
    else
    wprintw(w1, "CPU  \n");
    wprintw(w1, "  sys\n");
    wprintw(w1, " user\n");
    if (CpuFlag) {
    wprintw(w1, " idle\n");
    wprintw(w1, "  srv\n");
    } else
    wprintw(w1, " util\n");
    wprintw(w1, "RPC  \n");
    wprintw(w1, " conn\n");
    wprintw(w1, " wkst\n");
    wprintw(w1, " call\n");
    wprintw(w1, "  pki\n");
    wprintw(w1, "  pko\n");
    wprintw(w1, "  byi\n");
    wprintw(w1, "  byo\n");
    wprintw(w1, "DSK %%\n");
    wprintw(w1, " max1\n");
    wprintw(w1, " max2\n");
    wprintw(w1, " max3\n");
    wmove(w1, 0, 0);
    wrefresh(w1);
    }


char *when(time_t now, time_t then)
    /* then, now: Unix times in seconds */
    {
    long days;
    struct tm *t;
    static char answer[20];
    double fdays;

    days = (now - then)/86400;

    if ((now < then) || (days == 0))
	{
	/* now < then ==> client/server clock skew */
	t = localtime(&then);
	sprintf(answer, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
	}
    else
	{
	fdays = (now - then)/86400.0;
    	sprintf(answer, "%3.1f days", fdays);
	}
    return(answer);
    }


static void PrintServer(struct server *s)
    {
    char shortname[SRVCOLWIDTH+1];
    register WINDOW *w;
    struct printvals pv;
    register int i;
    
    w = s->win;
    
    strncpy(shortname, s->srvname, SRVCOLWIDTH);
    shortname[SRVCOLWIDTH] = 0;

    wclear(w);
    wmove(w, 0, 0);
    wprintw(w, "%*s\n", SRVCOLWIDTH, shortname);
    wprintw(w, "\n");
    wprintw(w, "%*s\n", SRVCOLWIDTH, when(s->probe, MonBirthTime));
    wprintw(w, "%*s\n", SRVCOLWIDTH, when(s->probe, s->probe));
    if (s->succ)
	wprintw(w, "%*s\n", SRVCOLWIDTH, when(s->probe, s->succ));
    else wprintw(w, "%*s\n", SRVCOLWIDTH, "NoBinding");

    if (s->state == DEAD)
	{
#define QROWS(n)\
    for (i = 0; i < n; i++)\
    wprintw(w, "%*s\n", SRVCOLWIDTH, "???");

	QROWS(2);
	wprintw(w, "\n");
	QROWS(3);
	if (CpuFlag)
	QROWS(1);
	wprintw(w, "\n");
	/* QROWS(7) below causes term to go "beep beep..."
	 Can't figure out why this fix works but it does! */
	QROWS(3);
	QROWS(3);
	QROWS(1);
	wprintw(w, "\n");
	QROWS(3);
#undef QROWS
	}
    else
	{

#ifdef	DEBUG
	print_stats(&s->newvs);
#endif
	ComputePV(s, &pv);

#define WPRINT(x) do {\
	if (pv.x == 0xffffffff) wprintw(w, "%*s\n", SRVCOLWIDTH, "***");\
	else if (pv.x > SRVCOLDECML) wprintw(w, "%*luM\n", SRVCOLWIDTH-1, pv.x/(SRVCOLDECML+1));\
	else wprintw(w, "%*lu\n", SRVCOLWIDTH, pv.x);\
} while (0)

	wprintw(w, "%*s\n", SRVCOLWIDTH, when(s->probe, s->newvs.StartTime));
	wprintw(w, "%*d\n", SRVCOLWIDTH, s->binds);
	wprintw(w, "\n");
	WPRINT(cpu_sys);
	WPRINT(cpu_user);
	WPRINT(cpu_util);
	if (CpuFlag)
	WPRINT(cpu_srv);
	wprintw(w, "\n");
	WPRINT(rpc_conn);
	WPRINT(rpc_wkst);
	WPRINT(rpc_call);
	WPRINT(rpc_pki);
	WPRINT(rpc_pko);
	WPRINT(rpc_byi);
	WPRINT(rpc_byo);
	wprintw(w, "\n");
#undef WPRINT

	for (i = 0; i < 3; i++)
	    {
	    char buf[10];
	    if (*pv.diskname[i] == 0)
	    	{wprintw(w, "%*s\n", SRVCOLWIDTH, "***"); continue;}
	    sprintf(buf,  "%s:%2lu", pv.diskname[i], pv.diskutil[i]);
	    wprintw(w, "%*s\n", SRVCOLWIDTH, buf);
	    }
	}
    wrefresh(w);
    HOME();
    }


static void ComputePV(struct server *s, struct printvals *pv)
    {
    long t;
    long total;
    ViceDisk *di[10];
    register int i;

    pv->rpc_conn = s->newvs.CurrentConnections;
    pv->rpc_wkst = s->newvs.WorkStations;
    
    pv->cpu_sys = 0xffffffff;
    pv->cpu_user = 0xffffffff;
    pv->cpu_util = 0xffffffff;
    pv->cpu_srv = 0xffffffff;
    if (AbsFlag) {
	if (CpuFlag) {
	    total = (s->newvs.SystemCPU + s->newvs.UserCPU + s->newvs.IdleCPU);
	    if (total) {
		pv->cpu_sys = s->newvs.SystemCPU * 100 / total;
		pv->cpu_user = (s->newvs.UserCPU + s->newvs.NiceCPU) * 100 / total;
		/* really idle */
		pv->cpu_util = (int)((float)s->newvs.IdleCPU * 100 / total);
		pv->cpu_srv = ((s->newvs.UsrTime+s->newvs.SysTime) * s->hz) /total;
	    }
	} else {
	    pv->cpu_sys = s->newvs.SystemCPU/s->hz;
	    pv->cpu_user = (s->newvs.UserCPU + s->newvs.NiceCPU)/s->hz;
	    t = pv->cpu_sys + pv->cpu_user;
	    total = (t + s->newvs.IdleCPU/s->hz);
	    if (total)
		pv->cpu_util = (int) (0.5 + 100.0*t/total);
	    else
		pv->cpu_util = 0;
	}
	pv->rpc_call = s->newvs.TotalViceCalls;
	pv->rpc_pki = s->newvs.TotalRPCPacketsReceived;
	pv->rpc_pko = s->newvs.TotalRPCPacketsSent;
	pv->rpc_byi = s->newvs.TotalRPCBytesReceived;
	pv->rpc_byo = s->newvs.TotalRPCBytesSent;
    } else {
	if (s->state == NEWBORN)
	    {
	    pv->cpu_sys = 0xffffffff;
	    pv->cpu_user = 0xffffffff;
	    pv->cpu_util = 0xffffffff;
	    pv->rpc_call = 0xffffffff;
	    pv->rpc_pki = 0xffffffff;
	    pv->rpc_pko = 0xffffffff;
	    pv->rpc_byi = 0xffffffff;
	    pv->rpc_byo = 0xffffffff;
	} else {
#define DIFF(x) (s->newvs.x - s->oldvs.x)

	    if (CpuFlag) {
		total = DIFF(SystemCPU) + DIFF(UserCPU) + DIFF(IdleCPU);
		if (total) {
		    pv->cpu_sys = DIFF(SystemCPU) * 100 / total;
		    pv->cpu_user = (DIFF(UserCPU) + DIFF(NiceCPU)) * 100 / total;
		    /* really idle */
		    pv->cpu_util = DIFF(IdleCPU) * 100 / total;
		    pv->cpu_srv = ((DIFF(UsrTime)+DIFF(SysTime)) * s->hz) /total;
		}
	    } else {
		pv->cpu_sys = DIFF(SystemCPU)/s->hz;
		pv->cpu_user = (DIFF(UserCPU) + DIFF(NiceCPU))/s->hz;
		t = pv->cpu_sys + pv->cpu_user;
		total = (t + DIFF(IdleCPU)/s->hz);
		if (total)
		    pv->cpu_util = (int) (0.5 + 100.0*t/total);
		else
		pv->cpu_util = 0;
	    }
	    pv->rpc_call = DIFF(TotalViceCalls);
	    pv->rpc_pki = DIFF(TotalRPCPacketsReceived);
	    pv->rpc_pko = DIFF(TotalRPCPacketsSent);
	    pv->rpc_byi = DIFF(TotalRPCBytesReceived);
	    pv->rpc_byo = DIFF(TotalRPCBytesSent);
#undef DIFF
	    }
	}

    /* Find out 3 most full disks */
    di[0] = &s->newvs.Disk1;
    di[1] = &s->newvs.Disk2;
    di[2] = &s->newvs.Disk3;
    di[3] = &s->newvs.Disk4;
    di[4] = &s->newvs.Disk5;
    di[5] = &s->newvs.Disk6;
    di[6] = &s->newvs.Disk7;
    di[7] = &s->newvs.Disk8;
    di[8] = &s->newvs.Disk9;
    di[9] = &s->newvs.Disk10;
    
    qsort(di, 10, sizeof(ViceDisk *), 
	  (int (*)(const void *, const void *)) CmpDisk);
    for (i = 0; i < 3; i++) 
    	{
	strcpy(pv->diskname[i], ShortDiskName((char *)di[i]->Name));
	if (di[i]->TotalBlocks != 0)
                pv->diskutil[i] = (int) (0.5 +
	            (100.0*(di[i]->TotalBlocks - di[i]->BlocksAvailable)) /
                     di[i]->TotalBlocks);
	else
		pv->diskutil[i] = 0;
	}
    }

int CmpDisk(ViceDisk **d1, ViceDisk **d2)
    /* comparison yields disks in descending order of utilization
       null name ==> lowest utilization */
    {
    double u1, u2;

    if (*((*d1)->Name) == 0 && *((*d2)->Name) == 0) return(0);
    if (*((*d1)->Name) == 0) return (1);
    if (*((*d2)->Name) == 0) return(-1);

    u1 = ((1.0*((*d1)->BlocksAvailable))/(1.0*((*d1)->TotalBlocks)));
    u2 = ((1.0*((*d2)->BlocksAvailable))/(1.0*((*d2)->TotalBlocks)));
    if (u1 < u2) return(-1);
    if (u1 > u2) return(1);
    return(0);
    }

static int ValidServer(char *s)
{
    struct RPC2_addrinfo hints, *res = NULL;
    int ret;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    ret = RPC2_getaddrinfo(s, "codasrv", &hints, &res);
    RPC2_freeaddrinfo(res);
    
    return (ret == 0);
}

static char *ShortDiskName(char *s)
    /* Returns 5-char abbreviation of a disk name returned
       by ViceGetStatistics(). */
    {
    static char sdn[SDNLEN+1];
    register char *c;
    
    /* Eliminate leading '/' unless that's all there is */
    if (*s == '/' && *(s+1) != 0) c = s+1;
    else c = s;
    
    /* Obtain full or abbreviated name */
    if (strlen(c) <= SDNLEN) strcpy(sdn, c);
    else
	{
	strncpy(sdn, c, SDNLEN-2);
	sdn[SDNLEN-2] = '$';
	sdn[SDNLEN-1] = *(c + strlen(c) - 1); /* last char */
	sdn[SDNLEN] = 0;
	}
    return(sdn);
    }

#ifdef	DEBUG
void
print_stats(struct ViceStatistics *stats)
{
    fprintf(dbg, "\n");
    fprintf(dbg, "Obsolete1 = %d, ", stats->Obsolete1);
    fprintf(dbg, "Obsolete2 = %d, ", stats->Obsolete2);
    fprintf(dbg, "CurrentTime = %d, ", stats->CurrentTime);
    fprintf(dbg, "BootTime = %d, ", stats->BootTime);
    fprintf(dbg, "StartTime = %d, ", stats->StartTime);
    fprintf(dbg, "\n");
    fprintf(dbg, "CurrentConnections = %d, ", stats->CurrentConnections);
    fprintf(dbg, "TotalViceCalls = %d, ", stats->TotalViceCalls);
    fprintf(dbg, "\n");
    fprintf(dbg, "TotalFetches = %d, ", stats->TotalFetches);
    fprintf(dbg, "FetchDatas = %d, ", stats->FetchDatas);
    fprintf(dbg, "FetchedBytes = %d, ", stats->FetchedBytes);
    fprintf(dbg, "FetchDataRate = %d, ", stats->FetchDataRate);
    fprintf(dbg, "\n");
    fprintf(dbg, "TotalStores = %d, ", stats->TotalStores);
    fprintf(dbg, "StoreDatas = %d, ", stats->StoreDatas);
    fprintf(dbg, "StoredBytes = %d, ", stats->StoredBytes);
    fprintf(dbg, "StoreDataRate = %d, ", stats->StoreDataRate);
    fprintf(dbg, "\n");
    fprintf(dbg, "TotalRPCBytesSent = %d, ", stats->TotalRPCBytesSent);
    fprintf(dbg, "TotalRPCBytesReceived = %d, ", stats->TotalRPCBytesReceived);
    fprintf(dbg, "TotalRPCPacketsSent = %d, ", stats->TotalRPCPacketsSent);
    fprintf(dbg, "TotalRPCPacketsReceived = %d, ", stats->TotalRPCPacketsReceived);
    fprintf(dbg, "TotalRPCPacketsLost = %d, ", stats->TotalRPCPacketsLost);
    fprintf(dbg, "TotalRPCBogusPackets = %d, ", stats->TotalRPCBogusPackets);
    fprintf(dbg, "\n");

    fprintf(dbg, "SystemCPU = %d, ", stats->SystemCPU);
    fprintf(dbg, "UserCPU = %d, ", stats->UserCPU);
    fprintf(dbg, "NiceCPU = %d, ", stats->NiceCPU);
    fprintf(dbg, "IdleCPU = %d, ", stats->IdleCPU);
    fprintf(dbg, "\n");

    fprintf(dbg, "TotalIO = %d, ", stats->TotalIO);
    fprintf(dbg, "ActiveVM = %d, ", stats->ActiveVM);
    fprintf(dbg, "TotalVM = %d, ", stats->TotalVM);
    fprintf(dbg, "\n");
    fprintf(dbg, "EtherNetTotalErrors = %d, ", stats->EtherNetTotalErrors);
    fprintf(dbg, "EtherNetTotalWrites = %d, ", stats->EtherNetTotalWrites);
    fprintf(dbg, "EtherNetTotalInterupts = %d, ", stats->EtherNetTotalInterupts);
    fprintf(dbg, "EtherNetGoodReads = %d, ", stats->EtherNetGoodReads);
    fprintf(dbg, "EtherNetTotalBytesWritten = %d, ", stats->EtherNetTotalBytesWritten);
    fprintf(dbg, "\n");
    fprintf(dbg, "ProcessSize = %d, ", stats->ProcessSize);
    fprintf(dbg, "WorkStations = %d, ", stats->WorkStations);
    fprintf(dbg, "ActiveWorkStations = %d, ", stats->ActiveWorkStations);
    fprintf(dbg, "\n");
    fprintf(dbg, "MinFlt = %d, ", stats->MinFlt);
    fprintf(dbg, "MajFlt = %d, ", stats->MajFlt);
    fprintf(dbg, "NSwaps = %d, ", stats->NSwaps);
    fprintf(dbg, "Spare4 = %d, ", stats->Spare4);
    fprintf(dbg, "UsrTime = %d, ", stats->UsrTime);
    fprintf(dbg, "SysTime = %d, ", stats->SysTime);
    fprintf(dbg, "VmRSS = %d, ", stats->VmRSS);
    fprintf(dbg, "VmData = %d, ", stats->VmData);
    fprintf(dbg, "\n");
    fflush(dbg);
}
#endif

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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vtools/cmon.cc,v 4.5.4.1 1998/06/22 19:13:01 jaharkes Exp $";
#endif /*_BLURB_*/






/*  Simple program to monitor Coda servers
    M. Satyanarayanan, June 1990
*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <strings.h>
#include <errno.h>

#ifdef	__linux__
#include <ncurses.h>
#else
#include <curses.h>
#endif

#include <lwp.h>
#include <rpc2.h>
#include <se.h>
#include <timer.h>
#include <sftp.h>

#include "vice.h"

int iopen(int x, int y, int z){return(0);};  /* BLETCH!! */


#ifdef __cplusplus
}
#endif __cplusplus



static long MonBirthTime; /* when this monitor was born */

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


#define SRVCOLWIDTH 9  /* width of each server's window */
#define FIRSTSRVCOL 7 /* index of first server's column */
#define SDNLEN (SRVCOLWIDTH - 3)   /* length of short disk names */

/* Digested data for printing */
struct printvals
    {
    unsigned long cpu_sys;
    unsigned long cpu_user;
    unsigned long cpu_util;
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


PRIVATE void GetArgs(int argc, char *argv[]);
PRIVATE void InitRPC();
PRIVATE void DrawCaptions();
PRIVATE void PrintServer(struct server *);
PRIVATE void cbserver();
PRIVATE void srvlwp(int);
PRIVATE void kbdlwp(char *);
PRIVATE CmpDisk(ViceDisk **, ViceDisk **);
PRIVATE ValidServer(char *);
PRIVATE void ComputePV(struct server *s, struct printvals *pv);
PRIVATE char *ShortDiskName(char *s);

char Dummy; /* dummy variable for LWP_WaitProcess() */

main(int argc, char *argv[])
    {
    int i;
    
    MonBirthTime = time(0);
    initscr();
    curWin = newwin(1, 1, 0, 0);
    GetArgs(argc, argv);
    InitRPC();
    DrawCaptions();

    LWP_CreateProcess((PFIC)kbdlwp, 0x4000, LWP_NORMAL_PRIORITY, NULL, "KBD", (PROCESS *)&i);
    for (i = 0; i < SrvCount; i++)
	{
	LWP_CreateProcess((PFIC)srvlwp, 0x8000, LWP_NORMAL_PRIORITY, (char *)i, (char *)srv[i].srvname, (PROCESS *)&srv[i].pid);
	}
    
    LWP_WaitProcess(&Dummy); /* wait for Godot */
    }

PRIVATE void srvlwp(int slot)
    {
    struct server *moi;
    RPC2_HostIdent hi;
    RPC2_PortalIdent pi;
    RPC2_SubsysIdent si;
    RPC2_Handle cid;
    int rc;
    
    moi = &srv[slot];

    hi.Tag = RPC2_HOSTBYNAME;
    strcpy(hi.Value.Name, moi->srvname);
    pi.Tag = RPC2_PORTALBYINETNUMBER;
    pi.Value.InetPortNumber = htons(1361); /* wired-in! YUKKK! */
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
	    moi->oldvs = moi->newvs;
	    rc = (int) ViceGetStatistics(cid, &moi->newvs);
	    if (rc != RPC2_SUCCESS)
		{
		moi->state = DEAD;
		RPC2_Unbind(cid);
		}
	    else moi->succ = moi->probe; /* ignoring RPC call delays */
	    }


	PrintServer(moi);

	IOMGR_Select(32, 0, 0, 0, &ProbeInterval);  /* sleep */
	}
    }

PRIVATE void kbdlwp(char *p)
    /* LWP to listen for keyboard activity */
    {
    fd_set rset;
    int rc;
    char c;
    
    
    while(1)
	{
	FD_ZERO(&rset);
	FD_SET(fileno(stdin), &rset);
	rc = IOMGR_Select(32, (int *)&rset, 0, 0, 0); /* await activity */
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

PRIVATE void GetArgs(int argc, char *argv[])
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
	    if (!c) goto BadArgs;
	    *c = 0; /* so servername is null terminated */
	    c++;
	    if (!ValidServer(srv[SrvCount].srvname))
		{
		printf("%s is not a valid server\n", srv[SrvCount].srvname);
		exit(-1);
		}
	    srv[SrvCount].hz  = atoi(c);
	    srv[SrvCount].win = newwin(24, SRVCOLWIDTH + 1, 0,
		FIRSTSRVCOL + SrvCount*(SRVCOLWIDTH + 2));
	    SrvCount++;
	    }
	}


    return;

BadArgs:
    printf("Usage: cmon [-a] [-t probeinterval] server1:hz1 server2:hz2 ...\n");
    exit(-1);
    }


PRIVATE void InitRPC()
    {
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
    RPC2_Init(RPC2_VERSION, 0, 0, -1, &tv);
    }


PRIVATE void DrawCaptions()
    {
    WINDOW *w1;
    
    w1 = newwin(23, FIRSTSRVCOL - 1, 1, 0);
    
    wprintw(w1, "TIM  \n");
    wprintw(w1, "  mon\n");
    wprintw(w1, " prob\n");
    wprintw(w1, " succ\n");
    wprintw(w1, "   up\n");
    wprintw(w1, " bind\n");
    wprintw(w1, "CPU  \n");
    wprintw(w1, "  sys\n");
    wprintw(w1, " user\n");
    wprintw(w1, " util\n");
    wprintw(w1, "RPC  \n");
    wprintw(w1, " conn\n");
    wprintw(w1, " wkst\n");
    wprintw(w1, " call\n");
    wprintw(w1, "  pki\n");
    wprintw(w1, "  pko\n");
    wprintw(w1, "  byi\n");
    wprintw(w1, "  byo\n");
    wprintw(w1, "DSK  \n");
    wprintw(w1, " max1\n");
    wprintw(w1, " max2\n");
    wprintw(w1, " max3\n");
    wmove(w1, 0, 0);
    wrefresh(w1);
    }


char *when(long now, long then)
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
    	sprintf(answer, "%3.1lf days", fdays);
	}
    return(answer);
    }


PRIVATE void PrintServer(struct server *s)
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
    else wprintw(w, "%*s\n", SRVCOLWIDTH, "***");

    if (s->state == DEAD)
	{
#define QROWS(n)\
    for (i = 0; i < n; i++)\
    wprintw(w, "%*s\n", SRVCOLWIDTH, "???");

	QROWS(2);
	wprintw(w, "\n");
	QROWS(3);
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
	ComputePV(s, &pv);

#define WPRINT(x)\
	if (pv.x == 0xffffffff) wprintw(w, "%*s\n", SRVCOLWIDTH, "***");\
	else wprintw(w, "%*lu\n", SRVCOLWIDTH, pv.x);

	wprintw(w, "%*s\n", SRVCOLWIDTH, when(s->probe, s->newvs.StartTime));
	wprintw(w, "%*d\n", SRVCOLWIDTH, s->binds);
	wprintw(w, "\n");
	WPRINT(cpu_sys);
	WPRINT(cpu_user);
	WPRINT(cpu_util);
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
	    sprintf(buf,  "%s:%lu", pv.diskname[i], pv.diskutil[i]);
	    wprintw(w, "%*s\n", SRVCOLWIDTH, buf);
	    }
	}
    wrefresh(w);
    HOME();
    }


PRIVATE void ComputePV(struct server *s, struct printvals *pv)
    {
    long t;
    ViceDisk *di[10];
    register int i;

    pv->rpc_conn = s->newvs.CurrentConnections;
    pv->rpc_wkst = s->newvs.WorkStations;
    
    if (AbsFlag)
	{
	pv->cpu_sys = s->newvs.SystemCPU/s->hz;
	pv->cpu_user = (s->newvs.UserCPU + s->newvs.NiceCPU)/s->hz;
	t = pv->cpu_sys + pv->cpu_user;
	pv->cpu_util = (int) (0.5 + 100.0*t/(t + s->newvs.IdleCPU/s->hz));
	pv->rpc_call = s->newvs.TotalViceCalls;
	pv->rpc_pki = s->newvs.TotalRPCPacketsReceived;
	pv->rpc_pko = s->newvs.TotalRPCPacketsSent;
	pv->rpc_byi = s->newvs.TotalRPCBytesReceived;
	pv->rpc_byo = s->newvs.TotalRPCBytesSent;
	}
    else
	{
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
	    }
	else
	    {
#define DIFF(x) (s->newvs.x - s->oldvs.x)
	    pv->cpu_sys = DIFF(SystemCPU)/s->hz;
	    pv->cpu_user = (DIFF(UserCPU) + DIFF(NiceCPU))/s->hz;
	    t = pv->cpu_sys + pv->cpu_user;
	    pv->cpu_util = (int) (0.5 + 100.0*t/(t + DIFF(IdleCPU)/s->hz));
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
	pv->diskutil[i] = (int) (0.5 +
	    (100.0*(di[i]->TotalBlocks - di[i]->BlocksAvailable))/di[i]->TotalBlocks);
	}
    }

CmpDisk(ViceDisk **d1, ViceDisk **d2)
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

PRIVATE ValidServer(char *s)
    {
    struct hostent *he;
    
    he = gethostbyname(s);
    if (he) return(1);
    else return(0);
    }

PRIVATE char *ShortDiskName(char *s)
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

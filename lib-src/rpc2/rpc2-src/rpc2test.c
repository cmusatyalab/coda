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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


/* real hack only for debugging - should modify ctlwp */
int lwp_nextindex;

/* Produces: rpc2.log, rpc2.trace and sftp.trace in the directory
   specified by the user. Also creates junk data files in that directory */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <assert.h>
#include "coda_string.h"
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include "sftp.h"

#define SUBSYS_SRV 1001

extern int errno;
extern long sftp_windowfulls, sftp_ackslost, sftp_duplicates, sftp_bogus;
long lostunbinds;  /* no of times a NAK was received because of lost
			reply to an Unbind RPC */
long connbusies;  /* no of times an RPC2_CONNBUSY was seen */

static PROCESS ParentPid;
static PROCESS ListenerPid;

static long Workers, Clients;
static long MaxThinkTime, MaxComputeTime, MaxListenPause;
static long AvoidUnbinds, Announce, AvoidBulk;
static long rpc2rc;

#define WhatHappened(X) ((rpc2rc = X), printf("%s\n", RPC2_ErrorMsg(rpc2rc)), rpc2rc)
#define FLUSH() (fflush(stdout))
#define MYNAME (LWP_Name())
#define TESTPORT 5000  /* for advertising services */

#define TBSIZE 1000 /* Size of RPC2 trace buffer, if enabled */

static long ListenerBody(char *listenerName);
static long WorkerBody(char *workerName);
static long ClientBody(char *clientName);
static long GetPasswd(RPC2_CountedBS *Who, RPC2_EncryptionKey *Key1,
		      RPC2_EncryptionKey *Key2);
static void GetVar(long *gVar, char *gPrompt);
static void GetStringVar(char *gSVar, char *gPrompt);

static void DumpAndQuit(int opcode);
static void SelectParms(long *cid, long *opcode);
static void BulkErr(RPC2_Handle cid, SE_Descriptor *sed, int retcode, int op);
static time_t mytime(void);
static void MakeFiles(void);
static int mkfile(char *name, int length);
static void GetConns(void);
static void DoBindings(void);
static void MakeWorkers(void);
static void MakeClients(void);
static void InitRPC(void);
static void GetRoot(void);
static void GetParms(void);
static void RanDelay(int t);
static void HandleRPCFailure(long cid, long rcode, long op);
static void PrintStats(void);
static void mktee(char *logfile);

static char **SysFiles;	/* list of files to be used */
static int SysFileCount;    /* How many there are */

struct CVEntry
    {
    long CallsMade;
    RPC2_Handle ConnHandle;
    enum Status {SFREE, BUSY, BROKEN, UNBOUND} Status;
    RPC2_HostIdent RemoteHost; 
    RPC2_CountedBS Identity;
    long SecurityLevel;
    char  Password[RPC2_KEYSIZE+1];	/* 1 for trailing null */
    char NameBuf[30];
    };

#define MAXCON 100
static struct CVEntry ConnVector[MAXCON];
static long CVCount;	/* actual number in use */
#define OPCODESINUSE 7	/* excludes 999 for quit and 8 for rebind */

FILE *LogFile;	/* in "/tmp/rpc2test/rpc2test.log" */
long VerboseFlag;	/* TRUE if full output is to be produced */
char NextHashMark = '#';	/* each worker and client increments by one */
 
RPC2_PortIdent PortId;
RPC2_SubsysIdent SubsysId;
char TestDir[256];


static long ClientsReady;  /* How many clients are ready; will be signalled by main() to start real action  */

char *TimeNow(void)
{
    int t;
    
    t = mytime();
#if defined(ibmrt) || (__GNUC__ >= 2)
    return(ctime((const time_t *)&t));
#else 
    return(ctime(&t));
#endif ibmrt
}


char *MakeName(char *leaf)
{
    static char buf[200];

    strcpy(buf, TestDir);
    strcat(buf, "/");
    strcat(buf, leaf);
    return(buf);
}

int main(void)
{
    long go;

    GetRoot();  /* Also creates a child process for transcribing stdout */
    GetParms();
    MakeFiles(); /* in test directory */

    InitRPC();

    MakeWorkers();
    GetConns();
    GetVar(&go, "Say when: ");
    DoBindings();
    MakeClients();

    /* wait for all clients to get ready */
    while (ClientsReady < Clients) LWP_DispatchProcess();

    LWP_NoYieldSignal((char *)&ClientsReady);
    LWP_WaitProcess((char *)main);  /* infinite wait */
    
    return 0; /* make compiler happy */
}


static long WorkerBody(char *workerName)
{
    long i, rc;
    RPC2_RequestFilter reqfilter;
    RPC2_PacketBuffer *InBuff, *OutBuff;
    RPC2_Handle workercid;
    char myprivatefile[256];
    char myhashmark;
    SE_Descriptor sed;


    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;

    strcpy(myprivatefile, MakeName(workerName));
    myhashmark = NextHashMark++;

    LWP_DispatchProcess();	/* initial courtesy to parent */

    reqfilter.FromWhom = ONESUBSYS;
    reqfilter.ConnOrSubsys.SubsysId = SUBSYS_SRV;
    assert(reqfilter.ConnOrSubsys.SubsysId != -1);
    reqfilter.OldOrNew = OLD;

    RPC2_AllocBuffer(1000, &OutBuff);
    InBuff = NULL;

    while (TRUE)
	{
	RanDelay(MaxComputeTime);

	if (InBuff != NULL) RPC2_FreeBuffer(&InBuff);
	i = RPC2_GetRequest(&reqfilter, &workercid, &InBuff, NULL, NULL, 0, NULL);
	if (i != RPC2_SUCCESS)
	    {
	    printf("\n%s: GetRequest failed (%s) at %s", MYNAME, RPC2_ErrorMsg(i), TimeNow());
	    DumpAndQuit(0);
	    }
	
	switch(InBuff->Header.Opcode)
	    {
	    case 1: /* return Unix epoch time */
		{
		strcpy(OutBuff->Body, TimeNow());
		OutBuff->Header.ReturnCode = RPC2_SUCCESS;
		OutBuff->Header.BodyLength = sizeof(struct RPC2_PacketHeader) + strlen(OutBuff->Body) + 1;
		break;
		}
		
	    case 2: /* square the input integer */
		{
		long x = ntohl(*(long *)(InBuff->Body));
		*(long *)(OutBuff->Body) = htonl(x*x);
		OutBuff->Header.ReturnCode = RPC2_SUCCESS;
		OutBuff->Header.BodyLength = sizeof(RPC2_Integer);
		break;
		}
	    
	    case 3: /* cube the input integer */
		{
		long x = ntohl(*(long *)(InBuff->Body));

		*(long *)(OutBuff->Body) = htonl(x*x*x);
		OutBuff->Header.ReturnCode = RPC2_SUCCESS;
		OutBuff->Header.BodyLength = sizeof(RPC2_Integer);
		break;
		}

	    case 4: /* Return your machine name */
		{
		gethostname(OutBuff->Body, 100);
		OutBuff->Header.ReturnCode = RPC2_SUCCESS;
		OutBuff->Header.BodyLength = strlen(OutBuff->Body) + 1;
		break;
		}

	    case 5: /* Fetch a random file */
		{
		if (VerboseFlag) sed.Value.SmartFTPD.hashmark = myhashmark;
		else sed.Value.SmartFTPD.hashmark = 0;
		strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName,
			SysFiles[rpc2_NextRandom(0) % SysFileCount]);
		sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
		sed.Value.SmartFTPD.SeekOffset = 0;		
		sed.Value.SmartFTPD.Tag = FILEBYNAME;

		if ((rc = RPC2_InitSideEffect(workercid, &sed)) != RPC2_SUCCESS)
		    {
		    BulkErr(workercid, &sed, rc, InBuff->Header.Opcode);
		    assert(RPC2_Unbind(workercid) == RPC2_SUCCESS);
		    continue;
		    }
		if ((rc = RPC2_CheckSideEffect(workercid, &sed, SE_AWAITLOCALSTATUS)) != RPC2_SUCCESS)
			{
			BulkErr(workercid, &sed, rc, InBuff->Header.Opcode);
			assert(RPC2_Unbind(workercid) == RPC2_SUCCESS);
			continue;
			}
		else
		    if (VerboseFlag)
			fprintf(stderr, "%ld bytes transferred\n",
				sed.Value.SmartFTPD.BytesTransferred);
		OutBuff->Header.ReturnCode = (long)sed.LocalStatus;
		OutBuff->Header.BodyLength = 0;
		break;
		}

	    case 6: /* Store a random file */
		{
		if (VerboseFlag) sed.Value.SmartFTPD.hashmark = myhashmark;
		else sed.Value.SmartFTPD.hashmark = 0;
		strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, myprivatefile);
		sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
		sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
		sed.Value.SmartFTPD.SeekOffset = 0;
		sed.Value.SmartFTPD.Tag = FILEBYNAME;
		
		if ((rc = RPC2_InitSideEffect(workercid, &sed)) != RPC2_SUCCESS)
			{
			BulkErr(workercid, &sed, rc, InBuff->Header.Opcode);
			}
		if ((rc = RPC2_CheckSideEffect(workercid, &sed, SE_AWAITLOCALSTATUS)) != RPC2_SUCCESS)
			{
			BulkErr(workercid, &sed, rc, InBuff->Header.Opcode);
			}
		else
		    if (VerboseFlag)
			fprintf(stderr, "%ld bytes transferred\n",
				sed.Value.SmartFTPD.BytesTransferred);
		OutBuff->Header.ReturnCode = (long)sed.LocalStatus;
		OutBuff->Header.BodyLength = 0;
		break;
		}

	    case 7:	/* Unbind */
		{
		OutBuff->Header.ReturnCode = RPC2_SUCCESS;
		OutBuff->Header.BodyLength = 0;
		break;
		}

	    case 999: /* Quit */
		{
		OutBuff->Header.ReturnCode = RPC2_SUCCESS;
		OutBuff->Header.BodyLength = 0;
		break;
		}

	    default: /* unknown opcode */
		OutBuff->Header.ReturnCode = RPC2_FAIL;
		OutBuff->Header.BodyLength = 1 + strlen("Get your act together");
		strcpy(OutBuff->Body, "Get your act together");
	    break;
	    }


	i = RPC2_SendResponse(workercid, OutBuff);
	if (i != RPC2_SUCCESS) 
	    {
	    printf ("\n%s: response for opcode %d on connection 0x%lX  failed (%s) at %s", 
		MYNAME,	(int)InBuff->Header.Opcode, workercid,
		RPC2_ErrorMsg(i), TimeNow());
	    DumpAndQuit(InBuff->Header.Opcode);
	    }
	if (InBuff->Header.Opcode == 7)
	    assert(RPC2_Unbind(workercid) == RPC2_SUCCESS);
	}
}
	
static void BulkErr(RPC2_Handle cid, SE_Descriptor *sed, int retcode, int op)
{
    char *x;

    printf ("\n%s: File transfer failed  conn: 0x%lx   code: %s  op: %d  time: %s\n", 
	MYNAME,	cid, RPC2_ErrorMsg(retcode), op, TimeNow());
    if (sed->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER)
	x = "CLIENTTOSERVER";
    else x = "SERVERTOCLIENT";
    
    printf("\t\tFile: %s  Direction: %s\n",
    	sed->Value.SmartFTPD.FileInfo.ByName.LocalFileName, x);
    DumpAndQuit(op);  
}




static long ListenerBody(char *listenerName)
{
    long i;
    RPC2_RequestFilter reqfilter;
    RPC2_PacketBuffer *InBuff;
    RPC2_NewConnectionBody *newconnbody;
    RPC2_Handle newcid;

    LWP_DispatchProcess();	/* initial courtesy to parent */
    reqfilter.FromWhom = ONESUBSYS;
    reqfilter.ConnOrSubsys.SubsysId = SUBSYS_SRV;
    assert(reqfilter.ConnOrSubsys.SubsysId != -1);
    reqfilter.OldOrNew = NEW;

    InBuff = NULL;

    while (TRUE)
	{
	RanDelay(MaxListenPause);

	if (InBuff != NULL) RPC2_FreeBuffer(&InBuff);
	i = RPC2_GetRequest(&reqfilter, &newcid, &InBuff, NULL, GetPasswd, RPC2_XOR, NULL);
	if (i != RPC2_SUCCESS)
	    {
	    printf("Listener error: ");
	    WhatHappened(i);
	    }

	switch(InBuff->Header.Opcode)
	    {
	    case RPC2_NEWCONNECTION: /* new connection */
		{
		newconnbody = (RPC2_NewConnectionBody *)InBuff->Body;

		if (VerboseFlag)
		    fprintf(stderr, "Newconn: 0x%lx  \"%s\"  at  %s",
			newcid, (char*)&newconnbody->ClientIdent.SeqBody,
			TimeNow());

		RPC2_Enable(newcid);	/* unfreeze the connection */
		break;
		}
		
	    default: /* unknown opcode */
		assert(InBuff->Header.Opcode == RPC2_NEWCONNECTION);
	    break;
	    }

	}
}


static long ClientBody(char *clientName)
{
    long thisconn, thisopcode;
    RPC2_PacketBuffer *request, *reply;
    long retcode, rpctime = 0;
    struct timeval t1, t2;
    char myprivatefile[256];
    char myhashmark;
    RPC2_BindParms bp;
    SE_Descriptor sed;


#define MakeTimedCall(whichse)\
	if (VerboseFlag) gettimeofday(&t1, 0);\
	retcode = RPC2_MakeRPC(ConnVector[thisconn].ConnHandle, request, whichse, &reply, NULL, 0);\
	if (VerboseFlag) gettimeofday(&t2, 0);\
	if (VerboseFlag) rpctime = ((t2.tv_sec - t1.tv_sec)*1000) + ((t2.tv_usec - t1.tv_usec)/1000);


    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;

    strcpy(myprivatefile, MakeName(clientName));
    myhashmark = NextHashMark++;

    LWP_DispatchProcess();	/* initial courtesy to parent */
    
    RPC2_AllocBuffer(1000, &request);
    reply = NULL;


    ClientsReady++;
    LWP_WaitProcess((char *)&ClientsReady);	/* wait for main() to tap me on shoulder */

    while(TRUE)
	{
	if (reply) RPC2_FreeBuffer(&reply);

	RanDelay(MaxThinkTime);
	SelectParms(&thisconn, &thisopcode);
	ConnVector[thisconn].Status = BUSY;
	request->Header.Opcode = thisopcode;
	
	if (VerboseFlag)
	    fprintf(stderr, "Making request %ld to %s for %s\n", thisopcode,
		ConnVector[thisconn].RemoteHost.Value.Name,
		ConnVector[thisconn].NameBuf);

	switch(thisopcode)
	    {
	    case 1: /* return Unix epoch time */
		{
		request->Header.BodyLength = 0;
		MakeTimedCall(NULL);
		if (retcode == RPC2_SUCCESS)
		    {
		    if (VerboseFlag)
			fprintf(stderr, "Time on %s is %s (%ld msecs)\n",
				ConnVector[thisconn].RemoteHost.Value.Name,
				reply->Body, rpctime);
		    break;
		    }
		else HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));
		}
		
	    case 2: /* square the input integer */
		{
		long x = rpc2_NextRandom(0) % 100;
		*(long *)(request->Body) = htonl(x);

		request->Header.BodyLength = sizeof(long);
		MakeTimedCall(NULL);
		if (retcode == RPC2_SUCCESS)
		    {
		    if (VerboseFlag)
			fprintf(stderr, " %s says square of %ld is %lu (%ld msecs)\n",
				ConnVector[thisconn].RemoteHost.Value.Name, x,
				(unsigned long)ntohl(*(long *)reply->Body),
				rpctime);
		    break;
		    }
		else HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));
		break;
		}
	    
	    case 3: /* cube the input integer */
		{
		long x = rpc2_NextRandom(0) % 100;
		*(long *)(request->Body) = htonl(x);

		request->Header.BodyLength = sizeof(long);
		MakeTimedCall(NULL);
		if (retcode == RPC2_SUCCESS)
		    {
		    if (VerboseFlag)
			fprintf(stderr, "%s says cube of %ld is %lu (%ld msecs)\n",
				ConnVector[thisconn].RemoteHost.Value.Name, x,
				(unsigned long)ntohl(*(long *)reply->Body),
				rpctime);
		    break;
		    }
		else HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));
		break;
		}

	    case 4: /* Return your machine name */
		{
		request->Header.BodyLength = 0;
		MakeTimedCall(NULL);
		if (retcode == RPC2_SUCCESS)
		    {
		    if (VerboseFlag)
			fprintf(stderr, "%s says its name is \"%s\" (%ld msecs)\n",
				ConnVector[thisconn].RemoteHost.Value.Name,
				reply->Body, rpctime);
		    break;
		    }
		else HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));
		break;
		}

	    case 5: /* Fetch a random file */
		{
		if (AvoidBulk)
		     {
		     ConnVector[thisconn].Status = SFREE;
		     continue;
		     }
		request->Header.BodyLength = 0;
		sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
		sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
		sed.Value.SmartFTPD.SeekOffset = 0;
		sed.Value.SmartFTPD.Tag = FILEBYNAME;
		strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, myprivatefile);
		if (VerboseFlag)
		    sed.Value.SmartFTPD.hashmark = myhashmark;
		else sed.Value.SmartFTPD.hashmark = 0;
		MakeTimedCall(&sed);
		if (retcode != RPC2_SUCCESS)
		    {
		    HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));
		    break;		    
		    }
		else
		    if (VerboseFlag)
			fprintf(stderr, "%ld bytes transferred\n",
				sed.Value.SmartFTPD.BytesTransferred);

		break;
		}
		
	    case 6: /* Store a random file */
		{
		if (AvoidBulk)
		     {
		     ConnVector[thisconn].Status = SFREE;
		     continue;
		     }
		request->Header.BodyLength = 0;
		sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
		sed.Value.SmartFTPD.SeekOffset = 0;
		sed.Value.SmartFTPD.Tag = FILEBYNAME;
		strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, SysFiles[rpc2_NextRandom(0) % SysFileCount]);
		if (VerboseFlag)
		    sed.Value.SmartFTPD.hashmark = myhashmark;
		else sed.Value.SmartFTPD.hashmark = 0;
		MakeTimedCall(&sed);
		if (retcode !=  RPC2_SUCCESS)
		    {
		    HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));
		    break;		    
		    }
		else
		    if (VerboseFlag)
			fprintf(stderr, "%ld bytes transferred\n",
				sed.Value.SmartFTPD.BytesTransferred);		
		break;
		}


	    case 7:   /* Unbind */
		{
		request->Header.BodyLength = 0;
		MakeTimedCall(NULL);
		if (retcode == RPC2_SUCCESS)
		    {
		    if (VerboseFlag)
			fprintf(stderr, "Unbound connection to %s for %s after %ld calls\n",
				ConnVector[thisconn].RemoteHost.Value.Name,
				ConnVector[thisconn].Identity.SeqBody,
				ConnVector[thisconn].CallsMade);
		    assert(RPC2_Unbind(ConnVector[thisconn].ConnHandle) == RPC2_SUCCESS);
		    }
		else
		    {HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));}
		ConnVector[thisconn].Status = UNBOUND;
		break;
		}


	    case 8:	/* Rebind */
		{
		bp.SecurityLevel = ConnVector[thisconn].SecurityLevel;
		bp.EncryptionType = RPC2_XOR;
		bp.SideEffectType = SMARTFTP;
		bp.ClientIdent = &ConnVector[thisconn].Identity;
		bp.SharedSecret = (RPC2_EncryptionKey *)ConnVector[thisconn].Password; 
		retcode = RPC2_NewBinding(&ConnVector[thisconn].RemoteHost,
					  &PortId, &SubsysId, &bp, 
					  &ConnVector[thisconn].ConnHandle); 
		if (retcode < RPC2_ELIMIT)
		    {
		    HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));
		    }
		else
		    {
		    if (VerboseFlag)
			fprintf(stderr, "Rebound connection to %s for %s\n",
				ConnVector[thisconn].RemoteHost.Value.Name,
				ConnVector[thisconn].Identity.SeqBody);
		    }
		break;
		}



	    case 999: /* Quit */
		{
		}

	    default: /* unknown opcode */
		printf("Arrrgggghhh .... bogus opcode\n"); abort();
		break;
	    }
	
	if (ConnVector[thisconn].Status == BUSY) ConnVector[thisconn].Status = SFREE;
	if (retcode == RPC2_CONNBUSY) continue; /* you didn't really do it */

	/* Indicate progress  */
	ConnVector[thisconn].CallsMade++;
	if(ConnVector[thisconn].CallsMade % Announce == 1)
	    {
	    struct CVEntry *ce = &ConnVector[thisconn];
	    printf("\n%ld successful calls to %s for %s at %s", ce->CallsMade,
		ce->RemoteHost.Value.Name, ce->NameBuf, TimeNow());
	    PrintStats();
	    }
	else
	    {
	    int xx;
	    xx = (1.0*Announce)/100.0 + 0.5; /* ceiling */
	    if (xx == 0 || ConnVector[thisconn].CallsMade % xx == 1)
		printf("%c", myhashmark);
	    }
	
	}

}



void iopen(void )
{
    assert(1 == 0);
}

static struct Password {
    char *name; char *password;
} PList[] = {
    {"satya",   "banana"},
    {"john",    "howard"},
    {"mike",    "kazar"},
    {"jim",     "morris"},
    {"tom",     "peters"},
    {"mikew",   "west"},
    {"carolyn", "council"}
};


static long GetPasswd(RPC2_CountedBS *Who, RPC2_EncryptionKey *Key1,
		      RPC2_EncryptionKey *Key2)
{
    long i;
    long maxpw = sizeof(PList)/sizeof(struct Password);

    for (i = 0; i < maxpw; i++)
	if(strcmp(PList[i].name, Who->SeqBody) == 0)
	    {
	    bcopy("          ", Key1, RPC2_KEYSIZE);
	    strcpy((char *)Key1, PList[i].password);
	    bcopy(Key1, Key2, RPC2_KEYSIZE);
	    return(0);
	    }
    return(-1);
}

static time_t mytime(void)
{
    struct timeval t;
    TM_GetTimeOfDay(&t, NULL);
    return(t.tv_sec);
}


static void MakeFiles(void)
{
    /* Variety of sizes to test file transfer ability
	Files get created in test directory  */

    static char *fsize[] = {"10", "235", "1310", "14235", "100234", "1048576"};
    static char *fname[sizeof(fsize)/sizeof(char *)];
    int i;

    SysFileCount = sizeof(fname)/sizeof(char *);

    for (i = 0; i < SysFileCount; i++)
	{
	fname[i] = (char *)malloc(1+strlen(MakeName(fsize[i])));
	strcpy(fname[i], MakeName(fsize[i]));
	if (mkfile(fname[i], atoi(fsize[i])) < 0) exit(-1);
	}

    SysFiles = fname;
}



static void GetConns(void)
{
    int i;
    char myname[30];

    GetVar(&CVCount, "How many client connections: ");
    assert(CVCount < MAXCON);

    gethostname(myname, sizeof(myname));
    for (i = 0; i < CVCount; i++)
	{
	ConnVector[i].Status = SFREE;
	ConnVector[i].RemoteHost.Tag = RPC2_HOSTBYNAME;
	GetStringVar(ConnVector[i].RemoteHost.Value.Name, "Next Host: ");
	sprintf(ConnVector[i].NameBuf, "%s.%d", myname,i);
	ConnVector[i].SecurityLevel = RPC2_OPENKIMONO;
	ConnVector[i].Identity.SeqBody = (RPC2_ByteSeq)ConnVector[i].NameBuf;
	ConnVector[i].Identity.SeqLen = 1+strlen(ConnVector[i].NameBuf);
	}
}


static void DoBindings(void)
{
    int i, rc;
    RPC2_BindParms bp;

    for (i = 0; i < CVCount; i++)
	{
	 bp.SecurityLevel = ConnVector[i].SecurityLevel;
	 bp.EncryptionType = RPC2_XOR;
	 bp.SideEffectType = SMARTFTP;
	 bp.ClientIdent = &ConnVector[i].Identity;
	 bp.SharedSecret = (RPC2_EncryptionKey *)ConnVector[i].Password;
	 rc = RPC2_NewBinding(&ConnVector[i].RemoteHost, &PortId, 
			      &SubsysId, &bp,&ConnVector[i].ConnHandle); 
	if (rc < RPC2_ELIMIT)
	     {
	     printf("Couldn't bind to %s for %s ---> %s\n", ConnVector[i].RemoteHost.Value.Name,
	     	ConnVector[i].Identity.SeqBody, RPC2_ErrorMsg(rc));
	     ConnVector[i].Status = BROKEN;
	     continue;
	     }
	}
}

static void MakeWorkers(void)
{
    int i;
    char thisname[20];
    PROCESS thispid;

    for (i = 0; i < Workers; i++)
	{
	sprintf(thisname, "Worker%02d", i);
	LWP_CreateProcess((PFIC)WorkerBody, 16384, LWP_NORMAL_PRIORITY, thisname, thisname, &thispid);
	}
}


static void MakeClients(void)
{
    int i;
    char thisname[20];
    PROCESS thispid;

    for (i = 0; i < Clients; i++)
	{
	sprintf(thisname, "Client%02d", i);
	LWP_CreateProcess((PFIC)ClientBody, 16384, LWP_NORMAL_PRIORITY, thisname, thisname, &thispid);
	}
}


static void InitRPC(void)
{
    SFTP_Initializer sftpi;
    char *cstring;
    int rc;

    LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &ParentPid);

    PortId.Tag = RPC2_PORTBYINETNUMBER;
    PortId.Value.InetPortNumber = htons(TESTPORT);

    SFTP_SetDefaults(&sftpi);
    SFTP_Activate(&sftpi);

    rc = RPC2_Init(RPC2_VERSION, 0, &PortId, -1, NULL);
    if (rc != RPC2_SUCCESS)
	{
	printf("RPC2_Init() --> %s\n", RPC2_ErrorMsg(rc));
	exit(-1);
	}

    if (RPC2_Trace && (RPC2_InitTraceBuffer(TBSIZE) != RPC2_SUCCESS))
	exit(-1);

    SubsysId.Tag = RPC2_SUBSYSBYID;
    SubsysId.Value.SubsysId = SUBSYS_SRV;
    RPC2_Export(&SubsysId);

    cstring = "Listener1";
    LWP_CreateProcess((PFIC)ListenerBody, 16384, LWP_NORMAL_PRIORITY, cstring, cstring, &ListenerPid);
}


static void GetRoot(void)
{
    printf("Test dir: ");
    fflush(stdin);
    fgets(TestDir, sizeof(TestDir), stdin);

    mktee(MakeName("rpc2.log"));
}

static void GetParms(void)
{
    GetVar(&RPC2_DebugLevel, "Debug level? (0): ");
    GetVar(&VerboseFlag, "Verbosity (0): ");
    GetVar(&Announce, "Announce? (100): ");
    GetVar(&RPC2_Trace, "Tracing [0 = OFF, 1 = ON]? ");
    GetVar(&Workers, "Workers: ");
    GetVar(&Clients, "Clients: ");
    GetVar(&MaxThinkTime, "Max think time (ms): ");
    GetVar(&MaxComputeTime, "Max compute time (ms): ");
    GetVar(&MaxListenPause, "Max listen pause (ms): ");
    GetVar(&AvoidBulk, "Avoid bulk transfer? (0): ");
}


static void RanDelay(int t) /* milliseconds */
{
    int tx;
    struct timeval tval;

    if (t > 0)
	{
	tx = rpc2_NextRandom(0) % t;
	tval.tv_sec = tx / 1000;
	tval.tv_usec = 1000*(tx % 1000);
	if (VerboseFlag)
	    fprintf(stderr, "delaying for %ld:%ld seconds ....\n", 
		    tval.tv_sec, tval.tv_usec);
	FLUSH();
	assert(IOMGR_Select(32, 0,0,0, &tval) == 0);
	}
}


static void GetVar(long *gVar, char *gPrompt)
{
    char LineBuf[100];

    if (isatty(fileno(stdin))) printf(gPrompt);
    fgets(LineBuf, sizeof(LineBuf), stdin); *gVar = atoi(LineBuf);
    if (!isatty(fileno(stdin))) printf( "%s%ld\n", gPrompt, *gVar);
}

static void GetStringVar(char *gSVar, char *gPrompt)
{
    if (isatty(fileno(stdin))) printf(gPrompt);
    fgets(gSVar, sizeof(gSVar), stdin);
    *(gSVar + strlen(gSVar)) = 0;
    if (!isatty(fileno(stdin))) printf( "%s%s\n", gPrompt, gSVar);
}

static void HandleRPCFailure(long cid, long rcode, long op)
{

    ConnVector[cid].Status = BROKEN;

    if (rcode == RPC2_CONNBUSY)
	{
	connbusies++;
	return;
	}

    if (op == 7 && rcode == RPC2_NAKED)
	{
	lostunbinds++;
	assert(RPC2_Unbind(ConnVector[cid].ConnHandle) == RPC2_SUCCESS);
        FLUSH();
	return;
	}

    printf ("\n%s: call %ld on 0x%lX to %s for %s failed (%s) at %s", MYNAME,
	op, ConnVector[cid].ConnHandle,
	ConnVector[cid].RemoteHost.Value.Name,
	ConnVector[cid].NameBuf, RPC2_ErrorMsg(rcode), TimeNow());
    DumpAndQuit(op);
}


static void PrintStats(void)
{
    printf("Packets:    Sent=%ld  Retries=%ld  Received=%ld  Bogus=%ld\n",
	rpc2_Sent.Total, rpc2_Sent.Retries, rpc2_Recvd.Total, rpc2_Recvd.Bogus);
    printf("Bytes:      Sent=%ld  Received=%ld\n", rpc2_Sent.Bytes, rpc2_Recvd.Bytes);
    printf("Creation:   Spkts=%ld  Mpkts=%ld  Lpkts=%ld  SLEs=%ld  Conns=%ld\n",
		rpc2_PBSmallCreationCount, rpc2_PBMediumCreationCount, 
		rpc2_PBLargeCreationCount, rpc2_SLCreationCount, rpc2_ConnCreationCount);
    printf("Free:       Spkts=%ld  Mpkts=%ld  Lpkts=%ld  SLEs=%ld  Conns=%ld\n",
		rpc2_PBSmallFreeCount, rpc2_PBMediumFreeCount, 
		rpc2_PBLargeFreeCount, rpc2_SLFreeCount, rpc2_ConnFreeCount);
    printf("            Unbinds=%ld  FreeConns=%ld\n", rpc2_Unbinds, rpc2_FreeConns);
    printf("SFTP:       WindowFulls=%ld  AcksLost=%ld  Duplicates=%ld  Bogus=%ld LostUnbinds=%ld  ConnBusies=%ld\n",
		sftp_windowfulls, sftp_ackslost, sftp_duplicates, sftp_bogus,
		lostunbinds, connbusies);

    FLUSH();
}


static void SelectParms(long *cid, long *opcode)
{
    do
	{
	*cid = rpc2_NextRandom(0) % CVCount;
	}
    while (ConnVector[*cid].Status == BUSY);

    if (ConnVector[*cid].Status == UNBOUND || 
	ConnVector[*cid].Status == BROKEN)
	{
	*opcode = 8;	/* rebind */
	}
    else
	{
	do
	    {
	    *opcode = 1 + (rpc2_NextRandom(0) % OPCODESINUSE);
	    }
	while (AvoidUnbinds && *opcode == 7);
	}
}


void DumpAndQuit(int opcode) /* of failing call; 0 if not an RPC call */
{
    FILE *tracefile;
    
    
    if (RPC2_Trace)
	{
	tracefile = fopen(MakeName("rpc2.trace"), "w");
	RPC2_DumpTrace(tracefile, TBSIZE);
	RPC2_DumpState(tracefile, TBSIZE);
	if (opcode == 5 || opcode == 6)
	    sftp_DumpTrace(MakeName("sftp.trace"));
	}
    exit(-1);
}


void mktee(char *logfile)
    /* Creates a child process that will transcribe everything printed by the
     * parent on stdout to both stdout and logfile */
{
    int pid;
    int filedes[2];
    char *teeargs[3];

    if (pipe(filedes) < 0)
	{
	perror("pipe");
	exit(-1);
	}

    fflush(stdout);
    fflush(stdin);
    pid = fork();
    
    if (pid < 0)
	{
	perror("fork");
	exit(-1);
	}

    if (pid > 0)
	{
	/* Parent process */
	dup2(filedes[1], fileno(stdout));
	close(filedes[1]);
	return;
	}
    else
	{
	/* Child process */;
	dup2(filedes[0], fileno(stdin));
	close(filedes[0]);
	teeargs[0] = "tee";
	teeargs[1] = logfile;
	teeargs[2] = 0;
	execve("/usr/bin/tee",teeargs, 0);
	perror("execve");   /* should never get here */
	exit(-1);
	}
}

static int mkfile(char *name, int length)
{
    int fd;
    fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0)
	{
	perror(name);
	return(-1);
	}
    lseek(fd, length-1, SEEK_SET);
    write(fd, "0", 1);
    close(fd);    
    return(0);
}

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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rpc2/rpc2test.c,v 4.4 1998/08/26 17:08:12 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


/* real hack only for debugging - should modify ctlwp */
int lwp_nextindex;

/* Produces: rpc2.log, rpc2.trace and sftp.trace in the directory
   specified by the user. Also creates junk data files in that directory */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"
#include "se.h"

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
#define TESTPORTAL 5000  /* for advertising services */

#define TBSIZE 1000 /* Size of RPC2 trace buffer, if enabled */

static long ListenerBody(), WorkerBody(), ClientBody(), GetPasswd();
static void GetVar(), GetStringVar();
static BulkErr(), mytime(), MakeFiles(), GetConns(), DoBindings(),
	MakeWorkers(), MakeClients(), InitRPC(), GetRoot(), GetParms(),
	RanDelay(), HandleRPCFailure(), PrintStats(), SelectParms();

static char **SysFiles;	/* list of files to be used */
static SysFileCount;    /* How many there are */

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
 
RPC2_PortalIdent PortalId;
RPC2_SubsysIdent SubsysId;
char TestDir[256];


static long ClientsReady;  /* How many clients are ready; will be signalled by main() to start real action  */

char *TimeNow()
    {
    int t;
    
    t = mytime();
#if defined(ibmrt) || (__GNUC__ >= 2)
    return(ctime((const time_t *)&t));
#else 
    return(ctime(&t));
#endif ibmrt
    }


char *MakeName(leaf)
    char *leaf;
    {
    static char buf[200];

    strcpy(buf, TestDir);
    strcat(buf, "/");
    strcat(buf, leaf);
    return(buf);
    }

main()
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
    }


static long WorkerBody(workerName)
    char *workerName;
    {
    long i, rc;
    RPC2_RequestFilter reqfilter;
    RPC2_PacketBuffer *InBuff, *OutBuff;
    RPC2_Handle workercid;
    char myprivatefile[256];
    char myhashmark;
    SE_Descriptor sed;


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
	i = RPC2_GetRequest(&reqfilter, &workercid, &InBuff, NULL, NULL, NULL, NULL);
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
		    say(0, VerboseFlag, "%ld bytes transferred\n", sed.Value.SmartFTPD.BytesTransferred);
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
		    say(0, VerboseFlag, "%ld bytes transferred\n", sed.Value.SmartFTPD.BytesTransferred);
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
	    printf ("\n%s: response for opcode %d on connection 0x%X  failed (%s) at %s", 
		MYNAME,	InBuff->Header.Opcode, workercid, RPC2_ErrorMsg(i), TimeNow());
	    DumpAndQuit(InBuff->Header.Opcode);
	    }
	if (InBuff->Header.Opcode == 7)
	    assert(RPC2_Unbind(workercid) == RPC2_SUCCESS);
	}
    }
	
static BulkErr(cid, sed, retcode, op)
    RPC2_Handle cid;
    SE_Descriptor *sed;
    int retcode;
    int op;
    {
    char *x;

    printf ("\n%s: File transfer failed  conn: 0x%x   code: %s  op: %d  time: %s\n", 
	MYNAME,	cid, RPC2_ErrorMsg(retcode), op, TimeNow());
    if (sed->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER)
	x = "CLIENTTOSERVER";
    else x = "SERVERTOCLIENT";
    
    printf("\t\tFile: %s  Direction: %s\n",
    	sed->Value.SmartFTPD.FileInfo.ByName.LocalFileName, x);
    DumpAndQuit(op);  
    }




static long ListenerBody(listenerName)
    char *listenerName;
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

		say(0, VerboseFlag, ("Newconn: 0x%x  \"%s\"  at  %s",
			newcid, &newconnbody->ClientIdent.SeqBody, TimeNow()));

		RPC2_Enable(newcid);	/* unfreeze the connection */
		break;
		}
		
	    default: /* unknown opcode */
		assert(InBuff->Header.Opcode == RPC2_NEWCONNECTION);
	    break;
	    }

	}
    }


static long ClientBody(clientName)
    char *clientName;
    {
    long thisconn, thisopcode;
    RPC2_PacketBuffer *request, *reply;
    long retcode, rpctime;
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
	
	say(0, VerboseFlag, ("Making request %ld to %s for %s\n", thisopcode,
		ConnVector[thisconn].RemoteHost.Value.Name, ConnVector[thisconn].NameBuf));

	switch(thisopcode)
	    {
	    case 1: /* return Unix epoch time */
		{
		request->Header.BodyLength = 0;
		MakeTimedCall(NULL);
		if (retcode == RPC2_SUCCESS)
		    {
		    say(0, VerboseFlag, "Time on %s is %s (%ld msecs)\n",
		    	ConnVector[thisconn].RemoteHost.Value.Name, reply->Body, rpctime);
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
		    say(0, VerboseFlag, " %s says square of %ld is %ld (%ld msecs)\n",
		    	ConnVector[thisconn].RemoteHost.Value.Name, x, ntohl(*(long *)reply->Body), rpctime);
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
		    say(0, VerboseFlag, "%s says cube of %ld is %ld (%ld msecs)\n",
		    	ConnVector[thisconn].RemoteHost.Value.Name, x, ntohl(*(long *)reply->Body), rpctime);
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
		    say(0, VerboseFlag, "%s says its name is \"%s\" (%ld msecs)\n",
		    	ConnVector[thisconn].RemoteHost.Value.Name, reply->Body, rpctime);
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
		    say(0, VerboseFlag, "%ld bytes transferred\n", sed.Value.SmartFTPD.BytesTransferred);

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
		    say(0, VerboseFlag,"%ld bytes transferred\n", sed.Value.SmartFTPD.BytesTransferred);		
		break;
		}


	    case 7:   /* Unbind */
		{
		request->Header.BodyLength = 0;
		MakeTimedCall(NULL);
		if (retcode == RPC2_SUCCESS)
		    {
		    say(0, VerboseFlag, "Unbound connection to %s for %s after %d calls\n",
			ConnVector[thisconn].RemoteHost.Value.Name, ConnVector[thisconn].Identity.SeqBody,
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
					  &PortalId, &SubsysId, &bp, 
					  &ConnVector[thisconn].ConnHandle); 
		if (retcode < RPC2_ELIMIT)
		    {
		    HandleRPCFailure(thisconn, retcode, ntohl(request->Header.Opcode));
		    }
		else
		    {
		    say(0, VerboseFlag, ("Rebound connection to %s for %s\n",
			ConnVector[thisconn].RemoteHost.Value.Name, ConnVector[thisconn].Identity.SeqBody));
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



iopen()
    {
    assert(1 == 0);
    }

static struct Password 
    {
    char *name;
    char *password;
    }
    PList[] = { "satya", "banana", "john", "howard", "mike", "kazar", "jim", "morris",
	"tom", "peters", "mikew", "west",  "carolyn", "council"};
	


static long GetPasswd(Who, Key1, Key2)
    RPC2_CountedBS *Who;
    RPC2_EncryptionKey Key1, Key2;
    {
    register long i;
    long maxpw = sizeof(PList)/sizeof(struct Password);

    for (i = 0; i < maxpw; i++)
	if(strcmp(PList[i].name, Who->SeqBody) == 0)
	    {
	    bcopy("          ", Key1, RPC2_KEYSIZE);
	    strcpy(Key1, PList[i].password);
	    bcopy(Key1, Key2, RPC2_KEYSIZE);
	    return(0);
	    }
    return(-1);
    }

static mytime()
    {
    struct timeval t;
    TM_GetTimeOfDay(&t, NULL);
    return(t.tv_sec);
    }


static MakeFiles()
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



static GetConns()
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


static DoBindings()
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
	 rc = RPC2_NewBinding(&ConnVector[i].RemoteHost, &PortalId, 
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

static MakeWorkers()
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


static MakeClients()
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


static InitRPC()
    {
    SFTP_Initializer sftpi;
    char *cstring;
    int rc;

    LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &ParentPid);

    PortalId.Tag = RPC2_PORTALBYINETNUMBER;
    PortalId.Value.InetPortNumber = htons(TESTPORTAL);

    SFTP_SetDefaults(&sftpi);
    SFTP_Activate(&sftpi);

    rc = RPC2_Init(RPC2_VERSION, 0, &PortalId, -1, NULL);
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


static GetRoot()
    {

    printf("Test dir: ");
    fflush(stdin);
    gets(TestDir);

    mktee(MakeName("rpc2.log"));
    }

static GetParms()
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


static RanDelay(t)
    int t;	/* milliseconds */
    {
    int tx;
    struct timeval tval;

    if (t > 0)
	{
	tx = rpc2_NextRandom(0) % t;
	tval.tv_sec = tx / 1000;
	tval.tv_usec = 1000*(tx % 1000);
	say(0, VerboseFlag, "delaying for %ld:%ld seconds ....\n", 
		    tval.tv_sec, tval.tv_usec);
	FLUSH();
	assert(IOMGR_Select(32, 0,0,0, &tval) == 0);
	}
    }


static void GetVar(gVar, gPrompt)
    long *gVar;
    char *gPrompt;
    {
    char LineBuf[100];

    if (isatty(fileno(stdin))) printf(gPrompt);
    gets(LineBuf); *gVar = atoi(LineBuf);
    if (!isatty(fileno(stdin))) printf( "%s%ld\n", gPrompt, *gVar);
    }

static void GetStringVar(gSVar, gPrompt)
    char *gSVar, *gPrompt;
    {
    if (isatty(fileno(stdin))) printf(gPrompt);
    gets(gSVar);
    *(gSVar + strlen(gSVar)) = 0;
    if (!isatty(fileno(stdin))) printf( "%s%s\n", gPrompt, gSVar);
    }

static HandleRPCFailure(cid, rcode, op)
    long cid, rcode, op;
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

    printf ("\n%s: call %d on 0x%X to %s for %s failed (%s) at %s", MYNAME,
	op, ConnVector[cid].ConnHandle,
	ConnVector[cid].RemoteHost.Value.Name,
	ConnVector[cid].NameBuf, RPC2_ErrorMsg(rcode), TimeNow());
    DumpAndQuit(op);
    }


static PrintStats()
    {
    printf("Packets:    Sent=%ld  Retries=%ld  Received=%ld  Bogus=%ld\n",
	rpc2_Sent.Total, rpc2_Sent.Retries, rpc2_Recvd.Total, rpc2_Recvd.Bogus);
    printf("Bytes:      Sent=%ld  Received=%ld\n", rpc2_Sent.Bytes, rpc2_Recvd.Bytes);
    printf("Creation:   Spkts=%d  Mpkts=%d  Lpkts=%d  SLEs=%d  Conns=%d\n",
		rpc2_PBSmallCreationCount, rpc2_PBMediumCreationCount, 
		rpc2_PBLargeCreationCount, rpc2_SLCreationCount, rpc2_ConnCreationCount);
    printf("Free:       Spkts=%d  Mpkts=%d  Lpkts=%d  SLEs=%d  Conns=%d\n",
		rpc2_PBSmallFreeCount, rpc2_PBMediumFreeCount, 
		rpc2_PBLargeFreeCount, rpc2_SLFreeCount, rpc2_ConnFreeCount);
    printf("            Unbinds=%d  FreeConns=%d\n", rpc2_Unbinds, rpc2_FreeConns);
    printf("SFTP:       WindowFulls=%ld  AcksLost=%ld  Duplicates=%ld  Bogus=%ld LostUnbinds=%ld  ConnBusies=%ld\n",
		sftp_windowfulls, sftp_ackslost, sftp_duplicates, sftp_bogus,
		lostunbinds, connbusies);

    FLUSH();
    }

static SelectParms(cid, opcode)
    long *cid, *opcode;
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




DumpAndQuit(opcode)
    int	opcode;	/* of failing call; 0 if not an RPC call */
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


mktee(logfile)
    char *logfile;
    /* Creates a child process that will transcribe everything printed by the parent
    	on stdout to both stdout and logfile */
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

mkfile(name, length)
    char *name;
    int length;
    {
    int fd;
    fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0)
	{
	perror(name);
	return(-1);
	}
    lseek(fd, length-1, L_SET);
    write(fd, "0", 1);
    close(fd);    
    return(0);
    }

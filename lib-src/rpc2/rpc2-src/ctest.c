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

#define DEBUG
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <assert.h>
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include "sftp.h"
#include "test.h"

#define SUBSYS_SRV 1001

#ifdef FAKESOCKETS
extern int fake;
#endif

#ifdef RPC2DEBUG
#include "rpc2.private.h"
#endif

void DoBinding(RPC2_Handle *cid);
void PrintHelp(void);
void PrintStats(void);
void ClearStats(void);
void GetPort(    RPC2_PortIdent *p);
void GetWho(RPC2_CountedBS *w, long s, RPC2_EncryptionKey e);

extern struct SStats rpc2_Sent;
extern struct RStats rpc2_Recvd;
extern long RPC2_Perror;
extern long RPC2_DebugLevel;
#ifndef FAKESOCKETS
extern long SFTP_DebugLevel;
#endif
FILE *ErrorLogFile;
static char ShortText[200];
static char LongText[3000];
PROCESS mypid;			/* Pid of main process */

long VMMaxFileSize; /* length of VMFileBuf, initially 0 */
long VMCurrFileSize; /* amount of useful data in VMFileBuf */
char *VMFileBuf;    /* for FILEINVM transfers */

long rpc2rc;
#define WhatHappened(X,Y) ((rpc2rc = X), printf("%s: %s\n", Y, RPC2_ErrorMsg(rpc2rc)), rpc2rc)

FILE *ifd;
int fflag = 0;
int qflag = 0;
int bwflag = 0;
char *bwfile;
struct timeval start, middle;
FILE *BW_f;

int bwi = 0;
void bwcb(void *userp, unsigned int offset)
{
	bwi++;
	gettimeofday(&middle, (struct timezone *)0);
	if (start.tv_usec > middle.tv_usec) {
	    middle.tv_sec--;
	    middle.tv_usec += 1000000;
	}
	middle.tv_sec -= start.tv_usec;
	middle.tv_usec -= start.tv_usec;
        fprintf(BW_f,"%ld.%06ld %d\n", middle.tv_sec, middle.tv_usec, offset);
}

int main(int arg, char **argv)
{
	RPC2_Handle cid;
	long i, j, tt;
	long opcode;
	
	char *nextc;
	RPC2_PacketBuffer *Buff1;
	RPC2_PacketBuffer *Buff2;
	SFTP_Initializer sftpi;

	long rpctime;
	struct timeval t1, t2;
	
	SE_Descriptor sed;

	while (arg > 1 && argv[1][0] == '-') {
		if (!strcmp(argv[1], "-q")) {
			qflag++;
			arg--; argv++;
			continue;
		} else if (!strcmp(argv[1], "-b")) {
			bwflag++;
			arg--; argv++;
			bwfile = argv[1];
			if ((BW_f = fopen(argv[1], "w")) == NULL) {
				printf("ctest: can not open bandwidth file \"%s\"\n",
					bwfile);
				perror("fopen");
				BW_f = stdout;
			}
			arg--; argv++;
			continue;
		}
	}
	if (arg > 1) {
		if ((ifd = fopen(argv[1], "r")) == NULL) {
			printf("ctest: can not open script file \"%s\"\n", argv[1]);
			perror("fopen");
			exit(1);
		} else {
			fflag++;
		}
	} else {
		ifd = stdin;
	}

	ErrorLogFile = stderr;
	assert(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mypid) == LWP_SUCCESS);


	for (i='a'; i < 'z' + 1; i++) {
		j = 6*(i-'a');
		ShortText[j] = ShortText[j+1] = ShortText[j+2] = 
			ShortText[j+3] = ShortText[j+4]
			= ShortText[j+5] = i;
	}
	LongText[0] = 0;
	for (i = 0; i < 10; i++)
		(void) strcat(LongText, ShortText);


	if (!qflag) printf("Debug level? ");
	(void) fscanf(ifd, "%ld", &RPC2_DebugLevel);
	if (!qflag && fflag) printf(" %ld\n", RPC2_DebugLevel);

#ifndef FAKESOCKETS
	SFTP_SetDefaults(&sftpi);
	sftpi.WindowSize = 32;
	sftpi.SendAhead = 8;
	sftpi.AckPoint = 8;
	sftpi.PacketSize = 2800;
	SFTP_Activate(&sftpi);
	SFTP_EnforceQuota = 1;
#endif

#ifdef PROFILE
	InitProfiling();
#endif

	if(WhatHappened(RPC2_Init(RPC2_VERSION, (RPC2_Options *)NULL, 
				  (RPC2_PortIdent *)NULL, -1, (struct timeval *)NULL), "Init") != RPC2_SUCCESS)
	exit(-1);

    lwp_stackUseEnabled = 0;

    assert(RPC2_AllocBuffer(RPC2_MAXPACKETSIZE - 500, &Buff1) == RPC2_SUCCESS);
                       /* 500 is a fudge factor */

    Buff2 = NULL; /* only for safety; RPC2_MakeRPC() will set it */


    DoBinding(&cid);

    while (1)
    {
	if (!qflag) printf("RPC operation (%d for help)? ", HELP);
	(void) fscanf(ifd, "%ld", &opcode);
	if (!qflag && fflag) printf(" %ld\n", opcode);

	Buff1->Header.Opcode = opcode;
	switch((int) opcode)
	    {
	    case HELP: 
		    FT_GetTimeOfDay(&t1, NULL);
		    
		    PrintHelp();
		    FT_GetTimeOfDay(&t2, NULL);
		    break;

	    case REMOTESTATS:
	    case BEGINREMOTEPROFILING:
	    case ENDREMOTEPROFILING:
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		Buff1->Header.BodyLength = 0;
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL, 
					       &Buff2, (struct timeval *)NULL, 
					       (long) 0), "MakeRPC");
		if (tt != RPC2_SUCCESS)
		    continue;
		FT_GetTimeOfDay(&t2, NULL);
		break;

	    case STATS:
		PrintStats();
		break;

	    case SETVMFILESIZE:
		if (!qflag) printf("Local buffer size? ");
		(void) fscanf(ifd, "%ld", &VMMaxFileSize);
		if (!qflag && fflag) printf(" %ld\n", VMMaxFileSize);
		if (VMFileBuf) free(VMFileBuf);
		assert(VMFileBuf = (char *)malloc((unsigned)VMMaxFileSize));
		break;
		
	    case SETREMOTEVMFILESIZE:
		{
		if (!qflag) printf("Remote buffer size? ");
		(void) fscanf(ifd, "%ld", &tt);
		if (!qflag && fflag) printf(" %ld\n", tt);
		tt = (long) htonl((unsigned long)tt);
		memcpy(Buff1->Body, &tt, sizeof(long));
		Buff1->Header.BodyLength = sizeof(long);
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL,  
					       &Buff2, (struct timeval *)NULL, 
					       (long) 0), "MakeRPC");
		FT_GetTimeOfDay(&t2, NULL);
		if (tt != RPC2_SUCCESS)continue;
		break;
		}

		


	    case DUMPTRACE:
		{
		long count;
		if (!qflag) printf("How many elements? ");
		(void) fscanf(ifd, "%ld", &count);
		if (!qflag && fflag) printf(" %ld\n", count);
		(void) RPC2_DumpTrace(stdout, count);
		break;
		}

	    case FETCHFILE:
	    case STOREFILE:
		memset(&sed, 0, sizeof(SE_Descriptor));  /* initialize */
		sed.Tag = SMARTFTP;
		sed.Value.SmartFTPD.Tag = FILEBYNAME;
		sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
	
		if (bwflag) {
			sed.userp = (void *)0x12344321;
			sed.XferCB = bwcb;
		}

		if (opcode  == (long) STOREFILE)
			sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
		else sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	
		if (!qflag) printf("Request body length (0 unless testing piggybacking): ");
		(void) fscanf(ifd, "%lu", &Buff1->Header.BodyLength);
		if (!qflag && fflag) printf(" %ld\n", Buff1->Header.BodyLength);

		if (!qflag) printf("Local seek offset? (0): ");
		(void) fscanf(ifd, "%ld", &sed.Value.SmartFTPD.SeekOffset);
		if (!qflag && fflag) printf(" %ld\n", sed.Value.SmartFTPD.SeekOffset);

		if (!qflag) printf("Local byte quota? (-1): ");
		(void) fscanf(ifd, "%ld", &sed.Value.SmartFTPD.ByteQuota);
		if (!qflag && fflag) printf(" %ld\n", sed.Value.SmartFTPD.ByteQuota);

		if (!qflag) printf("Local file name ('-' for stdin/stdout, '/dev/mem' for VM file): ");
		(void) fscanf(ifd, "%s", sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName);
		if (!qflag && fflag) printf(" %s\n", sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName);

		if (strcmp(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, "-") == 0)
		    {
		    sed.Value.SmartFTPD.Tag = FILEBYFD;
		    sed.Value.SmartFTPD.FileInfo.ByFD.fd = (opcode == FETCHFILE) ? fileno(stdout) : 
								fileno(stdin);
		    
		    }

		if (strcmp(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, "/dev/mem") == 0)
		    {
		    sed.Value.SmartFTPD.Tag = FILEINVM;
		    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody= (RPC2_ByteSeq)VMFileBuf;
		    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = VMMaxFileSize;
		    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = VMCurrFileSize; /* ignored for fetch */
		    }
		


		/* Request packet contains: reply length, remote seek offset, remote byte quota, 
			hash mark, remote name*/
		if (!qflag) printf("Reply body length (0 unless testing piggybacking): ");
		(void) fscanf(ifd, "%ld", &tt);
		if (!qflag && fflag) printf(" %ld\n", tt);
		tt = (long) htonl((unsigned long)tt);
		memcpy(Buff1->Body, &tt, sizeof(long));

		if (!qflag) printf("Remote seek offset (0) : ");
		(void) fscanf(ifd, "%ld", &tt);
		if (!qflag && fflag) printf(" %ld\n", tt);
		tt = (long) htonl((unsigned long)tt);
		memcpy(Buff1->Body + sizeof(long), &tt, sizeof(long));

		if (!qflag) printf("Remote byte quota (-1): ");
		(void) fscanf(ifd, "%ld", &tt);
		if (!qflag && fflag) printf(" %ld\n", tt);
		tt = (long) htonl((unsigned long)tt);
		memcpy(Buff1->Body + 2*sizeof(long), &tt, sizeof(long));

		if (!qflag) printf("Remote file name ('-' for stdin/stdout, '/dev/mem' for VM file): ");
		(void) fscanf(ifd, "%s", (char *)Buff1->Body+1+3*sizeof(long));
		if (!qflag && fflag) printf(" %s\n", (char *)Buff1->Body+1+3*sizeof(long));
		Buff1->Header.BodyLength += 3*sizeof(long)+2+strlen((char *)(Buff1->Body+1+3*sizeof(long)));

		if (!qflag) printf("Hash mark: ");

		(void) fscanf(ifd, "%c", &sed.Value.SmartFTPD.hashmark);
		if (!qflag && fflag) printf(" %c\n", sed.Value.SmartFTPD.hashmark);
		if (sed.Value.SmartFTPD.hashmark == '0')
			sed.Value.SmartFTPD.hashmark = 0;
		*(Buff1->Body + 3*sizeof(long)) = sed.Value.SmartFTPD.hashmark;

                


		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
#ifdef PROFILE
		ProfilingOn();
#endif
		gettimeofday(&start, (struct timezone *)0);
		tt = RPC2_MakeRPC(cid, Buff1, &sed, &Buff2, (struct timeval *)NULL, (long) 0);
		if (bwflag) fclose(BW_f);
#ifdef PROFILE
		ProfilingOff();
#endif
		FT_GetTimeOfDay(&t2, NULL);


		if (tt != RPC2_SUCCESS)
		    {
		    WhatHappened(tt, "MakeRPC");
		    continue;
		    }
		tt = Buff2->Header.ReturnCode;
		if (tt == (long)SE_SUCCESS && sed.LocalStatus == SE_SUCCESS)
		    {
		    rpctime = ((t2.tv_sec - t1.tv_sec)*1000) + ((t2.tv_usec - t1.tv_usec)/1000);
		    printf("%ld bytes transferred in %ld milliseconds (%ld kbytes/second) \n", sed.Value.SmartFTPD.BytesTransferred,
		    	rpctime, sed.Value.SmartFTPD.BytesTransferred/rpctime);
		    printf("QuotaExceeded = %ld\n", sed.Value.SmartFTPD.QuotaExceeded);
		    if (opcode == (long)FETCHFILE &&
			    (sed.Value.SmartFTPD.Tag == FILEINVM))
			{
			VMCurrFileSize = sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
			printf("VMCurrFileSize = %ld\n", VMCurrFileSize);
			}
		    }
		else printf("RemoteStatus: %s\tLocalStatus: %s\n",
			SE_ErrorMsg(tt), SE_ErrorMsg((long) sed.LocalStatus));

	    break;

	    
	    case QUIT:
		goto Finish;
		
	    case ONEPING:
		printf("1 ping:\n");
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		Buff1->Header.BodyLength = 0;
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL, &Buff2, 
					       (struct timeval *)NULL, (long)0), "MakeRPC");
		FT_GetTimeOfDay(&t2, NULL);
		if (tt != RPC2_SUCCESS)
		    continue;
	    break;

	    case MANYPINGS:
		if (!qflag) printf("How many pings?: ");
		(void) fscanf(ifd, "%ld", &i);
		if (!qflag && fflag) printf(" %ld\n", i);
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
#ifdef PROFILE
		ProfilingOn();
#endif
		while(i--)
		    {
		    Buff1->Header.BodyLength = 0;
		    Buff1->Header.Opcode = opcode;
		    tt = RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL, &Buff2, 
				      (struct timeval *)NULL, (long)0);
		    if (tt != RPC2_SUCCESS)break;
		    assert(RPC2_FreeBuffer(&Buff2) == RPC2_SUCCESS);
		    }
#ifdef PROFILE
		ProfilingOff();
#endif
		if (tt != RPC2_SUCCESS)
		    {
		    WhatHappened(tt, "MakeRPC");
		    continue;
		    }
		FT_GetTimeOfDay(&t2, NULL);
	    break;

	    case LENGTHTEST:
		if (!qflag) printf("Length? ");
		(void) fscanf(ifd, "%ld", &tt);
		if (!qflag && fflag) printf(" %ld\n", tt);
		tt = (long) htonl((unsigned long)tt);
		memcpy(Buff1->Body, &tt, sizeof(long));
		tt = (long) ntohl((unsigned long)tt);
		Buff1->Header.BodyLength = sizeof(long) + tt;
		memcpy(Buff1->Body+sizeof(long), LongText, tt);
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL,  
					       &Buff2, (struct timeval *)NULL, 
					       (long) 0), "MakeRPC");
		FT_GetTimeOfDay(&t2, NULL);
		if (tt != RPC2_SUCCESS)continue;
	    break;

	    case DELACKTEST:
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		Buff1->Header.BodyLength = 0;
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL,  
					       &Buff2, (struct timeval *)NULL, 
					       (long) 0), "MakeRPC");
		FT_GetTimeOfDay(&t2, NULL);
		if (tt != RPC2_SUCCESS)continue;
	    break;


	    case REBIND:
		WhatHappened(RPC2_Unbind(cid), "Unbind");
                DoBinding(&cid);
		continue;
	    }
		

	if(Buff2 != NULL)
	    {
#ifdef RPC2DEBUG
	    if (RPC2_DebugLevel > 0) {
		rpc2_htonp(Buff2);  /* rpc2_PrintPacketHeader assumes net order */
		rpc2_PrintPacketHeader(Buff2, rpc2_tracefile);
		rpc2_ntohp(Buff2);
	    }
	    (void) fflush(rpc2_tracefile);
#endif
	    printf("Response Body: ``");
	    nextc = (char *)Buff2->Body;
	    for (i=0; i < Buff2->Header.BodyLength; i++)
		(void) putchar(*nextc++);
	    printf("''\n");
	    assert(RPC2_FreeBuffer(&Buff2) == RPC2_SUCCESS);
	    }

	rpctime = ((t2.tv_sec - t1.tv_sec)*1000) + ((t2.tv_usec - t1.tv_usec)/1000);

	printf("RPC2_MakeRPC(): %ld milliseconds elapsed time\n", rpctime);
    }
Finish:
    (void) RPC2_Unbind(cid);

#ifdef PROFILE
    DoneProfiling();
#endif
    return 0;

    }



void PrintStats()
{
    printf("RPC2:\n");
    printf("Packets Sent = %lu\tPacket Retries = %lu (of %lu)\tPackets Received = %lu\n",
	   rpc2_Sent.Total, rpc2_Sent.Retries, 
	   rpc2_Sent.Retries + rpc2_Sent.Cancelled, rpc2_Recvd.Total);
    printf("Bytes sent = %lu\tBytes received = %lu\n", rpc2_Sent.Bytes, rpc2_Recvd.Bytes);
    printf("Received Packet Distribution:\n");
    printf("\tRequests = %lu\tGoodRequests = %lu\n",
	   rpc2_Recvd.Requests, rpc2_Recvd.GoodRequests);
    printf("\tReplies = %lu\tGoodReplies = %lu\n",
	   rpc2_Recvd.Replies, rpc2_Recvd.GoodReplies);
    printf("\tBusies = %lu\tGoodBusies = %lu\n",
	    rpc2_Recvd.Busies, rpc2_Recvd.GoodBusies);
    printf("SFTP:\n");
    printf("Packets Sent = %lu\t\tStarts Sent = %lu\t\tDatas Sent = %lu\n",
	   sftp_Sent.Total, sftp_Sent.Starts, sftp_Sent.Datas);
    printf("Data Retries Sent = %lu\t\tAcks Sent = %lu\t\tNaks Sent = %lu\n",
	   sftp_Sent.DataRetries, sftp_Sent.Acks, sftp_Sent.Naks);
    printf("Busies Sent = %lu\t\t\tBytes Sent = %lu\n",
	   sftp_Sent.Busies, sftp_Sent.Bytes);
    printf("Packets Received = %lu\t\tStarts Received = %lu\tDatas Received = %lu\n",
	   sftp_Recvd.Total, sftp_Recvd.Starts, sftp_Recvd.Datas);
    printf("Data Retries Received = %lu\tAcks Received = %lu\tNaks Received = %lu\n",
	   sftp_Recvd.DataRetries, sftp_Recvd.Acks, sftp_Recvd.Naks);
    printf("Busies Received = %lu\t\tBytes Received = %lu\n",
	   sftp_Recvd.Busies, sftp_Recvd.Bytes);
    (void) fflush(stdout);
    }


void ClearStats()
    {
    memset(&rpc2_Sent, 0, sizeof(struct SStats));
    memset(&rpc2_Recvd, 0, sizeof(struct RStats));
    memset(&sftp_Sent, 0, sizeof(struct sftpStats));
    memset(&sftp_Recvd, 0, sizeof(struct sftpStats));
    }



void GetHost(RPC2_HostIdent *h)
{
    h->Tag = RPC2_HOSTBYNAME;
    if (!qflag) printf("Host name? ");
    (void) fscanf(ifd, "%s", h->Value.Name);
    if (!qflag && fflag) printf(" %s\n", h->Value.Name);
}

void GetPort(    RPC2_PortIdent *p)
    {
    long i;

    p->Tag = RPC2_PORTBYINETNUMBER;
    if (!qflag) printf("Port number? ");
    (void) fscanf(ifd, "%ld", &i);
    if (!qflag && fflag) printf(" %ld\n", i);
    p->Value.InetPortNumber = i;
    p->Value.InetPortNumber = htons(p->Value.InetPortNumber);
    }

void GetWho(RPC2_CountedBS *w, long s, RPC2_EncryptionKey e)
{
    if (s != RPC2_OPENKIMONO)
	{
	if (!qflag) printf("Identity? ");
	(void) fscanf(ifd, "%s", (char *)w->SeqBody);
	if (!qflag && fflag) printf(" %s\n", (char *)w->SeqBody);
	w->SeqLen = 1+strlen((char *)w->SeqBody);
	printf("Encryption Key (7 chars)? ");
	(void) sprintf((char *)e, "       ");
	(void) fscanf(ifd, "%s", (char *)e);
	}
    else
	{
	if (!qflag) printf("Identity [.]? ");
	(void) fscanf(ifd, "%s", (char *)w->SeqBody);
	if (!qflag && fflag) printf(" %s\n", (char *)w->SeqBody);
	w->SeqLen = 1+strlen((char *)w->SeqBody);
	}
    }


void PrintHelp(void)
{
    int i, n;
    n = sizeof(Opnames)/sizeof(char *);
    
    for (i = 0; i < n; i++) printf("%s = %d   ", Opnames[i], i);
    printf("\n");
}


void DoBinding(RPC2_Handle *cid)
{
    RPC2_HostIdent hid;
    RPC2_PortIdent sid;
    RPC2_SubsysIdent ssid;
    RPC2_BindParms bparms;
    RPC2_EncryptionKey ekey;
    RPC2_CountedBS who;
    char whobuff[100];

    hid.Tag = RPC2_HOSTBYNAME;
    if (!qflag) printf("Host? ");
    (void) fscanf(ifd, "%s", hid.Value.Name);
    if (!qflag && fflag) printf(" %s\n", hid.Value.Name);

    GetPort(&sid);
    ssid.Tag = RPC2_SUBSYSBYID;
    ssid.Value.SubsysId = SUBSYS_SRV;

    if (!qflag) printf("Side Effect Type (%d or %d)? ", 0, SMARTFTP);
    (void) fscanf(ifd, "%ld", &bparms.SideEffectType);
    if (!qflag && fflag) printf(" %ld\n", bparms.SideEffectType);

    if (!qflag) printf("Security [98(OK), 12(AO), 73(HO), 66(S)]? ");
    (void) fscanf(ifd, "%ld", &bparms.SecurityLevel);
    if (!qflag && fflag) printf(" %ld\n", bparms.SecurityLevel);

    who.SeqLen = 0;
    who.SeqBody = (RPC2_Byte *)whobuff;
    GetWho(&who, bparms.SecurityLevel, ekey);


    bparms.ClientIdent = &who;
    if (bparms.SecurityLevel == RPC2_OPENKIMONO)
	{
	if (strcmp((char *)who.SeqBody, ".") == 0)
	    bparms.ClientIdent = NULL;
	}
    else
	{
	bparms.EncryptionType = RPC2_XOR;
	bparms.SharedSecret = (RPC2_EncryptionKey *)ekey;
	}

    bparms.AuthenticationType = 0;	/* server doesn't care */

    if (WhatHappened(RPC2_NewBinding(&hid, &sid, &ssid, &bparms, cid),
            "NewBinding") < RPC2_ELIMIT)
	exit(-1);

    }

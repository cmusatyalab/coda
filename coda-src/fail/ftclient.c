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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/fail/ftclient.c,v 1.1.1.1 1996/11/22 19:09:20 rvb Exp";
#endif /*_BLURB_*/





/* 
 * client to do file transfers.
 * uses speed part of failure package.
 * -- L. Mummert 3/92
 */
#include <stdio.h>
#include <libc.h>
#include <strings.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <lwp.h>
#include <rpc2.h>
#include <se.h>
#include <ci.h>
#include "ft.h"
#include "fail.h"

#define DEFAULT_HOST "copland"

RPC2_HostIdent serverHost; 
RPC2_Handle cid;

int InitRPC();
int NewConn();
void SubTime();
void PrintStat();

GetFile(), PutFile(), LoadFile(), StoreFile(), RPC2Stats(), 
	SetBufferLength(), Quit();
extern long RPC2_DebugLevel;

int sftp_windowsize, sftp_sendahead, sftp_ackpoint, sftp_packetsize;

char *bufPtr = NULL;
long buflen = 0; 
FILE *cmdFile = NULL;

CIENTRY list[] = {
    CICMD ("buf", SetBufferLength),
    CICMD ("get", GetFile),
    CICMD ("load", LoadFile),
    CICMD ("put", PutFile),
    CICMD ("quit", Quit),
    CILONG ("RPC2_DebugLevel", RPC2_DebugLevel),
    CICMD ("stats", RPC2Stats),
    CICMD ("store", StoreFile),
    CIEND
};

int FTLWP();

main(argc, argv)
int argc;
char *argv[];
{
	int i;
	extern int optind;
	extern char *optarg;
	PROCESS mypid;

	serverHost.Tag = RPC2_HOSTBYNAME;
	(void) strcpy(serverHost.Value.Name, DEFAULT_HOST);

	while ((i = getopt(argc, argv, "a:f:h:p:s:w:")) != EOF)
		switch (i) {
		case 'a': 
			sftp_ackpoint = atoi(optarg);
			break;
		case 'f':
			if ((cmdFile = fopen(optarg, "r")) == NULL) {
				printf("Can't open %s\n", optarg);
				exit(1);
			}
			break;
		case 'h':
			(void) strcpy(serverHost.Value.Name, optarg);
			break;
		case 'p':
			sftp_packetsize = atoi(optarg);
			break;
		case 's':
			sftp_sendahead = atoi(optarg);
			break;
		case 'w':
			sftp_windowsize = atoi(optarg);
			break;
		default:
			printf("ftclient [-a ackpoint] [-f cmdfile] [-h host] [-p packetsize] [-s sendahead] [-w windowsize]\n");
			exit(1);
		}

	InitRPC();
	Fail_Initialize("ftclient", 0);
	Fcon_Init();

	NewConn();

	LWP_CreateProcess((PFIC) FTLWP, 16384, LWP_NORMAL_PRIORITY, "FTLWP", NULL, &mypid);
	LWP_WaitProcess((char *)main);
}

FTLWP() 
{
	ci("FT>", cmdFile, 0, list, NULL, NULL);
}

NewConn()
{
	int rc;
	RPC2_PortalIdent pident;
	RPC2_SubsysIdent sident;
	RPC2_BindParms bind_parms;

	sident.Value.SubsysId = FTSUBSYSID;
	sident.Tag = RPC2_SUBSYSBYID;
	pident.Tag = RPC2_PORTALBYINETNUMBER;
	pident.Value.InetPortNumber = htons(FTPORTAL);
	bind_parms.SecurityLevel = RPC2_OPENKIMONO;
	bind_parms.EncryptionType = NULL;
	bind_parms.SideEffectType = SMARTFTP;
	bind_parms.ClientIdent = NULL;
	bind_parms.SharedSecret = NULL;
	printf("Connecting to host %s...", serverHost.Value.Name);
	fflush(stdout);

	rc = RPC2_NewBinding(&serverHost, &pident, &sident, &bind_parms, &cid);
	if (rc != RPC2_SUCCESS)
		printf("bind failed --> %s\n", RPC2_ErrorMsg(rc));
	else 
		printf("succeeded\n");

	return(rc);
}

/* GetFile local-file remote-file */
GetFile(arglist)
char *arglist;
{
	SE_Descriptor sed;
	struct timeval startTime, endTime;
	int rc;
	char *p, *name, nameBuffer[MAXPATHLEN];

	p = arglist;
	name = strarg(&p, " ", "local file name?", "/dev/mem", nameBuffer);

	bzero(&sed, sizeof(sed));
	sed.Tag = SMARTFTP;
	sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sed.Value.SmartFTPD.ByteQuota = -1;

	if (strcmp(name, "/dev/mem") == 0) {
		if (bufPtr == NULL) {
			printf("Set the buffer length first!");
			return(0);
		}
		sed.Value.SmartFTPD.Tag = FILEINVM;   
		sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq) bufPtr;
		sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = FTMAXSEQLEN;
	} else {
		sed.Value.SmartFTPD.Tag = FILEBYNAME;   
		sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
		strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, name);
	}

	name = strarg(&p, " ", "remote file name?", "/dev/mem", nameBuffer);

	(void) gettimeofday(&startTime, 0);
	rc = FTGet(cid, name, &sed);
	(void) gettimeofday(&endTime, 0);

	if (rc != RPC2_SUCCESS) {
		printf("FTGet failed --> %s\n", RPC2_ErrorMsg(rc));
		return(rc);
	}
	PrintStat(sed.Value.SmartFTPD.BytesTransferred, &startTime, &endTime);

	if (sed.Value.SmartFTPD.Tag == FILEINVM) 
		buflen = sed.Value.SmartFTPD.BytesTransferred;

	return(0);
}

/* PutFile local-file remote-file */
PutFile(arglist)
char *arglist;
{
	SE_Descriptor sed;
	struct timeval startTime, endTime;
	int rc;
	char *p, *name, nameBuffer[MAXPATHLEN];

	p = arglist;
	name = strarg(&p, " ", "local file name?", "/dev/mem", nameBuffer);

	bzero(&sed, sizeof(sed));
	sed.Tag = SMARTFTP;
	sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sed.Value.SmartFTPD.ByteQuota = -1;

	if (strcmp(name, "/dev/mem") == 0) {
		if (bufPtr == NULL) {
			printf("Set the buffer length first!");
			return(0);
		}
		sed.Value.SmartFTPD.Tag = FILEINVM;   
		sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq) bufPtr;
		sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = buflen;
	} else {
		sed.Value.SmartFTPD.Tag = FILEBYNAME;   
		strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, name);
	}

	name = strarg(&p, " ", "remote file name?", "/dev/mem", nameBuffer);

	(void) gettimeofday(&startTime, 0);
	rc = FTPut(cid, name, &sed);
	(void) gettimeofday(&endTime, 0);

	if (rc != RPC2_SUCCESS) {
		printf("FTPut failed --> %s\n", RPC2_ErrorMsg(rc));
		return(rc);
	}
	PrintStat(sed.Value.SmartFTPD.BytesTransferred, &startTime, &endTime);

	return(0);
}

/* LoadFile local-file */
LoadFile(arglist)
char *arglist;
{
	/* loads a file into memory */
	
	return(0);
}

/* StoreFile local-file */
StoreFile(arglist)
char *arglist;
{
	/* stores a file from mem to disk */
	
	return(0);
}

SetBufferLength(arglist)
char *arglist;
{
	char *p;

	p = arglist;
	buflen = intarg(&p, " ", "buffer size?", 0, FTMAXSEQLEN, FTMAXSEQLEN);

	if (bufPtr) free(bufPtr);
	bufPtr = (char *) malloc(buflen);
	if (bufPtr == NULL) {
		printf("can't alloc %d bytes\n", buflen);
		exit(1);
	}
	return(0);
}


Quit()
{
	extern int ciexit;

	RPC2_Unbind(cid);
	if (cmdFile) fclose(cmdFile);
	LWP_TerminateProcessSupport();
	ciexit = 1;
}

RPC2Stats()
{
    printf("RPC2:\n");
    printf("Packets Sent = %ld\tPacket Retries = %ld\tPackets Received = %ld\n",
	   rpc2_Sent.Total, rpc2_Sent.Retries, 
	   rpc2_Sent.Retries, rpc2_Recvd.Total);
    printf("\t%Multicasts = %ld\tBusies Sent = %ld\tNaks Sent = %ld\n", 
	   rpc2_Sent.Multicasts, rpc2_Sent.Busies, rpc2_Sent.Naks);
    printf("Bytes sent = %ld\tBytes received = %ld\n", rpc2_Sent.Bytes, rpc2_Recvd.Bytes);
    printf("Received Packet Distribution:\n");
    printf("\tRequests = %ld\tGoodRequests = %ld\n",
	   rpc2_Recvd.Requests, rpc2_Recvd.GoodRequests);
    printf("\tReplies = %ld\tGoodReplies = %ld\n",
	   rpc2_Recvd.Replies, rpc2_Recvd.GoodReplies);
    printf("\tBusies = %ld\tGoodBusies = %ld\n",
	   rpc2_Recvd.Busies, rpc2_Recvd.GoodBusies);
    printf("\tMulticasts = %ld\tGoodMulticasts = %ld\n",
	   rpc2_Recvd.Multicasts, rpc2_Recvd.GoodMulticasts);
    printf("\tBogus packets = %ld\n\tNaks = %ld\n",
	   rpc2_Recvd.Bogus, rpc2_Recvd.Naks);
	  
    printf("\nSFTP:\n");
    printf("Packets Sent = %ld\t\tStarts Sent = %ld\t\tDatas Sent = %ld\n",
	   sftp_Sent.Total, sftp_Sent.Starts, sftp_Sent.Datas);
    printf("Data Retries Sent = %ld\t\tAcks Sent = %ld\t\tNaks Sent = %ld\n",
	   sftp_Sent.DataRetries, sftp_Sent.Acks, sftp_Sent.Naks);
    printf("Busies Sent = %ld\t\t\tBytes Sent = %ld\n",
	   sftp_Sent.Busies, sftp_Sent.Bytes);
    printf("Packets Received = %ld\t\tStarts Received = %ld\tDatas Received = %ld\n",
	   sftp_Recvd.Total, sftp_Recvd.Starts, sftp_Recvd.Datas);
    printf("Data Retries Received = %ld\tAcks Received = %ld\tNaks Received = %ld\n",
	   sftp_Recvd.DataRetries, sftp_Recvd.Acks, sftp_Recvd.Naks);
    printf("Busies Received = %ld\t\tBytes Received = %ld\n",
	   sftp_Recvd.Busies, sftp_Recvd.Bytes);

    return(0);
}

void PrintStat(bytes, startp, endp)
long bytes;
struct timeval *startp, *endp;
{
	float sec, bsec;

	SubTime(endp, startp);
	sec = endp->tv_sec + endp->tv_usec/1000000.0;
	if (sec == 0) /* ha */ bsec = bytes*8;
	else bsec = bytes*8/sec;
	printf("Transferred %ld bytes in %.2g seconds (%.2g bps)\n",
	       bytes, sec, bsec);
}

void SubTime(fromp, amtp)
struct timeval *fromp, *amtp;
{
	if (amtp->tv_usec > fromp->tv_usec) {
		fromp->tv_sec--;
		fromp->tv_usec += 1000000;
	}
	fromp->tv_sec -= amtp->tv_sec;
	fromp->tv_usec -= amtp->tv_usec;
}

InitRPC()
{
	int mylpid, rc;
	SFTP_Initializer sftpi;
	RPC2_PortalIdent portalid, *portallist[1];

	assert(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) == LWP_SUCCESS);

	SFTP_SetDefaults(&sftpi);
	if (sftp_ackpoint) sftpi.AckPoint = sftp_ackpoint;
	if (sftp_packetsize) sftpi.PacketSize = sftp_packetsize;
	if (sftp_sendahead) sftpi.SendAhead = sftp_sendahead;
	if (sftp_windowsize) sftpi.WindowSize = sftp_windowsize;
	SFTP_Activate(&sftpi);

	portalid.Tag = RPC2_PORTALBYINETNUMBER;
	portalid.Value.InetPortNumber = htons(FTPORTAL);
	portallist[0] = &portalid;
	rc = RPC2_Init(RPC2_VERSION, 0, portallist, 1,  -1, NULL);
	if (rc == RPC2_SUCCESS) return;
	printf("agent: RPC2_Init() --> %s\n", RPC2_ErrorMsg(rc));
	if (rc < RPC2_ELIMIT) exit(-1);
}

iopen(int dummy1, int dummy2, int dummy3){}

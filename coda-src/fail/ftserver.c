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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/fail/ftserver.c,v 4.1 1997/01/08 21:49:38 rvb Exp $";
#endif /*_BLURB_*/





/* 
 * server to do file transfers.
 * uses speed part of failure package.
 * -- L. Mummert 3/92
 */
#include <stdio.h>
#include <strings.h>
#include "coda_assert.h"
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <lwp.h>
#include <rpc2.h>
#include <se.h>
#include "ft.h"
#include "fail.h"

extern long RPC2_DebugLevel;
extern RPC2_PortalIdent sftp_Portal;

char *bufPtr = NULL;
long length = 0;  /* length of data in buffer ptd to by bufPtr. */ 
		  /* set on vm transfers in FTPut, used in FTGet */

int FTLWP();
void RPC2Stats();

main(argc, argv)
int argc;
char *argv[];
{
	extern int optind;
	extern char *optarg;
	int i;
	PROCESS mypid;

	while ((i = getopt(argc, argv, "d:")) != EOF)
		switch (i) {
		case 'd':
			RPC2_DebugLevel = atoi(optarg);
			break;
		default:
			printf("ftserver [-d RPC debug]\n");
			exit(1);
		}

	bufPtr = (char *) malloc(FTMAXSEQLEN);
	if (bufPtr == NULL) return(RPC2_FAIL);

	InitRPC();
	Fail_Initialize("ftserver", 0);
	Fcon_Init();

	signal(SIGURG, RPC2Stats);

	LWP_CreateProcess((PFIC) FTLWP, 16384, LWP_NORMAL_PRIORITY, "FTLWP", "FTLWP", &mypid);
	LWP_WaitProcess((char *)main);
}

FTLWP()
{
	RPC2_Handle cid;
	RPC2_RequestFilter reqfilter;
	RPC2_PacketBuffer *reqbuffer;
	int rc;

	/* Set filter  to accept trace requests on new or existing connections */
	reqfilter.FromWhom = ONESUBSYS;
	reqfilter.OldOrNew = OLDORNEW;
	reqfilter.ConnOrSubsys.SubsysId = FTSUBSYSID;
    
	while(1) {
		rc = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, 
				     NULL, NULL, NULL, NULL);
		if (rc != RPC2_SUCCESS)
			printf("GetRequest failed, rc %s\n", RPC2_ErrorMsg(rc));

		rc = ft_ExecuteRequest(cid, reqbuffer);
		if (rc != RPC2_SUCCESS)
			printf("ExecuteRequest failed, rc %s\n", RPC2_ErrorMsg(rc));
	}
}

long FTGet(cid, name)
RPC2_Handle cid;
char *name;
{
	SE_Descriptor sed;
	int rc;

	bzero(&sed, sizeof(sed));
	sed.Tag = SMARTFTP;
	sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sed.Value.SmartFTPD.ByteQuota= -1;

	if (strcmp(name, "/dev/mem") == 0) {
		sed.Value.SmartFTPD.Tag = FILEINVM; 
		sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq) bufPtr;
		sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = length;
	} else {
		sed.Value.SmartFTPD.Tag = FILEBYNAME; 
		strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, name);
	}

	if (RPC2_InitSideEffect(cid, &sed) != RPC2_SUCCESS) 
		return(RPC2_FAIL);
	if (RPC2_CheckSideEffect(cid, &sed, SE_AWAITLOCALSTATUS) != RPC2_SUCCESS)
		return(RPC2_FAIL);
	return(RPC2_SUCCESS);
}

long FTPut(cid, name)
RPC2_Handle cid;
char *name;
{
	SE_Descriptor sed;
	int rc;

	bzero(&sed, sizeof(sed));
	sed.Tag = SMARTFTP;
	sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sed.Value.SmartFTPD.ByteQuota= -1;

	if (strcmp(name, "/dev/mem") == 0) {
		bufPtr = (char *) malloc(FTMAXSEQLEN);
		if (bufPtr == NULL) return(RPC2_FAIL);
	
		sed.Value.SmartFTPD.Tag = FILEINVM; 
		sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq) bufPtr;
		sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = FTMAXSEQLEN;
	} else {
		sed.Value.SmartFTPD.Tag = FILEBYNAME; 
		sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
		strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, name);
	}
	if (RPC2_InitSideEffect(cid, &sed) != RPC2_SUCCESS) 
		return(RPC2_FAIL);
	if (RPC2_CheckSideEffect(cid, &sed, SE_AWAITLOCALSTATUS) != RPC2_SUCCESS)
		return(RPC2_FAIL);

	if (sed.Value.SmartFTPD.Tag == FILEINVM) 
		length = sed.Value.SmartFTPD.BytesTransferred;

	return(RPC2_SUCCESS);
}

void RPC2Stats()
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
}

InitRPC()
{
	int mylpid = -1, rc;
	SFTP_Initializer sftpi;
	RPC2_PortalIdent portalid, *portallist[1];
	RPC2_SubsysIdent subsysid;

	CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) == LWP_SUCCESS);

	portalid.Tag = RPC2_PORTALBYINETNUMBER;
	portalid.Value.InetPortNumber = htons(FTPORTAL);
	portallist[0] = &portalid;
	SFTP_SetDefaults(&sftpi);
	SFTP_Activate(&sftpi);
	rc = RPC2_Init(RPC2_VERSION, 0, portallist, 1, -1, NULL);
	if (rc != RPC2_SUCCESS)
	{
		printf("RPC2_Init() --> %s\n", RPC2_ErrorMsg(rc));
		if (rc < RPC2_ELIMIT) exit(-1);
	}
	subsysid.Tag = RPC2_SUBSYSBYID;
	subsysid.Value.SubsysId = FTSUBSYSID;
	CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);
}

iopen(int dummy1, int dummy2, int dummy3){}

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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/fail/slow.c,v 1.1 1996/11/22 19:09:23 braam Exp $";
#endif /*_BLURB_*/





#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include <del.h>
#include "fail.h"

extern void ntohFF(FailFilter *);
extern void htonFF(FailFilter *);
void PrintError();
void PrintUsage();

#define MAXHOSTNAMELEN 32
char *host1 = NULL;
char *host2 = NULL;
short port1 = 0;
short port2 = 0;
int speed1 = MAXNETSPEED;
int speed2 = MAXNETSPEED;

main(argc, argv)
int argc;
char **argv;
{
	int i;
	unsigned long cid1, cid2;
	struct hostent *he1;
	struct hostent *he2;
	FailFilter filter;
	int rc;


	ParseArgs(argc, argv);
	InitRPC();

	he1 = gethostbyname(host1);
	if ( he1 == NULL) {
		printf("invalid host %s\n", host1);
		exit(-1);
	}
	he2 = gethostbyname(host2);
	if ( he2 == NULL) {
		printf("invalid host %s\n", host2);
		exit(-1);
	}

	/* bind to each host */
	printf("Trying to bind to %s on port %d...\n", host1, port1);
	rc = NewConn(host1, port1, &cid1);
	if (rc != RPC2_SUCCESS) {
		PrintError("Can't bind", rc);
		exit(-1);
	}
	printf("Bind Succeded \n");
	RPC2_SetColor(cid1, FAIL_IMMUNECOLOR);

	printf("Trying to bind to %s on port %d...\n", host2, port2);
	rc = NewConn(host2, port2, &cid2);
	if (rc != RPC2_SUCCESS) {
		PrintError("Can't bind", rc);
		exit(-1);
	}
	printf("Bind Succeded \n");
	RPC2_SetColor(cid2, FAIL_IMMUNECOLOR);

	he1 = gethostbyname(host1);
	assert(he1 != NULL);
	filter.ip1 = ((unsigned char *)he1->h_addr)[0];
	filter.ip2 = ((unsigned char *)he1->h_addr)[1];
	filter.ip3 = ((unsigned char *)he1->h_addr)[2];
	filter.ip4 = ((unsigned char *)he1->h_addr)[3];
	filter.color = -1;
	filter.lenmin = 0;
	filter.lenmax = 65535;
	filter.factor = 10000;
	filter.speed = speed1;

	/* insert a filter on send side of host 2 */
	if ((rc = InsertFilter(cid2, sendSide, 0, &filter)) < 0) {
		PrintError("Couldn't insert filter", rc);
	} else {
		printf("Inserted filter on host %s with speed %d\n",
		       host2, speed1);
	}
	/* insert a filter on recv side of host 2 */
	filter.speed = 10000000;  
	if ((rc = InsertFilter(cid2, recvSide, 0, &filter)) < 0) {
		PrintError("Couldn't insert filter", rc);
	} else {
		printf("Inserted filter on host %s with speed %d\n",
		       host2, speed1);
	}

	he2 = gethostbyname(host2);
	assert(he2 != NULL);
	filter.ip1 = ((unsigned char *)he2->h_addr)[0];
	filter.ip2 = ((unsigned char *)he2->h_addr)[1];
	filter.ip3 = ((unsigned char *)he2->h_addr)[2];
	filter.ip4 = ((unsigned char *)he2->h_addr)[3];
	filter.speed = speed2;

	/* insert a filter on send side of host 1 */
	if ((rc = InsertFilter(cid1, sendSide, 0, &filter)) < 0) {
		PrintError("Couldn't insert filter", rc);
	} else {
		printf("Inserted filter on host %s with speed %d\n",
		       host1, speed2);
	}

	/* insert a filter on recv side of host 1 */
	filter.speed = 10000000;
	if ((rc = InsertFilter(cid1, recvSide, 0, &filter)) < 0) {
		PrintError("Couldn't insert filter", rc);
	} else {
		printf("Inserted filter on host %s with speed %d\n",
		       host1, speed2);
	}

	RPC2_Unbind(cid1);
	RPC2_Unbind(cid2);
}


ParseArgs(argc, argv) 
int argc;
char **argv;
{
    int i;
    if (argc != 9)
	    PrintUsage();

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-h") == 0) {
	    if (!host1) {
		host1 = argv[i+1];
		sscanf(argv[i+2], "%hd", &port1);
		sscanf(argv[i+3], "%d", &speed1);
		i = i + 3;
	    }
	    else if (!host2) {
		host2 = argv[i+1];
		sscanf(argv[i+2], "%hd", &port2);
		sscanf(argv[i+3], "%d", &speed2);
		i = i + 3;
	    }
	    else
		    PrintUsage();
	}
    }
}
		
InitRPC()
{
	PROCESS mylpid;
	int rc;

	assert(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) == LWP_SUCCESS);

	rc = RPC2_Init(RPC2_VERSION, 0, NULL, 0,  -1, NULL);
	if (rc == RPC2_SUCCESS) return;
	PrintError("InitRPC", rc);
	if (rc < RPC2_ELIMIT) exit(-1);
}

NewConn(hostname, port, cid)
char *hostname;
short port;
unsigned long *cid;
{
	int rc;
	RPC2_HostIdent hident;
	RPC2_PortalIdent pident;
	RPC2_SubsysIdent sident;
	RPC2_BindParms bparms;

	hident.Tag = RPC2_HOSTBYNAME;
	strcpy(hident.Value.Name, hostname);
    
	sident.Value.SubsysId = FCONSUBSYSID;
	sident.Tag = RPC2_SUBSYSBYID;

	pident.Tag = RPC2_PORTALBYINETNUMBER;
	pident.Value.InetPortNumber = htons(port);

	bparms.SecurityLevel = RPC2_OPENKIMONO;
	bparms.SharedSecret = NULL;
	bparms.ClientIdent = NULL;
	bparms.SideEffectType = 0;
	bparms.Color = FAIL_IMMUNECOLOR;

	rc = RPC2_NewBinding(&hident, &pident, &sident, &bparms, cid);

	return rc;
}

void PrintError(msg, err)
char *msg;
int err;
{
	extern int errno;
    
	if (err == 0) perror(msg);
	else printf("%s: %s\n", msg, RPC2_ErrorMsg(err));
}

void PrintUsage()
{
	printf("Usage: slow -h hostname port speed -h hostname port speed\n");
	exit(-1);
}

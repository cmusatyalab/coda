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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/fail/Attic/partition.c,v 4.1 1997/01/08 21:49:38 rvb Exp $";
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

#define MAXHOSTNAMELEN 32
char *host1 = NULL;
char *host2 = NULL;
short port1 = 0;
short port2 = 0;

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
    filter.factor = 0;
    
	
    /* insert a filter on the recv  side of host 2 */
    if ((rc = InsertFilter(cid2, recvSide, 0, &filter)) < 0) {
	PrintError("Couldn't insert filter", rc);
    }
    /* insert a filter on send side of host 2 */
    if ((rc = InsertFilter(cid2, sendSide, 0, &filter)) < 0) {
	PrintError("Couldn't insert filter", rc);
    }
    he2 = gethostbyname(host2);
    assert(he2 != NULL);
    filter.ip1 = ((unsigned char *)he2->h_addr)[0];
    filter.ip2 = ((unsigned char *)he2->h_addr)[1];
    filter.ip3 = ((unsigned char *)he2->h_addr)[2];
    filter.ip4 = ((unsigned char *)he2->h_addr)[3];
    if ((rc = InsertFilter(cid1, recvSide, 0, &filter)) < 0) {
	PrintError("Couldn't insert filter", rc);
    }
    /* insert a filter on send side of host 2 */
    if ((rc = InsertFilter(cid1, sendSide, 0, &filter)) < 0) {
	PrintError("Couldn't insert filter", rc);
    }
    
    RPC2_Unbind(cid1);
    RPC2_Unbind(cid2);
}


ParseArgs(argc, argv) 
int argc;
char **argv;
{
    int i;
    if (argc != 7) {
	printf("Usage: partition -h hostname port -h hostname port\n");
	exit(-1);
    }
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-h") == 0) {
	    if (!host1) {
		host1 = argv[i+1];
		sscanf(argv[i+2], "%hd", &port1);
		i = i + 2;
	    }
	    else if (!host2) {
		host2 = argv[i+1];
		sscanf(argv[i+2], "%hd", &port2);
		i = i + 2;
	    }
	    else {
		printf("Usage: partition -h hostname port -h hostname port\n");
		exit(-1);
	    }
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


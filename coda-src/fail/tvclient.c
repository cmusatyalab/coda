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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/fail/tvclient.c,v 1.1 1996/11/22 19:09:24 braam Exp $";
#endif /*_BLURB_*/









/*
   Test client for Toy Venus

   Walter Smith
   26 October 1987
 */

#include <stdio.h>
#include <assert.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <lwp.h>
#include <rpc2.h>
#include "tv.h"

extern int errno;

main()
{
    char buf[128];
    int cid, err, delay, pos;
    RPC2_CountedBS cbs;
    
    printf("Debug Level? [0] ");
    gets(buf);
    RPC2_DebugLevel = atoi(buf);

    InitRPC();
    Fail_Initialize("tvclient", 0);
    Fcon_Init();

    NewConn(&cid);

    if ((errno = Open(cid, "/etc/passwd", "r")) != TV_SUCCESS) {
	printerror("Open failed");
	exit(1);
    }

    while (1) {
	pos = URand(0, 1000);
	printf("Seek %d, ", pos);
	if ((errno = Seek(cid, pos)) != TV_SUCCESS) {
	    printerror("Seek failed");
	}
	cbs.SeqLen = URand(4, 40);
	cbs.SeqBody = (RPC2_ByteSeq) buf;
	printf("Read %d : ", cbs.SeqLen);
	if ((errno = Read(cid, &cbs)) != TV_SUCCESS) {
	    printerror("Read failed");
	}
	printf("%.*s\n", cbs.SeqLen, cbs.SeqBody);
	delay = URand(0, 5);
	sleep(delay);
    }
}

/* Return a random integer between a and b */
int URand(a, b)
int a, b;
{
    return a + ((random() & ~(1<<31)) % (b - a));
}

/* Like perror, but handle RPC2 errors also */
printerror(msg)
char *msg;
{
    extern int errno;

    if (errno > 0) perror(msg);
    else printf("%s: %s\n", msg, RPC2_ErrorMsg(errno));
}

NewConn(cid)
unsigned long *cid;
{
    char hname[100], buf[100];
    int newcid, rc;
    RPC2_HostIdent hident;
    RPC2_PortalIdent pident;
    RPC2_SubsysIdent sident;

    GetHost(&hident);
    sident.Value.SubsysId = TVSUBSYSID;
    
    sident.Tag = RPC2_SUBSYSBYID;
    pident.Tag = RPC2_PORTALBYINETNUMBER;
    pident.Value.InetPortNumber = htons(TVPORTAL);
    rc = RPC2_Bind(RPC2_OPENKIMONO, NULL, &hident, &pident, &sident, 
    					NULL, NULL, NULL, cid);
    if (rc == RPC2_SUCCESS)
	printf("Binding succeeded, this connection id is %d\n", *cid);
    else
	printf("Binding failed: %s\n", RPC2_ErrorMsg(rc));
}

/* RPC2 stuff */

InitRPC()
{
    PROCESS mylpid;
    int rc;
    RPC2_PortalIdent portalid, *portallist[1];
    RPC2_SubsysIdent subsysid;
    struct timeval tout;

    assert(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) == LWP_SUCCESS);

    /* We have to have a port to use fcon */

    portalid.Tag = RPC2_PORTALBYINETNUMBER;
    portalid.Value.InetPortNumber = htons(TVPORTAL);
    portallist[0] = &portalid;
    rc = RPC2_Init(RPC2_VERSION, 0, portallist, 1, -1, NULL);
    if (rc != RPC2_SUCCESS) {
	printf("RPC2_Init() --> %s\n", RPC2_ErrorMsg(rc));
	if (rc < RPC2_ELIMIT) exit(-1);
    }
}

iopen(int dummy1, int dummy2, int dummy3) {/* fake ITC system call */} 

GetHost(h)
    RPC2_HostIdent *h;
    {
    h->Tag = RPC2_HOSTBYNAME;
    printf("Host? ");
    gets(h->Value.Name);
    }

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
                           none currently

#*/

/*
   Test client for Toy Venus

   Walter Smith
   26 October 1987
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
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

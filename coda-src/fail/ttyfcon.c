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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/fail/Attic/ttyfcon.c,v 4.2 1997/09/23 17:41:51 braam Exp $";
#endif /*_BLURB_*/








/*
  Network failure emulation package

  Dumb (really dumb!) TTY control interface for fcon
      
  Walter Smith
 */

#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ci.h>
#include <del.h>
#include <netinet/in.h>
#include "fail.h"

extern void ntohFF(FailFilter *);
extern void htonFF(FailFilter *);

#define MAXHOSTNAMELEN 32
typedef struct conninfo {
    int cid;			/* Connection ID */
    char hostname[MAXHOSTNAMELEN];
    int port;
    char clientName[MAXNAMELEN];
    int clientNumber;
    struct conninfo *next;
} ConnInfo;

int numConns;
ConnInfo *conns;

/* CMU Command Interpreter stuff */

int AddClient(), DeleteClient(), ListClients(), SaveClients(),
    cmdInsertFilter(), cmdRemoveFilter(), cmdReplaceFilter(),
    cmdGetFilters(), cmdPurgeFilters(), Status(), Quit();

extern long RPC2_DebugLevel;	/* secret! */

CIENTRY list[] = {
    CICMD ("addclient", AddClient),
    CICMD ("deleteclient", DeleteClient),
    CICMD ("listclients", ListClients),
    CICMD ("saveclients", SaveClients),
    CICMD ("insertfilter", cmdInsertFilter),
    CICMD ("removefilter", cmdRemoveFilter),
#if 0
    CICMD ("replacefilter", cmdReplaceFilter),
#endif
    CICMD ("getfilters", cmdGetFilters),
    CICMD ("purgeFilters", cmdPurgeFilters),
    CICMD ("status", Status),
    CICMD ("quit", Quit),
    CILONG ("RPC2_DebugLevel", RPC2_DebugLevel),
    CIEND
};

int maxFilterID = 999;

iopen(int dummy1, int dummy2, int dummy3) {/* fake ITC system call */} 

main()
{
    printf("TTY fcon\n");
    InitRPC();
    ci("fcon>", NULL, 0, list, NULL, NULL);
}

#define MAXARGS		16

/* for convenience, since practically everything uses them */
int argc;
char *argv[MAXARGS];

int BreakupArgs(args, argv)
char *args;
char **argv;
{
    register char *p = args;
    int i = 0;

    while (*p && isspace(*p)) p++;
    if (!*p) return 0;
    while (*p && i < MAXARGS) {
	argv[i++] = p;
	while (*p && !isspace(*p)) p++;
	if (*p) *p++ = 0;
    }
    return i;
}
	

/* addclient <host> <port> */

AddClient(args)
char *args;
{
    char hostname[128];
    short port;
    unsigned long cid;
    ConnInfo *info;
    RPC2_BoundedBS name;
    int rc;
    int count;
    ConnInfo *tmp, *lasttmp;

    if (!*args) {
	gethostname(hostname, 128);
	getstr("Host", hostname, hostname);
	port = getshort("Port", 0, 32767, 0);
    }
    else {
	argc = BreakupArgs(args, argv);
	if (argc != 2) {
	    printf("addclient <host> <port>\n");
	    return;
	}
	strcpy(hostname, argv[0]);
	port = atoi(argv[1]);
    }

    printf("Trying to bind to %s on port %d...\n", hostname, port);

    rc = NewConn(hostname, port, &cid);
    if (rc != RPC2_SUCCESS) {
	PrintError("Can't bind", rc);
	return;
    }
    RPC2_SetColor(cid, FAIL_IMMUNECOLOR);

    printf("Succeeded.\n");

    info = (ConnInfo *) malloc(sizeof(ConnInfo));
    info->cid = cid;
    strncpy(info->hostname, hostname, MAXHOSTNAMELEN);
    info->port = port;
    name.MaxSeqLen = MAXNAMELEN;
    name.SeqLen = 1;
    name.SeqBody = (RPC2_ByteSeq) info->clientName;
    if (rc = /*Fcon_*/GetInfo(cid, &name)) {
	PrintError("Can't get client info", rc);
	free(info);
	return;
    }

    /* Find the first available location in list. */
    count = 1; tmp = conns; lasttmp = NULL;
    while (tmp != NULL)
    {
      if (tmp->clientNumber > count)
        break;
      lasttmp = tmp;
      tmp = tmp->next;
      count++;
    }
    info->clientNumber = count;
    info->next = tmp;
    if (lasttmp == NULL)
      conns = info;
    else
      lasttmp->next = info;

    numConns++;
}

/* RPC2 stuff */

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

/* DeleteClient <num> */

DeleteClient(args)
char *args;
{
    char *p;
    int which, rc;
    ConnInfo *conn, *temp;

    if (!*args) {
	if (numConns == 0) {
	    printf("There are no clients.\n");
	    return;
	}
	which = getint("Client No.", 1, MAXCLIENTS, 1);
    }
    else {
	p = args;
	which = intarg(&p, " ", "Client No.", 1, MAXCLIENTS, 1);
    }

    conn = conns; temp = NULL;
    while (conn != NULL)
    {    
      if (conn->clientNumber == which)
        break;
      if (conn->clientNumber > which)
      {
        printf("No client with number %d\n", which);
        return;
      }
      temp = conn;
      conn = conn->next;
    }
    if (conn == NULL)
    {
      printf("No client with number %d\n", which);
      return;
    }

    if (temp == NULL)
      conns = conns->next;
    else
      temp->next = conn->next;
    numConns--;

    rc = RPC2_Unbind(conn->cid);
    if (rc) PrintError("Couldn't unbind", rc);

    free(conn);
}

/* ListClients */

ListClients(args)
char *args;
{
    ConnInfo *conn;

    if (numConns == 0) {
	printf("There are no clients.\n");
	return;
    }

    for (conn = conns; conn; conn = conn->next)
      printf("%-2d: %s (%s, %d)\n", conn->clientNumber, conn->clientName, conn->hostname, conn->port);
}

/* SaveClients */

SaveClients(args)
char *args;
{
}

/* Help for parsing filter args */

FailFilterSide SideArg(p)
char **p;
{
    FailFilterSide side = noSide;
    
    if (!strncmp("in", *p, 2) || !strncmp("rec", *p, 3)) side = recvSide;
    else if (!strncmp("out", *p, 3) || !strncmp("send", *p, 4)) side = sendSide;
    else printf("Valid sides are in, rec, out, send\n");
    while (**p && !isspace(**p)) ++*p;
    return side;
}

FailFilterSide getside()
{
    FailFilterSide side;
    char input[128], *p;
    
    do {
	printf("Side (in, out) ");
	fflush(stdout);
	gets(input);
	p = input;
	side = SideArg(&p);
    } while (side == noSide);
    return side;
}

getipaddr(ip1, ip2, ip3, ip4)
int *ip1, *ip2, *ip3, *ip4;
{
    char hostname[128];
    struct hostent *host;

    do {
	gethostname(hostname, 128);
	getstr("Host", hostname, hostname);
	if (sscanf(hostname, "%d.%d.%d.%d", ip1, ip2, ip3, ip4) != 4) {
	    host = gethostbyname(hostname);
	    if (host == NULL)
		printf("No such host as %s.\n", hostname);
	    else {
		*ip1 = ((unsigned char *)host->h_addr)[0];
		*ip2 = ((unsigned char *)host->h_addr)[1];
		*ip3 = ((unsigned char *)host->h_addr)[2];
		*ip4 = ((unsigned char *)host->h_addr)[3];
	    }
	}
	else {
	    if (*ip1 >= -1 && *ip1 <= 255 &&
		*ip2 >= -1 && *ip2 <= 255 &&
		*ip3 >= -1 && *ip3 <= 255 &&
		*ip4 >= -1 && *ip4 <= 255) return;
	    printf("Use numbers from -1 to 255 in IP addresses.\n");
	    host = NULL;
	}
    } while (host == NULL);
}

getcid(ClientNumber)
int ClientNumber;
{
  ConnInfo *conn;

  conn = conns;
  while (conn != NULL)
  {
    if (conn->clientNumber == ClientNumber)
       return conn->cid;
    if (conn->clientNumber > ClientNumber)
    {
      printf("No client with number %d\n",ClientNumber);
      return -1;
    }
    conn = conn->next;
  }
  if (conn == NULL)
  {
    printf("No client with number %d\n",ClientNumber);
    return -1;
  }
}

/* insertfilter client side which hostname/ip1 ip2 ip3 ip4 color lenmin lenmax prob*10000  */

cmdInsertFilter(args)
char *args;
{
    int rc;
    FailFilterSide side;
    int client, maxFilter, which, cid;
    FailFilter filter;
    int ip1, ip2, ip3, ip4, color, lenmin, lenmax, prob, speed;
    struct hostent *host;

    if (!*args) {
	client = getint("Client No.", 1, MAXCLIENTS, 1);
	side = getside();
        if ((cid = getcid(client)) < 0)
          return;
	maxFilter = /*Fcon_*/CountFilters(cid, side);
	if (maxFilter < 0){
	    PrintError("Couldn't CountFilters", maxFilter);
	    return;
	}
	which = getint("After what filter", 0, maxFilterID, 0);
	getipaddr(&ip1, &ip2, &ip3, &ip4);
	color = getshort("Color", -1, 255, -1);
	lenmin = getint("Minimum length", 0, 65535, 0);
	lenmax = getint("Maximum length", 0, 65535, 65535);
	prob = getint("Probability (0 [off] - 10000 [on])", 0, MAXPROBABILITY, 0);
	if (prob == 0)
	    speed = 0;
	else 
	    speed = getint("Speed (bps) (0 [none] - 10000000 [ether])", 0, 
			   MAXNETSPEED, MAXNETSPEED);
    }
    else {
	argc = BreakupArgs(args, argv);
	if ((argc != 9) && (argc != 8)) {
	    printf("insertfilter <client> <side> <pos> <host> <color> <lenmin> <lenmax> <probability> [ <speed> ]\n");
	    return;
	}
	client = atoi(argv[0]);
	side = SideArg(&argv[1]);
	if (side == noSide) return;
	which = atoi(argv[2]);
	host = gethostbyname(argv[3]);
	if (host != NULL) {
	    ip1 = ((unsigned char *)host->h_addr)[0];
	    ip2 = ((unsigned char *)host->h_addr)[1];
	    ip3 = ((unsigned char *)host->h_addr)[2];
	    ip4 = ((unsigned char *)host->h_addr)[3];
	} else 
	    if ((sscanf(argv[3], "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4) ||
		ip1 < -1 || ip1 > 255 ||
		ip2 < -1 || ip2 > 255 ||
		ip3 < -1 || ip3 > 255 ||
		ip4 < -1 || ip4 > 255) {
		printf("No such host as %s.\n", argv[3]);
		return;
	    }
	
	color = atoi(argv[4]);
	lenmin = atoi(argv[5]);
	lenmax = atoi(argv[6]);
	prob = atoi(argv[7]);

	if (argc == 8) {
	    if (prob == 0)
		speed = 0;
	    else
		speed = MAXNETSPEED;
	} else
	    speed = atoi(argv[8]);
    }

    filter.ip1 = ip1;
    filter.ip2 = ip2;
    filter.ip3 = ip3;
    filter.ip4 = ip4;
    filter.color = color;
    filter.lenmin = lenmin;
    filter.lenmax = lenmax;
    filter.factor = prob;
    filter.speed = speed;

    if ((cid = getcid(client)) < 0)
      return;
    if ((rc = /*Fcon_*/InsertFilter(cid, side, which, &filter)) < 0) {
	if (rc == -2) { /* HACK */
	    printf("It is pointless to insert a slow filter on the receive side, insert failed.\n");
	} else 
	    PrintError("Couldn't InsertFilter", rc);
	return;
    }
    else {
        printf("Filter inserted with ID number %d\n", rc);
        maxFilterID = (rc > maxFilterID)?rc:maxFilterID;
    }
}

/* GetFilters client */

PrintFilters(side, num, filters)
FailFilterSide side;
int num;
FailFilter filters[];
{
    int i;
    register FailFilter *f;
    unsigned char hostaddr[4];
    struct hostent *he;
    char buf[256];

    printf("%s side (%d filters)\n", (side == sendSide) ? "send" : "recv", num);
    for (i = 0; i < num; i++) {
	f = &filters[i];
	hostaddr[0] = (unsigned char)f->ip1;
	hostaddr[1] = (unsigned char)f->ip2;
	hostaddr[2] = (unsigned char)f->ip3;
	hostaddr[3] = (unsigned char)f->ip4;
	if ((he = gethostbyaddr(hostaddr, 4, AF_INET)) != NULL)
	    sprintf(buf, "%s", he->h_name);
	else
	    sprintf(buf, "%d.%d.%d.%d", f->ip1, f->ip2, f->ip3, f->ip4);
	printf("%2d: host %s color %d len %d-%d prob %d speed %d\n", f->id,
	       buf, f->color, f->lenmin, f->lenmax, f->factor, f->speed);
    }
}

cmdGetFilters(args)
char *args;
{
    int client, cid;
    FailFilter filters[32];	/* demagic */
    RPC2_BoundedBS filtersBS;
    FailFilterSide side;
    int i, rc;
    int j;

    if (!*args) {
	client = getint("Client No.", 1, MAXCLIENTS, 1);
    }
    else {
	client = atoi(args);
    }
    filtersBS.MaxSeqLen = sizeof(filters);
    filtersBS.SeqLen = 1;
    filtersBS.SeqBody = (RPC2_ByteSeq) filters;
    if ((cid = getcid(client)) < 0)
      return;
    for (i = 0; i < 2; i++) {
	if (i == 0) side = sendSide;
	else side = recvSide;
	if (rc = GetFilters(cid, side, &filtersBS)) {
	    PrintError("Couldn't GetFilters", rc);
	    return;
	}
	rc = CountFilters(cid, side);
	if (rc < 0) {
	    PrintError("Couldn't CountFilters", rc);
	    return;
	}
	for (j = 0; j < rc; j++) 
	    ntohFF(&filters[j]);

	PrintFilters(side, rc, filters);
    }
}

/* purgeFilters client side */

cmdPurgeFilters(args)
char *args;
{
    int client, cid, rc;
    FailFilterSide side;

    if (!*args) {
	char p[128];
	client = getint("Client No.", 1, MAXCLIENTS, 1);
    
	printf("Side (in, out, both) ");
	fflush(stdout);
	gets(p);
	if (!strncmp("out", p, 3) || !strncmp("send", p, 4))
	    side = sendSide;
	else if (!strncmp("in", p, 2) || !strncmp("rec", p, 3))
	    side = recvSide;
	else
	    side = noSide;	/* If not specified it'll be both sides */
    }
    else {
	argc = BreakupArgs(args, argv);
	if (argc != 2) {
	    printf("purgeFilters client side\n");
	    return;
	}
	client = atoi(argv[0]);

	if (!strncmp("out", argv[1], 3) || !strncmp("send", argv[1], 4))
	    side = sendSide;
	else if (!strncmp("in", argv[1], 2) || !strncmp("rec", argv[1], 3))
	    side = recvSide;
	else
	    side = noSide;	/* If not specified it'll be both sides */
    }

    if ((cid = getcid(client)) < 0)
      return;

    if (rc = /*Fcon_*/PurgeFilters(cid, side)) {
	PrintError("Couldn't PurgeFilters", rc);
	return;
    }
}

/* RemoveFilter client side which */

cmdRemoveFilter(args)
char *args;
{
    int client, cid;
    FailFilterSide side;
    int maxFilter;
    int which;
    int rc;

    if (!*args) {
	client = getint("Client No.", 1, MAXCLIENTS, 1);
	side = getside();
        if ((cid = getcid(client)) < 0)
          return;
	maxFilter = /*Fcon_*/CountFilters(cid, side);
	if (maxFilter < 0) {
	    PrintError("Couldn't CountFilters", maxFilter);
	    return;
	}
	if (maxFilter == 0) {
	    printf("There are no filters.\n");
	    return;
	}
	which = getint("Which filter", 0, maxFilterID, 0);
    }
    else {
	argc = BreakupArgs(args, argv);
	if (argc != 3) {
	    printf("removefilter client side which\n");
	    return;
	}
	client = atoi(argv[0]);
	side = SideArg(&argv[1]);
	which = atoi(argv[2]);
        if ((cid = getcid(client)) < 0)
          return;
    }

    if (rc = /*Fcon_*/RemoveFilter(cid, side, which)) {
	PrintError("Couldn't RemoveFilter", rc);
    }
}
    

/* Status */

Status(args)
char *args;
{
}

/* Quit */

Quit(args)
char *args;
{
    extern int ciexit;

    LWP_TerminateProcessSupport();
    ciexit = 1;
}


/* Print an error msg.  Call perror if err = 0; otherwise call
   RPC2_ErrorMsg(err). */

PrintError(msg, err)
char *msg;
int err;
{
    extern int errno;
    
    if (err == 0) perror(msg);
    else printf("%s: %s\n", msg, RPC2_ErrorMsg(err));
}

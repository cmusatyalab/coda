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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/fail/filcon.c,v 4.3 1998/06/24 18:47:35 jaharkes Exp $";
#endif /*_BLURB_*/

/*
  Network failure emulation package

  Dumb (really dumb!) TTY control interface for fcon
      
  Walter Smith
 */

#include <assert.h>
#include <sys/param.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include <parser.h>
#include "fail.h"

extern void ntohFF(FailFilter *);
extern void htonFF(FailFilter *);

typedef struct conninfo {
    int cid;			/* Connection ID */
    char hostname[MAXHOSTNAMELEN];
    int port;
    char clientName[MAXHOSTNAMELEN];
    int clientNumber;
    struct conninfo *next;
} ConnInfo;

int numConns;
ConnInfo *conns;

int NewConn(char *, short, unsigned long *);

/* CMU Command Interpreter stuff */

void  AddClient(int argc, char **argv);
void  DeleteClient(int argc, char **argv);
void  ListClients(int argc, char **argv);
void  SaveClients(int argc, char **argv);
void  cmdInsertFilter(int argc, char **argv);
void  cmdRemoveFilter(int argc, char **argv);
void  cmdReplaceFilter(int argc, char **argv);
void  cmdGetFilters(int argc, char **argv);
void  cmdPurgeFilters(int argc, char **argv);
void  SetRPC2Debug(int argc, char **argv);
extern long RPC2_DebugLevel;	/* secret! */

command_t list[] = {
    {"addclient", AddClient, 0,"addclient hostname port"},
    {"deleteclient", DeleteClient, 0, "deleteclient clientnumber"},
    {"listclients", ListClients, 0, "shows all the clients"},
    {"saveclients", SaveClients, 0, "" },
    {"insertfilter", cmdInsertFilter, 0,  "insertfilter <clientnum> [in|out] <after post>  [<hostname>|-1.-1.-1.-1] <color(-1)> <lenmin(0)> <lenmax(65550)> <probability([0-10000]) <speed[0-10000000]>" },
    {"removefilter", cmdRemoveFilter, 0, "" },
#if 0
    {"replacefilter", cmdReplaceFilter, 0, "" },
#endif
    {"getfilters", cmdGetFilters, 0, "list filters at a client" },
    {"purgeFilters", cmdPurgeFilters, 0, "remove filters at a client" },
    {"quit", Parser_exit, 0, "" },
    {"help", Parser_help, 0, "" },
    {"?", Parser_qhelp, 0, "" },
    {"rpc2debug", SetRPC2Debug, 0, "set RPC2 debug value" },
    { 0, 0, 0, NULL }
};


int clear(int, char**);
int flist(int, char **);
int join(int, char **);
int partition(int, char **);
int oldpartition(int, char **);
int partition(int, char **);
int heal(int, char**);
int slow(int, char**);
int isolate(int, char**);

argcmd_t argcmdlist[] = {
	{"clear", clear, ""},
	{"list", flist, ""},
	{"isolate", isolate, ""},
	{"join", join, ""},
	{"oldpartition", oldpartition, ""},
	{"partition", partition, ""},
	{"slow", slow, ""},
	{"heal", heal, ""},
	{ 0, 0, 0}
};

int maxFilterID = 999;
/* for convenience, since practically everything uses them */
int argc;
char *argv[MAXARGS];

iopen(int dummy1, int dummy2, int dummy3) {/* fake ITC system call */} 

void
main(int argc, char **argv)
{
	if ( argc > 1 ) {
		Parser_execarg(argc-1, &argv[1], argcmdlist);
	} else {
		InitRPC();
		Parser_init("filcon> ", list);
		Parser_commands();
	}
	exit(0);
}

int BreakupArgs(char *args, char **argv)
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

void AddClient(int argc, char **argv)
{
    char hostname[MAXHOSTNAMELEN];
    short port;
    unsigned long cid;
    ConnInfo *info;
    RPC2_BoundedBS name;
    int rc;
    int count;
    ConnInfo *tmp, *lasttmp;

    if (argc == 1) {
	gethostname(hostname, MAXHOSTNAMELEN);
	Parser_getstr("Host", hostname, hostname, MAXHOSTNAMELEN);
	port = (short) Parser_getint("Port: ", 0, 32767, 0, 0);
    }
    else {
	if (argc != 3) {
	    printf("addclient <host> <port>\n");
	    return;
	}
	strcpy(hostname, argv[1]);
	port = atoi(argv[2]);
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
    name.MaxSeqLen = MAXHOSTNAMELEN;
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
    return;
}

/* RPC2 stuff */

int NewConn(char *hostname, short port, unsigned long *cid)
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
void DeleteClient(int argc, char **argv)
{
    int which, rc;
    ConnInfo *conn, *temp;

    if (argc == 1) {
	if (numConns == 0) {
	    printf("There are no clients.\n");
	    return;
	}
	which = Parser_getint("Client No. :", 1, MAXCLIENTS, 1, 0);
    } else {
	which = Parser_intarg(argv[1], "Client No.", 1, MAXCLIENTS, 1, 10);
    }

    conn = conns; temp = NULL;
    while (conn != NULL)
    {    
      if (conn->clientNumber == which)
        break;
      if (conn->clientNumber > which) {
        printf("No client with number %d\n", which);
        return;
      }
      temp = conn;
      conn = conn->next;
    }
    if (conn == NULL) {
      printf("No client with number %d\n", which);
      return;
    }

    if (temp == NULL)
      conns = conns->next;
    else
      temp->next = conn->next;
    numConns--;

    rc = RPC2_Unbind(conn->cid);
    if (rc) 
	PrintError("Couldn't unbind", rc);

    free(conn);
    return;
}

/* ListClients */
void ListClients(int argc, char **argv)
{
    ConnInfo *conn;

    if (numConns == 0) {
	printf("There are no clients.\n");
	return;
    }

    for (conn = conns; conn; conn = conn->next) { 
	printf("%-2d: %s (%s, %d)\n", conn->clientNumber, 
	       conn->clientName, conn->hostname, conn->port);
    }
    return;
}

/* SaveClients */
void SaveClients(int argc, char **argv)
{
return; 
}

/* Help for parsing filter args */

FailFilterSide SideArg(char **p)
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

int getipaddr(int *ip1, int *ip2, int *ip3, int *ip4)
{
    char hostname[MAXHOSTNAMELEN];
    struct hostent *host;

    do {
	gethostname(hostname, MAXHOSTNAMELEN);
	Parser_getstr("Host", hostname, hostname, MAXHOSTNAMELEN);
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

getcid(int ClientNumber)
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

/* insertfilter client side which hostname/ip1 ip2 ip3 ip4 color
   lenmin lenmax prob*10000 */
void cmdInsertFilter(int argc, char **argv)
{
    int rc;
    FailFilterSide side;
    int client, maxFilter, which, cid;
    FailFilter filter;
    int ip1, ip2, ip3, ip4, color, lenmin, lenmax, prob, speed;
    struct hostent *host;

    if (argc == 1) {
	client = Parser_getint("Client No.", 1, MAXCLIENTS, 1, 10);
	side = getside();
        if ((cid = getcid(client)) < 0)
          return;
	maxFilter = /*Fcon_*/CountFilters(cid, side);
	if (maxFilter < 0){
	    PrintError("Couldn't CountFilters", maxFilter);
	    return;
	}
	which = Parser_getint("After what filter", 0, maxFilterID, 0, 10);
	getipaddr(&ip1, &ip2, &ip3, &ip4);
	color = Parser_getint("Color", -1, 255, -1, 10);
	lenmin = Parser_getint("Minimum length", 0, 65535, 0, 10);
	lenmax = Parser_getint("Maximum length", 0, 65535, 65535, 10);
	prob = Parser_getint("Probability (0 [off] - 10000 [on])", 0, MAXPROBABILITY, 0, 10);
	if (prob == 0)
	    speed = 0;
	else 
	    speed = Parser_getint("Speed (bps) (0 [none] - 10000000 [ether])", 0, 
			   MAXNETSPEED, MAXNETSPEED, 10);
    }
    else {
	if ((argc != 10) && (argc != 9)) {
	    printf("insertfilter <client> <side> <pos> <host> <color> <lenmin> <lenmax> <probability> [ <speed> ]\n");
	    return;
	}
	client = atoi(argv[1]);
	side = SideArg(&argv[2]);
	if (side == noSide) return;
	which = atoi(argv[3]);
	host = gethostbyname(argv[4]);
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
		printf("No such host as %s.\n", argv[4]);
		return;
	    }
	
	color = atoi(argv[5]);
	lenmin = atoi(argv[6]);
	lenmax = atoi(argv[7]);
	prob = atoi(argv[8]);

	if (argc == 9) {
	    if (prob == 0)
		speed = 0;
	    else
		speed = MAXNETSPEED;
	} else
	    speed = atoi(argv[9]);
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
        printf("Filter inserted with ID number %d\n", filter.id);
        maxFilterID = (filter.id > maxFilterID)?filter.id:maxFilterID;
    }
    return;
}

/* GetFilters client */
PrintFilters(FailFilterSide side, int num, FailFilter *filters)
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

void cmdGetFilters(int argc, char **argv)
{
    int client, cid;
    FailFilter filters[32];	/* demagic */
    RPC2_BoundedBS filtersBS;
    FailFilterSide side;
    int i, rc;
    int j;

    if (argc == 1) {
	client = Parser_getint("Client No.", 1, MAXCLIENTS, 1, 10);
    }
    else {
	client = atoi(argv[1]);
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
void cmdPurgeFilters(int argc, char **argv)
{
    int client, cid, rc;
    FailFilterSide side;

    if (argc == 1) {
	char p[128];
	client = Parser_getint("Client No.", 1, MAXCLIENTS, 1, 10);
    
	printf("Side (in, out, both) ");
	fflush(stdout);
	gets(p);
	if (!strncmp("out", p, 3) || !strncmp("send", p, 4))
	    side = sendSide;
	else if (!strncmp("in", p, 2) || !strncmp("rec", p, 3))
	    side = recvSide;
	else
	    side = noSide;	/* If not specified it'll be both sides */
    } else {
	if (argc != 3) {
	    printf("purgeFilters client side\n");
	    return;
	}
	client = atoi(argv[1]);

	if (!strncmp("out", argv[2], 3) || !strncmp("send", argv[2], 4))
	    side = sendSide;
	else if (!strncmp("in", argv[2], 2) || !strncmp("rec", argv[2], 3))
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

void cmdRemoveFilter(int argc, char **argv)
{
    int client, cid;
    FailFilterSide side;
    int maxFilter;
    int which;
    int rc;

    if (argc == 1) {
	client = Parser_getint("Client No.", 1, MAXCLIENTS, 1, 10);
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
	which = Parser_getint("Which filter", 0, maxFilterID, 0, 10);
    }
    else {
	if (argc != 4) {
	    printf("removefilter client side which\n");
	    return;
	}
	client = atoi(argv[1]);
	side = SideArg(&argv[2]);
	which = atoi(argv[3]);
        if ((cid = getcid(client)) < 0)
          return;
    }

    if (rc = /*Fcon_*/RemoveFilter(cid, side, which)) {
	PrintError("Couldn't RemoveFilter", rc);
    }
    return;
}
    
void  SetRPC2Debug(int argc, char **argv)
{
    if (argc != 2) {
	printf("usage: %s level\n", argv[0]);
	return;
    } else {
	RPC2_DebugLevel = atoi(argv[1]);
    }
    return;
}


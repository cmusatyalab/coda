#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/advice/Attic/fail.cc,v 4.4.2.1 1998/10/08 11:26:18 jaharkes Exp $";
#endif /*_BLURB_*/



/*
 *
 *  Network failure interface 
 *	-- based on ../fail/ttyfcon.c
 */
 
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <fail.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <ports.h>


#include <rpc2.h>
#include <fail.h>

extern void ntohFF(FailFilter *);
extern void htonFF(FailFilter *);

#ifdef __cplusplus
}
#endif __cplusplus



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

void InitFail();
void InitRPC();
int NewConn(char *, short, unsigned long *);
int CheckRange(int, int, int);
int AddClient(char *, int);
int DeleteClient(int);
int FindClient(char *);
int cmdInsertFilter(int, char *, int, char *, int, int, int, int, int);
int cmdRemoveFilter(int, char *, int);
int CheckAllFilters(int, char *);
void Quit();

int maxFilterID = 999;

iopen(int dummy1, int dummy2, int dummy3) {/* fake ITC system call */} 

void InitFail() {
    InitRPC();
}

int CheckRange(int value, int minimum, int maximum) {
  if (value < minimum) return(-1);
  if (value > maximum) return(1);
  return(0);
}

int AddClient(char *hostname, int port) {
    unsigned long cid;
    ConnInfo *info;
    RPC2_BoundedBS name;
    int rc;
    int count;
    ConnInfo *tmp, *lasttmp;

    rc = NewConn(hostname, port, &cid);
    if (rc != RPC2_SUCCESS) 
	return(-1);
    RPC2_SetColor(cid, FAIL_IMMUNECOLOR);

    info = (ConnInfo *) malloc(sizeof(ConnInfo));
    info->cid = cid;
    strncpy(info->hostname, hostname, MAXHOSTNAMELEN);
    info->port = port;
    name.MaxSeqLen = MAXNAMELEN;
    name.SeqLen = 1;
    name.SeqBody = (RPC2_ByteSeq) info->clientName;
    if (rc = /*Fcon_*/GetInfo(cid, &name)) {
	free(info);
	return(-1);
    }

    /* Find the first available location in list. */
    count = 1; tmp = conns; lasttmp = NULL;
    while (tmp != NULL) {
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

    return(info->clientNumber);
}

void InitRPC() {
    PROCESS mylpid;
    int rc;

    assert(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) == LWP_SUCCESS);

    rc = RPC2_Init(RPC2_VERSION, 0, NULL, -1, NULL);
    if (rc == RPC2_SUCCESS) return;
    if (rc < RPC2_ELIMIT) exit(-1);
}


int NewConn(char *hostname, short port, unsigned long *cid) {
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

    rc = RPC2_NewBinding(&hident, &pident, &sident, &bparms, (RPC2_Handle *)cid);

    return(rc);
}


int DeleteClient(int client)
{
    char *p;
    int which, rc;
    ConnInfo *conn, *temp;

    if (numConns == 0) 
        return(-1);

    if (CheckRange(client, 1, MAXCLIENTS) != 0) return(-1);

    conn = conns; temp = NULL;
    while (conn != NULL)
    {    
      if (conn->clientNumber == client)
        break;
      if (conn->clientNumber > client)
        return(-1);

      temp = conn;
      conn = conn->next;
    }
    if (conn == NULL)
      return(-1);

    if (temp == NULL)
      conns = conns->next;
    else
      temp->next = conn->next;
    numConns--;

    rc = RPC2_Unbind(conn->cid);
    if (rc) return(-1);

    free(conn);
    return(0);
}

int FindClient(char *name) {
    ConnInfo *conn;

    if (numConns == 0) 
	return(-1);

    for (conn = conns; conn; conn = conn->next) {
      if (strcmp(name, conn->clientName) == 0)
	  return(conn->clientNumber);
    }

    return(-1);
}

/* Help for parsing filter args */

FailFilterSide SideArg(char *p) {
    FailFilterSide side = noSide;
    
    if (!strncmp("in", p, 2) || !strncmp("rec", p, 3)) side = recvSide;
    else if (!strncmp("out", p, 3) || !strncmp("send", p, 4)) side = sendSide;
    return side;
}

int getipaddr(char *hostname, int *ip1, int *ip2, int *ip3, int *ip4)
{
    struct hostent *host;

    host = gethostbyname(hostname);
    if (host == NULL) return(-1);
    *ip1 = (int)((unsigned char *)host->h_addr)[0];
    *ip2 = (int)((unsigned char *)host->h_addr)[1];
    *ip3 = (int)((unsigned char *)host->h_addr)[2];
    *ip4 = (int)((unsigned char *)host->h_addr)[3];
}


int getcid(int ClientNumber)
{
  ConnInfo *conn;

  conn = conns;
  while (conn != NULL)
  {
    if (conn->clientNumber == ClientNumber)
       return(conn->cid);
    if (conn->clientNumber > ClientNumber)
      return(-1);
    conn = conn->next;
  }
  return(-1);
}

int cmdInsertFilter(int client, char *sidestr, int which, char *hostname, int color, int lenmin, int lenmax, int prob, int speed) {
    int rc;
    FailFilterSide side;
    int maxFilter, cid;
    FailFilter filter;
    int ip1, ip2, ip3, ip4;
    struct hostent *host;

    maxFilter = /*Fcon_*/CountFilters(cid, side);
    if (maxFilter < 0) return(-1);

    /* Check the arguments... */
    if (CheckRange(client, 1, MAXCLIENTS) != 0) return(-1);
    if ((cid = getcid(client)) < 0) return(-1);
    if ((side = SideArg(sidestr)) == noSide) return(-1);
    if (CheckRange(which, 0, maxFilterID) != 0) return(-1);
    if (getipaddr(hostname, &ip1, &ip2, &ip3, &ip4) == -1) return(-1);
    if (CheckRange(color, -1, 255) != 0) return(-1);
    if (CheckRange(lenmin, 0, 65535) != 0) return(-1);
    if (CheckRange(lenmax, 0, 65535) != 0) return(-1);
    if (CheckRange(prob, 0, MAXPROBABILITY) != 0) return(-1);
    if (CheckRange(speed, 0, MAXNETSPEED) != 0) return(-1);
    if ((prob == 0) && (speed != 0)) return(-1);

    /* Setup filter */
    filter.ip1 = ip1;
    filter.ip2 = ip2;
    filter.ip3 = ip3;
    filter.ip4 = ip4;
    filter.color = color;
    filter.lenmin = lenmin;
    filter.lenmax = lenmax;
    filter.factor = prob;
    filter.speed = speed;

    /* Attempt to insert it */
    if ((rc = /*Fcon_*/InsertFilter(cid, side, which, &filter)) < 0) 
	return(-1);

    maxFilterID = (rc > maxFilterID)?rc:maxFilterID;
    return 0;
}

int CheckAllFilters(int client, char *hostname) {
    int cid;
    FailFilter filters[32];
    register FailFilter *f;
    RPC2_BoundedBS filtersBS;
    FailFilterSide side;
    unsigned char hostaddr[4];
    struct hostent *he;
    int i, j, k, rc;
    int filterCount = 0;

    /* Check the arguments... */
    if (CheckRange(client, 1, MAXCLIENTS) != 0) return(-1);
    if ((cid = getcid(client)) < 0) return(-1);

    filtersBS.MaxSeqLen = sizeof(filters);
    filtersBS.SeqLen = 1;
    filtersBS.SeqBody = (RPC2_ByteSeq) filters;

    for (i = 0; i < 2; i++) {
	if (i == 0) side = sendSide;
	else side = recvSide;

	filterCount = CountFilters(cid, side);
	if (filterCount <= 0) 
	    continue;

	if (rc = GetFilters(cid, side, &filtersBS)) 
	    return(-1);

	for (j = 0; j < filterCount; j++) 
	    ntohFF(&filters[j]);
	
	for (k = 0; k < filterCount; k++) {
            f = &filters[k];
	    hostaddr[0] = (unsigned char)f->ip1;
	    hostaddr[1] = (unsigned char)f->ip2;
	    hostaddr[2] = (unsigned char)f->ip3;
	    hostaddr[3] = (unsigned char)f->ip4;
	    he = gethostbyaddr((const char *)hostaddr, 4, AF_INET);
	    if (he != NULL)
		if (strcmp(he->h_name, hostname) == 0) {
		    return(1);
		}
	}
    }
    return(0);
}

int cmdRemoveFilter(int client, char *sidestr, int which)
{
    int cid;
    FailFilterSide side;
    int maxFilter;
    int rc;

    /* Check the arguments... */
    if (CheckRange(client, 1, MAXCLIENTS) != 0) return(-1);
    if ((cid = getcid(client)) < 0) return(-1);
    if ((side = SideArg(sidestr)) == noSide) return(-1);
    maxFilter = /*Fcon_*/CountFilters(cid, side);
    if (maxFilter < 0) return(-1);
    if (maxFilter == 0) return(-1);
    if (CheckRange(which, 0, maxFilterID) != 0) return(-1);

    /* Remove the filter */
    if (rc = /*Fcon_*/RemoveFilter(cid, side, which)) 
	return(-1);

    return 0;
}
    

void Quit()
{
    LWP_TerminateProcessSupport();
}

int CheckServer(char *client, char *server) {
    int clientNumber, rc;

    clientNumber = AddClient(server, PORT_codasrv);
    if (clientNumber <= 0) return(-1);
    rc = CheckAllFilters(clientNumber, client);
    if (DeleteClient(clientNumber) != 0) return(-1);
    return(rc);
}


int CheckClient(char *client, char *server) {
    int clientNumber, rc;

    clientNumber = AddClient(client, PORT_venus);
    if (clientNumber <= 0) return(-1);
    rc = CheckAllFilters(clientNumber, server);
    if (DeleteClient(clientNumber) != 0) return(-1);
    return(rc);
}


/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2008 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <sys/time.h>
#include <assert.h>
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <errno.h>
#include <execinfo.h>
#include <rpc2/secure.h>
#include <rpc2/fakeudp.h>

/* 
Daemon that does the relaying of packets to/from net and localhost.
Created via fork() by Venus or codasrv.
Uses single Unix domain socket to talk to Venus or codasrv on
localhost, and one TCP-tunnelled socket to talk to each distinct
remote codasrv or Venus.  Also has one UDP socket for upward
compatibility with legacy servers and clients.
*/

static int DebugLevel = 0; 

/* fds for Venus-codatunnel communication */

int fakeudp_vside_sockfd;  
int fakeudp_tside_sockfd;

/* fd and port number of single net-facing bidirectional socket
   for legacy servers and clients */
static int netfacing_udp_fd;
static short netfacing_udp_portnum;

/* Startup flag to indicate whether to initiate outbound TCP connections.
   Set to zero on server's codatunnel, and set to one on client's codatunnel 
   If not set, any packets to those destinateions are simply dropped; 
   Because a Venus may be behind a NAT firewall, its codatunnel should always
   be the one initiating TCP connections.
*/
static int initiate_tcp_flag = 0;

/* fd of single net-facing TCP bind (i.e., rendezvous) socket;
   only relevant when (initiate_tcp_flag == 0) */
static int netfacing_tcp_bind_fd = -1;  /* TCP portnum same as UDP portnum */

/* Set of file descriptors to wait on */
static fd_set BigFDList;
static int BigFDList_Highest;  /* one larger than highest fd in BigFDList */

/* forward refs to local functions */
static void GetParms(int, char **);
static void BogusArgs();
static void InitializeNetFacingSockets();
static void DoWork();
static void HandleFDException(int);
static void HandleOutgoingUDP();
static void HandleIncomingUDP();
static void HandleNewTCPconnect();
static void HandleIncomingTCP(int);


/* Global array of known destinations. 
   Some may be TCP but others (legacy) may be UDP.
   Use a very simple array data structure initially.  Can get
   fancier later if needed for efficiency.   For initial simplicity
   in implementation, we just preallocate an array of size MAXDEST.
   This can be made more dynamic using malloc, if needed.  On a modern
   fast machine, the simple linear searches below are much faster than
   any clever hash table etc for small MAXDEST.
*/

/* Format of one entry in array */
struct known_destination {
  int isactive; /* one if connection is active; zero if broken */
  struct sockaddr_in saddr; /* IPv4 address and port num of destination */
  enum desttype_t {USES_UDP, USES_TCP} whatkind;
  int tcp_fd; /* currently open TCP socket to this destination; 
		 only meaningful if isactive is one and whatkind is USES_TCP */

   /* Fields below help with assembling a full pkt from short reads on stream socket
     At most one packet is being assembled at a time from a destination
  */
  int fullpktsize; /* how big the entire packet is */
  int bytesfilled; /* how much of pkt is filled already */
  char pkt[FAKEUDP_MAXPACKETSIZE]; /* pkt being assembled */
};

#define MAXDEST 100   /* 100 should be big enough for a while! */
static struct known_destination DestArray[MAXDEST]; 
static int DestCount = 0;  /* how many entries relevant (even if not active currently) */

int CompareDest(struct sockaddr_in dest1, struct sockaddr_in dest2) {
  /* returns 1 if dest1 and dest2 are identical TCP addresses;
     returns 0 otherwise
  */

  if ((dest1.sin_family != AF_INET) || (dest2.sin_family != AF_INET)) return (0);
  if (dest1.sin_addr.s_addr != dest2.sin_addr.s_addr) return (0);
  if (dest1.sin_port != dest2.sin_port) return (0);
  return(1);
}

static int GetDestEntry(struct sockaddr_in dest) {
  /* returns index of matching destinaton in destarray;
     returns -1 if no match found
  */
  int i;

  for (i = 0; i < DestCount; i++) {
    if (CompareDest(dest, DestArray[i].saddr)) return (i);
  }
  return (-1);
}


static int AddNewDest(enum desttype_t mytype, struct sockaddr_in newdest) {
  /* returns index of newly allocated entry for newdest */

  assert(DestCount < MAXDEST); /* fix this to handle more gracefully */
  DestArray[DestCount].isactive = 0;
  DestArray[DestCount].whatkind = mytype;
  DestArray[DestCount].saddr = newdest;
  DestArray[DestCount].tcp_fd = -1;
  DestArray[DestCount].fullpktsize = 0;
  DestArray[DestCount].bytesfilled = 0;
  memset(DestArray[DestCount].pkt, 0, sizeof(DestArray[DestCount].pkt));
  DestCount++;
  return(DestCount-1);
}


/* main routine of coda tunnel daemon */
void codatunneld (short myport){

  printf("codatunneld: starting\n"); fflush(stdout);
  netfacing_udp_portnum = myport;
  InitializeNetFacingSockets();
  printf("codatunneld: after InitializeNetFacingSockets\n"); fflush(stdout);
  DoWork();

}


void InitializeNetFacingSockets() {
  int rc;
  struct sockaddr_in saddr;

 /* First create the single UDP socket for legacy clients & servers
    Do this even if netfacing_udp_portnum is zero, because that means
    "use any port"
 */

  netfacing_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (netfacing_udp_fd < 0){
    perror("socket: ");
    exit(-1);
  } else {
    printf("netfacing_udp_fd = %d\n", netfacing_udp_fd);
  }

  bzero(&saddr, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(netfacing_udp_portnum);
  printf("netfacing_udp_portnum = %d\n", netfacing_udp_portnum); fflush(stdout);
  rc = bind(netfacing_udp_fd, (struct sockaddr *)&saddr, sizeof(saddr));
  if (rc < 0){
    perror("bind: ");
    exit(-1);
  } else {
    printf("bind succeeded\n");
  }

  /* Start temporary sanity check code: delete after debugging */
  socklen_t slen = sizeof(saddr);
  uint8_t ip1, ip2, ip3, ip4;
  uint16_t  uport;
  rc = getsockname(netfacing_udp_fd, (struct sockaddr *)&saddr, &slen);
  if (rc < 0){perror("getsockname: "); exit(-1);}
  if (saddr.sin_addr.s_addr == 0) printf("saddr is zero ip address\n");
  ip4 = (ntohl(saddr.sin_addr.s_addr) & 0x000000ff);
  ip3 = (ntohl(saddr.sin_addr.s_addr) & 0x0000ff00) >> 8;
  ip2 = (ntohl(saddr.sin_addr.s_addr) & 0x00ff0000) >> 16;
  ip1 = (ntohl(saddr.sin_addr.s_addr) & 0xff000000) >> 24;
  uport = ntohs(saddr.sin_port);

  printf("UDP socket is: %d.%d.%d.%d [%d]\n", ip1, ip2, ip3, ip4, uport);

  /* End temporary sanity check code */


  netfacing_tcp_bind_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (netfacing_tcp_bind_fd < 0){
    perror("socket: ");
    exit(-1);
  } else {
    printf("netfacing_tcp_bind_fd = %d\n", netfacing_tcp_bind_fd);
  }

  bzero(&saddr, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(netfacing_udp_portnum);
  rc = bind(netfacing_tcp_bind_fd, (struct sockaddr *)&saddr, sizeof(saddr));
  if (rc < 0){
    perror("bind: ");
    exit(-1);
  } else {
    printf("bind succeeded\n");
  }

  /* Start temporary sanity check code: delete after debugging */
  slen = sizeof(saddr);
  rc = getsockname(netfacing_tcp_bind_fd, (struct sockaddr *)&saddr, &slen);
  if (rc < 0){perror("getsockname: "); exit(-1);}
  if (saddr.sin_addr.s_addr == 0) printf("saddr is zero ip address\n");
  ip4 = (ntohl(saddr.sin_addr.s_addr) & 0x000000ff);
  ip3 = (ntohl(saddr.sin_addr.s_addr) & 0x0000ff00) >> 8;
  ip2 = (ntohl(saddr.sin_addr.s_addr) & 0x00ff0000) >> 16;
  ip1 = (ntohl(saddr.sin_addr.s_addr) & 0xff000000) >> 24;
  uport = ntohs(saddr.sin_port);

  printf("TCP socket is: %d.%d.%d.%d [%d]\n", ip1, ip2, ip3, ip4, uport);
  
  printf("About to set global bit mask\n"); fflush(stdout);

  /* Initialize the global bit mask and find highest fd for select() 
     Note that we don't listen on fakeudp_pipefd; it is only for signals 
  */
  FD_ZERO(&BigFDList);
  FD_SET(fakeudp_tside_sockfd, &BigFDList);
  BigFDList_Highest = fakeudp_tside_sockfd + 1;

  FD_SET(netfacing_udp_fd, &BigFDList);
  if (BigFDList_Highest <= netfacing_udp_fd) 
    BigFDList_Highest = netfacing_udp_fd + 1;

  /* DEBUG:  FD_SET(netfacing_tcp_bind_fd, &BigFDList);  */
  if (BigFDList_Highest <= netfacing_tcp_bind_fd) 
    BigFDList_Highest = netfacing_tcp_bind_fd + 1;

  printf("codatunnel: BigFDList_Highest = %d\n", BigFDList_Highest);
  
}



void DoWork(){
  int i, rc;
  fd_set readlist, exceptlist;

  
  do {/* main work loop, non-terminating */
    readlist = BigFDList;
    exceptlist = BigFDList;
    printf("codatunnel: before select()\n"); fflush(stdout);
    rc = select(BigFDList_Highest, &readlist, 0, &exceptlist, 0);
    printf("codatunnel: after select() ---> %d\n", rc); fflush(stdout);

    /* first deal with all the exceptions */
    for (i = 0; i < BigFDList_Highest; i++){
      if (!FD_ISSET(i, &exceptlist)) continue;
      HandleFDException(i);
    }


    /* then deal with fds with incoming data */
    for (i = 0; i < BigFDList_Highest; i++){
      if (!FD_ISSET(i, &readlist)) continue;

      /* fd i has data for me */
      if (i == fakeudp_tside_sockfd) {
	HandleOutgoingUDP();
	continue;
      }

      if (i == netfacing_udp_fd) {
	HandleIncomingUDP();
	continue;
      }

      if (i == netfacing_tcp_bind_fd) {
	HandleNewTCPconnect();
	continue;
      }

      /* if we get here, it must be an incoming TCP packet */
      HandleIncomingTCP(i);
    }
  }
  while (1);
}


void HandleFDException(int whichfd) {
  printf("HandleFDExecption(%d)\n", whichfd); fflush(stdout);
}

void HandleOutgoingUDP() {
  int rc, errnoval;
  struct fakeudp_packet p;

  printf("HandleOutgoingUDP()\n"); fflush(stdout);

  /* we assume packet from socketpair() aligns perfectly with struct fakeudp_packet */
  errno = 0;
  rc = recvfrom(fakeudp_tside_sockfd, &p, sizeof(struct fakeudp_packet), 0, 0, 0);

  errnoval = errno;
  printf("recvfrom: rc = %d   errno = %02d (%s)\n", rc, errnoval, sys_errlist[errnoval]);

  if (rc < 0) return; /* error return */

  /* send it out on the network */
  errno = 0;
  rc = sendto(netfacing_udp_fd, p.out, p.outlen, 0, &p.to, p.tolen);

  errnoval = errno;
  printf("sendto: rc = %d   errno = %02d (%s) p.outlen = %d  p.tolen = [%d, %d]  \n", rc, errnoval, sys_errlist[errnoval], p.outlen, p.tolen, sizeof(struct sockaddr));

}

void HandleIncomingUDP() {
  int rc, errnoval;
  struct fakeudp_packet p;

  printf("HandleIncomingUDP()\n"); fflush(stdout);

  memset(&p, 0, sizeof(struct fakeudp_packet));
  p.tolen = sizeof(struct sockaddr);  /* in-out parameter for recvfrom () */
  errno = 0;
  rc = recvfrom(netfacing_udp_fd, p.out, sizeof(p.out), 0, &p.to, &p.tolen);
  errnoval = errno;
  printf("recvfrom: rc = %d   errno = %02d (%s)\n", rc, errnoval, sys_errlist[errnoval]);

  if (rc < 0) return; /* error return */

  p.outlen = rc; /* note actual bytes received */

  /* send it to the host */
  errno = 0;
  rc = sendto(fakeudp_tside_sockfd, &p, sizeof(p), 0, 0, 0);
  errnoval = errno;
  printf("sendto: rc = %d   errno = %02d (%s) p.outlen = %d  p.tolen = [%d, %d]  \n", rc, errnoval, sys_errlist[errnoval], p.outlen, p.tolen, sizeof(struct sockaddr));


}

void HandleNewTCPconnect() {
  printf("HandleNewTCPconnect()\n"); fflush(stdout);
}

void HandleIncomingTCP(int whichfd) {
  printf("HandleIncomingTCP()\n"); fflush(stdout);
}


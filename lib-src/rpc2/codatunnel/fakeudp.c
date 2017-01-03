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
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <execinfo.h>
static void PrintStackTrace(); /* forward reference */

#include <rpc2/fakeudp.h>  /* no other RPC2 or LWP header files needed */

/* 
   This code has to be linkable into two incompatible worlds:
   the LWP/RPC2 world of Venus & codasrv, and the pthreads/SSL/TLS 
   world of codatunneld.  Notice that other than fakeudp.h, there 
   are no dependencies on LWP or RPC2 header files.  

   This code layers UDP socket primitives on top of TCP connections.
   Maintains a single TCP connection for each (host, port) pair
   All UDP packets to/from that (host, port) pair are sent/recvd on this connection.
   All RPC2 connections to/from that (host,port are multiplexed on this connection.
   Minimal changes to rest of the RPC2 code.
   Discards all packets with "RETRY" bit set.

   Possible negative consequences:
   (a) serializes all transmissions to each (host,port) pair
       (but no guarantee that such serialization wasn't happening before)
   (b) SFTP becomes a stop and wait protocol for each 8-packet window
       (since RETRY flag triggered sendahead)

   (Satya, 2017-01-04)
*/

/* global flag below controls whether TCP tunnels are used */
int enable_codatunnel = 0; /* non zero to enable  tunneling */

char **fakeudp_saved_argv = 0; /* if non-null, it was set earlier to argv in main() */

/* fd in parent of open hfsocket */
int fakeudp_vside_sockfd;  /* v2t: venus to tunnel */
int fakeudp_tside_sockfd;  /* t2v: tunnel to venus */

/* fd of pipe for signalling */
int fakeudp_vside_pipefd;
int fakeudp_tside_pipefd;

int fakeudp_fork_codatunneld(short udplegacyportnum, int initiatetcpflag) {

  /* 
  Create the Coda tunnel process.  Returns 0 on success, -1 on error.
  Invoked before RPC2_Init() by venus (on client) or codasrv (on server).

  udplegacyportnum = UDP port number for talking to legacy clients and servers
  Note that this same port number is used for the socket that is as the TCP 
  bind port for incoming TCP connections (rendezvous).  The kernel correctly 
  separates UDP and TCP packets.
  initiatetcpflag = whether to initiate TCP connections  (1 = yes, 0 = no)
        
  We want the codatunnel process to die when Venus dies.  The only
  portable way to do this is to create a pipe and wait for signal HUP.
  Approaches like prctl(PR_SET_PDEATHSIG, SIGHUP); are Linux-specific
  and hence non-portable.

  Note that we use an old-fashioned pipe for this purpose rather than
  using the socketpair that is created for faking UDP transmission.  I
  am just not sure of the details of signal handling on the socketpair,
  so playing it safe.  Maybe we can eliminate the pipe altogether?

  */

  int rc, pipefd[2], sockfd[2];

  printf("fakeudp_fork_codatunneld(%d, %d)\n", udplegacyportnum, 
	 initiatetcpflag); fflush(stdout);

  /* Create pipe for discovering failures */
  rc = pipe(pipefd);
  if (rc < 0) {perror("fakeudp_fork_codatunneld: pipe() failed:"); return(-1);}
  /* DEBUG */  printf("pipefd after pipe() is [%d, %d]\n", pipefd[0], pipefd[1]); fflush(stdout);
  fakeudp_vside_pipefd = pipefd[1];
  fakeudp_tside_pipefd = pipefd[0];


  /* Create socketpair for host-facing UDP communication */
  rc = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockfd);
  if (rc < 0) {perror("fakeudp_fork_codatunneld: socketpair() failed: "); fflush(stdout); return(-1);}
  /* DEBUG */ printf("hfsocket_fdpair after socketpair() is: [%d, %d]\n", sockfd[0], sockfd[1]);
  fakeudp_vside_sockfd = sockfd[1];
  fakeudp_tside_sockfd = sockfd[0];


  /* fork and exec codatunneld */
  
  rc = fork ();
  if (rc < 0) {perror("fakeudp_fork_codatunneld: fork() failed: "); return(-1);}

  if (rc > 0) {/* I am the parent. */
    printf("Parent: fork succeeded, child pid is %d\n", rc);
    close(fakeudp_tside_pipefd);
    close(fakeudp_tside_sockfd);
    return(0);  /* this is the only success return */
  }

  /* If I get here, I must be the newborn child */
  printf("Child: codatunneld fork succeeded\n");

  close(fakeudp_vside_pipefd);
  close(fakeudp_vside_sockfd);

  /* if possible, rename child's command line for "ps ax" */
  if (fakeudp_saved_argv) {
    int argv0len = strlen(fakeudp_saved_argv[0]);  /* save max len for rename */
    strncpy(fakeudp_saved_argv[0], "codatunneld", argv0len); 
  }

  /* If Venus dies, I will get SIGHUP on pipe; make sure that I die too */
  signal(SIGHUP, SIG_DFL);  /* Default handling of SIGHUP is to die */

  codatunneld(udplegacyportnum); /* launch the tunnel and never return */
  assert(0); /* should never reach here */	  
}



ssize_t  fakeudp_sendto(int  s,  const  void *buf, size_t len, int flags, const
		struct sockaddr *to, socklen_t tolen){
  int rc, errnoval;
  struct fakeudp_packet p;

  printf("fakeudp_sendto()\n"); /* DEBUG */

  if (!enable_codatunnel) {return(sendto(s,  buf, len, flags, to, tolen));}

  /* construct the fakeudp packet */
  memset(&p, 0, sizeof(struct fakeudp_packet));
  p.to = *to;
  p.tolen = tolen;
  /* DEBUG (need to fix this) p.is_retry = fakeudp_isretry(buf); */
  p.outlen = len;
  memcpy(p.out, buf, len);

  /* then send it to codatunneld */
  rc = sendto(fakeudp_vside_sockfd,  &p, sizeof(struct fakeudp_packet), 0, 0, 0); /* last three args irrelevant for scoketpair() */
  errnoval = errno;
  printf("fakeudp_sendto(): socket = [%02d, %02d], rc = %d  errno = %02d (%s)\n", s, fakeudp_vside_sockfd, rc, errnoval, sys_errlist[errnoval]);
  return(rc);

}

ssize_t fakeudp_recvfrom(int s, void *buf, size_t len, int flags,
		 struct sockaddr *from, socklen_t *fromlen) {
  int rc, errnoval;
  struct fakeudp_packet p;

  printf("fakeudp_recvfrom()\n"); /* DEBUG */

  if (!enable_codatunnel) {return(recvfrom(s, buf, len, flags, from, fromlen));}

  /* get the packet from codatunneld */
  errno = 0;
  rc = recvfrom(fakeudp_vside_sockfd, &p, sizeof(p), 0, 0, 0);
  errnoval = errno;
  printf("fakeudp_recvfrom(): socket = [%02d, %02d], rc = %d  errno = %02d (%s)\n", s, fakeudp_vside_sockfd, rc, errnoval, sys_errlist[errnoval]); fflush(stdout);

  if (rc < 0) return(rc); /* error */

  /* are buf and from big enough? */
  if ((len < p.outlen) || ((*fromlen) < p.tolen)) {
    errno = ENOSPC;
    return (-1);
  }

  memcpy(buf, p.out, p.outlen); 
  *from = p.to;
  *fromlen = p.tolen;

  return(p.outlen);
}

int fakeudp_socket (int domain, int type, int protocol){
  printf("fakeudp_socket()\n"); /* DEBUG */
  if (!enable_codatunnel)
    return (socket(domain, type, protocol));
  else {
    return(fakeudp_vside_sockfd);  /* already createed by socketpair () */
  }
}

int fakeudp_setsockopt(int s, int  level,  int  optname,  const  void
		       *optval, socklen_t optlen){
  if (!enable_codatunnel)
    return(setsockopt(s, level, optname, optval, optlen));
  else {/*ignore the call*/ return(0);}
}

int fakeudp_bind(int   sockfd,   const  struct  sockaddr  *my_addr,
		 socklen_t addrlen){
  if (!enable_codatunnel)
    return(bind(sockfd, my_addr, addrlen));
  else {/*ignore the call*/ return(0);}
}

int fakeudp_fcntl(int fd, int cmd, long arg){

  if (!enable_codatunnel)
    return(fcntl(fd, cmd, arg));
  else  return(fcntl(fakeudp_vside_sockfd, cmd, arg));
}

int fakeudp_getsockname(int s, struct sockaddr *name, socklen_t *namelen) {

  if (!enable_codatunnel)
    return(getsockname(s, name, namelen));
  else {/*ignore the call*/ return(0);}
}


static void PrintStackTrace() {
 void* callstack[128];
  int i, frames = backtrace(callstack, 128);
  char** strs = backtrace_symbols(callstack, frames);
  for (i = 0; i < frames; ++i) {
    printf("%s\n", strs[i]);
  }
  free(strs);
}

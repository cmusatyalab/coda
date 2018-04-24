/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2017 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#ifndef _CODATUNNEL_PRIVATE_H_
#define _CODATUNNEL_PRIVATE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <sys/time.h>

#if 0
#define DEBUG(...) do { \
    struct timeval tt; gettimeofday(&tt, 0); \
    printf("%ld:%ld %s:%d ", tt.tv_sec, tt.tv_usec, __FUNCTION__, __LINE__); \
    printf(__VA_ARGS__); fflush(stdout); \
} while(0)
#else
#define DEBUG(...)
#endif

#define ERROR(...) do { \
    fprintf(stderr, "%s:%d ", __FUNCTION__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fflush(stderr); \
} while(0)


/* the actual tunnel daemon (defined in codatunneld.c) */
void codatunneld(int codatunnel_sockfd,
                 const char *tcp_bindaddr,
                 const char *udp_bindaddr,
                 const char *bind_service,
		 int onlytcp)
    __attribute__((noreturn));


/* Format of encapsulated UDP packets sent on Unix domain connections
   (i.e., between Venus and codatunneld, and between codasrv and
   codatunneld.)  All fields are in the clear. This header is followed
   by `msglen` encrypted bytes of the RPC2 packet that is sent or
   received.  On the network, this header is NOT sent in UDP packets; but
   it IS sent in TCP-tunneled packets
*/
typedef struct codatunnel_packet {
  char magic[8];
  uint32_t is_retry;                 /* 1 if this is a resend, 0 otherwise */
  uint32_t is_init1;                 /* 1 if this is an Init1 opcode,0 otherwise */
  size_t msglen;                /* actual number of bytes in the packet */
  struct sockaddr_storage addr; /* verbatim from sendto() or recvfrom () */
  socklen_t addrlen;            /* verbatim from sendto() or recvfrom () */
} ctp_t;

#define MAXRECEIVE (4500+sizeof(ctp_t))
   /* WARNING: gross hack! Above field is space for received packet
      that may be assembled piecemeal from multiple TCP reads; the
      figure of 4500 is RPC2_MAXPACKETSIZE; there are many cleverer
      ways of allocating this space, but the simplicity of this
      approach is fine for initial implementation; get it working
      first, and then fix later */

typedef struct remotedest {
  struct sockaddr_storage destaddr;
  enum {TCPBROKEN = 0, TCPACTIVE = 1} state;
/* All destinations are assumed to be capable of becoming TCPACTIVE;
   TCPBROKEN ouuld mean TCP is not supported, or destination unreachable;
   Setting TCPACTIVE should be a commit point:  all fields below
   should have been set before that happens, to avoid race conditions */

  uv_tcp_t *tcphandle; /* only valid if state is TCPACTIVE*/
  int  packets_sent;   /* for help with INIT1 retries */
  int nextbyte; /* index in received_packet[] into which next byte will be read;
		   needed because multiple read calls may be needed to obtain a
		   complete UDP packet that has been tunneled in TCP */
  int ntoh_done; /* whether ntohl() has already been done on
		    the header of this packet */
  char *received_packet;  /* malloced array of size MAXRECEIVE; initial
			     malloc when TCPACTIVE is set; after that a
			     new malloc is done for each received
			     packet; free() happens in
			     uv_udp_sent_cb() */

} dest_t;

/* Stuff for destination management */
void initdest(void);
/* return pointer to matching destination or NULL */
dest_t *getdest(struct sockaddr_storage *, socklen_t);
/* create new entry for specified destination */
dest_t *createdest(struct sockaddr_storage *, socklen_t);

/* Helper/debugging functions */
char *show_sockaddr(struct sockaddr_storage *s);
void hexdump(char *, void *, int);

#endif /* _CODATUNNEL_PRIVATE_H_ */

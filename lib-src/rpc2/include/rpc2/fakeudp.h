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

#ifndef _FAKEUDP_H_
#define _FAKEUDP_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* 
   With fakeudp.h, this code has to be linkable into two different
   worlds: the LWP/RPC2 world of Venus & codasrv, and the
   pthreads/SSL/TLS world of codatunneld.  Notice that there are no
   dependencies on LWP or RPC2 header files.  Although this source
   code is physically located in the RPC2 part of the code base for
   convenience, it is logically separable.
*/


/* WARNING: Ugly manual definitions */
#define FAKEUDP_MAXPACKETSIZE 4500
extern int  fakeudp_isretry (void *);  /* defined in rpc2b.c */
/* 
   For reasons of avoiding dependence on RPC2/LWP headers, manually
   define above value equal to MAXPACKETSIZE in RPC2.  I know this is
   bad programming practice, but it is the lesser evil.  MAXPACKETSIZE
   in RPC2 has not changed since 1984, and I can't see it changing in
   the future.  Similarly, manually define prototype of function to
   tell whether RPC2 packet is a retry.  
*/



extern int enable_codatunnel; /* non zero to enable  tunneling */

ssize_t fakeudp_write(int,  const  void *, size_t);
ssize_t fakeudp_sendto(int,  const  void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t fakeudp_recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
int fakeudp_socket(int, int, int);
int fakeudp_setsockopt(int, int, int, const void *, socklen_t);
int fakeudp_bind(int, const struct sockaddr *, socklen_t);
int fakeudp_fcntl(int, int, long);
int fakeudp_getsockname(int, struct sockaddr *, socklen_t *);

/* fds of socketpair() for fakeudp packets */
extern int fakeudp_vside_sockfd;  /* vside: venus side  */
extern int fakeudp_tside_sockfd;  /* tside: tunnel side */

/* fds of pipe for signalling */
extern int fakeudp_vside_pipefd;
extern int fakeudp_tside_pipefd;

/* saved value of argv from main(argc, argv); used to rename codatunneld for "ps ax" */
extern char **fakeudp_saved_argv;  /* initialized to null, meaning don't know */

extern void codatunneld(short); /* the actual tunnel daemon (defined in codatunnel.c */
extern int fakeudp_fork_codatunneld(short udplegacyportnum, int initiateflag);  /* works in both Venus and codasrv */

/* Format of encapsulated UDP packets sent on Unix domain connections
   (i.e., between Venus and codatunneld,  and between codasrv and codatunneld.)
   All fields are in the clear except last one (out).
*/
struct fakeudp_packet {
  struct sockaddr to;   /* verbatim from sendto()  or recvfrom () */
  socklen_t tolen;  /* verbatim from sendto()  or recvfrom () */
  uint32_t is_retry; /* 1 if this is a resend, 0 otherwise */
  size_t outlen; /* actual number of bytes in array out */
  char out[FAKEUDP_MAXPACKETSIZE]; /* original packet after encryption */
};


#endif /* _FAKEUDP_H_ */

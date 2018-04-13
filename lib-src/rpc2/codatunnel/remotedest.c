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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "codatunnel.private.h"

/* Code to track and manage known destinations We use a very simple
   array of fixed length, and brute force search to get started.
   Later, this can be made into a hash table or other more efficient
   data structure
*/

#define MAXDEST 100   /* should be plenty for early debugging */
dest_t destarray[MAXDEST];
int ndests = 0; /* how many entries allocated in destarray */

void cleardest(dest_t *d)
{
    bzero(&d->destaddr, sizeof (struct sockaddr_storage));
    d->state = TCPBROKEN;
    d->tcphandle = 0;
    d->received_packet = 0; /* null pointer */
    d->nextbyte = 0;
    d->ntoh_done = 0;
    d->packets_sent = 0;
}


void initdestarray()
{/* initialize the global data structure, so that
    the destaddr fields are completely zeroed for memcmp() 
    comparisons in later getdest() calls; otherwise padding
    in structures may cause trouble */

  ndests = 0;

  for (int i = 0; i < MAXDEST; i++){
    cleardest(&destarray[i]);
  }
}

dest_t *getdest(struct sockaddr_storage *x, socklen_t xlen)
{
  /* returns pointer to structure in destarray[] if x is a known destination;
     returns NULL otherwise
     xlen says how many bytes of *x to compare; rest is don't care
  */
  /*  DEBUG("sockaddr: %p  socklen: %d   %s\n", x, xlen, show_sockaddr(x));*/
  for (int i = 0; i < ndests; i++){
    dest_t *d = &destarray[i];
    if (!memcmp(&d->destaddr, x, xlen))
      {
	/*	DEBUG("entry found --> %d  %p %s\n", i, d, 
	  show_sockaddr(&d->destaddr)); */
	return (d);

}

  }
  return (0);  /* dest a not found */
}

dest_t *createdest(struct sockaddr_storage *x, socklen_t xlen)
{
  /* assumes that x refers to a destination that
     doesn't already exist in destarray[];
     creates a new entry for x and returns pointer to it 
     xlen says how many bytes of *x to use in comparisons
  */

  /*  DEBUG("sockaddr: %p  socklen: %d\n", x, xlen); */
  /* Gross hack for now; nicer error handling needed; eventually
     this should be a dynamically allocated structure that can grow */
  assert(ndests < MAXDEST);

  int i = ndests++; /* allocate an entry for this destination */
  dest_t *d = &destarray[i];
  cleardest(d);
  memcpy(&d->destaddr, x, xlen);
  return(d);
}

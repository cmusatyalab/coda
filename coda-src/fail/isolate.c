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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/fail/Attic/isolate.c,v 4.1 1997/12/23 17:19:27 braam Exp $";
#endif /*_BLURB_*/


#include "filtutil.h"


/* Inserts an all-blocking filter on every named machine.  On servers it
   inserts a filter that allows the server to talk to itself */
void isolate_targets(target_t *targets, int num_targets)
{
  int num;
  FailFilter *isolate, *server;

  create_filter(FILTER_ISOLATE, &isolate);
  create_filter(FILTER_SERVER, &server);

  if (!isolate || !server) {
    PrintError("Unable to create filters", 0);
    return;
  }

  for (num = 0; num < num_targets; num++)
    if (!open_connection(targets[num])) {
      insert_filter(isolate, 0);

      if (targets[num].server) {
	set_filter_host(targets[num], server);
	insert_filter(server, 0);
      }

      close_connection();
    }

  destroy_filter(&isolate);
  destroy_filter(&server);
}


void main(int argc, char **argv)
{
  int num_targets;
  target_t *targets;

  InitRPC();

  get_targets(argc, argv, &targets, &num_targets);

  if (num_targets)
    isolate_targets(targets, num_targets);
  else
    printf("usage: %s [-c client1 client2 ...] [-s server1 server2 ...]\n", argv[0]);
}

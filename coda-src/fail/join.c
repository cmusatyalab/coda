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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/fail/Attic/join.c,v 4.1 1997/12/23 17:19:28 braam Exp $";
#endif /*_BLURB_*/


#include "filtutil.h"


/* Inserts filters allowing communications in all directions between a
   group of targets */
void join_targets(target_t *targets, int num_targets)
{
  int i, j;
  FailFilter *join;

  create_filter(FILTER_JOIN, &join);

  if (!join) {
    printf("Unable to create filter\n");
    return;
  }

  for (i = 0; i < num_targets; i++)
    if (!open_connection(targets[i])) {
      for (j = 0; j < num_targets; j++)
	if (i != j) {
	  set_filter_host(targets[j], join);
	  insert_filter(join, 0);
	}
      close_connection();
    }

  destroy_filter(&join);
}


void main(int argc, char **argv)
{
  int num_targets;
  target_t *targets;

  InitRPC();

  get_targets(argc, argv, &targets, &num_targets);

  if (num_targets)
    join_targets(targets, num_targets);
  else
    printf("usage: %s [-c client1 client2 ...] [-s server1 server2 ...]\n", argv[0]);
}

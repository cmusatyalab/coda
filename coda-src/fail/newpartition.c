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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/fail/newpartition.c,v 4.2 1998/01/05 16:41:59 braam Exp $";
#endif /*_BLURB_*/


#include "filtutil.h"

/* For each of the two targets, install a filter that blocks communications
   to the other target */
void partition_targets(target_t target1, target_t target2)
{
  FailFilter *partition;

  create_filter(FILTER_PARTITION, &partition);

  if (!partition) {
    PrintError("Unable to create filter", 0);
    return;
  }

  if (!open_connection(target1)) {
    set_filter_host(target2, partition);
    insert_filter(partition, 0);
    close_connection();
  }

  if (!open_connection(target2)) {
    set_filter_host(target1, partition);
    insert_filter(partition, 0);
    close_connection();
  }

  destroy_filter(partition);
}


int partition(int argc, char **argv)
{
  int num_targets;
  target_t target1, target2;

  InitRPC();

  if (!get_targ_pair(argc, argv, &target1, &target2))
    partition_targets(target1, target2);
  else
    printf("usage: %s -[c|s] host1 -[c|s] host2\n", argv[0]);
  return -1;
}

/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


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

  destroy_filter(join);
}


int join(int argc, char **argv)
{
  int num_targets;
  target_t *targets;

  InitRPC();

  get_targets(argc, argv, &targets, &num_targets);

  if (num_targets)
    join_targets(targets, num_targets);
  else
    printf("usage: %s [-c client1 client2 ...] [-s server1 server2 ...]\n", argv[0]);
  return -1;
}

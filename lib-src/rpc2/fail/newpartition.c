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

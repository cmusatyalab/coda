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


/* For each of the two targets, remove all filters that have to do with the
   other target */
void heal_targets(target_t target1, target_t target2)
{
  int i, num_filters, num_matches;
  FailFilter *filters, *matches;

  if (!open_connection(target1)) {
    list_filters(&filters, &num_filters);

    match_filters(filters, num_filters, target2, &matches, &num_matches);

    if (!num_matches)
      printf("No suitable filters found to remove\n");

    for (i = 0; i < num_matches; i++)
      remove_filter(matches[i]);

    if (num_matches)
      free(filters);

    close_connection();
  }

  if (!open_connection(target2)) {
    list_filters(&filters, &num_filters);

    match_filters(filters, num_filters, target1, &matches, &num_matches);

    if (!num_matches)
      printf("No suitable filters found to remove\n");

    for (i = 0; i < num_matches; i++)
      remove_filter(matches[i]);

    if (num_matches)
      free(filters);

    close_connection();
  }
}


void main(int argc, char **argv)
{
  int num_targets;
  target_t target1, target2;

  InitRPC();

  if (!get_targ_pair(argc, argv, &target1, &target2))
    heal_targets(target1, target2);
  else
    printf("usage: %s -[c|s] host1 -[c|s] host2\n", argv[0]);
}

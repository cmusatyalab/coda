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


/* Removes all filters from all specified targets */
void clear_targets(target_t *targets, int num_targets)
{
  int num, num_filters, filt;
  FailFilter *filters;

  for (num = 0; num < num_targets; num++)
    if (!open_connection(targets[num])) {

      clear_filters();

      close_connection();
    }
}


int clear(int argc, char **argv)
{
  int num_targets;
  target_t *targets;

  InitRPC();

  get_targets(argc, argv, &targets, &num_targets);

  if (num_targets) {
    clear_targets(targets, num_targets);
    return 0;
  }  else {
	  printf("usage: %s [-c client1 client2 ...] [-s server1 server2 ...]\n", 
	   argv[0]);
	  return -1;
  }
  
}

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

  destroy_filter(isolate);
  destroy_filter(server);
}


int isolate(int argc, char **argv)
{
  int num_targets;
  target_t *targets;

  InitRPC();

  get_targets(argc, argv, &targets, &num_targets);

  if (num_targets)
    isolate_targets(targets, num_targets);
  else
    printf("usage: %s [-c client1 client2 ...] [-s server1 server2 ...]\n", argv[0]);
  return -1;
}

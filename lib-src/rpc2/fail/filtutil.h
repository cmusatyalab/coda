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


#include <sys/param.h>
#include "fail.h"

typedef struct target {
  char hostname[MAXHOSTNAMELEN];
  int  server;
} target_t;

typedef enum {
  FILTER_ISOLATE = 0,
  FILTER_PARTITION = 1,
  FILTER_SERVER = 2,
  FILTER_JOIN = 3,
} filter_type;

#define MAXFILTERTYPE 3

void create_filter(filter_type, FailFilter **);
void destroy_filter(FailFilter *);
int set_filter_host(target_t, FailFilter *);
int insert_filter(FailFilter *, int);
int match_filters(FailFilter *, int, target_t, FailFilter **, int *);
void remove_filter(FailFilter);
int clear_filters();
int list_filters(FailFilter **, int *);
int show_filter(FailFilter);

void get_targets(int, char **, target_t **, int *);
int get_targ_pair(int, char **, target_t *, target_t *);

void InitRPC();
int open_connection(target_t);
void close_connection();
void PrintError(char *, int);

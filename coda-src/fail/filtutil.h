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

static char *rcsid = "$Header: $";
#endif /*_BLURB_*/


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

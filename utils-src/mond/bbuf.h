#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/bbuf.h,v 3.2 1995/10/09 19:26:38 satya Exp $";
#endif /*_BLURB_*/




// bbuf1.h
//
// class and method defns for the bbuf

#ifndef _BBUF_H_
#define _BBUF_H_

enum BbufStatus { EBBUFMT, EBBUFFL, BBUFOK};

// parameterized types would be *so* much easier (grumble)

typedef vmon_data *bbuf_item;

class bbuf {
  bbuf_item *buf;
  int        bnd;
  int        head,tail,count;
  int        low_fuel_mark; // point at which you stop spooling out.;
  bool       dbg;
  MUTEX      lock;          // simple mutex scheme - maybe change later;
  CONDITION  low_fuel;      // signal spool out threads;
  CONDITION  full_tank;     // signal spool in threads;
  void       bbuf_error(char*,int);
 public:
  bool       full(void);
  bool       empty(void);
  BbufStatus insert(bbuf_item);
  BbufStatus remove(bbuf_item*);
//  void       print_it(void);
  void       debug(bool);
  void       flush_the_tank(void);
  bbuf(int, int =0);
  ~bbuf(void);
  inline void examine(void) {fprintf(stderr,"head %d, tail %d, count %d\n",
				     head,tail,count); }
};

#endif _BBUF_H_

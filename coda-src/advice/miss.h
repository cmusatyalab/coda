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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/advice/Attic/miss.h,v 4.2 1997/12/16 15:54:52 braam Exp $";
#endif /*_BLURB_*/




class miss {
  friend int PathnamePriorityFN(bsnode *, bsnode *);
  friend void PrintMissList(char *);
  friend void ReinstatePreviousMissQueue();
    char *path;
    char *program;
    int num_instances;

    bsnode queue_handle;                 /* link for the cache miss queues */

  public:
    miss(char *Path, char *Program);
    miss(miss&);                         /* not supported! */
    operator=(miss&);                    /* not supported! */
    ~miss();
    void print(FILE *f);
};

extern void InitMissQueue();
extern void OutputMissStatistics();

#define TMPMISSLIST "/tmp/misslist"


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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/Attic/timing_paths.h,v 4.1 1997/01/08 21:51:15 rvb Exp $";
#endif /*_BLURB_*/






#include <cargs.h>
enum timewrt {BASELINE = 1, DELTA =2};



struct tientry
    {/* one entry of timing array */
    long id;
    volatile long dt2806[12];
    long bogus;  /* indicates bad reading due to ripple */
    struct timeval tval;
    };


#ifdef ibm032
struct dtc_counters
    {
    long  timer2;
    long  timer1;
    long  timer0;
    };
#else
 /* no other machines at present */
#endif ibm032


struct tie
    {
    enum timewrt timtype;
    long nEntries;
    long inuse; /* how many entries of tiarray have been used */
    long num_bogus; /*Keeps track of the total number of bogus reads */
    struct tientry *tiarray;  /* malloc'ed by ti_create() */
#ifdef ibm032
    struct dtc_counters initcounters; /* value of timer registers
                                     at creation */
#endif ibm032
    };

    
struct pth
    {
    short p_name;
    short p_length;
    short freq_occ;
    struct p_ind *start_ind;
    struct pth *nxt_pth;
    };

struct pths_info
    {
    struct pth *first_cand;
    int total_num_paths;
    };


struct p_ind
   {
   int ind;
   struct p_ind *nxt_ind;
   };



/* Functions in package */

extern ti_init();
extern ti_end();
extern ti_create C_ARGS((int nEntries, struct tie *thistie));
extern ti_destroy C_ARGS((struct tie *thistie));
extern ti_notetime C_ARGS((struct tie *thistie, long id));
extern ti_postprocess C_ARGS((struct tie *thistie, enum timewrt twrt));
extern ti_discoverpaths C_ARGS((struct tie *thistie,struct pths_info *pinfo));
extern ti_stat C_ARGS((struct tie *thistie,struct pths_info *pinfo));

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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/util/histo.h,v 1.1.1.1 1996/11/22 19:08:10 rvb Exp";
#endif /*_BLURB_*/









#ifndef _HISTO_
#define _HISTO_

/* Histogram scaling */
enum htype {LINEAR=1, LOG2=2, LOG10=3};
#define LN2 0.69315  /* natural logarithm of 2.0 */
#define RAISE2(x)  (pow(2.0, 1.0*(x)))
#define RAISE10(x)  (pow(10.0, 1.0*(x)))


/* One entry in a histogram. */
struct histo
    {
    /* I know it's not the most efficient in storage.
       hival and loval are not both necessary.
       But it makes life so much simpler!
    */
    double loval;  /* matching values are >= loval */
    double hival;  /* matching values are < hival */
    int count;  /* number of matching values */
    };

/* An entire histogram */
struct hgram
    {
    int maxb;               /* number of buckets */
    enum htype type;	    /* what kind of histogram */
    struct histo *buckets;  /* malloc'ed array of maxb buckets */
    struct histo oflow;  /* values > buckets[maxb-1].hival */
    struct histo uflow; /* values < buckets[0].loval */
    int count;   /* total no of entries not in oflow or uflow */
    double sum;  /* sum of values (not in oflow or uflow) */
    double sum2; /* sum of squares of values (not in oflow or uflow) */
    };



/* Functions */
#ifndef C_ARGS
#if (__cplusplus | __STDC__)
#define C_ARGS(arglist) arglist
#else __cplusplus
#define C_ARGS(arglist)()
#endif __cplusplus
#endif C_ARGS

extern int InitHisto C_ARGS((struct hgram *, double, double, int,enum htype));
extern void ClearHisto C_ARGS((struct hgram *));
extern void UpdateHisto C_ARGS((struct hgram *, double));
extern void MUpdateHisto C_ARGS((struct hgram *, double, int));
extern int PrintHisto C_ARGS((FILE *, struct hgram *));
extern int PlotHisto C_ARGS((FILE *, struct hgram *, char *, char *, char *, char*));
#endif _HISTO_

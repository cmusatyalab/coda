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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/resolution/timing.h,v 4.1 1997/01/08 21:50:06 rvb Exp $";
#endif /*_BLURB_*/





#ifndef _RES_TIMING_H_
#define _RES_TIMING_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/time.h>
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus


#define TIMEGROWSIZE 10
extern int pathtiming;
extern int probingon;
#define MAXPROBES 1000
#define PROBE(info, num) \
if (pathtiming && probingon && (info)) \
     (info)->insert((num));

/* probe numbers */
#define RESBEGIN	0
#define RUNTUPDATEBEGIN 1
#define RUNTUPDATEEND	2
#define WEAKEQBEGIN	3
#define COLLECTLOGBEGIN	4 	/* begin of regular res */
#define COLLECTLOGEND	5
#define COORP1BEGIN	6
#define COORP1END	7
#define P1PANDYBEGIN	8
#define COORP2BEGIN	9
#define COORP3BEGIN	10
#define COORP3END	11
#define COORMARKINCBEGIN 12
#define COORMARKINCEND	13
#define RESEND		14
#define CFETCHLOGBEGIN	15
#define CFETCHLOGEND	16
#define CPHASE1BEGIN	17
#define CPHASE1END	18
#define COMPOPSBEGIN 	19
#define COMPOPSEND	20
#define PERFOPSBEGIN	21
#define PERFOPSEND	22
#define P1PUTOBJBEGIN	23
#define P1PUTOBJEND	24
#define CPHASE2BEGIN	25
#define CPHASE2END	26
#define CPHASE3BEGIN	27
#define CPHASE3END	28

/* for timing file resolution */
#define FILERESBASE	50

#define COORDSTARTVICERESOLVE	FILERESBASE+1
#define COORDSTARTFILERES	FILERESBASE+2
#define COORDSTARTFILEFETCH	FILERESBASE+3
#define COORDENDFILEFETCH	FILERESBASE+4
#define COORDENDFORCEFILE	FILERESBASE+5
#define COORDENDFILERES		FILERESBASE+6
#define COORDENDVICERESOLVE	FILERESBASE+7

struct tpe {
    int id;
    struct timeval tv;
};

class timing_path {
    int	nentries;
    int maxentries; 
    tpe *arr;
    void grow_storage();

  public:
    timing_path(int);
    ~timing_path();
    void insert(int);
    void postprocess();
    void postprocess(FILE *);
    void postprocess(int);
};

extern timing_path *tpinfo;
extern timing_path *FileresTPinfo; 
#endif _RES_TIMING_H

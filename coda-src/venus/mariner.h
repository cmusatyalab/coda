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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/mariner.h,v 4.1 1997/01/08 21:51:32 rvb Exp $";
#endif /*_BLURB_*/








/*
 *
 * Specification of the Venus Mariner facility.
 *
 */

#ifndef _VENUS_MARINER_H_
#define _VENUS_MARINER_H_ 1


class mariner;
class mariner_iterator;


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "vproc.h"


const int MWBUFSIZE = 80;

extern int MarinerMask;

extern void MarinerInit();
extern void MarinerMux(int);
extern void MarinerLog(char * ...);
extern void MarinerReport(ViceFid *, vuid_t);
extern void PrintMariners();
extern void PrintMariners(FILE *);
extern void PrintMariners(int);


class mariner : public vproc {
  friend void MarinerInit();
  friend void MarinerMux(int);
  friend void MarinerLog(char * ...);
  friend void MarinerReport(ViceFid *, vuid_t);
  friend void PrintMariners(int);

    static int muxfd;
    static int nmariners;

    unsigned DataReady : 1;
    unsigned dying : 1;
    unsigned logging : 1;	    /* for MarinerLog() */
    unsigned reporting : 1;	    /* for MarinerReport() */
    vuid_t vuid;		    /* valid iff reporting = 1 */
    int fd;
    char commbuf[MWBUFSIZE];

    mariner(int);
    operator=(mariner&);    /* not supported! */
    virtual ~mariner();

    int Read();
    int Write(char * ...);
    void AwaitRequest();
    void Resign(int);
    void PathStat(char *);
    void FidStat(ViceFid *);
    void Rpc2Stat();

  public:
    void main(void *);
};


class mariner_iterator : public vproc_iterator {

  public:
    mariner_iterator();
    mariner *operator()();
};

#endif	not _VENUS_MARINER_H_

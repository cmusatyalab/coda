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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/advice.h,v 4.1 97/01/08 21:51:17 rvb Exp $";
#endif /*_BLURB_*/



/*
 *
 * Specification of the Venus Advice Monitor enumerated types.
 *
 */

#ifndef _ADVICE_H_
#define _ADVICE_H_

enum AdviceState {AdviceInvalid, AdviceDying, AdviceWaiting, AdviceValid};

enum ReadDiscAdvice {ReadDiscUnknown=-1, 
		     ReadDiscFetch, 
		     ReadDiscHOARDimmedFETCH, 
		     ReadDiscHOARDdelayFETCH, 
		     ReadDiscTimeout};
const int MaxReadDiscAdvice = 3;

enum WeaklyAdvice {WeaklyUnknown=-1,
		   WeaklyFetch,
		   WeaklyMiss};
const int MaxWeaklyAdvice = 1;

extern int ASRinProgress;
extern int ASRresult;
#define ASR_INTERVAL 300

#define ADMON_FAIL -1
#define ADMON_SUCCESS 0
#define ADMON_DUPLICATE 1

#define HOARDLIST_FILENAME "/tmp/hoardlist."
#define HOARDADVICE_FILENAME "/tmp/hoardadvice."

#define PROGRAMLOG "program.log"
#define REPLACEMENTLOG "replacement.log"

/* User Patience Parameters. */
const int UNSET_PATIENCE_ALPHA = -1;
const int DFLT_PATIENCE_ALPHA = 6;
const int UNSET_PATIENCE_BETA = -1;
const int DFLT_PATIENCE_BETA = 1;
const int UNSET_PATIENCE_GAMMA = -1;
const int DFLT_PATIENCE_GAMMA = 10000; 
/*
 * Note GAMMA is reciprocal of that described in the wcc submission to SOSP15.  
 * It is also adjusted to cope with the fact that priorities are 1-100000,
 * rather than 1-1000 as advertised. 
 */

extern int PATIENCE_ALPHA;
extern int PATIENCE_BETA;
extern int PATIENCE_GAMMA;

#endif _ADVICE_H_

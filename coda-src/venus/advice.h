/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _ADVICE_H_
#define _ADVICE_H_

enum ConnectionState {Nil, Valid, Init, Dead};

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

#define ASR_INTERVAL 300

#define HOARDLIST_FILENAME "/tmp/hoardlist."
#define HOARDADVICE_FILENAME "/tmp/hoardadvice."

#define PROGRAMLOG "program.log"
#define REPLACEMENTLOG "replacement.log"

#define NumRPCResultTypes 7

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

#endif /* _ADVICE_H_ */

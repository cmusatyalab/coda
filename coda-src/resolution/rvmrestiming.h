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






#ifndef _RVMRES_TIMING_H_
#define _RVMRES_TIMING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
#include <stdio.h>

#ifdef __cplusplus
}
#endif


#define TIMEGROWSIZE 10
extern int pathtiming;
extern int probingon;
#define MAXPROBES 1000
#define PROBE(info, num) \
if (pathtiming && probingon && (info)) \
     (info)->insert((num));

/* probe numbers */
#define RESBEGIN	0

#define RecovTimingBase		50

#define RecovCoorP1Begin 	RecovTimingBase + 1
#define RecovSubP1Begin		RecovTimingBase + 2
#define RecovSubP1End		RecovTimingBase + 3
#define RecovCoorP1End		RecovTimingBase + 4

#define RecovCoorP2Begin	RecovTimingBase + 5
#define RecovSubP2Begin		RecovTimingBase + 6
#define RecovSubP2End		RecovTimingBase + 7 
#define RecovCoorP2End		RecovTimingBase + 8 

#define RecovCoorP3Begin	RecovTimingBase + 9 
#define RecovSubP3Begin		RecovTimingBase + 10
#define RecovCompOpsBegin	RecovTimingBase + 11
#define RecovCompOpsEnd  	RecovTimingBase + 12
#define RecovPerformResOpBegin	RecovTimingBase + 13 
#define RecovPerformResOpEnd	RecovTimingBase + 14
#define RecovSubP3End		RecovTimingBase + 15 
#define RecovCoorP3End		RecovTimingBase + 16

#define RecovCoorP34Begin	RecovTimingBase + 17
#define RecovSubP34Begin	RecovTimingBase + 18
#define RecovSubP34End		RecovTimingBase + 19
#define RecovCoorP34End		RecovTimingBase + 20

#define RecovCoorP4Begin	RecovTimingBase + 21
#define RecovSubP4Begin		RecovTimingBase + 22
#define RecovSubP4End		RecovTimingBase + 23 
#define RecovCoorP4End		RecovTimingBase + 24




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

#endif /* _RVMRES_TIMING_H_ */

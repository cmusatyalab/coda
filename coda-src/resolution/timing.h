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





#ifndef _RES_TIMING_H_
#define _RES_TIMING_H_

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

#endif /* _RES_TIMING_H_ */


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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/librepair/restest.cc,v 4.1 1997/01/08 21:50:09 rvb Exp $";
#endif /*_BLURB_*/







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include "resolve.h"
#include "repio.h"

extern int getunixdirreps C_ARGS((int , char **, resreplica **));
extern int dirresolve C_ARGS((int , resreplica *, int (*)(char *), struct listhdr **));

void main C_ARGS((int argc, char **argv))
{
    int	nreplicas;
    resreplica *dirs;
    struct listhdr *k;

    if (argc < 5) {
	printf("There must be atleast 2 directories to resolve \n");
	printf("Usage: resolve <number of dirs> <replicatedVolumeNumber> <dir1> <dir2> ...\n");
	exit(-1);
    }
    
    nreplicas = atoi(argv[1]);
    RepVolume = atoi(argv[2]);
    getunixdirreps(nreplicas, &(argv[3]), &dirs);
    dirresolve(nreplicas, dirs, NULL, &k);
    printf("There are %d conflicts \n", nConflicts);
    /* print the listhdr structure */
    for(int i = 0; i < nreplicas; i++){
	printf("\nreplica %lu \n", k[i].replicaId);
	for (int j = 0; j < k[i].repairCount; j++)
	    repair_printline(&(k[i].repairList[j]), stdout);
    }
    resClean(nreplicas, dirs, k);
}

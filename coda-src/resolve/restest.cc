/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include "resolve.h"
#include "repio.h"

extern int getunixdirreps (int , char **, resreplica **);
extern int dirresolve (int , resreplica *, int (*)(char *), struct listhdr **);

void main (int argc, char **argv)
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

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

#include <stdio.h>



void MallocStats(char *m, FILE *fp) {
    fprintf(fp, "Dummy MallocStats (%s)\n", m);
}


long CheckAllocs(char *m) {
    return(0);
}


void plumber(FILE *fp) {
    fprintf(fp, "Dummy Plumber\n");
}

#ifdef __cplusplus
}
#endif __cplusplus

void newPlumber(FILE *fp) {
    fprintf(fp, "Dummy newPlumber\n");
}

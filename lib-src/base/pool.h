/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include "bitvect.h"

typedef struct Pool *PPool;

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

void P_Destroy(PPool *pool);
PPool P_New(int count, int size);
void *P_Malloc(PPool);
void P_Free(PPool, void *);

#ifdef __cplusplus
}
#endif __cplusplus


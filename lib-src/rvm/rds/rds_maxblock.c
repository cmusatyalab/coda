/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/* rds_maxblock - check if we will be able to allocate enough RVM data
 * written by J. Michael German */

#include "rds_private.h"

int rds_maxblock(unsigned long size) 
{
    unsigned long nblocks = size / RDS_CHUNK_SIZE;
    free_block_t *tempbp;
    int i;

    if (RDS_FREE_LIST[RDS_MAXLIST].head != NULL) {
	tempbp = RDS_FREE_LIST[RDS_MAXLIST].head;
	while (tempbp) {
	    if (tempbp->size >= nblocks) 
		return 1;
	    tempbp = tempbp->next;
	}
    } else {
	for (i = RDS_MAXLIST - 1; i > nblocks; i--) {
	    if (RDS_FREE_LIST[i].head) 
		return 1;
	}
    }
    return 0;
}


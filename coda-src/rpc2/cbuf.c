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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include "coda_string.h"
#include "cbuf.h"
#include "rpc2.h"
#include "rpc2.private.h"

#ifdef RPC2DEBUG

struct CBUF_Header *CBUF_Init(long elemSize, long noofElems, char *printName)
{
    struct CBUF_Header *bufId;

    bufId = (struct CBUF_Header *)malloc(sizeof(struct CBUF_Header));
    if (bufId == NULL) return(NULL);
    bufId->ElemSize = elemSize;
    bufId->NoOfElems = noofElems;
    bufId->LastAllocatedSlot = -1;
    bufId->TotalElemsAdded = 0;
    strncpy(bufId->PrintName, printName, sizeof(bufId->PrintName) - 1);
    bufId->Buffer = (char *)malloc(elemSize*noofElems);    
    if (bufId->Buffer == NULL && noofElems != 0) return(NULL);
    return(bufId);
}

char *CBUF_NextSlot(struct CBUF_Header *bufId)
{
    char *p;
    bufId->TotalElemsAdded++;
    bufId->LastAllocatedSlot++;
    if (bufId->LastAllocatedSlot > bufId->NoOfElems-1)
	bufId->LastAllocatedSlot = 0;
    p = bufId->Buffer + (bufId->LastAllocatedSlot)*(bufId->ElemSize);
    return(p);
}


void CBUF_WalkBuff(struct CBUF_Header *bufId, void (*userProc)(), long howMany,
		   FILE *outFile)
/* userProc	called with (<ptr to elem>, <index of elem>, outFile)
 * howMany;	userProc is invoked only for the last howMany elems
 * outFile;	opened for writing already */
{
    long i, j;

    if (bufId->TotalElemsAdded <= bufId->NoOfElems)
    {
	for (i = 0; i < bufId->TotalElemsAdded; i++)
	    if (bufId->TotalElemsAdded - i <= howMany)
		(*userProc)(bufId->Buffer+(i*bufId->ElemSize), i, outFile);
    }
    else
    {
	i = (bufId->LastAllocatedSlot == bufId->NoOfElems-1) ? 0 :
	    bufId->LastAllocatedSlot+1;

	for (j = 0; j < bufId->NoOfElems; j++)
	{
	    if (bufId->NoOfElems - j <= howMany)
		(*userProc)(bufId->Buffer+(i*bufId->ElemSize),
			    bufId->TotalElemsAdded - bufId->NoOfElems + j,
			    outFile);

	    if (i == bufId->NoOfElems-1) i = 0;
	    else			 i++;
	}
    }
    
}


void CBUF_Free(struct CBUF_Header **whichBuff)
{
    free((*whichBuff)->Buffer);
    free(*whichBuff);
    *whichBuff = NULL;
}
#endif RPC2DEBUG

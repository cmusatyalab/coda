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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rpc2/Attic/cbuf.c,v 4.1.12.1 1998/05/02 21:40:38 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/

#include <stdio.h>
#include <strings.h>
#include <sys/time.h>

#include "cbuf.h"
#include <lwp.h>
#include <timer.h>
#include "rpc2.h"
#include "rpc2.private.h"

#ifdef RPC2DEBUG

#ifdef CMUCS
#include <stdlib.h>
#endif CMUCS

struct CBUF_Header *CBUF_Init(elemSize, noofElems, printName)
    long elemSize;
    long noofElems;
    char *printName;
    {
    register struct CBUF_Header *bufId;

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

char *CBUF_NextSlot(bufId)
    register struct CBUF_Header *bufId;
    {
    register char *p;
    bufId->TotalElemsAdded++;
    bufId->LastAllocatedSlot++;
    if (bufId->LastAllocatedSlot > bufId->NoOfElems-1) bufId->LastAllocatedSlot = 0;
    p = bufId->Buffer + (bufId->LastAllocatedSlot)*(bufId->ElemSize);
    return(p);
    }


void CBUF_WalkBuff(bufId, userProc, howMany, outFile)
    register struct CBUF_Header *bufId;
    void (*userProc)();	/* called with (<ptr to elem>, <index of elem>, outFile) */
    long howMany;	/* userProc is invoked only for the last howMany elems */
    FILE *outFile;	/* opened for writing already */
    {
    register long i, j;
    if (bufId->TotalElemsAdded <= bufId->NoOfElems)
	{
	for (i = 0; i < bufId->TotalElemsAdded; i++)
	    if (bufId->TotalElemsAdded - i <= howMany) (*userProc)(bufId->Buffer+(i*bufId->ElemSize), i, outFile);
	}
    else
	{
	for (j = 0, i = (bufId->LastAllocatedSlot == bufId->NoOfElems-1) ? 0 : bufId->LastAllocatedSlot+1;
		 j < bufId->NoOfElems; j++, (i == bufId->NoOfElems-1) ?  (i = 0) : i++)
	    if (bufId->NoOfElems - j <= howMany)
		(*userProc)(bufId->Buffer+(i*bufId->ElemSize), bufId->TotalElemsAdded - bufId->NoOfElems + j, outFile);
	}
    
    }


void CBUF_Free(whichBuff)
    struct CBUF_Header **whichBuff;
    {
    free((*whichBuff)->Buffer);
    free(*whichBuff);
    *whichBuff = NULL;
    }
#endif RPC2DEBUG

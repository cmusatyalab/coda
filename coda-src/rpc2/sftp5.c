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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rpc2/sftp5.c,v 4.2 1998/09/15 14:28:01 jaharkes Exp $";
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

/*
	-- Bit string manipulation routines
	--sftp.h contains some macros too
*/


#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"
#include "se.h"
#include "sftp.h"

void B_ShiftLeft(bMask, bShift)		/* rightmost bits are ZERO-filled */
    unsigned int *bMask;		/* Bit string integer array */
    int bShift;		/* Amount to shift by */
    {
    /*  The bit string is made up of an integral number of integer parts. (assumption)
	Each integer part is affected by at most two other integer parts, adjacent to each other.
	In each iteration,
	    current:	points to the next integer part of the bit string.
	    first:	points to first integer part affecting current
	(32-shift) low-order bits of *first will become the high order bits of *current
	(shift) high-order bits of *(first+1) will become the low-order bits of *current.
    */
    
    register unsigned int shift, *current, *first, *last;
    
    shift = bShift & 31;	/* modulo 32 */
    
    current = bMask;
    first = bMask + (bShift >> 5);
    last = bMask + BITMASKWIDTH - 1;

    while(first < last)
	{
	if(shift == 0)
		*current = *first;
	else
		*current = ((*first) << shift) | ((*(first+1)) >> (32-shift));
	current++;
	first++;
	}
    if (first == last)
	{
	*current = ((*first) << shift);
	current++;
	}
    while (current <= last)
	{
	*current++ = 0;
	}
    }


void B_ShiftRight(bMask, bShift)		/* leftmost bits are ONE-filled */
    unsigned int *bMask;		/* Bit string integer array */
    int bShift;		/* Amount to shift by */
    {
    /*  The bit string is made up of an integral number of integer parts. (assumption)
	Each integer part is affected by at most two other integer parts, adjacent to each other.
	In each iteration,
	    current:	points to the next integer part of the bit string.
	    first:	points to first integer part affecting current
	(32-shift) high-order bits of *first will become the low order bits of *current
	(shift) low-order bits of *(first-1) will become the high-order bits of *current.
    */
    
    register unsigned int shift, *current, *first;
    
    shift = bShift & 31;	/* modulo 32 */
    current = bMask + BITMASKWIDTH - 1;
    first = current - (bShift >> 5);

    while(first > bMask)
	{
	if(shift == 0)
		*current-- = *first;
	else
		*current-- = ((*first) >> shift)| ((*(first-1)) << (32-shift));
	first--;
	}
    if (first == bMask)
	{
	if(shift == 0)
		*current-- = *first;
	else
		*current-- = ((*first) >> shift) | (0xFFFFFFFF << (32-shift));
	}
    while (current >= bMask)
	{
	*current-- = 0xFFFFFFFF;
	}
    }


void B_Assign(dest, src)
    register unsigned int *dest, *src;
    {
    bcopy(src, dest, sizeof(int)*BITMASKWIDTH);
    }


void B_CopyToPacket(bMask, whichPacket)
    register unsigned int *bMask;
    register RPC2_PacketBuffer *whichPacket;
    {
    CODA_ASSERT(BITMASKWIDTH <= 2);	/* for now */
    whichPacket->Header.BitMask0 = (unsigned) bMask[0];
    whichPacket->Header.BitMask1 = (unsigned) bMask[1];
    }

void B_CopyFromPacket(whichPacket, bMask)
    register unsigned int *bMask;
    register RPC2_PacketBuffer *whichPacket;
    {
    CODA_ASSERT(BITMASKWIDTH <= 2);	/* for now */
    bMask[0] = (unsigned) whichPacket->Header.BitMask0;
    bMask[1] = (unsigned) whichPacket->Header.BitMask1;
    }


void B_And(arg1, arg2)	    /* *arg1 = *arg1 & *arg2 */
    unsigned int    *arg1, *arg2;
    {
    int	i;

    for (i = 0; i < BITMASKWIDTH; i++, arg1++, arg2++)
	*arg1 = (*arg1 & *arg2);
    }


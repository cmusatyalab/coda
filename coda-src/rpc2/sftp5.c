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

/*
	-- Bit string manipulation routines
	--sftp.h contains some macros too
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include "coda_string.h"
#include <lwp/lwp.h>
#include <lwp/timer.h>
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
    
    unsigned int shift, *current, *first, *last;
    
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
    
    unsigned int shift, *current, *first;
    
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
    unsigned int *dest, *src;
    {
    bcopy(src, dest, sizeof(int)*BITMASKWIDTH);
    }


void B_CopyToPacket(bMask, whichPacket)
    unsigned int *bMask;
    RPC2_PacketBuffer *whichPacket;
    {
    CODA_ASSERT(BITMASKWIDTH <= 2);	/* for now */
    whichPacket->Header.BitMask0 = (unsigned) bMask[0];
    whichPacket->Header.BitMask1 = (unsigned) bMask[1];
    }

void B_CopyFromPacket(whichPacket, bMask)
    unsigned int *bMask;
    RPC2_PacketBuffer *whichPacket;
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


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


#ifdef ibm032
#include <frame.h>

/* Fast exclusive-OR encryption routine.  Assumes that source, destination and key are fullword
	aligned, that the length to be encrypted is a multiple of 4 bytes, and source and destination
	are identical or disjoint (no overlap).  Also assumes key is 8 bytes long.
	NO CHECKING IS DONE.	CAVEAT CALLER!!

Called: 	fxor(a, b, c, d)
		R2: a = pointer to source
		R3: b = pointer to dest
		R4: c = length of data
		R5: d = pointer to key

The basic loop is unrolled so that 2 4-byte chunks are xored in each iteration.
R2 and R3 are always incremented, R4 is always decremented, and R5 is reset each time.

*/
	    .globl	.oVncs
	    .set	.oVncs,0

	    .text
	    .globl	_.fxor

_.fxor:  stm		r6,REG_OFFSET(sp)   # save registers
	    lis		r6,4		    # Squirrel away valuable constant in R6
	    lis		r10,8		    # and another in R10
	    l		r7,0(r5)	    # Prefetch lower part of key in R7 
	    l		r8,4(r5)	    # ... and upper part in R8

loop:
	    cis		r4,0	# any more bytes left?
	    blex	quit	# if not, we are done
	    s		r4,r6	# decrement bytes left
	    l		r9,0(r2)# get next 4 source bytes
	    x		r9,r7	# xor it with top key half
	    st		r9,0(r3)# and put it at the destination

	    cis		r4,0	# any more bytes left?
	    blex	quit	# if not, we are done
	    s		r4,r6	# decrement bytes left
	    l		r9,4(r2)# get next 4 source bytes
	    x		r9,r8	# xor it with bottom key half
	    st		r9,4(r3)# and put it at the destination
	    
	    a		r2,r10	# bump source pointer
	    bx		loop
	    a		r3,r10	# bump dest pointer
	    
	    

	    # End of xoring
quit:
	    lm		r6,REG_OFFSET(sp)   # restore regs
	    br		r15


	    .data
	    .globl	_fxor
_fxor:


#endif ibm032

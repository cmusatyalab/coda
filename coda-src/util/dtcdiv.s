#ifndef _BLURB_
#define _BLURB_
#ifdef undef
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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/util/dtcdiv.s,v 1.1.1.1 1996/11/22 19:08:07 rvb Exp";
#endif undef
#endif /*_BLURB_*/






/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 * 23-May-90  David Steere (dcs) at Carnegie-Mellon University
 *	Created for the Coda File System.
 *
 * dtcdiv.s,v
 * Revision 1.1.1.1  1996/11/22 19:08:07  rvb
 *  almost done
 *
 * Revision 1.1  1996/11/22 19:08:07  braam
 * First Checkin (pre-release)
 *
Revision 1.1  96/11/22  13:27:59  raiff
First Checkin (pre-release)

Revision 3.3.1.1  96/08/26  12:30:33  raiff
Branch for release beta-26Aug1996_41240

Revision 3.3  96/08/01  09:54:01  satya
Syntactic changes for NetBSD port

Revision 3.2.4.1  96/07/24  11:08:33  raiff
Branch for release beta-24Jul1996_36690

Revision 3.2  95/10/09  19:31:18  satya
Reblurbed with new CMU and IBM notices for SOSP-15 CD-ROM

Revision 3.1  95/06/08  16:08:34  satya
*** empty log message ***

Revision 2.1.7.1  95/05/11  11:32:35  raiff
Branch for release beta-11May1995_36561

Revision 2.1  94/07/21  16:41:14  satya
Conversion to C++ 3.0; start of Coda Release 2.0

Revision 1.1  92/04/03  17:39:58  satya
Initial revision

Revision 1.3  90/06/28  17:07:14  dcs
Added symbols for ibmrt's for port to PMAX.

Revision 1.2  90/06/18  19:02:49  dcs
Changed names from dcs* to dtc* to agree with kernel name change.

Revision 1.1  90/05/31  17:04:24  dcs
Initial revision

 * 
 */

#ifdef	romp
#include <dtc.h>
#if	NDTC > 0

 #	Divide of dtcsec//dtcusec by 2000000
 #		-- We have a 48 bit counter that holds the number of 
 #		   1/2 microsecond ticks that have elapsed since the clock
 #		   was started. This routine converts this number into
 #		   seconds and microseconds

 #				r2//mq = dividend
 #				r3 = divisor
 #				r15= link
 #			

.set mq      ,       r10                # multiplier quotient system control reg
 
	.globl	.oVncs
	.set	.oVncs,0

	.globl  _dtcsec
	.globl  _dtcusec
	.globl	_twomeg
	.data
L00_DATA:
	.set	_twomeg,L00_DATA+0x00000000
	.long	2000000

	.text
	.globl	_.dtcdiv
_.dtcdiv:
	mr	r0,r4
	load    r2,_dtcsec
	load	r4,_dtcusec
	mts	%mq,r4
	load	r3,_twomeg

here: 				# 32 divide steps:
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3		# 8

        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3		# 16

        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3		# 24

        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3
        d       r2,r3		# 32
 
	bc0x	1f		# if carry-0, no correction needed
	mfs	%mq,r4		# (retrieve the quotient)
	a	r2,r3		# correction: complete the final restore

1:	
	#the remainder is actually the number of 1/2 usecs, so 
	sri	r2,1
        #return to the calling routine
        store	r4,_dtcsec,r3
	store	r2,_dtcusec,r3
	brx	r15
	mr	r4,r0


	.globl	_dtcdiv
	.data
_dtcdiv:

#endif	/* NDTC */
#endif	romp

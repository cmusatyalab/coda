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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/Attic/dtcreg.h,v 4.1 1997/01/08 21:51:04 rvb Exp $";
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
 * $Log: dtcreg.h,v $
 * Revision 4.1  1997/01/08 21:51:04  rvb
 * r = 4.1; fix $ HEADERS
 *
 * Revision 1.1  1996/11/22 19:08:08  braam
 * First Checkin (pre-release)
 *
 * Revision 1.1  96/11/22  13:28:03  raiff
 * First Checkin (pre-release)
 * 
 * Revision 3.3.1.1  96/08/26  12:30:36  raiff
 * Branch for release beta-26Aug1996_41240
 * 
 * Revision 3.3  96/08/01  09:42:18  satya
 * Syntactic changes for NetBSD port
 * 
 * Revision 3.2.4.1  96/07/24  11:08:36  raiff
 * Branch for release beta-24Jul1996_36690
 * 
 * Revision 3.2  95/10/09  19:31:18  satya
 * Reblurbed with new CMU and IBM notices for SOSP-15 CD-ROM
 * 
 * Revision 3.1  95/06/08  16:08:34  satya
 * *** empty log message ***
 * 
 * Revision 2.1.7.1  95/05/11  11:32:36  raiff
 * Branch for release beta-11May1995_36561
 * 
 * Revision 2.1  94/07/21  16:41:15  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 * 
 * Revision 1.1  92/04/03  17:39:58  satya
 * Initial revision
 * 
 * Revision 1.2  90/06/18  19:03:14  dcs
 * Changed names from dcs* to dtc* to agree with kernel name change.
 * 
 * Revision 1.1  90/05/31  17:04:40  dcs
 * Initial revision
 * 
 * 
 */

#ifndef _DTCREG_H_
#define _DTCREG_H_

/* location of major registers on Data Translation 2806 board */

#define	Timer0	(caddr_t) 0xF0000370
#define Timer1	(caddr_t) 0xF0000770
#define Timer2	(caddr_t) 0xF0000B70
#define ModeReg (caddr_t) 0xF0000F70
#define FPLA	(caddr_t) 0xF0001770

/*
 * Support for the IOCTLs for DTC .
 */
struct dtcclk {
	long dtc_low, dtc_high;
	};

#define DTCSCLK	_IO(d,1)		/* set device bounds */
#define DTCBCLK	_IOR(d,3, struct dtcclk) /* return bound settings */
#define DTCRCLK	_IOR(d,2, struct dtcclk) /* return bound settings */
#define DTCRLS  _IOR(d,4, struct timeval)/* return base time value */ 
#define DTCCTL  _IO(d,5)

/* debugging values */

#define DTCDEBUG 0x1			/* turn on debugging in dtc.c */

#endif _DTCREG_H_

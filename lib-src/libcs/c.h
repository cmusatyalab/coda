#ifndef _BLURB_
#define _BLURB_
/*
 * This code was originally part of the CMU SCS library "libcs".
 * A branch of that code was taken in May 1996, to produce this
 * standalone version of the library for use in Coda and Odyssey
 * distribution.  The copyright and terms of distribution are
 * unchanged, as reproduced below.
 *
 * Copyright (c) 1990-96 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND CARNEGIE MELLON UNIVERSITY
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT
 * SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Users of this software agree to return to Carnegie Mellon any
 * improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * Export of this software is permitted only after complying with the
 * regulations of the U.S. Deptartment of Commerce relating to the
 * Export of Technical Data.
 */

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/libcs/c.h,v 1.1 1996/11/22 19:19:17 braam Exp $";
#endif /*_BLURB_*/


#ifndef	_C_INCLUDE_
#define	_C_INCLUDE_

/*
 * Standard C macros
 */

#ifndef ABS
#define ABS(x) ((x)>=0?(x):-(x))
#endif

#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef	FALSE
#define FALSE	0
#endif	/* FALSE */
#ifndef	TRUE
#define TRUE	1
#endif	/* TRUE */

#define	CERROR		(-1)

#ifndef	bool
typedef enum	{ false = 0, true = 1 } bool;
#endif	/* bool */

#define	sizeofS(string)	(sizeof(string) - 1)
#define sizeofA(array)	(sizeof(array)/sizeof(array[0]))

#define caseE(enum_type)	case (int)(enum_type)

#endif	/* _C_INCLUDE_ */

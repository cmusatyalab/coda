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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/libcs/getshort.c,v 1.1 1996/11/22 19:19:46 braam Exp $";
#endif /*_BLURB_*/

/*  getshort --  prompt user for short
 *
 *  Usage:  i = getshort (prompt,min,max,defalt)
 *	short i,min,max,defalt;
 *	char *prompt;
 *
 *  Getshort prints the message:  prompt  (min to max)  [defalt]
 *  and accepts a line of input from the user.  If the input
 *  is not null or numeric, an error message is printed; otherwise,
 *  the value is converted to an short (or the value "defalt" is
 *  substituted if the input is null).  Then, the value is
 *  checked to ensure that is lies within the range "min" to "max".
 *  If it does not, an error message is printed.  As long as
 *  errors occur, the cycle is repeated; when a legal value is
 *  entered, this value is returned by getshort.
 *  On error or EOF in the standard input, the default is returned.
 *
 */

#include <stdio.h>
#include <ctype.h>

#include "libcs.h"

#ifdef __STDC__
short getshort (const char *prompt, short min, short max, short defalt)
#else /*!__STDC__*/
short getshort (prompt,min,max,defalt)
short min,max,defalt;
#endif /*!__STDC__*/
{
	char input [200];
	register char *p;
	register short i = 0, err;

	fflush (stdout);
	do {

		fprintf (stderr,"%s  (%d to %d)  [%d]  ",prompt,min,max,defalt);
		fflush (stderr);

		if (gets (input) == NULL) {
			i = defalt;
			err = (i < min || max < i);
		}
		else {
			err = 0;
			for (p=input; *p && (isdigit(*p) || *p == '-' || *p == '+'); p++) ;
	
			if (*p) {		/* non-numeric */
				err = 1;
			} 
			else {
				if (*input)	i = atoi (input);
				else		i = defalt;
				err = (i < min || max < i);
			}
		}

		if (err) fprintf (stderr,"Must be a number between %d and %d\n",
		min,max);
	} 
	while (err);

	return (i);
}

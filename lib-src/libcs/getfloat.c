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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/lib-src/libcs/Attic/getfloat.c,v 4.1 1997/01/08 21:53:53 rvb Exp $";
#endif /*_BLURB_*/

/*  getfloat --  prompt user for float
 * 
 *  Usage:  f = getfloat (prompt,min,max,defalt)
 * 	float f,min,max,defalt;
 * 	char *prompt;
 * 
 *  Getfloat prints the message:  prompt  (min to max)  [defalt]
 *  and accepts a line of input from the user.  If the input
 *  is not null or numeric, an error message is printed; otherwise,
 *  the value is converted to a float (or the value "defalt" is
 *  substituted if the input is null).  Then, the value is
 *  checked to ensure that is lies within the range "min" to "max".
 *  If it does not, an error message is printed.  As long as
 *  errors occur, the cycle is repeated; when a legal value is
 *  entered, this value is returned by getfloat.
 *  The default is returned on EOF or error in the standard input.
 * 
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "libcs.h"

#ifdef __STDC__
float getfloat (const char *prompt, float min, float max, float defalt)
#else /*!__STDC__*/
float getfloat (prompt,min,max,defalt)
float min,max,defalt;
const char *prompt;
#endif /*!__STDC__*/
{
	char input [200];
	register char *p;
	register int err;
	double i = 0.0;

	fflush (stdout);
	do {

		fprintf (stderr,"%s  (%g to %g)  [%g]  ",prompt,min,max,defalt);
		fflush (stderr);

		err = 0;
		if (gets(input) == NULL) {
			i = defalt;
			err = (i < min || max < i);
		}
		else {
			for (p=input; *p &&
		 	(isdigit(*p) || *p=='-' || *p=='.' || *p=='+'
			  || *p=='e' || *p=='E');
			 p++);
			if (*p) {		/* non-numeric */
				err = 1;
			} 
			else {
				if (*input)	i = atof (input);
				else		i = defalt;
				err = (i < min || max < i);
			}
		}

		if (err) fprintf (stderr,"Must be a number between %g and %g\n",
		min,max);
	} 
	while (err);

	return (i);
}

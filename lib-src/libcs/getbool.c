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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/lib-src/libcs/getbool.c,v 1.1.1.1 1996/11/22 19:19:10 rvb Exp";
#endif /*_BLURB_*/

/*  getbool -- ask user a yes/no question
 *
 *  Usage:  i = getbool (prompt, defalt);
 *
 *  Example:  do {...} while (getbool ("More?",1));
 *
 *  Prints prompt string, asks user for response.  Defalt is
 *  0 (no) or 1 (yes), and is used if user types just carriage return,
 *  or on end-of-file or error in the standard input.
 *
 */

#include <stdio.h>

#include "libcs.h"
#include <c.h>

int getbool (prompt, defalt)
const char *prompt;
int defalt;
{
	register int valu;
	register char ch;
	char input [100];

	fflush (stdout);
	if (defalt != TRUE && defalt != FALSE)  defalt = TRUE;
	valu = 2;				/* meaningless value */
	do {
		fprintf (stderr,"%s  [%s]  ",prompt,(defalt ? "yes" : "no"));
		fflush (stderr);			/* in case it's buffered */
		if (gets (input) == NULL) {
			valu = defalt;
		}
		else {
			ch = *input;			/* first char */
			if (ch == 'y' || ch == 'Y')		valu = TRUE;
			else if (ch == 'n' || ch == 'N')	valu = FALSE;
			else if (ch == '\0')		valu = defalt;
			else fprintf (stderr,"Must begin with 'y' (yes) or 'n' (no).\n");
		}
	} 
	while (valu == 2);			/* until correct response */
	return (valu);
}

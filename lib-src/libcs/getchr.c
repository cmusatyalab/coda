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

static char *rcsid = "$Header: getchr.c,v 1.1 96/06/03 19:00:26 satya Exp $";
#endif /*_BLURB_*/

/*  getchr  --  ask user to select a character
 *
 *  Usage:  i = getchr (prompt,legals,defalt)
 *	int i;
 *	char *prompt, *legals, defalt;
 *
 *  Prints the message:		prompt  (legals)  [defalt]
 *  and allows the user to type a line.  The first character
 *  the user types is searched for in the string "legals"; if it
 *  is there, its index is returned; otherwise, the user must
 *  try again.  If the user just types carriage return, the
 *  "defalt" character will be searched for.  In any case, note
 *  that is the INDEX (i.e. 0, 1, 2, ...) of the
 *  character that is returned.
 *  On error or EOF in the standard input, the default is used.
 *
 */

#include <stdio.h>
#include <string.h>

#include "libcs.h"

int getchr (prompt, legals, defalt)
const char *prompt, *legals, defalt;
{
	register int i = 0;
	register char *p;
	char input [200];

	fflush (stdout);
	do {
		fprintf (stderr,"%s  (%s)  [%c]  ",prompt,legals,defalt);
		fflush (stderr);
		if (gets (input) == NULL)  *input = defalt;
		if (*input == '\0')  *input = defalt;
		p = strchr (legals, *input);
		if (p == 0)
			fprintf (stderr,"Must be one of: %s\n",legals);
		else
			i = (p - legals);
	} 
	while (p == 0);

	return (i);
}

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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/lib-src/libcs/boolarg.c,v 1.1.1.1 1996/11/22 19:19:51 rvb Exp";
#endif /*_BLURB_*/

/*  boolarg  --  parse boolean from string
 *
 *  Usage:  i = boolarg (ptr,brk,prompt,defalt);
 *	int i,defalt;
 *	char **ptr,*brk,*prompt;
 *
 *  Boolarg will parse an argument from the string pointed to by "ptr",
 *  bumping ptr to point to the next argument in the string.  The
 *  argument parsed will be converted to a boolean (TRUE if it begins
 *  with 'y' or 'Y'; FALSE for 'n' or 'N').  If there is no
 *  argument, or if it is not a boolean value, then getbool() will
 *  be used to ask the user for a boolean value.  In any event,
 *  the boolean value obtained (from the string or from the user) is
 *  returned.
 *  "Brk" is the list of characters which may terminate an argument;
 *  if 0, then " " is used.
 *
 */

#include <stdio.h>
#include <string.h>

#include "libcs.h"
#include <c.h>

int boolarg (ptr,brk,prompt,defalt)
char **ptr,*brk,*prompt;
int defalt;
{
	register char *arg;
	register int valu;

	valu = 2;		/* meaningless value */
	fflush (stdout);

	arg = nxtarg (ptr,brk);	/* parse an argument */
	if (*arg) {		/* there was an argument */
		switch (*arg) {
		case 'n':
		case 'N':
			valu = FALSE;
			break;
		case 'y':
		case 'Y':
			valu = TRUE;
			break;
		case '?':
			break;
		default:
			fprintf (stderr,"%s not 'yes' or 'no'.  ",arg);
		}
	}

	if (valu == 2)  valu = getbool (prompt,defalt);

	return (valu);
}

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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/lib-src/libcs/chrarg.c,v 1.1.1.1 1996/11/22 19:19:29 rvb Exp";
#endif /*_BLURB_*/

/*  chrarg  --  parse character and return its index
 *
 *  Usage:  i = chrarg (ptr,brk,prompt,legals,defalt);
 *	int i;
 *	char **ptr,*brk,*prompt,*legals,defalt;
 *
 *  Chrarg will parse an argument from the string pointed to by "ptr",
 *  bumping ptr to point to the next argument.  The first character
 *  of the arg will be searched for in "legals", and its index
 *  returned; if it is not found, or if there is no argument, then
 *  getchr() will be used to ask the user for a character.
 *  "Brk" is the list of characters which may terminate an argument;
 *  if it is 0, then " " is used.
 *
 */

#include <stdio.h>
#include <string.h>

#include "libcs.h"

int chrarg (ptr,brk,prompt,legals,defalt)
char **ptr, *brk, *prompt, *legals, defalt;
{
	register int i;
	register char *arg,*p;

	i = -1;			/* bogus value */
	fflush (stdout);

	arg = nxtarg (ptr,brk);	/* parse argument */

	if (*arg) {		/* there was an arg */
		p = strchr (legals,*arg);
		if (p) {
			i = p - legals;
		} 
		else if (strcmp("?",arg) != 0) {
			fprintf (stderr,"%s: not valid.  ",arg);
		}
	}

	if (i < 0) {
		i = getchr (prompt,legals,defalt);
	}

	return (i);
}

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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/lib-src/libcs/Attic/octarg.c,v 4.1 1997/01/08 21:53:59 rvb Exp $";
#endif /*_BLURB_*/

/*  octarg  --  parse octal integer argument
 *
 *  Usage:  i = octarg (ptr,brk,prompt,min,max,default)
 *    unsigned int i,min,max,default;
 *    char **ptr,*brk,*prompt;
 *
 *  Will attempt to parse an argument from the string pointed to
 *  by "ptr", incrementing ptr to point to the next arg.  If
 *  an arg is found, it is converted into an octal integer.  If there is
 *  no arg or the value of the arg is not within the range
 *  [min..max], then "getoct" is called to ask the user for an
 *  octal integer value.
 *  "Brk" is the list of characters which may terminate an argument;
 *  if 0, then " " is used.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "libcs.h"

unsigned int octarg (ptr,brk,prompt,min,max,defalt)
char **ptr;
const char *brk,*prompt;
unsigned int min,max,defalt;
{
	register unsigned int i = 0;
	register char *arg,*p = NULL;

	arg = nxtarg (ptr,brk);
	fflush (stdout);

	if (*arg != '\0') {		/* if there was an arg */
		for (p=arg; *p && (*p >= '0' && *p <= '7'); p++) ;
		if (*p) {
			if (strcmp(arg,"?") != 0)  fprintf (stderr,"%s not octal number.  ",arg);
		} 
		else {
			i = atoo (arg);
			if (i<min || i>max) {
				fprintf (stderr,"%s%o out of range.  ",
					(i ? "0" : ""),i);
			}
		}
	}

	if (*arg == '\0' || (p && *p != '\0') || i<min || i>max) {
		i = getoct (prompt,min,max,defalt);
	}

	return (i);
}

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

static char *rcsid = "$Header: path.c,v 1.1 96/06/03 19:00:58 satya Exp $";
#endif /*_BLURB_*/

/*  path  --  break filename into directory and file
 *
 *  path (filename,direc,file);
 *  char *filename,*direc,*file;
 *  filename is input; direc and file are output (user-supplied).
 *  file will not have any trailing /; direc might.
 *
 *  Note these rules:
 *  1.  trailing / are ignored (except as first character)
 *  2.  x/y is x;y where y contains no / (x may contain /)
 *  3.  /y  is /;y where y contains no /
 *  4.  y   is .;y where y contains no /
 *  5.      is .;. (null filename)
 *  6.  /   is /;. (the root directory)
 *
 * Algorithm is this:
 *  1.  delete trailing / except in first position
 *  2.  if any /, find last one; change to null; y++
 *      else y = x;		(x is direc; y is file)
 *  3.  if y is null, y = .
 *  4.  if x equals y, x = .
 *      else if x is null, x = /
 *
 */

#include "libcs.h"

void
path (original,direc,file)
const char *original;
char *direc,*file;
{
	register char *y;
	/* x is direc */
	const char *p;

	/* copy and note the end */
	p = original;
	y = direc;
	while ((*y++ = *p++)) ;		/* copy string */
	/* y now points to first char after null */
	--y;	/* y now points to null */
	--y;	/* y now points to last char of string before null */

	/* chop off trailing / except as first character */
	while (y>direc && *y == '/') --y;	/* backpedal past / */
	/* y now points to char before first trailing / or null */
	*(++y) = 0;				/* chop off end of string */
	/* y now points to null */

	/* find last /, if any.  If found, change to null and bump y */
	while (y>direc && *y != '/') --y;
	/* y now points to / or direc.  Note *direc may be / */
	if (*y == '/') {
		*y++ = 0;
	}

	/* find file name part */
	if (*y)  strcpy (file,y);
	else     strcpy (file,".");

	/* find directory part */
	if (direc == y)        strcpy (direc,".");
	else if (*direc == 0)  strcpy (direc,"/");
	/* else direc already has proper value */
}

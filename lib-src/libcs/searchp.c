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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/libcs/searchp.c,v 1.1 1996/11/22 19:19:47 braam Exp $";
#endif /*_BLURB_*/

/*  searchp  --  search through pathlist for file
 *
 *  Usage:  p = searchp (path,file,fullname,func);
 *	char *p, *path, *file, *fullname;
 *	int (*func)();
 *
 *  Searchp will parse "path", a list of pathnames separated
 *  by colons, prepending each pathname to "file".  The resulting
 *  filename will be passed to "func", a function provided by the
 *  user.  This function must return zero if the search is
 *  successful (i.e. ended), and non-zero if the search must
 *  continue.  If the function returns zero (success), then
 *  searching stops, the full filename is placed into "fullname",
 *  and searchp returns 0.  If the pathnames are all unsuccessfully
 *  examined, then searchp returns -1.
 *  If "file" begins with a slash, it is assumed to be an
 *  absolute pathname and the "path" list is not used.  Note
 *  that this rule is used by Bell's cc also; whereas Bell's
 *  sh uses the rule that any filename which CONTAINS a slash
 *  is assumed to be absolute.  The execlp and execvp procedures
 *  also use this latter rule.  In my opinion, this is bogosity.
 *
 */

#include "libcs.h"

int searchp (path,file,fullname,func)
const char *path, *file;
char *fullname;
int (*func)();
{
	const char *nextpath,*nextchar,*lastchar;
	char *fname;
	int failure;

	nextpath = ((*file == '/') ? "" : path);
	do {
		fname = fullname;
		nextchar = nextpath;
		while (*nextchar && (*nextchar != ':'))
			*fname++ = *nextchar++;
		if (nextchar != nextpath && *file) *fname++ = '/';
		lastchar = nextchar;
		nextpath = ((*nextchar) ? nextchar + 1 : nextchar);
		nextchar = file;	/* append file */
		while (*nextchar)  *fname++ = *nextchar++;
		*fname = '\0';
		failure = (*func) (fullname);
	} 
	while (failure && (*lastchar));
	return (failure ? -1 : 0);
}

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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/lib-src/libcs/openp.c,v 4.1 1997/01/08 21:54:00 rvb Exp $";
#endif /*_BLURB_*/

/*  openp, fopenp  --  search pathlist and open file
 *
 *  Usage:
 *	i = openp (path,file,complete,flags,mode)
 *	f = fopenp (path,file,complete,type)
 *	int i,flags,mode;
 *	FILE *f;
 *	char *path,*file,*complete,*type;
 *
 *  Openp searches for "file" in the pathlist "path";
 *  when the file is found and can be opened by open()
 *  with the specified "flags" and "mode", then the full filename
 *  is copied into "complete" and openp returns the file
 *  descriptor.  If no such file is found, openp returns -1.
 *  Fopenp performs the same function, using fopen() instead
 *  of open() and type instead of flags/mode; it returns 0 if no
 *  file is found.
 *
 */

#include <stdio.h>
#include <unistd.h>

#include "libcs.h"

static int flgs,mod,value;
static const char *ftyp;
static FILE *fvalue;

static int func (fnam)
char *fnam;
{
	value = open (fnam,flgs,mod);
	return (value < 0);
}

static int ffunc (fnam)
char *fnam;
{
	fvalue = fopen (fnam,ftyp);
	return (fvalue == 0);
}

int openp (path,file,complete,flags,mode)
const char *path, *file;
char *complete;
int flags, mode;
{
	flgs = flags;
	mod = mode;
	if (searchp(path,file,complete,func) < 0)  return (-1);
	return (value);
}

FILE *fopenp (path,file,complete,ftype)
const char *path, *file, *ftype;
char *complete;
{
	ftyp = ftype;
	if (searchp(path,file,complete,ffunc) < 0)  return (0);
	return (fvalue);
}

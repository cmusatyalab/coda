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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/lib-src/libcs/vmnlist.c,v 1.1.1.1 1996/11/22 19:19:19 rvb Exp";
#endif /*_BLURB_*/

/*
 * quick routine for loading up the short nlist info from vmunix.
 *
 * return zero on success, 1 on failure
 */

#include <stdio.h>
#include <ctype.h>
#include <nlist.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/stat.h>

#if	CMUCS
#define NLISTFILE "/usr/cs/etc/vmnlist"
#else	CMUCS
#define NLISTFILE "/etc/vmnlist"
#endif	CMUCS

vmnlist(nlistf,nl)
char *nlistf;
struct nlist nl[];
{
    register FILE *f;
    struct stat stb,stb2;
    char symnam[100];
    register char *p;
    struct nlist n;
    register struct nlist *t;
    register nlsize;

    if (strcmp (nlistf, "/vmunix"))
	return 1;
    if (stat ("/vmunix", &stb) < 0 || stat (NLISTFILE, &stb2) < 0)
	return 1;
    if (stb.st_mtime >= stb2.st_mtime || stb2.st_size == 0)
	return 1;
    if ((f = fopen (NLISTFILE, "r")) == NULL)
	return 1;
    nlsize = 0;
    for (t=nl; t->n_name != NULL && t->n_name[0] != '\0'; ++t) {
	t->n_type = 0;
	t->n_value = 0;
	nlsize++;
    }
    for(;;) {
	p = symnam;
	while ((*p++ = fgetc(f)) != '\0' && !feof(f))
	    ;
	if (feof(f)) break;
	if (fread((char *)&n, sizeof(struct nlist), 1, f) != 1) break;
	/* find the symbol in nlist */
	for (t=nl; t->n_name != NULL && t->n_name[0] != '\0'; ++t)
	    if (t->n_type == 0 && t->n_value == 0 &&
		strcmp(symnam,t->n_name) == 0) {
		n.n_name = t->n_name;
		*t = n;
		if (--nlsize == 0) {
		    fclose(f);
		    return(0);
		}
		break;
	    }
    }
    fclose(f);
    return(1);
}

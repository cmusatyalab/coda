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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/lib-src/libcs/expand.c,v 1.1.1.1 1996/11/22 19:19:21 rvb Exp";
#endif /*_BLURB_*/

/*
 *  expand - expand wildcard filename specifications
 *
 *  Usage:
 *	int expand(spec, buffer, bufsize);
 *	char *spec, **buffer;
 *	int bufsize;
 *
 *  Expand takes a file specification, and expands it into filenames
 *  by resolving the characters '*', '?', '[', ']', '{', '}' and '~'
 *  in the same manner as the shell.  You provide "buffer", which is
 *  an array of char *'s, and you tell how big it is in bufsize.
 *  Expand will compute the corresponding filenames, and will fill up
 *  the entries of buffer with pointers to malloc'd strings.
 *
 *  The value returned by expand is the number of filenames found.  If
 *  this value is -1, then malloc failed to allocate a string.  If the
 *  value is bufsize + 1, then too many names were found and you can try
 *  again with a bigger buffer.
 *
 *  This routine was basically created from the csh sh.glob.c file with
 *  the following intended differences:
 *
 *	Filenames are not sorted.
 *	All expanded filenames returned exist.
 *
 */
#include <sys/param.h>
#include <sys/stat.h>
#if	defined(__NetBSD__) || defined(__WIN32__)
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
#include <pwd.h>
#include <ctype.h>
#include <setjmp.h>
#include <string.h>

#define path hide_path
#include "libcs.h"
#undef path

static	jmp_buf	sjbuf;

static	char	pathbuf[MAXPATHLEN];
static	char	*path, *pathp, *lastpathp;

static	char	*globchars = "{[*?";	/* meta characters */
static	char	*entp;			/* current dir entry pointer */

static	char	**BUFFER;		/* pointer to the buffer */
static	int	BUFSIZE;		/* maximum number in buffer */
static	int	bufcnt;			/* current number in buffer */

static	void	glob();
static	void	matchdir();
static	int	execbrc();
static	int	match(), amatch(),  addpath();
static	void	addone();
static	int	gethdir();

int expand(spec, buffer, bufsize)
	const char *spec;
	char **buffer;
	int bufsize;
{
	pathp = path = pathbuf;
	*pathp = 0;
	lastpathp = &path[MAXPATHLEN - 2];
	BUFFER = buffer;
	BUFSIZE = bufsize;
	bufcnt = 0;
	if (setjmp(sjbuf) == 0)
	    glob(spec);
	return(bufcnt);
}

static void glob(as)
	const char *as;
{
	const char *cs, *oldcs;
	char *spathp;
	struct stat stb;

	spathp = pathp;
	cs = as;
	if (*cs == '~' && pathp == path) {
		if (addpath('~')) goto endit;
		for (cs++; isalnum(*cs) || *cs == '_' || *cs == '-';)
			if (addpath(*cs++)) goto endit;
		if (!*cs || *cs == '/') {
			if (pathp != path + 1) {
				*pathp = 0;
				if (gethdir(path + 1)) goto endit;
				strcpy(path, path + 1);
			} else
				strcpy(path, getenv("HOME"));
			pathp = path;
			while (*pathp) pathp++;
		}
	}
	while (*cs == 0 || strchr(globchars, *cs) == 0) {
		if (*cs == 0) {
			if (lstat(path, &stb) >= 0) addone(path, "");
			goto endit;
		}
		if (addpath(*cs++)) goto endit;
	}
	oldcs = cs;
	while (cs > as && *cs != '/')
		cs--, pathp--;
	if (*cs == '/')
		cs++, pathp++;
	*pathp = 0;
	if (*oldcs == '{') {
		execbrc(cs, NULL);
		return;
	}
	/* this should not be an lstat */
	if (stat(path, &stb) >= 0 && (stb.st_mode&S_IFMT) == S_IFDIR)
		matchdir(cs);
endit:
	pathp = spathp;
	*pathp = 0;
}

static void matchdir(pattern)
	char *pattern;
{
#if	defined(__NetBSD__) || defined(__WIN32__)
	register struct dirent *dp;
#else
	register struct direct *dp;
#endif
	DIR *dirp;

	dirp = opendir(path);
	if (dirp == NULL)
		return;
	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_ino == 0) continue;
		if (match(dp->d_name, pattern))
			addone(path, dp->d_name);
	}
	closedir(dirp);
}

static int execbrc(p, s)
	char *p, *s;
{
	char restbuf[MAXPATHLEN + 1];
	register char *pe, *pm, *pl;
	int brclev = 0;
	char *lm, savec, *spathp;

	for (lm = restbuf; *p != '{'; *lm++ = *p++)
		continue;
	for (pe = ++p; *pe; pe++)
	switch (*pe) {
	case '{':
		brclev++;
		continue;
	case '}':
		if (brclev == 0) goto pend;
		brclev--;
		continue;
	case '[':
		for (pe++; *pe && *pe != ']'; pe++)
			continue;
		if (!*pe) break;
		continue;
	}
pend:
	if (brclev || !*pe) return (0);
	for (pl = pm = p; pm <= pe; pm++)
		switch (*pm & 0177) {
		case '{':
			brclev++;
			continue;
		case '}':
			if (brclev) {
				brclev--;
				continue;
			}
			goto doit;
		case ',':
			if (brclev) continue;
doit:
			savec = *pm;
			*pm = 0;
			strcpy(lm, pl);
			strcat(restbuf, pe + 1);
			*pm = savec;
			if (s == 0) {
				spathp = pathp;
				glob(restbuf);
				pathp = spathp;
				*pathp = 0;
			} else if (amatch(s, restbuf))
				return (1);
			pl = pm + 1;
			continue;

		case '[':
			for (pm++; *pm && *pm != ']'; pm++)
				continue;
			if (!*pm) break;
			continue;
		}
	return (0);
}

static int match(s, p)
	char *s, *p;
{
	register int c;
	register char *sentp;

	if (*s == '.' && *p != '.') return(0);
	sentp = entp;
	entp = s;
	c = amatch(s, p);
	entp = sentp;
	return (c);
}

static int amatch(s, p)
	register char *s, *p;
{
	register int scc;
	int ok, lc;
	char *spathp;
	struct stat stb;
	int c, cc;

	for (;;) {
		scc = *s++ & 0177;
		switch (c = *p++) {
		case '{':
			return (execbrc(p - 1, s - 1));
		case '[':
			ok = 0;
			lc = 077777;
			while ((cc = *p++)) {
				if (cc == ']') {
					if (ok) break;
					return (0);
				}
				if (cc == '-') {
					if (lc <= scc && scc <= *p++)
						ok++;
				} else
					if (scc == (lc = cc))
						ok++;
			}
			if (cc == 0) return (0);
			continue;
		case '*':
			if (!*p) return (1);
			if (*p == '/') {
				p++;
				goto slash;
			}
			for (s--; *s; s++)
				if (amatch(s, p))
					return (1);
			return (0);
		case 0:
			return (scc == 0);
		default:
			if (c != scc) return (0);
			continue;
		case '?':
			if (scc == 0) return (0);
			continue;
		case '/':
			if (scc) return (0);
slash:
			s = entp;
			spathp = pathp;
			while (*s)
				if (addpath(*s++)) goto pathovfl;
			if (addpath('/')) goto pathovfl;
			if (stat(path, &stb) >= 0 &&
			    (stb.st_mode&S_IFMT) == S_IFDIR)
				if (*p == 0)
					addone(path, "");
				else
					glob(p);
pathovfl:
			pathp = spathp;
			*pathp = 0;
			return (0);
		}
	}
}

static void addone(s1, s2)
	register char *s1, *s2;
{
	register char *ep;

	if (bufcnt >= BUFSIZE) {
		bufcnt = BUFSIZE + 1;
		longjmp(sjbuf, 1);
	}
	ep = malloc(strlen(s1) + strlen(s2) + 1);
	if (ep == 0) {
		bufcnt = -1;
		longjmp(sjbuf, 1);
	}
	BUFFER[bufcnt++] = ep;
	while (*s1) *ep++ = *s1++;
	while ((*ep++ = *s2++));
}

static int addpath(c)
	char c;
{
	if (pathp >= lastpathp)
		return(1);
	*pathp++ = c;
	*pathp = 0;
	return(0);
}

static int gethdir(home)
	char *home;
{
	struct passwd *getpwnam();
	register struct passwd *pp = getpwnam(home);

	if (pp == 0)
		return(1);
	strcpy(home, pp->pw_dir);
	return(0);
}

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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/libcs/libcs.h,v 1.1 1996/11/22 19:19:26 braam Exp $";
#endif /*_BLURB_*/

#ifndef	_LIBCS_H_
#define	_LIBCS_H_ 1

#include <sys/types.h>
#include <ctype.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#if	__STDC__
#include <stdarg.h>		/* for va_list */
#endif

#ifndef	__P
#if	__STDC__
#define __P(x) x
#else
#define __P(x) ()
#endif
#endif

/*  CMU stdio additions */
extern FILE *fopenp __P((const char *, const char *, char *, const char *));
extern FILE *fwantread __P((const char *, const char *, char *, const char *));
extern FILE *fwantwrite __P((const char *, const char *, char *, const char *, int));
#if   !(__NetBSD__ || LINUX)
extern int snprintf __P((char *, int, const char *, ...));
extern int vsnprintf __P((char *, int, const char *, va_list));
#endif

/* CMU string routines */
#define sindex strstr

extern char _argbreak;

extern char *foldup __P((char *, const char *));
extern char *folddown __P((char *, const char *));
extern char *skipto __P((const char *, const char *));
extern char *skipover __P((const char *, const char *));
extern char *nxtarg __P((char **, const char *));
extern char *getstr __P((const char *, const char *, char *));
extern int getstab __P((const char *, /* const */ char **, const char *));
extern int getsearch __P((const char *, /* const */ char **, const char *));
#ifdef	__MACH__
extern char *strarg __P((char **, const char *, const char *, char *, char *));
#else
extern char *strarg __P((char **, const char *, const char *, const char *, char *));
#endif
extern int stabarg __P((char **, const char *, const char *, /* const */ char **, const char *));
extern int stabsearch __P((char *, /* const */ char **, int));
extern int searchp __P((const char *, const char *, char *, int (*)()));
extern int searcharg __P((char **, const char *, const char *, /* const */ char **, const char *));
extern int getint __P((const char *, int, int, int));
extern int intarg __P((char **, const char *, const char *, int, int, int));
extern long getlong __P((const char *, long, long, long));
extern long longarg __P((char **, const char *, const char *, long, long, long));
extern short getshort __P((const char *, short, short, short));
extern short shortarg __P((char **, const char *, const char *, short, short, short));
extern float getfloat __P((const char *, float, float, float));
extern float floatarg __P((char **, const char *, const char *, float, float, float));
extern double getdouble __P((const char *, double, double, double));
extern double doublearg __P((char **, const char *, const char *, double, double, double));
extern unsigned int getoct __P((const char *, unsigned int, unsigned int, unsigned int));
extern unsigned int octarg __P((char **, const char *, const char *, unsigned int, unsigned int, unsigned int));
extern unsigned int gethex __P((const char *, unsigned int, unsigned int, unsigned int));
extern unsigned int hexarg __P((char **, const char *, const char *, unsigned int, unsigned int, unsigned int));
extern int getbool __P((const char *, int));
extern int boolarg __P((char **, char *, char *, int));
extern int getchr __P((const char *, const char *, int));
extern int chrarg __P((char **, char *, char *, char *, int));
extern unsigned int atoo __P((const char *));
extern unsigned int atoh __P((const char *));
extern char *concat __P((char *, int, ...));
extern char *vconcat __P((char *, int, va_list));
extern char *salloc __P((const char *));
extern int stablk __P((const char *, /* const */ char **, int));
extern int stlmatch __P((const char *, const char *));


/* CMU library routines */
extern char *getname __P((int));
extern char *pathof __P((const char *));
extern char *errmsg __P((int));
extern int abspath __P((char *, char *));
extern int dfork __P((void));
extern int editor __P((char *, char *));
extern int expand __P((const char *, char **, int));
extern int ffilecopy __P((FILE *, FILE *));
extern char *fgetpass __P((const char *, FILE *));
extern int filecopy __P((int, int));
extern int srchscore __P((const char *, const char *));
extern int wantread __P((const char *, const char *, char *, const char *));
extern int wantwrite __P((const char *, const char *, char *, const char *, int));
extern int makepath __P((const char *, const char *, int, int));
extern int openp __P((const char *, const char *, char *, int, int));
extern void path __P((const char *, char *, char *));
extern void prstab __P((/* const */ char **));
extern void fprstab __P((FILE *, /* const */ char **));
extern int runv __P((const char *, /* const */ char **));
extern int runvp __P((const char *, /* const */ char **));
extern int runp __P((const char *, ...));
extern int runcv __P((int (*)(), const char *, /* const */ char **));
extern int runc __P((int (*)(), const char *, ...));
extern int runcvp __P((int (*)(), const char *, /* const */ char **));
extern int runcp __P((int (*)(), const char *, ...));
extern int run __P((const char *, ...));
extern int gethostattr __P((char **, int));
extern int movefile __P((const char *, const char *));
extern void quit __P((int, char *, ...));
extern struct passwd *getpwwho __P((const char *));
extern struct passwd *getpwambig __P((void));

/*  CMU time additions */
extern time_t gtime __P((const struct tm *));
extern time_t atot __P((const char *));
extern char *fdate __P((char *, const char *, const struct tm *));
extern int parsedate __P((const char *, struct tm *, int, int, int));

#endif	/* _LIBCS_H_ */

/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * @sys and @cpu at the end of a pathname component will be expanded by
 * fsobj::Lookup to platform dependent strings as declared in this file.
 */

#ifndef _CODA_EXPANSION_H_
#define _CODA_EXPANSION_H_

#ifdef __alpha__
#define CPUTYPE "alpha"
#endif
#ifdef arm32
#define CPUTYPE "arm32"
#endif
#ifdef i386
#define CPUTYPE "i386"
#endif 
#ifdef __powerpc__
#define CPUTYPE "powerpc"
#endif
#ifdef sparc
#define CPUTYPE "sparc"
#endif
#ifdef sun3
#define CPUTYPE "sun3"
#endif
#ifndef CPUTYPE
#define CPUTYPE "unknown"
#endif

#ifdef __FreeBSD__
#define SYSTYPE CPUTYPE"_fbsd2"
#endif
#ifdef __linux__
#define SYSTYPE CPUTYPE"_linux"
#endif
#ifdef __NetBSD__
#define SYSTYPE CPUTYPE"_nbsd1"
#endif
#if defined(__CYGWIN32__) || defined(DJGPP)
#define SYSTYPE CPUTYPE"_win32"
#endif 
#ifdef sun					/* Brr. JK */
#define SYSTYPE CPUTYPE"_solaris2"
#endif
#ifndef SYSTYPE
#define SYSTYPE CPUTYPE"_unknown"
#endif

#endif /* _CODA_EXPANSION_H_ */

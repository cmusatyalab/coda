/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

*/

#ifndef CODA_WAIT_H
#define CODA_WAIT_H

/* Header files and macros for a common denominator in wait and friends */

#ifdef __linux__
#define _USE_BSD
#include <sys/types.h>
#include <sys/resource.h>
#elif defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif
#include <sys/wait.h>
#ifndef WEXITSTATUS
#define WEXITSTATUS(x)  ((unsigned(x) >> 8)
#endif
#ifndef WTERMSIG
#define WTERMSIG(x)     ((x) & 255)
#endif
#ifndef WCOREDUMP
#define WCOREDUMP(x)    (x) & 0200)
#endif

#ifdef __linux__
#undef _USE_BSD
#endif
#endif

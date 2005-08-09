/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CODA_OFFSETOF_H_
#define _CODA_OFFSETOF_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stddef.h>

#if defined(CODA_OFFSETOF_OFFSETOF)
#define coda_offsetof(type,member) offsetof(type,member)

#elif defined(CODA_OFFSETOF_PTR_TO_MEMBER)
#define coda_offsetof(type,member) ((size_t)(&type::member))

#elif defined(CODA_OFFSETOF_REINTERPRET_CAST)
#define coda_offsetof(type,member) ((size_t)(&(reinterpret_cast<type*>(__alignof__(type*)))->member)-__alignof__(type*))

#else /* default should work most of the time but might get compile warnings */
#define coda_offsetof(type,member) offsetof(type,member)
#endif

#endif /* _CODA_OFFSETOF_H_ */


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

*/

#ifndef CODA_STRING_H
#define CODA_STRING_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#ifdef HAVE_BCOPY_IN_STRINGS_H
/* bcopy et al are in strings.h on Solaris */
#include <strings.h>
#endif

#ifndef HAVE_STRERROR
#define strerror(err) \
    ((err >= 0 && err < sys_nerr) ? sys_errlist[err] : "Unknown errorcode")
#endif

#ifndef HAVE_SNPRINTF
/* yeah, sprintf is not as safe, but snprintf is prety much included on all
 * platforms anyway. */
#define snprintf(str, size, format...) sprintf(str, ## format);
#endif

#endif

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

#*/

#ifndef SCANDIR_H
#define SCANDIR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dirent.h>
#ifndef HAVE_SCANDIR
extern int scandir (const char *dir, struct dirent ***namelist, int (*select)(struct dirent *), int (*cmp)(const void *, const void *));
#endif

#ifdef __cplusplus
}
#endif

#endif /* SCANDIR_H */

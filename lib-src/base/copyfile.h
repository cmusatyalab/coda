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

#*/

#ifndef _COPYFILE_H_
#define _COPYFILE_H_

/* 
 * functions for copying files around
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_SIZE 8192  /* size of buffer for looping copy */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

int copyfile(int, int);
int copyfile_byname(const char *, const char *);

#ifdef __cplusplus
}
#endif __cplusplus

#endif /* _COPYFILE_H_ */


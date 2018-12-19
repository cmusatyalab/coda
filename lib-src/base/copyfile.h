/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
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
#include <stdint.h>

#define BUF_SIZE 8192  /* size of buffer for looping copy */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copy a segment of a file to another file
 *
 * @param infd  file descriptor of the source file
 * @param outfd file descriptor of the destination file
 * @param pos   offset within the source file
 * @param count amount of bytes to be copied
 * 
 * @return -1 on errors and 0 otherwise
 */
int copyfile_seg(int infd, int outfd, uint64_t pos, int64_t count);

/**
 * Copy a file to another file
 *
 * @param fromfd file descriptor of the source file
 * @param tofd   file descriptor of the destination file
 * 
 * @return -1 on errors and 0 otherwise
 */
int copyfile(int fromfd, int tofd);

/**
 * Copy a file to another file
 *
 * @param fromname file path of the source file
 * @param toname   file path of the destination file
 * 
 * @return -1 on errors and 0 otherwise
 */
int copyfile_byname(const char *fromname, const char *toname);

#ifdef __cplusplus
}
#endif

#endif /* _COPYFILE_H_ */


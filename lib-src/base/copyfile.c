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

#include "copyfile.h"

/* Copies file open for reading on file descriptor infd
 *     to file open for writing on file descriptor outfd.
 * Closes both file descriptors before exiting.
 * Returns -1 on error, 0 on success.
 */
int copyfile(int infd, int outfd) {
  char databuf[BUF_SIZE];
  int cnt, ret;

  /* truncate output file */
  if (ftruncate(outfd, 0) < 0) return(-1);

  while ((cnt = read(infd, databuf, BUF_SIZE)) > 0) {
    ret = write(outfd, databuf, cnt);
    if (ret < cnt) return(-1);
  }
  if (cnt < 0) return(-1);
  if (close(infd) < 0) return(-1);
  if (close(outfd) < 0) return(-1);
  return(0);
}

/* Wrapper function for copyfile -- takes two pathnames,
 * opens one for reading, other for writing, and calls copyfile.
 * Returns -1 on error, 0 on success.
 */
int copyfile_byname(const char *in, const char *out) {
  int infd, outfd, save;

  if ((infd = open(in, O_RDONLY)) < 0)
    return(-1);
  if ((outfd = open(out, O_WRONLY|O_CREAT)) < 0) {
    save = errno;
    close(infd);
    errno = save;
    return(-1);
  }
  return(copyfile(infd, outfd));
}

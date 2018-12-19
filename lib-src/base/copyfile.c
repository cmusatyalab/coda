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

#include "copyfile.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Copies segment of file open for reading on file descriptor infd to file
 * open for writing on file descriptor outfd.
 * Returns -1 on error, 0 on success.
 */
int copyfile_seg(int infd, int outfd, uint64_t pos, int64_t count)
{
    char databuf[BUF_SIZE];
    int cnt, ret;

    lseek(infd, pos, SEEK_SET);
    lseek(outfd, pos, SEEK_SET);

    while ((cnt = read(infd, databuf, BUF_SIZE)) > 0) {

        if (count > cnt) {
            ret = write(outfd, databuf, cnt);
        } else {
            ret = write(outfd, databuf, count);
        }

        count -= cnt;

        if (count <= 0) break;

        if (ret < cnt) return(-1);
    }

    return (cnt < 0 ? -1 : 0);
}

/* Copies file open for reading on file descriptor infd
 *     to file open for writing on file descriptor outfd.
 * Returns -1 on error, 0 on success.
 */
int copyfile(int infd, int outfd)
{
    char databuf[BUF_SIZE];
    int cnt, ret;

    while ((cnt = read(infd, databuf, BUF_SIZE)) > 0) {
	ret = write(outfd, databuf, cnt);
	if (ret < cnt) return(-1);
    }
    return (cnt < 0 ? -1 : 0);
}

/* Wrapper function for copyfile -- takes two pathnames,
 * opens one for reading, other for writing, and calls copyfile.
 * Returns -1 on error, 0 on success.
 */
int copyfile_byname(const char *in, const char *out)
{
    int outopen = 0, infd, outfd, save;

    if ((infd = open(in, O_RDONLY)) < 0)
	return -1;

    if ((outfd = open(out, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0600)) < 0)
	goto err_exit;
    outopen = 1;

    if (copyfile(infd, outfd) < 0)
	goto err_exit;

    outopen = 0;
    if (close(outfd) < 0)
	goto err_exit;

    if (close(infd) < 0)
	return -1;

    return 0;

err_exit:
    save = errno;
    if (outopen) close(outfd);
    close(infd);
    errno = save;
    return -1;
}


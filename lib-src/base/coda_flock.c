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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <unistd.h>
#include <errno.h>

#ifdef HAVE_FLOCK
#include <sys/file.h>
#else /* HAVE_FCNTL */
#include <fcntl.h>
#include <string.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

int myflock(int fd, int type, int block) {
#ifdef HAVE_FLOCK
    return flock(fd, lk | block);
#else /* HAVE_FCNTL */
    struct flock lock;
    int rc;
    
    memset((char *) &lock, 0, sizeof(struct flock));
    lock.l_type = type;
    while ((rc = fcntl(fd, block, &lock)) < 0
    	   && block == F_SETLKW
    	   && errno == EINTR)		/* interrupted */
    	sleep(1);
    return rc;
#endif
}

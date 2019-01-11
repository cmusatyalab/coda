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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_RANDOM_H
#include <sys/random.h>
#endif
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <pioctl.h>

#include "codaconf.h"
#include "coda_config.h"
#include "coda.h"

#ifdef __CYGWIN32__
#include <ctype.h>
#include <windows.h>
#endif

#ifdef __cplusplus
}
#endif

static const char *getMountPoint(void)
{
    static const char *mountPoint = NULL;

    if (!mountPoint) {
        codaconf_init("venus.conf");
        CODACONF_STR(mountPoint, "mountpoint", "/coda");
    }
    return mountPoint;
}

static const char *strip_prefix(const char *path)
{
    const char *mountPoint = getMountPoint();
    int mPlen              = strlen(mountPoint);

#if defined(__CYGWIN32__) // Windows NT and 2000
    char cygdrive[13]                   = "/cygdrive/./";
    char driveletter                    = '\0';
    static char prefix[CODA_MAXPATHLEN] = "";
    char *cwd;

    if (mPlen == 2 && mountPoint[1] == ':') {
        driveletter  = tolower(mountPoint[0]);
        cygdrive[10] = driveletter;
    }

    if (driveletter && path[1] == ':' && tolower(path[0]) == driveletter) {
        return path + 2;
    }
    if (strncmp(cygdrive, path, 12) == 0) {
        return path + 12;
    }
    if (strncmp("/coda/", path, 6) == 0) {
        return path + 6;
    }
    if (path[0] != '/') {
        char *cwd = getwd(NULL);

        if (strncmp(mountPoint, cwd, mPlen) == 0) {
            strncpy(prefix, cwd + mPlen + 1, CODA_MAXPATHLEN);
        } else if (strncmp(cygdrive, cwd, 12) == 0) {
            strncpy(prefix, cwd + 12, CODA_MAXPATHLEN);
        } else {
            /* does not look like a coda path! */
            free(cwd);
            errno = ENOENT;
            return -1;
        }
        strncat(prefix, "/", CODA_MAXPATHLEN - strlen(prefix));
        strncat(prefix, path, CODA_MAXPATHLEN - strlen(prefix) - 1);
        free(cwd);
        return prefix;
    }
#else
    if (path[0] != '/') {
        static char buf[PATH_MAX];
        char *cwd = getcwd(buf, sizeof(buf));

        if (cwd == NULL)
            return NULL;

        /* strip "." and "./" */
        if (path[0] == '.') {
            if (path[1] == '\0')
                path += 1;
            else if (path[1] == '/')
                path += 2;
        }

        if ((strlen(cwd) + 1 + strlen(path) + 1) >= PATH_MAX) {
            errno = ENAMETOOLONG;
            return NULL;
        }

        /* concatenate current directory and path */
        strcat(cwd, "/");
        strcat(cwd, path);

        path = buf;
    }
#endif
    if (strncmp(mountPoint, path, mPlen) == 0) {
        return path + mPlen;
    }

    // else: Does not look like a coda file!
    errno = ENOENT;
    return NULL;
}

int pioctl(const char *path, unsigned long com, struct ViceIoctl *vidata,
           int follow)
{
    const char *mtpt = getMountPoint();
    char *pioctlfile = NULL;
    FILE *f;
    ssize_t n;

    uint32_t unique = getpid(); /* unique, but predictable */

#ifdef HAVE_GETRANDOM
    /* try to read from /dev/urandom */
    n = getrandom(&unique, sizeof(uint32_t), 0);
    if (n < sizeof(uint32_t))
        fprintf(stderr, "WARNING: Unable to obtain reliable random data\n");
#endif

    pioctlfile = malloc(strlen(mtpt) + 1 + strlen(PIOCTL_PREFIX) + 8 + 1);
    sprintf(pioctlfile, "%s/" PIOCTL_PREFIX "%08x", mtpt, unique);

    f = fopen(pioctlfile, "w");
    if (f == NULL) {
        fprintf(stderr, "Failed to open unique pioctl file\n");
        errno = EBADF;
        return -1;
    }

    if (path) {
        /* and now strip the path-prefix outside of the mounted Coda tree */
        path = strip_prefix(path);
    } else
        path = "/";

    if (!path) {
        errno = EBADF;
        return -1;
    }

    uint8_t cmd   = (uint8_t)_IOC_NR(com);
    uint16_t plen = (uint16_t)strlen(path);
    //uint16_t size = (uint16_t)_IOC_SIZE(com);

    if (fprintf(f, "%u\n%u\n%u\n%u\n%u\n%c", cmd, plen, follow, vidata->in_size,
                vidata->out_size, '\0') == -1 ||
        fwrite(path, 1, plen, f) != plen ||
        fwrite(vidata->in, 1, vidata->in_size, f) != vidata->in_size) {
        fprintf(stderr, "Failed to write to pioctl file\n");
        fclose(f);
        errno = EBADF;
        return -1;
    }
    fclose(f);

    f = fopen(pioctlfile, "r");
    if (f == NULL) {
        fprintf(stderr, "Failed to open pioctl file\n");
        errno = EBADF;
        return -1;
    }

    int code          = 0;
    unsigned int size = 0;

    n = fscanf(f, "%d\n%u\n%*c", &code, &size);
    if (n != 2) {
        fprintf(stderr, "Failed to parse results from pioctl file\n");
        fclose(f);
        errno = EBADF;
        return -1;
    }

    if (size > vidata->out_size) {
        fprintf(stderr, "pioctl response too large\n");
        fclose(f);
        errno = EBADF;
        return -1;
    }

    if (fread(vidata->out, 1, size, f) != size) {
        fprintf(stderr, "Failed to read response from pioctl\n");
        fclose(f);
        errno = EBADF;
        return -1;
    }

    fclose(f);

    if (code) {
        errno = code;
        return -1;
    }
    return 0;
}

/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *
 *    Implementation of the Venus path expansion facility.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

#include <coda_config.h>

/* interfaces */
#include <vice.h>

/* from venus */
#include "fso.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"

inline void GetComponent(char **pptr_addr, int *plen_addr, char *nextcompptr)
{
    char c;

    while ((c = **pptr_addr) && c != '/') {
        /* Move on to next character. */
        *nextcompptr++ = c;
        (*pptr_addr)++;
        (*plen_addr)--;
    }
    *nextcompptr = 0; /* make comp a real string */
}

inline void SkipSlashes(char **pptr_addr, int *plen_addr)
{
    while (**pptr_addr == '/') {
        (*pptr_addr)++;
        (*plen_addr)--;
    }
}

/* Should be possible to inhibit symlink expansion (i.e., consider it an error)! */
/* Returns {0, 1}.  On 0, u.u_error is set to appropriate Unix errno and *vpp is 0. */
/* On 1, u.u_error is 0 and *vpp is a valid vnode pointer. */
/* Caller must set u_cred, u_priority, u.u_cdir and u_nc fields as appropriate. */
int vproc::namev(char *path, int flags, struct venus_cnode *vpp)
{
    LOG(1, ("vproc::namev: %s, %d\n", path, flags));

    /* Initialize some global variables. */
    u.u_error = 0;
    u.u_flags = flags;
    struct venus_cnode pvp;
    struct venus_cnode vp;
    char comp[CODA_MAXNAMLEN + 1];
    comp[0] = '\0';
    char workingpath[CODA_MAXPATHLEN + 1];
    strncpy(workingpath, path, CODA_MAXPATHLEN);
    workingpath[CODA_MAXPATHLEN] = '\0';
    char *pptr                   = workingpath;
    int plen                     = strlen(pptr);
    int nlinks                   = 0;

    /* Initialize the parent (i.e., the root of the expansion). */
    {
        vget(&pvp, &u.u_cdir);
        if (u.u_error)
            goto Exit;

        /* Skip over leading slashes. */
        if (plen != 0)
            SkipSlashes(&pptr, &plen);

        /* Check for degenerate case of asking for Cdir. */
        if (plen == 0) {
            *vpp = pvp;
            goto Exit;
        }
    }

    /* Each loop iteration moves down through one pathname component. */
    for (;;) {
        if (plen <= 0) {
            print(GetLogFile());
            CHOKE("vproc::namev: plen <= 0");
        }

        /* Get the next component. */
        GetComponent(&pptr, &plen, comp);

        /* Skip over trailing slashes. */
        SkipSlashes(&pptr, &plen);

        /* Handle ".." out of venus here! */
        if (FID_EQ(&pvp.c_fid, &rootfid) && STREQ(comp, "..")) {
            LOG(100, ("vproc::namev: .. out of this venus\n"));

            u.u_error = ENOENT;
            goto Exit;
        }

        /* Now lookup the object in the directory. */
        lookup(&pvp, comp, &vp, CLU_CASE_SENSITIVE);
        if (u.u_error)
            goto Exit;

        /* We have the new object.  The next action depends on what type of object it is. */
        /* If it is a file, we check that we are at the end of the path; */
        /* If it is a directory, we simply make it the new parent object. */
        /* If it is a symbolic link, we reset the pathname to be it, and continue scanning. */
        switch (vp.c_type) {
        case C_VREG: {
            if (plen == 0) {
                *vpp = vp;
                goto Exit;
            }

            /* File must be the last comp in the path. */
            u.u_error = ENOTDIR;
            goto Exit;
        }

        case C_VDIR: {
            if (plen == 0) {
                *vpp = vp;
                goto Exit;
            }

            /* Child becomes the new parent. */
            pvp     = vp;
            comp[0] = '\0';

            break;
        }

        case C_VLNK: {
            /* Return the link if we are not to "follow" and this is the last component. */
            if (plen == 0 && !(u.u_flags & FOLLOW_SYMLINKS)) {
                *vpp = vp;
                goto Exit;
            }

            /* Guard against looping. */
            if (++nlinks > CODA_MAXSYMLINK) {
                u.u_error = ELOOP;
                goto Exit;
            }

            /* Examine the link contents.  We will find either: */
            /*    - an absolute pathname within this venus; */
            /*    - an absolute pathname leaving this venus; */
            /*    - a relative pathname. */
            /* Note that the result of ReadLink is not necessarily a proper string. */

            /* Get the link contents. */
            char linkdata[CODA_MAXPATHLEN];
            int linklen = CODA_MAXPATHLEN - 1; /* -1 for trailing \0 */
            struct coda_string string;
            string.cs_buf    = linkdata;
            string.cs_maxlen = linklen;
            string.cs_len    = 0;
            readlink(&vp, &string);
            if (u.u_error) {
                linklen = 0;
                goto Exit;
            }
            linklen = string.cs_len;
            if (linklen == 0) {
                u.u_error = EINVAL;
                goto Exit;
            }

            /* Append the trailing part of the original pathname. */
            if (linklen + plen >= CODA_MAXPATHLEN) {
                u.u_error = ENAMETOOLONG;
                goto Exit;
            }
            if (plen > 0) {
                linkdata[linklen]     = '/';
                linkdata[linklen + 1] = '\0';
                strcat(linkdata, pptr);
            } else
                linkdata[linklen] = '\0';

            /* Figure out the type of the new path and act accordingly. */
            static int venusRootLength = -1;
            if (venusRootLength == -1)
                venusRootLength = strlen(venusRoot);
            if (linklen >= venusRootLength &&
                STRNEQ(linkdata, venusRoot, venusRootLength) &&
                (linklen == venusRootLength ||
                 linkdata[venusRootLength] == '/')) {
                LOG(100, ("vproc::namev: abspath within this venus (%s)\n",
                          linkdata));

                /* Copy the part after the VenusRoot to workingpath. */
                strcpy(workingpath, linkdata + venusRootLength);

                /* Release the child. */
                comp[0] = '\0';

                /* Release the parent and reset it to the VenusRoot. */
                vget(&pvp, &rootfid);
                if (u.u_error)
                    goto Exit;
            } else if (linkdata[0] == '/') {
                LOG(100, ("vproc::namev: abspath leaving this venus (%s)\n",
                          linkdata));

                u.u_error = ENOENT;
                goto Exit;
            } else {
                LOG(100, ("vproc::namev: relpath (%s)\n", linkdata));

                /* Copy the whole path to workingpath. */
                strcpy(workingpath, linkdata);

                /* Release the child. */
                comp[0] = '\0';

                /* Parent stays the same. */
            }

            /* Reset the name pointers. */
            pptr = workingpath;
            plen = strlen(pptr);

            /* Skip over leading slashes. */
            SkipSlashes(&pptr, &plen);

            /* Check (again) for degenerate case of asking for root. */
            if (plen == 0) {
                *vpp = pvp;
                goto Exit;
            }

            break;
        }

        default: {
            print(GetLogFile());
            CHOKE("vproc::namev: bogus vnode type (%d)!", vp.c_type);
        }
        }
    }

Exit:
    LOG(1, ("vproc::namev: returns %s\n", VenusRetStr(u.u_error)));
    return (u.u_error == 0);
}

/* Map fid to full or volume-relative pathname.  Kind of like getwd(). */
/* XXX - Need fsobj::IsRealRoot predicate which excludes "fake" roots! -JJK */
void vproc::GetPath(VenusFid *fid, char *out, int *outlen, int fullpath)
{
    char start[CODA_MAXPATHLEN], *end = start + sizeof(start) - 1, *p;
    int len, have_last = 0, done = 0;
    VenusFid current, last;
    LOG(1, ("vproc::GetPath: %s, %d\n", FID_(fid), fullpath));

    p  = end;
    *p = '\0';

    current = *fid;
    while (!done) {
        fsobj *f = 0;

        Begin_VFS(&current, CODA_VGET);
        if (u.u_error)
            goto Exit;

        u.u_error =
            FSDB->Get(&f, &current, u.u_uid, have_last ? RC_DATA : RC_STATUS);
        if (u.u_error)
            goto FreeLocks;

        if (have_last) {
            char comp[CODA_MAXNAMLEN + 1];
            u.u_error = f->dir_LookupByFid(comp, &last);
            if (u.u_error)
                goto FreeLocks;

            /* Prefix the pathname with this latest component. */
            len = strlen(comp);
            if (p - len - 1 < start) {
                u.u_error = ENAMETOOLONG;
                goto FreeLocks;
            }
            p -= len;
            strncpy(p, comp, len);
            *(--p) = '/';
        }

        /* Termination condition is when current object is root. */
        if (FID_IsVolRoot(&current)) {
            if (fullpath && FID_EQ(&current, &rootfid)) {
                len = strlen(venusRoot);
                if (p - len < start) {
                    u.u_error = ENAMETOOLONG;
                    goto FreeLocks;
                }
                p -= len;
                strncpy(p, venusRoot, len);
                done = 1;
                goto FreeLocks;
            }

            /* can't find where we are mounted, we try to return at least
		 * as much as we've discovered up until now */
            if (!fullpath || !f->u.mtpoint) {
                len = strlen(f->vol->GetName());
                if (p - len - 2 < start)
                    u.u_error = ENAMETOOLONG;
                else {
                    *(--p) = '>';
                    p -= len;
                    strncpy(p, f->vol->GetName(), len);
                    *(--p) = '<';
                }
                done = 1;
                goto FreeLocks;
            }

            /* continue with the mountlink object */
            last    = f->u.mtpoint->fid;
            current = f->u.mtpoint->pfid;
        } else {
            last    = current;
            current = f->pfid;
        }
        have_last = 1;

    FreeLocks:
        FSDB->Put(&f);
        int retry_call = 0;
        End_VFS(&retry_call);
        if (u.u_error && !retry_call)
            break;
    }

Exit:
    len = end - p;
    if (len < *outlen) {
        strncpy(out, p, len + 1);
        *outlen = len + 1;
    } else {
        *out      = '\0';
        *outlen   = 0;
        u.u_error = ENAMETOOLONG;
    }
    LOG(1, ("vproc::GetPath: returns %s (%s)\n", VenusRetStr(u.u_error), out));
}

const char *vproc::expansion(const char *path)
{
    size_t len = strlen(path);

    if (len < 4 || path[len - 4] != '@')
        return NULL;

    if (STREQ(&path[len - 3], "cpu"))
        return CPUTYPE;

    if (STREQ(&path[len - 3], "sys"))
        return SYSTYPE;

    return NULL;
}

void vproc::verifyname(char *name, int flags)
{
    int length;

    /* Disallow '', '.', and '..' */
    if (flags & NAME_NO_DOTS) {
        if (!name[0] || /* "" */
            (name[1] == '.' && (!name[2] || /* "." */
                                (name[2] == '.' && !name[3])))) { /* ".." */
            u.u_error = EINVAL;
            return;
        }
    }

    length = strlen(name);

    /* Disallow names of the form "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ@RRRRRRRR". */
    if ((flags & NAME_NO_CONFLICT) && length > 27 && name[0] == '@' &&
        name[9] == '.' && name[18] == '.' && name[27] == '@') {
        u.u_error = EINVAL;
        return;
    }

    /* Disallow names ending in @sys or @cpu. */
    if ((flags & NAME_NO_EXPANSION) && expansion(name) != NULL) {
        u.u_error = EINVAL;
        return;
    }

    return;
}

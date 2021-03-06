/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
   Routines pertaining to pathname processing for repair tool.
   NONE of these routines have any global side effects.

   Created: M. Satyanarayanan
            October 1989
*/

#include "repcmds.h"

static char *repair_abspath(char *result, unsigned int len, char *name);
static int repair_getvid(char *path, VolumeId *vid, char *realm, char *msg,
                         int msgsize);

/* leftmost: check pathname for inconsistent object
 * path:	user-provided path of alleged object in conflict
 * realpath:	true path (sans sym links) of object in conflict
 *
 * Returns 0 iff path refers to an object in conflict and this is the
 *           leftmost such object on its true path (as returned by getwd())
 * Returns -1 on error and fills in msg if non-NULL. */
int repair_isleftmost(char *path, char *realpath, int len, char *msg,
                      int msgsize)
{
    char *car, *cdr;
    int symlinks;
    char buf[MAXPATHLEN], symbuf[MAXPATHLEN], here[MAXPATHLEN], tmp[MAXPATHLEN];

    strncpy(buf, path, sizeof(buf)); /* tentative */
    symlinks = 0;
    if (!getcwd(here, sizeof(here))) { /* remember where we are */
        strerr(msg, msgsize, "Could not get current working directory");
        return (-1);
    }

    /* simulate namei() */
    while (1) {
        /* start at beginning of buf */
        if (*buf == '/') {
            if (chdir("/") < 0) {
                strerr(msg, msgsize, "cd /: %s", strerror(errno));
                break;
            }
            car = buf + 1;
        } else
            car = buf;

        /* proceed left to right */
        while (1) {
            /* Lop off next piece */
            cdr = strchr(car, '/');
            if (!cdr) {
                /* We're at the end */
                if (repair_inconflict(car, NULL, NULL) == 0) {
                    repair_abspath(realpath, len, car);
                    if (chdir(here) < 0) {
                        strerr(msg, msgsize, "cd %s: %s", here,
                               strerror(errno));
                        break;
                    }
                    return (0);
                } else {
                    strerr(msg, msgsize, "Object not in conflict");
                    goto Exit;
                }
            }
            *cdr = 0; /* clobber slash */
            cdr++;

            /* Is this piece ok? */
            if (repair_inconflict(car, NULL, NULL) == 0) {
                strerr(msg, msgsize,
                       "%s is to the left of %s and is in conflict",
                       repair_abspath(tmp, MAXPATHLEN, car), path);
                break;
            }

            /* Is this piece a sym link? */
            if (readlink(car, symbuf, MAXPATHLEN) > 0) {
                if (++symlinks >= CODA_MAXSYMLINK) {
                    errno = ELOOP;
                    strerr(msg, msgsize, "%s: %s", path, strerror(errno));
                    goto Exit;
                }
                strcat(symbuf, "/");
                strcat(symbuf, cdr);
                strcpy(buf, symbuf);
                break; /* to outer loop, and restart scan */
            }

            /* cd to next component */
            if (chdir(car) < 0) {
                strerr(msg, msgsize, "%s: %s",
                       repair_abspath(tmp, MAXPATHLEN, car), strerror(errno));
                goto Exit;
            }

            /* Phew! Traversed another component! */
            car        = cdr;
            *(cdr - 1) = '/'; /* Restore clobbered slash */
        }
    }
Exit:
    CODA_ASSERT(!chdir(here)); /* XXXX */
    return (-1);
}

/* Obtains mount point of last volume in realpath
 * Assumes realpath has no conflicts except (possibly) last component.
 * Returns 0 on success, -1 on error and fills in msg if non-NULL
 * NULL out parameters will not be filled.
 *
 * realpath:	abs pathname (sans sym links) of replicated object
 * prefix:	part before last volume encountered in realpath
 * suffix:	part inside last volume
 * vid:	        id of last volume */
int repair_getmnt(char *realpath, char *prefix, char *suffix, VolumeId *vid,
                  char *realm, char *msg, int msgsize)
{
    char msgbuf[DEF_BUF], buf[MAXPATHLEN];
    VolumeId currvid, oldvid;
    char currrealm[MAXHOSTNAMELEN], oldrealm[MAXHOSTNAMELEN];
    char *slash;
    char *tail;
    int rc;

    /* Find abs path */
    if (*realpath != '/') {
        strerr(msg, msgsize, "%s not an absolute pathname", realpath);
        return (-1);
    }
    strcpy(buf, realpath);

    /* obtain volume id of last component */
    if (repair_getvid(buf, &oldvid, oldrealm, msgbuf, sizeof(msgbuf)) < 0) {
        strerr(msg, msgsize, "%s", msgbuf);
        return (-1);
    }

    /* Work backwards, shrinking realpath and testing if we have
       crossed a mount point -- invariant:
       - when leaving slash points to charactar before the suffix,
       which is the relative path withoin the volume, of the
       object in conflict
       -  will always point at starting char of suffix
    */
    tail  = buf + strlen(buf);
    slash = buf + strlen(buf); /* points at trailing null */
    while (1) {
        /* break the string and find nex right slash */
        slash = strrchr(buf, '/');
        CODA_ASSERT(slash); /* abs path ==> '/' guaranteed */

        /* possibility 1: ate whole path up */
        if (slash == buf)
            break;

        *slash = '\0';
        rc = repair_getvid(buf, &currvid, currrealm, msgbuf, sizeof(msgbuf));
        *slash = '/'; /* restore the nuked component */

        /* possibility 2: crossed out of Coda */
        if (rc < 0) {
            /* not in Coda probably */
            if (errno == EINVAL)
                break;
            /* this is an unacceptable error */
            strerr(msg, msgsize, "%s", msgbuf);
            return (-1);
        }

        /* possibility 3: crossed an internal Coda mount point */
        if (oldvid != currvid || strcmp(oldrealm, currrealm) != 0)
            break; /* restore slash to previous value and break */

        /* possibility 4: we are still in the same volume */
        *slash = '\0';
        /* restore the previous null */
        if (*tail != '\0')
            *(tail - 1) = '/';
        tail = slash + 1;
    }

    /* set OUT parameters */
    if (prefix)
        strcpy(prefix, buf); /* this gives us the mount point */
    if (vid)
        *vid = oldvid;
    if (realm)
        strcpy(realm, oldrealm);
    if (suffix)
        strcpy(suffix, tail);
    return (0);
}

/* Assumes no conflicts to left of last component.  This is NOT checked.
 * Returns 0 if name refers to an object in conflict and fills in conflictfid if non-NULL.
 * Returns -1 on error */
int repair_inconflict(char *name, ViceFid *conflictfid, char *conflictrealm)
{
    char symval[MAXPATHLEN];
    struct stat statbuf;
    int rc;

    rc = stat(name, &statbuf);
    if ((rc == 0) || (errno != ENOENT))
        return (-1);

    /* is it a sym link? */
    symval[0] = 0;
    rc        = readlink(name, symval, MAXPATHLEN);
    if (rc < 0)
        return (-1);

    /* it's a sym link, alright */
    if (symval[0] == '@') {
        if (conflictfid) {
            sscanf(symval, "@%x.%x.%x@", &conflictfid->Volume,
                   &conflictfid->Vnode, &conflictfid->Unique);
        }
        if (conflictrealm)
            strcpy(conflictrealm, strrchr(symval, '@') + 1);

        return (0);
    } else
        return (-1);
}

/* Fills outfid and outvv with fid and version vector for specified Coda path.
 * If version vector is not accessible, the StoreId fields of outvv are set to -1.
 * Garbage may be copied into outvv for non-replicated files
 *
 * Returns 0 on success, Returns -1 on error and fills in msg if non-null */
int repair_getfid(char *path, ViceFid *outfid, char *outrealm,
                  ViceVersionVector *outvv, char *msg, int msgsize)
{
    int rc, saveerrno;
    struct ViceIoctl vi;
    char junk[DEF_BUF];

    vi.in       = 0;
    vi.in_size  = 0;
    vi.out      = junk;
    vi.out_size = sizeof(junk);
    memset(junk, 0, sizeof(junk));

    rc        = pioctl(path, _VICEIOCTL(_VIOC_GETFID), &vi, 0);
    saveerrno = errno;

    /* Easy: no conflicts */
    if (!rc) {
        if (outfid)
            memcpy(outfid, junk, sizeof(ViceFid));
        if (outvv)
            memcpy(outvv, junk + sizeof(ViceFid), sizeof(ViceVersionVector));
        if (outrealm)
            strcpy(outrealm,
                   junk + sizeof(ViceFid) + sizeof(ViceVersionVector));
        return (0);
    }

    /* Perhaps the object is in conflict? Get fid from dangling symlink */
    rc = repair_inconflict(path, outfid, outrealm);
    if (!rc) {
        if (outvv) {
            outvv->StoreId.HostId =
                (unsigned)-1; /* indicates VV is undefined */
            outvv->StoreId.Uniquifier = (unsigned)-1;
        }
        return (0);
    }

    /* No: 'twas some other bogosity */
    if (errno != EINVAL)
        strerr(msg, msgsize, "GETFID %s: %d", path, saveerrno);
    return (-1);
}

static char *repair_abspath(char *result, unsigned int len, char *name)
{
    CODA_ASSERT(getcwd(result, len)); /* XXXX */
    CODA_ASSERT(strlen(name) + 1 <= len); /* XXXX */

    strcat(result, "/");
    strcat(result, name);
    return (result);
}

/* Returns 0 and fills volid with the volume id of path.
 * Returns -1 on error and fills in msg if non-NULL. */
static int repair_getvid(char *path, VolumeId *vid, char *realm, char *msg,
                         int msgsize)
{
    char msgbuf[DEF_BUF];
    ViceFid vfid;
    ViceVersionVector vv;

    CODA_ASSERT(0);

    if (repair_getfid(path, &vfid, realm, &vv, msgbuf, sizeof(msgbuf)) < 0) {
        strerr(msg, msgsize, "repair_getfid: %s", msgbuf);
        return (-1);
    }
    *vid = vfid.Volume;
    return (0);
}

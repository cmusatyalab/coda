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
                           none currently

#*/

/* filerepair: Takes 2 filenames as input - an inconsistent file and a
   filename representing the new contents of the file.

   Replaces the inconsistent file with the latter file.  */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "coda_string.h"
#include "coda_assert.h"
#include <errno.h>
#include <venusioctl.h>
#include <vcrcommon.h>
#include <inconsist.h>

extern int path(char *, char *, char *);
extern int wildmat(char *text, char *pattern);

#ifdef __cplusplus
}
#endif

#define ISDIR(vnode) ((vnode) & 1)  /* directory vnodes are odd */

int IsObjInc(char *name, ViceFid *fid) {
    int rc;
    char symval[MAXPATHLEN];
    struct stat statbuf;

    rc = stat(name, &statbuf);
    if ((rc == 0) || (errno != ENOENT)) return(0);
    
    /* is it a sym link */
    symval[0] = 0;
    rc = readlink(name, symval, MAXPATHLEN);
    if (rc < 0) return(0);
    
    /* it's a sym link, alright  */
    if (symval[0] == '@') {
	sscanf(symval, "@%x.%x.%x", &fid->Volume, &fid->Vnode, &fid->Unique);

	return(1);
    }
    else return(0);
}

/* Returns 0 and fills outfid and outvv with fid and version vector
   for specified Coda path.  If version vector is not accessible,
   the StoreId fields of outvv are set to -1.
   Garbage may be copied into outvv for non-replicated files
   
   Returns -1 after printing error msg on failures. */
int getfid(char *path, ViceFid *outfid, ViceVersionVector *outvv) {
    int rc, saveerrno;
    struct ViceIoctl vi;
    char junk[2048];

    vi.in = 0;
    vi.in_size = 0;
    vi.out = junk;
    vi.out_size = (short) sizeof(junk);
    memset(junk, 0, (int) sizeof(junk));

    rc = pioctl(path, VIOC_GETFID, &vi, 0);
    saveerrno = errno;

    /* Easy: no conflicts */
    if (!rc) {
	memmove((void *)outfid, (const void *)junk, (int) sizeof(ViceFid));
	memmove((void *)outvv, (const void *)junk+sizeof(ViceFid), (int)sizeof(ViceVersionVector));
	return(0);
    }

    /* if there are conflicts then can't use this object for the
       repair anyway.  A begin repair should have been done by this
       point. */
    return(-1);
}


int main(int argc, char **argv) {
    struct stat statbuf;
    int rc;
    ViceFid fixfid;
    vv_t fixvv;
    char fixpath[MAXPATHLEN];
    struct ViceIoctl vioc;
    char space[2048];

    if (argc != 3) {
	fprintf(stderr, "Usage: %s <inc-file-name> <merged-file-name>\n", argv[0]);
	exit(-1);
    }

    /*  make sure repair file exists  */
    rc = stat(argv[2], &statbuf);
    if (rc != 0) {
	fprintf(stderr, "Couldn't find %s(errno = %d)\n", argv[2], errno);
	exit(-1);
    }
    if (!(statbuf.st_mode & S_IFREG)) {
	fprintf(stderr, "File %s cannot be used for repair\n", argv[2]);
	exit(-1);
    }

    if (!getfid(argv[2], &fixfid, &fixvv))
	sprintf(fixpath, "@%x.%x.%x", fixfid.Volume, fixfid.Vnode, fixfid.Unique);
    else strcpy(fixpath, argv[2]);
	
    /* do the repair */
    vioc.in_size = (short)(1+strlen(fixpath));
    vioc.in = fixpath;
    vioc.out_size = (short)sizeof(space);
    vioc.out = space;
    memset(space, 0, sizeof(space));
    rc = pioctl(argv[1], VIOC_REPAIR, &vioc, 0);
    if (rc < 0 && errno != ETOOMANYREFS) {
	fprintf(stderr, "Error %d for repair\n", errno);
	exit(-1);
    }
	
    if (stat(argv[1], &statbuf)) 
	exit(-1);
    exit(0);
}

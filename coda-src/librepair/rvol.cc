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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#ifdef __cplusplus
}
#endif

#include "repcmds.h"

static int srvstr(char *rwpath, char *retbuf, int size);
static int volstat(char *path, char *space, int size);

/* Assumes pathname refers to a conflict
 * Allocates new repvol and returns it in repv
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int repair_newrep(char *pathname, struct repvol **repv, char *msg, int msgsize) {
    char msgbuf[DEF_BUF], reppath[MAXPATHLEN], prefix[MAXPATHLEN], suffix[MAXPATHLEN];
    VolumeId vid;

    if (repv == NULL) {
	strerr(msg, msgsize, "NULL repv");
	return(-1);
    }

    if (repair_isleftmost(pathname, reppath, MAXPATHLEN, msg, msgsize) < 0)
	return(-1);

    if (repair_getmnt(reppath, prefix, suffix, &vid, msgbuf, sizeof(msgbuf)) < 0) {
	strerr(msg, msgsize, "Could not get volume mount point: %s", msgbuf);
	return(-1);
    }

    *repv = (struct repvol *)calloc(1, sizeof(struct repvol)); /* inits all fields to 0 */
    if (*repv == NULL) { 
	strerr(msg, msgsize, "Malloc failed");
	return(-1);
    }

    sprintf((*repv)->rodir, "%s", reppath); /* remember conflict path */
    (*repv)->vid = vid;                     /* remember the volume id */
    strcpy((*repv)->mnt, prefix);           /* remember its mount point */
    sprintf((*repv)->vname, "%#lx", vid);   /* GETVOLSTAT doesn't work on rep vols, 
					     * so just use hex version of volid */
    return(0);
}

/* rwarray:	non-zero elements specify volids of replicas
 * arraylen:	how many elements in rwarray
 *
 *  Determines conflict type and fills in repv->local
 *  Allocates and links each rw replica in rwarray 
 *     (after matching against directory entries)
 *  "mount" in function name is historical
 *  Returns 0 on success, -1 on failure (after cleaning up)
 */
int repair_mountrw(struct repvol *repv, VolumeId *rwarray, int arraylen, char *msg, int msgsize) {
    char tmppath[MAXPATHLEN], buf[DEF_BUF], space[DEF_BUF], *ptr, *volname;
    int rc, i, confl, cmlcnt, err = 0;
    struct volrep *rwv, *rwtail = NULL;
    struct dirent *de;
    VolumeStatus *vs;
    DIR *d;

    if (repv == NULL) {
	strerr(msg, msgsize, "NULL repv");
	return(-1);
    }

    /* determine conflict type (local/global or server/server) */
    if (volstat(repv->rodir, buf, sizeof(buf))) {
	strerr(msg, msgsize, "VIOCGETVOLSTAT %s failed", tmppath);
	return(-1);
    }
    ptr = buf; /* invariant: ptr always point to next obj to be read */
    vs = (VolumeStatus *)ptr;
    ptr += sizeof(VolumeStatus);
    volname = ptr;
    ptr += strlen(volname) + 1 + sizeof(int); /* skip past connection state */
    memcpy(&confl, ptr, sizeof(int));
    ptr += sizeof(int);
    memcpy (&cmlcnt, ptr, sizeof(int));
    repv->local = (confl && (cmlcnt > 0)) ? 1 : 0;

    if ((d = opendir(repv->rodir)) == NULL) {
	strerr(msg, msgsize, "opendir failed: %s", strerror(errno));
	return(-1);
    }

    if (repv->local) { /* local/global conflict */

	/* rwarray is useless here, since it holds the vid's of the replicas
	 * of the volume in conflict, while the directory entries 
	 * (local and global) have the vid's of the local fake volume (0xffffffff) 
	 * and the volume itself, not the replicas */

	i = 0;
	while ((de = readdir(d)) != NULL) {
	    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;

	    snprintf(tmppath, sizeof(tmppath), "%s/%s", repv->rodir, de->d_name);
	    if (volstat(tmppath, space, sizeof(space))) {
		strerr(msg, msgsize, "VIOCGETVOLSTAT %s failed", tmppath);
		return(-1);
	    }

	    /* allocate new replica and link it in */
	    rwv = (struct volrep *)calloc(1, sizeof(struct volrep));
	    if (rwv == NULL) goto CLEANUP;
	    if (rwtail != NULL) rwtail->next = rwv;
	    else repv->rwhead = rwv;
	    rwtail = rwv;

	    /* set replica values */
	    rwv->vid = ((VolumeStatus *)space)->Vid;
	    strcpy(rwv->vname, space + sizeof(VolumeStatus));
	    strcpy(rwv->compname, de->d_name);

	    if (strcmp(de->d_name, "global") == 0) { /* global entry */
		if ((vs->Vid != rwv->vid)
		    || (strcmp(rwv->vname, volname) != 0)) {
		    strerr(msg, msgsize, "Entry mismatch");
		    goto CLEANUP;
		}
		strcpy(rwv->srvname, "global"); /* XXXX */
	    }
	    else if (strcmp(de->d_name, "local") == 0) { /* local entry */
		if ((strcmp(rwv->vname, "A_Local_Fake_Volume") != 0)
		    || (rwv->vid != 0xffffffff)) {
		    strerr(msg, msgsize, "Entry mismatch");
		    goto CLEANUP;
		}
		strcpy(rwv->srvname, "localhost"); /* XXXX */
	    }
	    else { /* problem */
		strerr(msg, msgsize, "Unexpected entry \"%s\"", de->d_name);
		goto CLEANUP;
	    }
	    i++;
	}
	if (i != 2) {
	    strerr(msg, msgsize, "Too %s directory entries", ((i < 2) ? "few" : "many"));
	    goto CLEANUP;
	}
    }
    else { /* server/server conflict */

	while ((de = readdir(d)) != NULL) {
	    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;

	    snprintf(tmppath, sizeof(tmppath), "%s/%s", repv->rodir, de->d_name);
	    if (volstat(tmppath, space, sizeof(space))) {
		strerr(msg, msgsize, "VIOCGETVOLSTAT %s failed", tmppath);
		return(-1);
	    }

	    for (i = 0; i < arraylen; i++) { /* find the array entry for this de */
		if (!rwarray[i]) continue;  /* pioctl output need not be contiguous */
		if (rwarray[i] == ((VolumeStatus *)space)->Vid) { /* found it! */

		    /* allocate new replica and link it in */
		    rwv = (struct volrep *)calloc(1, sizeof(struct volrep));
		    if (rwv == NULL) goto CLEANUP;
		    if (rwtail != NULL) rwtail->next = rwv;
		    else repv->rwhead = rwv;
		    rwtail = rwv;

		    /* set replica values */
		    rwv->vid = ((VolumeStatus *)space)->Vid;
		    strcpy(rwv->vname, space + sizeof(VolumeStatus));
		    strcpy(rwv->compname, de->d_name);
		    if (srvstr(tmppath, rwv->srvname, sizeof(rwv->srvname)) < 0)
			goto CLEANUP;

		    rwarray[i] = 0; /* blank this array entry */
		    break;
		}
	    }
	    if (i == arraylen) {
		strerr(msg, msgsize, "No such replica vid=0x%x", ((VolumeStatus *)space)->Vid);
		goto CLEANUP;
	    }
	}

    }

    if (closedir(d) < 0) {
	strerr(msg, msgsize, "closedir failed: %s", strerror(errno));
	d = NULL;
	goto CLEANUP;
    }

    return(0);

 CLEANUP:
    while ((rwv = repv->rwhead) != NULL) {
	repv->rwhead = rwv->next;
	free(rwv);
    }
    if (d != NULL) closedir(d);
    return(-1);
}

/* Frees all data structures associated with repv. */
void repair_finish(struct repvol *repv) {
    struct volrep *rwv, *next;

    if (repv == NULL) {
	printf("Error:  trying to free null repvol\n");
	exit(2);
    }
    rwv = repv->rwhead;
    while (rwv)	{
	next = rwv->next;
	free(rwv);
	rwv = next;
    }
    free(repv);
}

/* fills in retbuf with string identifying the server housing replica 
 * retbuf contains error message if pioctl (or something else) fails
 * returns 0 on success, -1 on failure */    
static int srvstr(char *rwpath, char *retbuf, int size) {
    struct ViceIoctl vioc;
    struct hostent *thp;
    char junk[DEF_BUF];
    long *hosts;
    int rc;

    /* get the server name by doing the pioctl (for compatibility with old venii) */
    vioc.in = NULL;
    vioc.in_size = 0;
    vioc.out = junk;
    vioc.out_size = sizeof(junk);
    memset(junk, 0, sizeof(junk));
    if (rc = pioctl(rwpath, VIOCWHEREIS, &vioc, 1)) return(-1);
    hosts = (long *)junk;
    memset(retbuf, 0, size);
    if (hosts[0] == 0) return(-1); /* fail if no hosts returned */
    if (hosts[1] != 0) return(-1); /* fail if more than one host returned */

    hosts[1] = htonl(hosts[0]); /* gethostbyaddr requires network order */

    thp = gethostbyaddr((char *)&hosts[1], sizeof(long), AF_INET);
    if (thp != NULL) snprintf(retbuf, size, "%s", thp->h_name);
    else snprintf(retbuf, size, "%08lx", hosts[1]);
    return(0);
}

static int volstat(char *path, char *space, int size) {
    struct ViceIoctl vioc;
    memset(space, 0, size);
    vioc.in = NULL;
    vioc.in_size = 0;
    vioc.out = space;
    vioc.out_size = size;
    return(pioctl(path, VIOCGETVOLSTAT, &vioc, 1));
}

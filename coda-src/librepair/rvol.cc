/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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

/* Assumes pathname refers to a conflict object
 * Allocates new conflict and returns it in conf
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int repair_newrep(char *pathname, struct conflict **conf, char *msg, int msgsize) {
    char msgbuf[DEF_BUF], reppath[MAXPATHLEN], prefix[MAXPATHLEN], suffix[MAXPATHLEN];
    char realm[MAXHOSTNAMELEN];

    if (conf == NULL) {
	strerr(msg, msgsize, "NULL conf");
	return(-1);
    }

    /* XXX: The importance of being leftmost needs to be looked at. -Adam */
#if 0
    if (repair_isleftmost(pathname, reppath, MAXPATHLEN, msg, msgsize) < 0) {
	strerr(msg, msgsize, "pathname not leftmost");
	return(-1);
    }
#endif

    *conf = (struct conflict *)calloc(1, sizeof(struct conflict)); /* inits all fields to 0 */
    if (*conf == NULL) {
	strerr(msg, msgsize, "Malloc failed");
	return(-1);
    }

    sprintf((*conf)->rodir, "%s", pathname); /* remember conflict path */
    return(0);
}

/*
 *  Determines conflict type and fills in conflict->local
 *  Allocates and links each replica based on directory entries
 *  "mount" and "rw" in function name are historical (neither true)
 *  Returns 0 on success, -1 on failure (after cleaning up)
 */
int repair_mountrw(struct conflict *conf, char *msg, int msgsize) {
    char tmppath[MAXPATHLEN], buf[DEF_BUF], space[DEF_BUF], *ptr, *volname;
    int i, confl, cmlcnt;
    struct replica *rwv, *rwtail = NULL;
    struct dirent *de;
    DIR *d;

    if (conf == NULL) {
	strerr(msg, msgsize, "NULL conflict");
	return(-1);
    }

    if ((d = opendir(conf->rodir)) == NULL) {
      printf("Oh shit! opendir:%s\n",conf->rodir);
      strerr(msg, msgsize, "opendir failed: %s", strerror(errno));
      return(-1);
    }

    while ((de = readdir(d)) != NULL) {
      if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..") ||
	  !strcmp(de->d_name, ".localcache"))
	continue;

      /* allocate new replica and link it in */
      rwv = (struct replica *)calloc(1, sizeof(struct replica));
      if (rwv == NULL) goto CLEANUP;
      if (rwtail != NULL) rwtail->next = rwv;
      else conf->head = rwv;
      rwtail = rwv;

#if 0 /* XXX: this seems to be done in BeginRepair more correctly */
      if(!strcmp(de->d_name, "_localcache"))
	conf->local = 1;
#endif

      snprintf(tmppath, sizeof(tmppath), "%s/%s", conf->rodir, de->d_name);

      /* set replica values */
      if (repair_getfid(tmppath, &rwv->fid, rwv->realmname, NULL,
			msg, msgsize) < 0) {
        printf("Oh shit! getfid %s!\n",tmppath);
	goto CLEANUP;
      }

      strcpy(rwv->compname, de->d_name);

      if(!strcmp(de->d_name, "_localcache")) {
	strcpy(rwv->srvname,"localhost");
      }
      else if (srvstr(tmppath, rwv->srvname, sizeof(rwv->srvname)) < 0) {
        printf("Oh shit! srvstr:%s!\n",tmppath);
	goto CLEANUP;
      }
    }

    if (closedir(d) < 0) {
      printf("Oh shit! closedir!\n");
      strerr(msg, msgsize, "closedir failed: %s", strerror(errno));
      d = NULL;
      goto CLEANUP;
    }

    return(0);

 CLEANUP:
    printf("Oh shit! Cleaning\n");
    while ((rwv = conf->head) != NULL) {
      conf->head = rwv->next;
      free(rwv);
    }
    if (d != NULL) closedir(d);
    return(-1);
}

/* Frees all data structures associated with repv. */
void repair_finish(struct conflict *conf)
{
    struct replica *rwv;

    if (conf == NULL) {
	printf("Error:  trying to free null conflict\n");
	exit(2);
    }
    while ((rwv = conf->head) != NULL) {
	conf->head = rwv->next;
	free(rwv);
    }
    free(conf);
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
    rc = pioctl(rwpath, _VICEIOCTL(_VIOCWHEREIS), &vioc, 1);
    if (rc) return(-1);
    hosts = (long *)junk;
    memset(retbuf, 0, size);
    if (hosts[0] == 0) return(-1); /* fail if no hosts returned */
    if (hosts[1] != 0) return(-1); /* fail if more than one host returned */

    thp = gethostbyaddr((char *)&hosts[0], sizeof(long), AF_INET);
    if (thp != NULL) snprintf(retbuf, size, "%s", thp->h_name);
    else snprintf(retbuf, size, "%s", inet_ntoa(*(struct in_addr *)&hosts[0]));
    return(0);
}

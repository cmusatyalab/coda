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

/* Assumes pathname refers to a conflict object
 * Allocates new conflict and returns it in conf
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int repair_newrep(char *pathname, struct conflict **conf, char *msg, int msgsize) {
  char reppath[MAXPATHLEN];

    if (conf == NULL) {
	strerr(msg, msgsize, "NULL conf");
	return(-1);
    }

    if (repair_isleftmost(pathname, reppath, MAXPATHLEN, msg, msgsize) < 0) {
	strerr(msg, msgsize, "pathname not leftmost");
	return(-1);
    }

    *conf = (struct conflict *)calloc(1, sizeof(struct conflict));
    if (*conf == NULL) {
	strerr(msg, msgsize, "Malloc failed");
	return(-1);
    }

    sprintf((*conf)->rodir, "%s", pathname); /* remember expanded directory */
    return(0);
}

/*
 *  Determines conflict type and fills in conflict->local
 *  Allocates and links each replica based on directory entries
 *  "mount" and "rw" in function name are historical (neither true)
 *  Returns 0 on success, -1 on failure (after cleaning up)
 */
int repair_mountrw(struct conflict *conf, char *msg, int msgsize) {
    char tmppath[MAXPATHLEN];
    struct replica *rwv, *rwtail = NULL;
    struct dirent *de;
    DIR *d;

    if (conf == NULL) {
	strerr(msg, msgsize, "NULL conflict");
	return(-1);
    }

    if ((d = opendir(conf->rodir)) == NULL) {
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

#if 0 /* XXX: this could be useful if REP_CMD_BEGIN ever changes its ret val */
      if(!strcmp(de->d_name, "_localcache"))
	conf->local = 1;
#endif

      snprintf(tmppath, sizeof(tmppath), "%s/%s", conf->rodir, de->d_name);

      /* set replica values */
      if (repair_getfid(tmppath, &rwv->fid, rwv->realmname, &rwv->VV,
			msg, msgsize) < 0) {
	goto CLEANUP;
      }

      strcpy(rwv->compname, de->d_name);

      if(!strcmp(de->d_name, "_localcache"))
	strcpy(rwv->srvname, "localhost");
      else
	strcpy(rwv->srvname, de->d_name); /* guaranteed to be correct */
    }

    if (closedir(d) < 0) {
      strerr(msg, msgsize, "closedir failed: %s", strerror(errno));
      d = NULL;
      goto CLEANUP;
    }

    return(0);

 CLEANUP:
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

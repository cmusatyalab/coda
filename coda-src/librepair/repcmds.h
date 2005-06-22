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

#ifndef _REPCMDS_H_
#define _REPCMDS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <inodeops.h>
#include <netdb.h>
#include <netinet/in.h>
#include <parser.h>
#include <rpc2/rpc2.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <auth2.h>
#include <avenus.h>
#include "coda_assert.h"
#include "coda_string.h"
#include <vice.h>

#include <copyfile.h>

#ifdef __cplusplus
}
#endif

#include <inconsist.h>
#include <repio.h>
#include <resolve.h>
#include <venusioctl.h>

#define MAXVOLNAME     32
#define MAXHOSTS       8     /* XXXX --- get the true definition for this */
#define HOSTNAMLEN     64    /* XXXX -- get the true definition for this */
/* MAXPATHLEN defined in <sys/param.h>, length in bytes of OUT path parameters */
#define DEF_BUF        2048  /* XXXX -- temporary buffer size */

int rep_ioctl(struct ViceIoctl *vioc);

/* Replicated volume under repair */
struct conflict {
    struct replica *head;  /* Singly-linked list of volume replicas */
    char rodir[MAXPATHLEN]; /* directory where replicas are mounted */
    char local;	  /* a flag indicating whether this is a local volume entry
		   * 0: server/server conflict
		   * 1: local/global conflict
		   * 2: mixed lg/ss */
    char dirconf; /* a flag indicating whether this is a directory conflict
		   *  -- if true it's a directory conflict, else a file conflict */
};

/* Element in singly-linked list of volume replicas of a replicated volume */
struct replica {
    ViceFid fid;                  /* fid of this replica*/
    char realmname[HOSTNAMLEN];   /* realm name of this object */
    char srvname[HOSTNAMLEN]; /* XXX: name of server on which this replica is located? */
    char compname[MAXNAMLEN]; /* component name corresponding to this rw id */
    struct replica *next;     /* next replica ptr */
};

/* Non-interactive repair calls */
int BeginRepair(char *pathname, struct conflict **conf, char *msg, int msgsize);
int ClearInc(struct conflict *conf, char *msg, int msgsize);
int CompareDirs(struct conflict *conf, char *fixfile, struct repinfo *inf, char *msg, int msgsize);
int DiscardAllLocal(struct conflict *conf, char *msg, int msgsize);
int DoRepair(struct conflict *conf, char *ufixpath, FILE *res, char *msg, int msgsize);
int EndRepair(struct conflict *conf, int commit, char *msg, int msgsize);
int RemoveInc(struct conflict *conf, char *msg, int msgsize);

/* Other utility functions */
int dorep(struct conflict *conf, char *fixpath, char *buf, int len);
int glexpand(char *rodir, char *fixfile, char *msg, int msgsize);
int makedff(char *extfile, char *intfile, char *msg, int msgsize);

/* Volume data structure manipulation routines -- rvol.cc */
int  repair_newrep(char *reppath, struct conflict **conf, char *msg, int msgsize);
int  repair_mountrw(struct conflict *conf, char *msg, int msgsize);
void repair_finish(struct conflict *conf);

/* Path processing routines -- path.cc */
int  repair_getfid(char *path, ViceFid *outfid, char *outrealm, ViceVersionVector *outvv, char *msg, int msgsize);
int  repair_inconflict(char *name, ViceFid *conflictfid, char *conflictrealm);

#define freeif(pointer)				\
do {						\
 if (pointer != NULL) {				\
   free(pointer);				\
   pointer = NULL;				\
 } 						\
} while (0)

#define strerr(str, len, msg...)		\
do {						\
  if (str != NULL) 				\
    snprintf(str, len, ##msg);			\
} while (0)

#endif /* _REPCMDS_H_ */

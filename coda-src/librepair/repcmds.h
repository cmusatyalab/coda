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

#define MAXVOLNAME     100   /* length in bytes of OUT volume parameters */
#define MAXHOSTS       8     /* XXXX --- get the true definition for this */
#define HOSTNAMLEN     64    /* XXXX -- get the true definition for this */
/* MAXPATHLEN defined in <sys/param.h>, length in bytes of OUT path parameters */
#define DEF_BUF        2048  /* XXXX -- temporary buffer size */

/* Replicated volume under repair */
struct repvol {
    VolumeId vid;           /* id of this volume */
    struct volrep *rwhead;  /* Singly-linked list of volume replicas */
    char vname[MAXVOLNAME]; /* name of this volume */
    char mnt[MAXPATHLEN];   /* permanent mount point in Coda */
    char rodir[MAXPATHLEN]; /* directory where replicas are mounted */
    char local;	  /* a flag indicating whether this is a local volume entry 
		   *  -- if true it's a local/global conflict, else server/server */
    char dirconf; /* a flag indicating whether this is a directory conflict
		   *  -- if true it's a directory conflict, else a file conflict */
};

/* Element in singly-linked list of volume replicas of a replicated volume */
struct volrep {
    VolumeId vid;             /* id of this volume */
    struct volrep *next;      /* next element ptr */
    char vname[MAXVOLNAME];   /* name of this volume */
    char srvname[HOSTNAMLEN]; /* name of server on which this rw volume is located */
    char compname[MAXNAMLEN]; /* component name corresponding to this rw id */
};

/* Non-interactive repair calls */
int BeginRepair(char *pathname, struct repvol **repv, char *msg, int msgsize);
int ClearInc(struct repvol *repv, char *msg, int msgsize);
int CompareDirs(struct repvol *repv, char *fixfile, struct repinfo *inf, char *msg, int msgsize);
int DiscardAllLocal(struct repvol *repv, char *msg, int msgsize);
int DoRepair(struct repvol *repv, char *ufixpath, FILE *res, char *msg, int msgsize);
int EndRepair(struct repvol *repv, int commit, char *msg, int msgsize);
int RemoveInc(struct repvol *repv, char *msg, int msgsize);

/* Other utility functions */
int dorep(struct repvol *repv, char *fixpath, char *buf, int len);
int glexpand(char *rodir, char *fixfile, char *msg, int msgsize);
int makedff(char *extfile, char *intfile, char *msg, int msgsize);

/* Volume data structure manipulation routines -- rvol.cc */
int  repair_cleanup(struct repvol *repv);
int  repair_countRWReplicas (struct repvol *repv);
void repair_finish(struct repvol *repv);
int  repair_getfid(char *path, VenusFid *outfid, ViceVersionVector *outvv, char *msg, int msgsize);
int  repair_mountrw(struct repvol *repv, VolumeId *rwarray, int arraylen, char *msg, int msgsize);
int  repair_newrep(char *reppath, struct repvol **repv, char *msg, int msgsize);

/* Path processing routines -- path.cc */
int  repair_getfid(char *path, VenusFid *outfid, ViceVersionVector *outvv, char *msg, int msgsize);
int  repair_getmnt(char *realpath, char *prefix, char *suffix, VolumeId *vid, char *msg, int msgsize);
int  repair_inconflict(char *name, VenusFid *conflictfid);
int  repair_isleftmost(char *path, char *realpath, int len, char *msg, int msgsize);

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
